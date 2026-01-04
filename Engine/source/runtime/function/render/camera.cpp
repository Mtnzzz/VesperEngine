#include "camera.h"

#include <cmath>
#include <algorithm>

namespace vesper {

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float PITCH_LIMIT = PI * 0.5f - 0.01f;  // Almost 90 degrees
}

Camera::Camera()
{
    m_rotation = Quaternion::IDENTITY;
}

Camera::Camera(const CameraConfig& config)
    : m_config(config)
{
    m_rotation = Quaternion::IDENTITY;
}

void Camera::setPosition(const Vector3& position)
{
    m_position = position;
    markViewDirty();
}

void Camera::setRotation(const Quaternion& rotation)
{
    m_rotation = rotation;
    // TODO: Extract pitch/yaw from quaternion for consistency
    markViewDirty();
}

void Camera::setRotationEuler(float pitch, float yaw, float roll)
{
    m_pitch = pitch;
    m_yaw = yaw;
    updateRotationFromEuler();
    markViewDirty();
}

void Camera::lookAt(const Vector3& target, const Vector3& up)
{
    Vector3 forward = (target - m_position).normalized();

    // Calculate yaw and pitch from forward direction
    // This sets the camera rotation to look at the target
    m_yaw = std::atan2(forward.x, forward.z);
    m_pitch = std::asin(-forward.y);

    updateRotationFromEuler();
    markViewDirty();
}

void Camera::setPerspective(float fovY, float nearPlane, float farPlane)
{
    m_config.projection = CameraProjection::Perspective;
    m_config.fovY = fovY;
    m_config.nearPlane = nearPlane;
    m_config.farPlane = farPlane;
    markProjDirty();
}

void Camera::setOrthographic(float size, float nearPlane, float farPlane)
{
    m_config.projection = CameraProjection::Orthographic;
    m_config.orthoSize = size;
    m_config.nearPlane = nearPlane;
    m_config.farPlane = farPlane;
    markProjDirty();
}

void Camera::setAspectRatio(float aspectRatio)
{
    if (m_aspectRatio != aspectRatio)
    {
        m_aspectRatio = aspectRatio;
        markProjDirty();
    }
}

void Camera::setClipPlanes(float nearPlane, float farPlane)
{
    m_config.nearPlane = nearPlane;
    m_config.farPlane = farPlane;
    markProjDirty();
}

Matrix4x4 Camera::getViewMatrix() const
{
    if (m_viewDirty)
    {
        // Always compute target from current rotation to ensure consistency
        // This avoids issues when camera moves past the original lookAt target
        Vector3 target = m_position + getForward();
        m_viewMatrix = Matrix4x4::lookAtLH(m_position, target, Vector3(0.0f, 1.0f, 0.0f));

        m_viewDirty = false;
    }
    return m_viewMatrix;
}

Matrix4x4 Camera::getProjectionMatrix() const
{
    if (m_projDirty)
    {
        if (m_config.projection == CameraProjection::Perspective)
        {
            m_projectionMatrix = Matrix4x4::perspectiveFovLH(
                m_config.fovY,
                m_aspectRatio,
                m_config.nearPlane,
                m_config.farPlane
            );
        }
        else
        {
            float height = m_config.orthoSize * 2.0f;
            float width = height * m_aspectRatio;
            m_projectionMatrix = Matrix4x4::orthographicLH(
                width,
                height,
                m_config.nearPlane,
                m_config.farPlane
            );
        }

        // Vulkan clip space has Y-down (opposite to DirectX/OpenGL)
        // Flip Y-axis by negating [1][1] element
        m_projectionMatrix[1][1] *= -1.0f;

        m_projDirty = false;
    }
    return m_projectionMatrix;
}

Matrix4x4 Camera::getViewProjectionMatrix() const
{
    return getViewMatrix() * getProjectionMatrix();
}

Vector3 Camera::getForward() const
{
    // Forward is +Z in left-handed coordinate system
    return m_rotation * Vector3(0, 0, 1);
}

Vector3 Camera::getRight() const
{
    return m_rotation * Vector3(1, 0, 0);
}

Vector3 Camera::getUp() const
{
    return m_rotation * Vector3(0, 1, 0);
}

void Camera::move(const Vector3& delta)
{
    m_position = m_position + delta;
    markViewDirty();
}

void Camera::moveLocal(const Vector3& delta)
{
    // Transform delta from local to world space
    Vector3 worldDelta = getRight() * delta.x + getUp() * delta.y + getForward() * delta.z;
    move(worldDelta);
}

void Camera::rotate(float deltaPitch, float deltaYaw)
{
    m_pitch += deltaPitch;
    m_yaw += deltaYaw;

    // Clamp pitch to prevent flipping
    m_pitch = std::clamp(m_pitch, -PITCH_LIMIT, PITCH_LIMIT);

    updateRotationFromEuler();
    markViewDirty();
}

void Camera::updateRotationFromEuler()
{
    // Create rotation from yaw (Y) then pitch (X)
    // Order: Yaw first, then Pitch (FPS-style camera)
    Quaternion yawQuat = Quaternion::rotationY(m_yaw);
    Quaternion pitchQuat = Quaternion::rotationX(m_pitch);

    // Combined rotation: first yaw, then pitch
    m_rotation = yawQuat * pitchQuat;
}

} // namespace vesper
