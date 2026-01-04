#pragma once

#include "runtime/core/math/vector3.h"
#include "runtime/core/math/matrix4x4.h"
#include "runtime/core/math/quaternion.h"

namespace vesper {

/// @brief Camera projection type
enum class CameraProjection
{
    Perspective,
    Orthographic
};

/// @brief Camera configuration
struct CameraConfig
{
    // Perspective settings
    float fovY      = 45.0f * (3.14159f / 180.0f);  // Field of view in radians
    float nearPlane = 0.1f;
    float farPlane  = 1000.0f;

    // Orthographic settings
    float orthoSize = 10.0f;  // Half-height of orthographic view

    CameraProjection projection = CameraProjection::Perspective;
};

/// @brief Camera class for view and projection matrix management
/// Pure data class - input handling should be done externally
class Camera
{
public:
    Camera();
    explicit Camera(const CameraConfig& config);

    // =========================================================================
    // Transform
    // =========================================================================

    /// @brief Set camera position in world space
    void setPosition(const Vector3& position);

    /// @brief Set camera rotation using quaternion
    void setRotation(const Quaternion& rotation);

    /// @brief Set camera rotation using Euler angles (in radians)
    /// @param pitch Rotation around X axis
    /// @param yaw Rotation around Y axis
    /// @param roll Rotation around Z axis
    void setRotationEuler(float pitch, float yaw, float roll);

    /// @brief Orient camera to look at a target point
    /// @param target Point to look at
    /// @param up Up vector (default: Y-up)
    void lookAt(const Vector3& target, const Vector3& up = Vector3(0, 1, 0));

    // =========================================================================
    // Projection
    // =========================================================================

    /// @brief Set perspective projection parameters
    void setPerspective(float fovY, float nearPlane, float farPlane);

    /// @brief Set orthographic projection parameters
    void setOrthographic(float size, float nearPlane, float farPlane);

    /// @brief Set aspect ratio (width / height)
    void setAspectRatio(float aspectRatio);

    /// @brief Set near and far planes
    void setClipPlanes(float nearPlane, float farPlane);

    // =========================================================================
    // Matrix Getters
    // =========================================================================

    /// @brief Get view matrix (world to camera transform)
    Matrix4x4 getViewMatrix() const;

    /// @brief Get projection matrix
    Matrix4x4 getProjectionMatrix() const;

    /// @brief Get combined view-projection matrix
    Matrix4x4 getViewProjectionMatrix() const;

    // =========================================================================
    // Accessors
    // =========================================================================

    const Vector3& getPosition() const { return m_position; }
    const Quaternion& getRotation() const { return m_rotation; }
    float getAspectRatio() const { return m_aspectRatio; }
    float getFovY() const { return m_config.fovY; }
    float getNearPlane() const { return m_config.nearPlane; }
    float getFarPlane() const { return m_config.farPlane; }
    CameraProjection getProjectionType() const { return m_config.projection; }

    /// @brief Get camera forward direction (negative Z in camera space)
    Vector3 getForward() const;

    /// @brief Get camera right direction (positive X in camera space)
    Vector3 getRight() const;

    /// @brief Get camera up direction (positive Y in camera space)
    Vector3 getUp() const;

    // =========================================================================
    // Movement Helpers
    // =========================================================================

    /// @brief Move camera in world space
    void move(const Vector3& delta);

    /// @brief Move camera in local/camera space
    void moveLocal(const Vector3& delta);

    /// @brief Rotate camera by delta angles (FPS-style)
    /// @param deltaPitch Change in pitch (up/down)
    /// @param deltaYaw Change in yaw (left/right)
    void rotate(float deltaPitch, float deltaYaw);

    /// @brief Get current pitch angle in radians
    float getPitch() const { return m_pitch; }

    /// @brief Get current yaw angle in radians
    float getYaw() const { return m_yaw; }

private:
    void updateRotationFromEuler();
    void markViewDirty() { m_viewDirty = true; }
    void markProjDirty() { m_projDirty = true; }

private:
    Vector3     m_position{0, 0, 0};
    Quaternion  m_rotation;

    // Euler angles for FPS-style camera control
    float       m_pitch = 0.0f;  // Rotation around X (up/down)
    float       m_yaw   = 0.0f;  // Rotation around Y (left/right)

    CameraConfig m_config;
    float        m_aspectRatio = 16.0f / 9.0f;

    // Cached matrices (mutable for lazy evaluation)
    mutable Matrix4x4 m_viewMatrix;
    mutable Matrix4x4 m_projectionMatrix;
    mutable bool      m_viewDirty = true;
    mutable bool      m_projDirty = true;
};

} // namespace vesper
