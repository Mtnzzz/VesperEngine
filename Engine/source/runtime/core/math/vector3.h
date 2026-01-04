#pragma once

#include "runtime/core/math/math_types.h"

#include <cassert>

namespace vesper {

class Quaternion; // Forward declaration

/// @brief 3D vector class with DirectXMath backend
class Vector3
{
public:
    float x{0.0f}, y{0.0f}, z{0.0f};

public:
    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit Vector3(float scalar) : x(scalar), y(scalar), z(scalar) {}
    explicit Vector3(const float* arr) : x(arr[0]), y(arr[1]), z(arr[2]) {}

    // DirectXMath conversion
    explicit Vector3(DirectX::FXMVECTOR v)
    {
        DirectX::XMFLOAT3 f;
        DirectX::XMStoreFloat3(&f, v);
        x = f.x; y = f.y; z = f.z;
    }

    DirectX::XMVECTOR toXMVector() const
    {
        return DirectX::XMVectorSet(x, y, z, 0.0f);
    }

    static Vector3 fromXMVector(DirectX::FXMVECTOR v)
    {
        DirectX::XMFLOAT3 f;
        DirectX::XMStoreFloat3(&f, v);
        return Vector3(f.x, f.y, f.z);
    }

    // Accessors
    float* ptr() { return &x; }
    const float* ptr() const { return &x; }

    float operator[](size_t i) const
    {
        assert(i < 3);
        return (&x)[i];
    }

    float& operator[](size_t i)
    {
        assert(i < 3);
        return (&x)[i];
    }

    // Comparison
    bool operator==(const Vector3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
    bool operator!=(const Vector3& rhs) const { return !(*this == rhs); }

    // Arithmetic
    Vector3 operator+(const Vector3& rhs) const { return Vector3(x + rhs.x, y + rhs.y, z + rhs.z); }
    Vector3 operator-(const Vector3& rhs) const { return Vector3(x - rhs.x, y - rhs.y, z - rhs.z); }
    Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }
    Vector3 operator*(const Vector3& rhs) const { return Vector3(x * rhs.x, y * rhs.y, z * rhs.z); }
    Vector3 operator/(float scalar) const
    {
        assert(scalar != 0.0f);
        float inv = 1.0f / scalar;
        return Vector3(x * inv, y * inv, z * inv);
    }
    Vector3 operator/(const Vector3& rhs) const
    {
        assert(rhs.x != 0.0f && rhs.y != 0.0f && rhs.z != 0.0f);
        return Vector3(x / rhs.x, y / rhs.y, z / rhs.z);
    }

    Vector3 operator+() const { return *this; }
    Vector3 operator-() const { return Vector3(-x, -y, -z); }

    // Friend operators
    friend Vector3 operator*(float scalar, const Vector3& v)
    {
        return Vector3(scalar * v.x, scalar * v.y, scalar * v.z);
    }
    friend Vector3 operator/(float scalar, const Vector3& v)
    {
        assert(v.x != 0.0f && v.y != 0.0f && v.z != 0.0f);
        return Vector3(scalar / v.x, scalar / v.y, scalar / v.z);
    }

    // Compound assignment
    Vector3& operator+=(const Vector3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    Vector3& operator-=(const Vector3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    Vector3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    Vector3& operator*=(const Vector3& rhs) { x *= rhs.x; y *= rhs.y; z *= rhs.z; return *this; }
    Vector3& operator/=(float scalar)
    {
        assert(scalar != 0.0f);
        float inv = 1.0f / scalar;
        x *= inv; y *= inv; z *= inv;
        return *this;
    }
    Vector3& operator/=(const Vector3& rhs)
    {
        assert(rhs.x != 0.0f && rhs.y != 0.0f && rhs.z != 0.0f);
        x /= rhs.x; y /= rhs.y; z /= rhs.z;
        return *this;
    }

    // Vector operations using DirectXMath
    float length() const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(toXMVector()));
    }

