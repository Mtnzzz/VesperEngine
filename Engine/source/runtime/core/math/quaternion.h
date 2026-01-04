#pragma once

#include <cassert>

#include "runtime/core/math/math_types.h"
#include "runtime/core/math/vector3.h"


namespace vesper
{

    class Matrix4x4; // Forward declaration

    /// @brief Quaternion class with DirectXMath backend for rotations
    /// @note DXM quaternion format is (x, y, z, w), stored the same way
    class Quaternion
    {
    public:
        float x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f};

    public:
        Quaternion() = default;

        Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

        // Construct from angle/axis
        Quaternion(const Radian &angle, const Vector3 &axis) { fromAngleAxis(angle, axis); }

        // DirectXMath conversion
        explicit Quaternion(DirectX::FXMVECTOR q)
        {
            DirectX::XMFLOAT4 f;
            DirectX::XMStoreFloat4(&f, q);
            x = f.x;
            y = f.y;
            z = f.z;
            w = f.w;
        }

        DirectX::XMVECTOR toXMVector() const { return DirectX::XMVectorSet(x, y, z, w); }

        static Quaternion fromXMVector(DirectX::FXMVECTOR q)
        {
            DirectX::XMFLOAT4 f;
            DirectX::XMStoreFloat4(&f, q);
            return Quaternion(f.x, f.y, f.z, f.w);
        }

        // Accessors
        float *ptr() { return &x; }

        const float *ptr() const { return &x; }

        // Comparison
        bool operator==(const Quaternion &rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }

        bool operator!=(const Quaternion &rhs) const { return !(*this == rhs); }

