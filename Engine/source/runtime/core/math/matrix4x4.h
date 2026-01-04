#pragma once

#include "runtime/core/math/math_types.h"
#include "runtime/core/math/vector3.h"
#include "runtime/core/math/vector4.h"
#include "runtime/core/math/quaternion.h"

#include <cassert>

namespace vesper {

/// @brief 4x4 Matrix class with DirectXMath backend
/// @note Uses row-major storage like DirectXMath (row vectors, left-to-right multiplication)
/// This is the opposite of OpenGL/GLM which uses column-major.
/// Matrix * Vector means the vector is treated as a row vector on the left.
class Matrix4x4
{
public:
    // Row-major storage: m[row][col]
    float m[4][4];

public:
    Matrix4x4()
    {
        *this = IDENTITY;
    }

    Matrix4x4(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33)
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
        m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
    }

    explicit Matrix4x4(const float* arr)
    {
        std::memcpy(m, arr, sizeof(float) * 16);
    }

    Matrix4x4(const Vector4& row0, const Vector4& row1, const Vector4& row2, const Vector4& row3)
    {
        m[0][0] = row0.x; m[0][1] = row0.y; m[0][2] = row0.z; m[0][3] = row0.w;
        m[1][0] = row1.x; m[1][1] = row1.y; m[1][2] = row1.z; m[1][3] = row1.w;
        m[2][0] = row2.x; m[2][1] = row2.y; m[2][2] = row2.z; m[2][3] = row2.w;
        m[3][0] = row3.x; m[3][1] = row3.y; m[3][2] = row3.z; m[3][3] = row3.w;
    }