    float squaredLength() const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(toXMVector()));
    }

    float distance(const Vector3& rhs) const { return (*this - rhs).length(); }
    float squaredDistance(const Vector3& rhs) const { return (*this - rhs).squaredLength(); }

    float dot(const Vector3& v) const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3Dot(toXMVector(), v.toXMVector()));
    }

    Vector3 cross(const Vector3& v) const
    {
        return fromXMVector(DirectX::XMVector3Cross(toXMVector(), v.toXMVector()));
    }

    void normalize()
    {
        DirectX::XMVECTOR normalized = DirectX::XMVector3Normalize(toXMVector());
        *this = fromXMVector(normalized);
    }

    Vector3 normalized() const
    {
        return fromXMVector(DirectX::XMVector3Normalize(toXMVector()));
    }

    bool isZeroLength() const { return squaredLength() < Math::EPSILON * Math::EPSILON; }
    bool isNaN() const { return Math::isNaN(x) || Math::isNaN(y) || Math::isNaN(z); }

    Vector3 reflect(const Vector3& normal) const
    {
        return fromXMVector(DirectX::XMVector3Reflect(toXMVector(), normal.toXMVector()));
    }

    Vector3 project(const Vector3& normal) const
    {
        // Project onto plane with given normal
        return *this - dot(normal) * normal;
    }

    Vector3 abs() const { return Vector3(Math::abs(x), Math::abs(y), Math::abs(z)); }

    Radian angleBetween(const Vector3& dest) const
    {
        float lenProduct = length() * dest.length();
        if (lenProduct < Math::EPSILON)
            lenProduct = Math::EPSILON;

        float f = dot(dest) / lenProduct;
        f = Math::clamp(f, -1.0f, 1.0f);
        return Radian(Math::acos(f));
    }

    static Vector3 lerp(const Vector3& a, const Vector3& b, float t)
    {
        return fromXMVector(DirectX::XMVectorLerp(a.toXMVector(), b.toXMVector(), t));
    }

    static Vector3 clamp(const Vector3& v, const Vector3& minVal, const Vector3& maxVal)
    {
        return Vector3(
            Math::clamp(v.x, minVal.x, maxVal.x),
            Math::clamp(v.y, minVal.y, maxVal.y),
            Math::clamp(v.z, minVal.z, maxVal.z)
        );
    }

    static float maxElement(const Vector3& v) { return Math::max3(v.x, v.y, v.z); }
    static float minElement(const Vector3& v) { return Math::min3(v.x, v.y, v.z); }

    // Special vectors
    static const Vector3 ZERO;
    static const Vector3 ONE;
    static const Vector3 UNIT_X;
    static const Vector3 UNIT_Y;
    static const Vector3 UNIT_Z;
    static const Vector3 NEG_UNIT_X;
    static const Vector3 NEG_UNIT_Y;
    static const Vector3 NEG_UNIT_Z;
    static const Vector3 UP;
    static const Vector3 FORWARD;
    static const Vector3 RIGHT;
};

// Static definitions
inline const Vector3 Vector3::ZERO(0.0f, 0.0f, 0.0f);
inline const Vector3 Vector3::ONE(1.0f, 1.0f, 1.0f);
inline const Vector3 Vector3::UNIT_X(1.0f, 0.0f, 0.0f);
inline const Vector3 Vector3::UNIT_Y(0.0f, 1.0f, 0.0f);
inline const Vector3 Vector3::UNIT_Z(0.0f, 0.0f, 1.0f);
inline const Vector3 Vector3::NEG_UNIT_X(-1.0f, 0.0f, 0.0f);
inline const Vector3 Vector3::NEG_UNIT_Y(0.0f, -1.0f, 0.0f);
inline const Vector3 Vector3::NEG_UNIT_Z(0.0f, 0.0f, -1.0f);
inline const Vector3 Vector3::UP(0.0f, 1.0f, 0.0f);
inline const Vector3 Vector3::FORWARD(0.0f, 0.0f, 1.0f);
inline const Vector3 Vector3::RIGHT(1.0f, 0.0f, 0.0f);

} // namespace vesper