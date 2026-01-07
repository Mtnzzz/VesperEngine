#pragma once

#include "runtime/core/math/vector3.h"
#include "runtime/core/math/vector4.h"
#include "runtime/core/math/matrix4x4.h"

#include <cmath>

namespace vesper {

/// @brief Plane representation for frustum culling
struct FrustumPlane
{
    Vector3 normal;
    float distance;

    /// @brief Construct from normal and point on plane
    static FrustumPlane fromNormalAndPoint(const Vector3& n, const Vector3& point)
    {
        FrustumPlane plane;
        plane.normal = n.normalized();
        plane.distance = -plane.normal.dot(point);
        return plane;
    }

    /// @brief Construct from plane equation coefficients (ax + by + cz + d = 0)
    static FrustumPlane fromCoefficients(float a, float b, float c, float d)
    {
        FrustumPlane plane;
        float invLength = 1.0f / std::sqrt(a * a + b * b + c * c);
        plane.normal = Vector3(a * invLength, b * invLength, c * invLength);
        plane.distance = d * invLength;
        return plane;
    }

    /// @brief Compute signed distance from point to plane
    float signedDistance(const Vector3& point) const
    {
        return normal.dot(point) + distance;
    }
};

/// @brief Frustum indices for plane array
enum class FrustumPlaneIndex : uint8_t
{
    Left = 0,
    Right = 1,
    Bottom = 2,
    Top = 3,
    Near = 4,
    Far = 5,
    Count = 6
};

/// @brief View frustum for culling
class Frustum
{
public:
    static constexpr int kPlaneCount = 6;

    FrustumPlane planes[kPlaneCount];

    /// @brief Extract frustum planes from view-projection matrix
    /// @param viewProjection Combined view * projection matrix
    void extractFromViewProjection(const Matrix4x4& viewProjection);

    /// @brief Test if a sphere intersects or is inside the frustum
    /// @param center Sphere center in world space
    /// @param radius Sphere radius
    /// @return true if sphere is at least partially visible
    bool testSphere(const Vector3& center, float radius) const;

    /// @brief Test if an AABB intersects or is inside the frustum
    /// @param min AABB minimum corner
    /// @param max AABB maximum corner
    /// @return true if AABB is at least partially visible
    bool testAABB(const Vector3& min, const Vector3& max) const;

    /// @brief Test if a point is inside the frustum
    /// @param point Point in world space
    /// @return true if point is inside
    bool testPoint(const Vector3& point) const;
};

} // namespace vesper