    // DirectXMath conversion
    explicit Matrix4x4(DirectX::FXMMATRIX mat)
    {
        DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(m), mat);
    }

    DirectX::XMMATRIX toXMMatrix() const
    {
        return DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(m));
    }

    static Matrix4x4 fromXMMatrix(DirectX::FXMMATRIX mat)
    {
        Matrix4x4 result;
        DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(result.m), mat);
        return result;
    }

    // Accessors
    float* operator[](size_t row)
    {
        assert(row < 4);
        return m[row];
    }

    const float* operator[](size_t row) const
    {
        assert(row < 4);
        return m[row];
    }

    float* ptr() { return &m[0][0]; }
    const float* ptr() const { return &m[0][0]; }

    Vector4 getRow(size_t row) const
    {
        assert(row < 4);
        return Vector4(m[row][0], m[row][1], m[row][2], m[row][3]);
    }

    Vector4 getColumn(size_t col) const
    {
        assert(col < 4);
        return Vector4(m[0][col], m[1][col], m[2][col], m[3][col]);
    }

    void setRow(size_t row, const Vector4& v)
    {
        assert(row < 4);
        m[row][0] = v.x; m[row][1] = v.y; m[row][2] = v.z; m[row][3] = v.w;
    }

    void setColumn(size_t col, const Vector4& v)
    {
        assert(col < 4);
        m[0][col] = v.x; m[1][col] = v.y; m[2][col] = v.z; m[3][col] = v.w;
    }

    // Comparison
    bool operator==(const Matrix4x4& rhs) const
    {
        return std::memcmp(m, rhs.m, sizeof(float) * 16) == 0;
    }
    bool operator!=(const Matrix4x4& rhs) const { return !(*this == rhs); }

    // Matrix multiplication
    Matrix4x4 operator*(const Matrix4x4& rhs) const
    {
        return fromXMMatrix(DirectX::XMMatrixMultiply(toXMMatrix(), rhs.toXMMatrix()));
    }

    Matrix4x4& operator*=(const Matrix4x4& rhs)
    {
        *this = *this * rhs;
        return *this;
    }

    // Scalar operations
    Matrix4x4 operator*(float scalar) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = m[i][j] * scalar;
        return result;
    }

    Matrix4x4 operator+(const Matrix4x4& rhs) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = m[i][j] + rhs.m[i][j];
        return result;
    }

    Matrix4x4 operator-(const Matrix4x4& rhs) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = m[i][j] - rhs.m[i][j];
        return result;
    }

    // Transform a point (w=1)
    Vector3 transformPoint(const Vector3& v) const
    {
        DirectX::XMVECTOR point = DirectX::XMVectorSet(v.x, v.y, v.z, 1.0f);
        DirectX::XMVECTOR result = DirectX::XMVector3TransformCoord(point, toXMMatrix());
        return Vector3::fromXMVector(result);
    }

    // Transform a direction (w=0)
    Vector3 transformDirection(const Vector3& v) const
    {
        DirectX::XMVECTOR dir = DirectX::XMVectorSet(v.x, v.y, v.z, 0.0f);
        DirectX::XMVECTOR result = DirectX::XMVector3TransformNormal(dir, toXMMatrix());
        return Vector3::fromXMVector(result);
    }

    // Transform Vector4
    Vector4 transform(const Vector4& v) const
    {
        DirectX::XMVECTOR vec = v.toXMVector();
        DirectX::XMVECTOR result = DirectX::XMVector4Transform(vec, toXMMatrix());
        return Vector4::fromXMVector(result);
    }

    // Matrix operations
    Matrix4x4 transpose() const
    {
        return fromXMMatrix(DirectX::XMMatrixTranspose(toXMMatrix()));
    }

    Matrix4x4 inverse() const
    {
        DirectX::XMVECTOR det;
        return fromXMMatrix(DirectX::XMMatrixInverse(&det, toXMMatrix()));
    }

    float determinant() const
    {
        return DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(toXMMatrix()));
    }

    // Extract translation
    Vector3 getTranslation() const
    {
        return Vector3(m[3][0], m[3][1], m[3][2]);
    }

    void setTranslation(const Vector3& v)
    {
        m[3][0] = v.x;
        m[3][1] = v.y;
        m[3][2] = v.z;
    }

    // Extract scale (assumes no shear)
    Vector3 getScale() const
    {
        float sx = Vector3(m[0][0], m[0][1], m[0][2]).length();
        float sy = Vector3(m[1][0], m[1][1], m[1][2]).length();
        float sz = Vector3(m[2][0], m[2][1], m[2][2]).length();
        return Vector3(sx, sy, sz);
    }

    // Decompose into translation, rotation, scale
    void decompose(Vector3& translation, Quaternion& rotation, Vector3& scale) const
    {
        DirectX::XMVECTOR s, r, t;
        DirectX::XMMatrixDecompose(&s, &r, &t, toXMMatrix());
        scale = Vector3::fromXMVector(s);
        rotation = Quaternion::fromXMVector(r);
        translation = Vector3::fromXMVector(t);
    }

    // ==========================================================================
    // Factory Functions (all using DirectXMath - row-major, left-handed by default)
    // ==========================================================================

    static Matrix4x4 identity()
    {
        return fromXMMatrix(DirectX::XMMatrixIdentity());
    }

    static Matrix4x4 translation(float x, float y, float z)
    {
        return fromXMMatrix(DirectX::XMMatrixTranslation(x, y, z));
    }

    static Matrix4x4 translation(const Vector3& v)
    {
        return translation(v.x, v.y, v.z);
    }

    static Matrix4x4 scaling(float x, float y, float z)
    {
        return fromXMMatrix(DirectX::XMMatrixScaling(x, y, z));
    }

    static Matrix4x4 scaling(const Vector3& v)
    {
        return scaling(v.x, v.y, v.z);
    }

    static Matrix4x4 scaling(float uniformScale)
    {
        return scaling(uniformScale, uniformScale, uniformScale);
    }

    static Matrix4x4 rotationX(float angle)
    {
        return fromXMMatrix(DirectX::XMMatrixRotationX(angle));
    }

    static Matrix4x4 rotationY(float angle)
    {
        return fromXMMatrix(DirectX::XMMatrixRotationY(angle));
    }

    static Matrix4x4 rotationZ(float angle)
    {
        return fromXMMatrix(DirectX::XMMatrixRotationZ(angle));
    }

    static Matrix4x4 rotationAxis(const Vector3& axis, float angle)
    {
        return fromXMMatrix(DirectX::XMMatrixRotationAxis(axis.toXMVector(), angle));
    }

    static Matrix4x4 rotationQuaternion(const Quaternion& q)
    {
        return fromXMMatrix(DirectX::XMMatrixRotationQuaternion(q.toXMVector()));
    }

    // Roll-Pitch-Yaw rotation (Euler angles)
    static Matrix4x4 rotationRollPitchYaw(float pitch, float yaw, float roll)
    {
        return fromXMMatrix(DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll));
    }

    // Create TRS matrix (Scale -> Rotate -> Translate)
    static Matrix4x4 TRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
    {
        // DXM convention: Scale * Rotation * Translation
        DirectX::XMMATRIX s = DirectX::XMMatrixScalingFromVector(scale.toXMVector());
        DirectX::XMMATRIX r = DirectX::XMMatrixRotationQuaternion(rotation.toXMVector());
        DirectX::XMMATRIX t = DirectX::XMMatrixTranslationFromVector(translation.toXMVector());
        return fromXMMatrix(s * r * t);
    }

    // ==========================================================================
    // View and Projection Matrices (Left-Handed coordinate system)
    // ==========================================================================

    // Left-handed look-at view matrix
    static Matrix4x4 lookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        return fromXMMatrix(DirectX::XMMatrixLookAtLH(
            eye.toXMVector(), target.toXMVector(), up.toXMVector()));
    }

    // Right-handed look-at view matrix
    static Matrix4x4 lookAtRH(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        return fromXMMatrix(DirectX::XMMatrixLookAtRH(
            eye.toXMVector(), target.toXMVector(), up.toXMVector()));
    }

    // Left-handed perspective projection (Vulkan/D3D style with depth [0,1])
    static Matrix4x4 perspectiveFovLH(float fovY, float aspectRatio, float nearZ, float farZ)
    {
        return fromXMMatrix(DirectX::XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearZ, farZ));
    }

    // Right-handed perspective projection
    static Matrix4x4 perspectiveFovRH(float fovY, float aspectRatio, float nearZ, float farZ)
    {
        return fromXMMatrix(DirectX::XMMatrixPerspectiveFovRH(fovY, aspectRatio, nearZ, farZ));
    }

    // Left-handed orthographic projection
    static Matrix4x4 orthographicLH(float width, float height, float nearZ, float farZ)
    {
        return fromXMMatrix(DirectX::XMMatrixOrthographicLH(width, height, nearZ, farZ));
    }

    // Right-handed orthographic projection
    static Matrix4x4 orthographicRH(float width, float height, float nearZ, float farZ)
    {
        return fromXMMatrix(DirectX::XMMatrixOrthographicRH(width, height, nearZ, farZ));
    }

    // Off-center orthographic (left-handed)
    static Matrix4x4 orthographicOffCenterLH(float left, float right, float bottom, float top, float nearZ, float farZ)
    {
        return fromXMMatrix(DirectX::XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ));
    }

    // Off-center orthographic (right-handed)
    static Matrix4x4 orthographicOffCenterRH(float left, float right, float bottom, float top, float nearZ, float farZ)
    {
        return fromXMMatrix(DirectX::XMMatrixOrthographicOffCenterRH(left, right, bottom, top, nearZ, farZ));
    }

    // Special matrices
    static const Matrix4x4 IDENTITY;
    static const Matrix4x4 ZERO;
};

// Static definitions
inline const Matrix4x4 Matrix4x4::IDENTITY = Matrix4x4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
);

inline const Matrix4x4 Matrix4x4::ZERO = Matrix4x4(
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
);

// Vector * Matrix (row vector on the left)
inline Vector4 operator*(const Vector4& v, const Matrix4x4& mat)
{
    return mat.transform(v);
}

} // namespace vesper