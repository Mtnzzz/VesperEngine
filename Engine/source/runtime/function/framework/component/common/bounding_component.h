#pragma once

#include "runtime/core/math/vector3.h"
#include "runtime/core/math/vector4.h"
#include "runtime/core/math/matrix4x4.h"

#include <algorithm>

namespace vesper {

/// @brief Bounding component - stores bounding volumes for culling
struct BoundingComponent
{
    // =========================================================================
    // Local Space Bounding Sphere
    // =========================================================================

    /// @brief Local space bounding sphere center
    Vector3 localCenter{0.0f, 0.0f, 0.0f};

    /// @brief Local space bounding sphere radius
    float localRadius{1.0f};

    // =========================================================================
    // Cached World Space Bounding (updated by system)
    // =========================================================================

    /// @brief World space bounding sphere center
    Vector3 worldCenter{0.0f, 0.0f, 0.0f};

    /// @brief World space bounding sphere radius (scaled)
    float worldRadius{1.0f};

    // =========================================================================
    // Local Space AABB (optional, for tighter culling)
    // =========================================================================

    /// @brief Local AABB minimum corner
    Vector3 aabbMin{-0.5f, -0.5f, -0.5f};

    /// @brief Local AABB maximum corner
    Vector3 aabbMax{0.5f, 0.5f, 0.5f};

    // =========================================================================
    // Initialization Helpers
    // =========================================================================

    /// @brief Initialize from AABB bounds
    void initFromAABB(const Vector3& min, const Vector3& max)
    {
        aabbMin = min;
        aabbMax = max;

        // Compute bounding sphere from AABB
        localCenter = (min + max) * 0.5f;
        Vector3 extents = max - min;
        localRadius = extents.length() * 0.5f;
    }

    /// @brief Initialize from center and radius
    void initFromSphere(const Vector3& center, float radius)
    {
        localCenter = center;
        localRadius = radius;

        // Create AABB from sphere
        aabbMin = center - Vector3(radius, radius, radius);
        aabbMax = center + Vector3(radius, radius, radius);
    }

    /// @brief Update world bounds from transform
    /// @param worldMatrix The entity's world transform matrix
    void updateWorldBounds(const Matrix4x4& worldMatrix)
    {
        // Transform center to world space
        Vector4 worldCenterH = worldMatrix.transform(Vector4(localCenter.x, localCenter.y, localCenter.z, 1.0f));
        worldCenter = Vector3(worldCenterH.x, worldCenterH.y, worldCenterH.z);

        // Compute approximate world radius (using max scale)
        // Extract scale from matrix (approximate - assumes no shear)
        float scaleX = Vector3(worldMatrix[0][0], worldMatrix[0][1], worldMatrix[0][2]).length();
        float scaleY = Vector3(worldMatrix[1][0], worldMatrix[1][1], worldMatrix[1][2]).length();
        float scaleZ = Vector3(worldMatrix[2][0], worldMatrix[2][1], worldMatrix[2][2]).length();
        float maxScale = std::max({scaleX, scaleY, scaleZ});

        worldRadius = localRadius * maxScale;
    }
};

} // namespace vesper
