#pragma once

#include "runtime/core/math/math_types.h"

#include <cassert>

namespace vesper {

/// @brief 2D vector class with DirectXMath backend
class Vector2
{
public:
    float x{0.0f}, y{0.0f};

public:
    Vector2() = default;
    Vector2(float x_, float y_) : x(x_), y(y_) {}
    explicit Vector2(float scalar) : x(scalar), y(scalar) {}
    explicit Vector2(const float* arr) : x(arr[0]), y(arr[1]) {}

    // DirectXMath conversion
    explicit Vector2(DirectX::FXMVECTOR v)
    {
        DirectX::XMFLOAT2 f;
        DirectX::XMStoreFloat2(&f, v);
        x = f.x; y = f.y;
    }

    DirectX::XMVECTOR toXMVector() const
    {
        return DirectX::XMVectorSet(x, y, 0.0f, 0.0f);
    }

    // Accessors
    float* ptr() { return &x; }
    const float* ptr() const { return &x; }

    float operator[](size_t i) const
    {
        assert(i < 2);
        return (&x)[i];
    }

    float& operator[](size_t i)
    {
        assert(i < 2);
        return (&x)[i];
    }

    // Comparison
    bool operator==(const Vector2& rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(const Vector2& rhs) const { return !(*this == rhs); }

    // Arithmetic
    Vector2 operator+(const Vector2& rhs) const { return Vector2(x + rhs.x, y + rhs.y); }
    Vector2 operator-(const Vector2& rhs) const { return Vector2(x - rhs.x, y - rhs.y); }
    Vector2 operator*(float scalar) const { return Vector2(x * scalar, y * scalar); }
    Vector2 operator*(const Vector2& rhs) const { return Vector2(x * rhs.x, y * rhs.y); }
    Vector2 operator/(float scalar) const
    {
        assert(scalar != 0.0f);
        float inv = 1.0f / scalar;
        return Vector2(x * inv, y * inv);
    }
    Vector2 operator/(const Vector2& rhs) const
    {
        assert(rhs.x != 0.0f && rhs.y != 0.0f);
        return Vector2(x / rhs.x, y / rhs.y);
    }

    Vector2 operator+() const { return *this; }
    Vector2 operator-() const { return Vector2(-x, -y); }

    // Friend operators
    friend Vector2 operator*(float scalar, const Vector2& v) { return Vector2(scalar * v.x, scalar * v.y); }
    friend Vector2 operator/(float scalar, const Vector2& v)
    {
        assert(v.x != 0.0f && v.y != 0.0f);
        return Vector2(scalar / v.x, scalar / v.y);
    }

    // Compound assignment
    Vector2& operator+=(const Vector2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    Vector2& operator-=(const Vector2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vector2& operator*=(const Vector2& rhs) { x *= rhs.x; y *= rhs.y; return *this; }
    Vector2& operator/=(float scalar)
    {
        assert(scalar != 0.0f);
        float inv = 1.0f / scalar;
        x *= inv; y *= inv;
        return *this;
    }
    Vector2& operator/=(const Vector2& rhs)
    {
        assert(rhs.x != 0.0f && rhs.y != 0.0f);
        x /= rhs.x; y /= rhs.y;
        return *this;
    }

    // Vector operations
    float length() const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2Length(toXMVector()));
    }

    float squaredLength() const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(toXMVector()));
    }

    float distance(const Vector2& rhs) const { return (*this - rhs).length(); }
    float squaredDistance(const Vector2& rhs) const { return (*this - rhs).squaredLength(); }

    float dot(const Vector2& v) const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2Dot(toXMVector(), v.toXMVector()));
    }

    float cross(const Vector2& v) const
    {
        return DirectX::XMVectorGetX(DirectX::XMVector2Cross(toXMVector(), v.toXMVector()));
    }

    void normalize()
    {
        DirectX::XMVECTOR normalized = DirectX::XMVector2Normalize(toXMVector());
        DirectX::XMFLOAT2 f;
        DirectX::XMStoreFloat2(&f, normalized);
        x = f.x; y = f.y;
    }

    Vector2 normalized() const
    {
        Vector2 result = *this;
        result.normalize();
        return result;
    }

    bool isZeroLength() const { return squaredLength() < Math::EPSILON * Math::EPSILON; }
    bool isNaN() const { return Math::isNaN(x) || Math::isNaN(y); }

    Vector2 perpendicular() const { return Vector2(-y, x); }

    Vector2 reflect(const Vector2& normal) const
    {
        return *this - 2.0f * dot(normal) * normal;
    }

    static Vector2 lerp(const Vector2& a, const Vector2& b, float t)
    {
        return Vector2(
            DirectX::XMVectorGetX(DirectX::XMVectorLerp(a.toXMVector(), b.toXMVector(), t)),
            DirectX::XMVectorGetY(DirectX::XMVectorLerp(a.toXMVector(), b.toXMVector(), t))
        );
    }

    // Special vectors
    static const Vector2 ZERO;
    static const Vector2 ONE;
    static const Vector2 UNIT_X;
    static const Vector2 UNIT_Y;
};

// Static definitions (in header for simplicity - could be moved to .cpp)
inline const Vector2 Vector2::ZERO(0.0f, 0.0f);
inline const Vector2 Vector2::ONE(1.0f, 1.0f);
inline const Vector2 Vector2::UNIT_X(1.0f, 0.0f);
inline const Vector2 Vector2::UNIT_Y(0.0f, 1.0f);

} // namespace vesper