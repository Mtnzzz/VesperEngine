#pragma once

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace vesper {

// =============================================================================
// Math Constants
// =============================================================================
namespace Math {
    constexpr float POS_INFINITY = std::numeric_limits<float>::infinity();
    constexpr float NEG_INFINITY = -std::numeric_limits<float>::infinity();
    constexpr float PI           = 3.14159265358979323846264338327950288f;
    constexpr float TWO_PI       = 2.0f * PI;
    constexpr float HALF_PI      = 0.5f * PI;
    constexpr float INV_PI       = 1.0f / PI;
    constexpr float DEG_TO_RAD   = PI / 180.0f;
    constexpr float RAD_TO_DEG   = 180.0f / PI;
    constexpr float EPSILON      = 1e-6f;
    constexpr float FLOAT_EPSILON = FLT_EPSILON;

    // Utility functions
    inline float abs(float value) { return std::fabs(value); }
    inline bool isNaN(float f) { return std::isnan(f); }
    inline float sqr(float value) { return value * value; }
    inline float sqrt(float value) { return std::sqrt(value); }
    inline float invSqrt(float value) { return 1.0f / std::sqrt(value); }

    inline float clamp(float v, float minVal, float maxVal)
    {
        return std::clamp(v, minVal, maxVal);
    }

    inline bool realEqual(float a, float b, float tolerance = FLOAT_EPSILON)
    {
        return std::fabs(a - b) <= tolerance;
    }

    inline float degreesToRadians(float degrees) { return degrees * DEG_TO_RAD; }
    inline float radiansToDegrees(float radians) { return radians * RAD_TO_DEG; }

    inline float sin(float rad) { return std::sin(rad); }
    inline float cos(float rad) { return std::cos(rad); }
    inline float tan(float rad) { return std::tan(rad); }
    inline float asin(float value) { return std::asin(clamp(value, -1.0f, 1.0f)); }
    inline float acos(float value) { return std::acos(clamp(value, -1.0f, 1.0f)); }
    inline float atan(float value) { return std::atan(value); }
    inline float atan2(float y, float x) { return std::atan2(y, x); }

    template<typename T>
    constexpr T max(T a, T b) { return std::max(a, b); }

    template<typename T>
    constexpr T min(T a, T b) { return std::min(a, b); }

    template<typename T>
    constexpr T max3(T a, T b, T c) { return std::max({a, b, c}); }

    template<typename T>
    constexpr T min3(T a, T b, T c) { return std::min({a, b, c}); }

    template<typename T>
    T lerp(const T& a, const T& b, float t) { return a + t * (b - a); }
}

// =============================================================================
// Radian / Degree Wrapper Classes
// =============================================================================
class Degree;

class Radian
{
public:
    explicit Radian(float r = 0.0f) : m_rad(r) {}
    explicit Radian(const Degree& d);

    float valueRadians() const { return m_rad; }
    float valueDegrees() const { return m_rad * Math::RAD_TO_DEG; }

    Radian operator+() const { return *this; }
    Radian operator-() const { return Radian(-m_rad); }
    Radian operator+(const Radian& r) const { return Radian(m_rad + r.m_rad); }
    Radian operator-(const Radian& r) const { return Radian(m_rad - r.m_rad); }
    Radian operator*(float f) const { return Radian(m_rad * f); }
    Radian operator/(float f) const { return Radian(m_rad / f); }

    Radian& operator+=(const Radian& r) { m_rad += r.m_rad; return *this; }
    Radian& operator-=(const Radian& r) { m_rad -= r.m_rad; return *this; }
    Radian& operator*=(float f) { m_rad *= f; return *this; }
    Radian& operator/=(float f) { m_rad /= f; return *this; }

    bool operator<(const Radian& r) const { return m_rad < r.m_rad; }
    bool operator<=(const Radian& r) const { return m_rad <= r.m_rad; }
    bool operator==(const Radian& r) const { return m_rad == r.m_rad; }
    bool operator!=(const Radian& r) const { return m_rad != r.m_rad; }
    bool operator>=(const Radian& r) const { return m_rad >= r.m_rad; }
    bool operator>(const Radian& r) const { return m_rad > r.m_rad; }

private:
    float m_rad;
};

class Degree
{
public:
    explicit Degree(float d = 0.0f) : m_deg(d) {}
    explicit Degree(const Radian& r) : m_deg(r.valueDegrees()) {}

    float valueDegrees() const { return m_deg; }
    float valueRadians() const { return m_deg * Math::DEG_TO_RAD; }

    Degree operator+() const { return *this; }
    Degree operator-() const { return Degree(-m_deg); }
    Degree operator+(const Degree& d) const { return Degree(m_deg + d.m_deg); }
    Degree operator-(const Degree& d) const { return Degree(m_deg - d.m_deg); }
    Degree operator*(float f) const { return Degree(m_deg * f); }
    Degree operator/(float f) const { return Degree(m_deg / f); }

    Degree& operator+=(const Degree& d) { m_deg += d.m_deg; return *this; }
    Degree& operator-=(const Degree& d) { m_deg -= d.m_deg; return *this; }
    Degree& operator*=(float f) { m_deg *= f; return *this; }
    Degree& operator/=(float f) { m_deg /= f; return *this; }

    bool operator<(const Degree& d) const { return m_deg < d.m_deg; }
    bool operator<=(const Degree& d) const { return m_deg <= d.m_deg; }
    bool operator==(const Degree& d) const { return m_deg == d.m_deg; }
    bool operator!=(const Degree& d) const { return m_deg != d.m_deg; }
    bool operator>=(const Degree& d) const { return m_deg >= d.m_deg; }
    bool operator>(const Degree& d) const { return m_deg > d.m_deg; }

private:
    float m_deg;
};

inline Radian::Radian(const Degree& d) : m_rad(d.valueRadians()) {}

inline Radian operator*(float a, const Radian& b) { return Radian(a * b.valueRadians()); }
inline Degree operator*(float a, const Degree& b) { return Degree(a * b.valueDegrees()); }

} // namespace vesper