        // Arithmetic
        Quaternion operator+(const Quaternion &rhs) const
        {
            return Quaternion(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
        }

        Quaternion operator-(const Quaternion &rhs) const
        {
            return Quaternion(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
        }

        Quaternion operator*(float scalar) const { return Quaternion(x * scalar, y * scalar, z * scalar, w * scalar); }

        Quaternion operator/(float scalar) const
        {
            assert(scalar != 0.0f);
            float inv = 1.0f / scalar;
            return Quaternion(x * inv, y * inv, z * inv, w * inv);
        }

        Quaternion operator-() const { return Quaternion(-x, -y, -z, -w); }

        friend Quaternion operator*(float scalar, const Quaternion &q)
        {
            return Quaternion(scalar * q.x, scalar * q.y, scalar * q.z, scalar * q.w);
        }

        // Quaternion multiplication
        Quaternion operator*(const Quaternion &rhs) const
        {
            return fromXMVector(DirectX::XMQuaternionMultiply(toXMVector(), rhs.toXMVector()));
        }

        Quaternion &operator*=(const Quaternion &rhs)
        {
            *this = *this * rhs;
            return *this;
        }

        // Rotate a vector by this quaternion
        Vector3 operator*(const Vector3 &v) const
        {
            return Vector3::fromXMVector(DirectX::XMVector3Rotate(v.toXMVector(), toXMVector()));
        }

        // Quaternion operations
        float length() const { return DirectX::XMVectorGetX(DirectX::XMQuaternionLength(toXMVector())); }

        float squaredLength() const { return DirectX::XMVectorGetX(DirectX::XMQuaternionLengthSq(toXMVector())); }

        float dot(const Quaternion &q) const
        {
            return DirectX::XMVectorGetX(DirectX::XMQuaternionDot(toXMVector(), q.toXMVector()));
        }

        void normalize() { *this = fromXMVector(DirectX::XMQuaternionNormalize(toXMVector())); }

        Quaternion normalized() const { return fromXMVector(DirectX::XMQuaternionNormalize(toXMVector())); }

        Quaternion conjugate() const { return fromXMVector(DirectX::XMQuaternionConjugate(toXMVector())); }

        Quaternion inverse() const { return fromXMVector(DirectX::XMQuaternionInverse(toXMVector())); }

        bool isNaN() const { return Math::isNaN(x) || Math::isNaN(y) || Math::isNaN(z) || Math::isNaN(w); }

        bool isIdentity() const { return DirectX::XMQuaternionIsIdentity(toXMVector()); }

        // Conversion functions
        void fromAngleAxis(const Radian &angle, const Vector3 &axis)
        {
            *this = fromXMVector(DirectX::XMQuaternionRotationAxis(axis.toXMVector(), angle.valueRadians()));
        }

        void toAngleAxis(Radian &angle, Vector3 &axis) const
        {
            DirectX::XMVECTOR axisVec;
            float angleRad;

            DirectX::XMVECTOR qNormalized = DirectX::XMQuaternionNormalize(toXMVector());

            DirectX::XMQuaternionToAxisAngle(&axisVec, &angleRad, qNormalized);

            angle = Radian(angleRad);
            axis  = Vector3::fromXMVector(axisVec);

            if (!axis.isZeroLength()) axis.normalize();
        }

        // Create rotation quaternion from Euler angles (pitch, yaw, roll)
        static Quaternion fromEulerAngles(float pitch, float yaw, float roll)
        {
            return fromXMVector(DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
        }

        static Quaternion fromEulerAngles(const Vector3 &eulerAngles)
        {
            return fromEulerAngles(eulerAngles.x, eulerAngles.y, eulerAngles.z);
        }

        // Get Euler angles from quaternion (returns pitch, yaw, roll)
        Vector3 toEulerAngles() const
        {
            // Extract Euler angles from quaternion
            float sinr_cosp = 2.0f * (w * x + y * z);
            float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
            float roll      = Math::atan2(sinr_cosp, cosr_cosp);

            float sinp      = 2.0f * (w * y - z * x);
            float pitch;
            if (Math::abs(sinp) >= 1.0f)
                pitch = std::copysign(Math::HALF_PI, sinp);
            else
                pitch = Math::asin(sinp);

            float siny_cosp = 2.0f * (w * z + x * y);
            float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
            float yaw       = Math::atan2(siny_cosp, cosy_cosp);

            return Vector3(pitch, yaw, roll);
        }

        // Get rotation axes
        Vector3 getAxisX() const { return *this * Vector3::UNIT_X; }

        Vector3 getAxisY() const { return *this * Vector3::UNIT_Y; }

        Vector3 getAxisZ() const { return *this * Vector3::UNIT_Z; }

        Vector3 getForward() const { return *this * Vector3::FORWARD; }

        Vector3 getUp() const { return *this * Vector3::UP; }

        Vector3 getRight() const { return *this * Vector3::RIGHT; }

        // Interpolation
        static Quaternion slerp(const Quaternion &a, const Quaternion &b, float t)
        {
            return fromXMVector(DirectX::XMQuaternionSlerp(a.toXMVector(), b.toXMVector(), t));
        }

        static Quaternion lerp(const Quaternion &a, const Quaternion &b, float t)
        {
            // Normalized linear interpolation
            Quaternion result = fromXMVector(DirectX::XMVectorLerp(a.toXMVector(), b.toXMVector(), t));
            result.normalize();
            return result;
        }

        // Factory functions
        static Quaternion rotationX(float angle)
        {
            return fromXMVector(DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), angle));
        }

        static Quaternion rotationY(float angle)
        {
            return fromXMVector(DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), angle));
        }

        static Quaternion rotationZ(float angle)
        {
            return fromXMVector(DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), angle));
        }

        static Quaternion lookRotation(const Vector3 &forward, const Vector3 &up = Vector3::UP)
        {
            // Calculate rotation that looks in the forward direction with the given up vector
            Vector3 f = forward.normalized();
            Vector3 r = up.cross(f).normalized();
            Vector3 u = f.cross(r);

            float m00 = r.x, m01 = r.y, m02 = r.z;
            float m10 = u.x, m11 = u.y, m12 = u.z;
            float m20 = f.x, m21 = f.y, m22 = f.z;

            float trace = m00 + m11 + m22;
            Quaternion q;

            if (trace > 0.0f) {
                float s = 0.5f / Math::sqrt(trace + 1.0f);
                q.w     = 0.25f / s;
                q.x     = (m12 - m21) * s;
                q.y     = (m20 - m02) * s;
                q.z     = (m01 - m10) * s;
            } else if (m00 > m11 && m00 > m22) {
                float s = 2.0f * Math::sqrt(1.0f + m00 - m11 - m22);
                q.w     = (m12 - m21) / s;
                q.x     = 0.25f * s;
                q.y     = (m01 + m10) / s;
                q.z     = (m02 + m20) / s;
            } else if (m11 > m22) {
                float s = 2.0f * Math::sqrt(1.0f + m11 - m00 - m22);
                q.w     = (m20 - m02) / s;
                q.x     = (m01 + m10) / s;
                q.y     = 0.25f * s;
                q.z     = (m12 + m21) / s;
            } else {
                float s = 2.0f * Math::sqrt(1.0f + m22 - m00 - m11);
                q.w     = (m01 - m10) / s;
                q.x     = (m02 + m20) / s;
                q.y     = (m12 + m21) / s;
                q.z     = 0.25f * s;
            }

            return q.normalized();
        }

        // Special quaternions
        static const Quaternion IDENTITY;
        static const Quaternion ZERO;
    };

    // Static definitions
    inline const Quaternion Quaternion::IDENTITY(0.0f, 0.0f, 0.0f, 1.0f);
    inline const Quaternion Quaternion::ZERO(0.0f, 0.0f, 0.0f, 0.0f);

} // namespace vesper
