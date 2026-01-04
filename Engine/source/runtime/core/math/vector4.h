#pragma once

#include "runtime/core/math/math_types.h"
#include "runtime/core/math/vector3.h"

#include <cassert>

namespace vesper {

/// @brief 4D vector class with DirectXMath backend
class Vector4
{
public:
    float x{0.0f}, y{0.0f}, z{0.0f}, w{0.0f};

public:
    Vector4() = default;
    Vector4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vector4(const Vector3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    explicit Vector4(float scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}
    explicit Vector4(const float* arr) : x(arr[0]), y(arr[1]), z(arr[2]), w(arr[3]) {}

    // DirectXMath conversion
    explicit Vector4(DirectX::FXMVECTOR v)
    {
        DirectX::XMFLOAT4 f;
        DirectX::XMStoreFloat4(&f, v);
        x = f.x; y = f.y; z = f.z; w = f.w;
    }

    DirectX::XMVECTOR toXMVector() const
    {
        return DirectX::XMVectorSet(x, y, z, w);
    }

    static Vector4 fromXMVector(DirectX::FXMVECTOR v)
    {
        DirectX::XMFLOAT4 f;
        DirectX::XMStoreFloat4(&f, v);
        return Vector4(f.x, f.y, f.z, f.w);
    }

    // Accessors
    float* ptr() { return &x; }
    const float* ptr() const { return &x; }

    float operator[](size_t i) const
    {
        assert(i < 4);
        return (&x)[i];
    }

    float& operator[](size_t i)
    {
        assert(i < 4);
        return (&x)[i];
    }

    // Get xyz as Vector3
    Vector3 xyz() const { return Vector3(x, y, z); }

    // Comparison
    bool operator==(const Vector4& rhs) const
    {
        return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
    }
    bool operator!=(const Vector4& rhs) const { return !(*this == rhs); }

    // Arithmetic
    Vector4 operator+(const Vector4& rhs) const
    {
        return Vector4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
    }
    Vector4 operator-(const Vector4& rhs) const
    {
        return Vector4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
    }
    Vector4 operator*(float scalar) const
    {
        return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
    }
    Vector4 operator*(const Vector4& rhs) const
    {
        return Vector4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
    }
    Vector4 operator/(float scalar) const
    {
        assert(scalar != 0.0f);
        float inv = 1.0f / scalar;
        return Vector4(x * inv, y * inv, z * inv, w * inv);
    }
    Vector4 operator/(const Vector4& rhs) const
    {
        assert(rhs.x != 0.0f && rhs.y != 0.0f && rhs.z != 0.0f && rhs.w != 0.0f);
        return Vector4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
    }

    Vector4 operator+() const { return *this; }
    Vector4 operator-() const { return Vector4(-x, -y, -z, -w); }

    // Friend operators
    friend Vector4 operator*(float scalar, const Vector4& v)
    {
        return Vector4(scalar * v.x, scalar * v.y, scalar * v.z, scalar * v.w);
    }
    friend Vector4 operator/(float scalar, const Vector4& v)
    {
        assert(v.x != 0.0f && v.y != 0.0f && v.z != 0.0f && v.w != 0.0f);
        return Vector4(scalar / v.x, scalar / v.y, scalar / v.z, scalar / v.w);
    }

    // Compound assignment
    Vector4& operator+=(const Vector4& rhs)
    {
        x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w;
        return *this;
    }
    Vector4& operator-=(const Vector4& rhs)
    {
        x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w;
        return *this;
    }
    Vector4& operator*=(float scalar)
    {
        x *= scalar; y *= scalar; z *= scalar; w *= scalar;
        return *this;
    }
    Vector4& operator*=(const Vector4& rhs)
    {
        x *= rhs.x; y *= rhs.y; z *= rhs.z; w *= rhs.w;
        return *this;
    }
    Vector4& operator/=(float scalar)
    {
        assert(scalar != 0.0f);
        float inv = 1.0f / scalar;
        x *= inv; y *= inv; z *= inv; w *= inv;
        return *this;
    }
    Vector4& operator/=(const Vector4& rhs)
    {
        assert(rhs.x != 0.0f && rhs.y != 0.0f && rhs.z != 0.0f && rhs.w != 0.0f);
        x /= rhs.x; y /= rhs.y; z /= rhs.z; w /= rhs.w;
        return *this;
    }

    // Vector operations using DirectXMath
    float length() const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector4Length(toXMVector()));
    }

    float squaredLength() const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(toXMVector()));
    }

    float dot(const Vector4& v) const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector4Dot(toXMVector(), v.toXMVector()));
    }

    void normalize()
    {
        DirectX::XMVECTOR normalized = DirectX::XMVector4Normalize(toXMVector());
        *this = fromXMVector(normalized);
    }

    Vector4 normalized() const
    {
        return fromXMVector(DirectX::XMVector4Normalize(toXMVector()));
    }

    bool isNaN() const
    {
        return Math::isNaN(x) || Math::isNaN(y) || Math::isNaN(z) || Math::isNaN(w);
    }

    static Vector4 lerp(const Vector4& a, const Vector4& b, float t)
    {
        return fromXMVector(DirectX::XMVectorLerp(a.toXMVector(), b.toXMVector(), t));
    }

    // Special vectors
    static const Vector4 ZERO;
    static const Vector4 ONE;
};

// Static definitions
inline const Vector4 Vector4::ZERO(0.0f, 0.0f, 0.0f, 0.0f);
inline const Vector4 Vector4::ONE(1.0f, 1.0f, 1.0f, 1.0f);

} // namespace vesper