#include "runtime/function/framework/ecs/systems/frustum.h"

#include <algorithm>

namespace vesper {

void Frustum::extractFromViewProjection(const Matrix4x4& vp)
{
    // Extract frustum planes from view-projection matrix
    // Using Gribb/Hartmann method
    // Note: Matrix4x4 uses row-major storage, so we access elements accordingly

    // For a row-major matrix M, a point p is transformed as p' = p * M
    // The plane equations are:
    // Left:   row3 + row0 >= 0
    // Right:  row3 - row0 >= 0
    // Bottom: row3 + row1 >= 0
    // Top:    row3 - row1 >= 0
    // Near:   row2 >= 0        (for D3D/Vulkan style [0,1] depth)
    // Far:    row3 - row2 >= 0

    // Get matrix elements
    // vp[row][col] - row-major access
    float m00 = vp[0][0], m01 = vp[0][1], m02 = vp[0][2], m03 = vp[0][3];
    float m10 = vp[1][0], m11 = vp[1][1], m12 = vp[1][2], m13 = vp[1][3];
    float m20 = vp[2][0], m21 = vp[2][1], m22 = vp[2][2], m23 = vp[2][3];
    float m30 = vp[3][0], m31 = vp[3][1], m32 = vp[3][2], m33 = vp[3][3];

    // Left plane: row3 + row0
    planes[static_cast<int>(FrustumPlaneIndex::Left)] =
        FrustumPlane::fromCoefficients(m30 + m00, m31 + m01, m32 + m02, m33 + m03);

    // Right plane: row3 - row0
    planes[static_cast<int>(FrustumPlaneIndex::Right)] =
        FrustumPlane::fromCoefficients(m30 - m00, m31 - m01, m32 - m02, m33 - m03);

    // Bottom plane: row3 + row1
    planes[static_cast<int>(FrustumPlaneIndex::Bottom)] =
        FrustumPlane::fromCoefficients(m30 + m10, m31 + m11, m32 + m12, m33 + m13);

    // Top plane: row3 - row1
    planes[static_cast<int>(FrustumPlaneIndex::Top)] =
        FrustumPlane::fromCoefficients(m30 - m10, m31 - m11, m32 - m12, m33 - m13);

    // Near plane: row2 (Vulkan/D3D style [0,1] depth range)
    planes[static_cast<int>(FrustumPlaneIndex::Near)] =
        FrustumPlane::fromCoefficients(m20, m21, m22, m23);

    // Far plane: row3 - row2
    planes[static_cast<int>(FrustumPlaneIndex::Far)] =
        FrustumPlane::fromCoefficients(m30 - m20, m31 - m21, m32 - m22, m33 - m23);
}

bool Frustum::testSphere(const Vector3& center, float radius) const
{
    for (int i = 0; i < kPlaneCount; ++i)
    {
        float dist = planes[i].signedDistance(center);

        // If sphere is completely behind any plane, it's outside
        if (dist < -radius)
        {
            return false;
        }
    }

    return true;
}

bool Frustum::testAABB(const Vector3& min, const Vector3& max) const
{
    for (int i = 0; i < kPlaneCount; ++i)
    {
        const FrustumPlane& plane = planes[i];

        // Find the positive vertex (the one furthest in the direction of the plane normal)
        Vector3 pVertex;
        pVertex.x = (plane.normal.x >= 0) ? max.x : min.x;
        pVertex.y = (plane.normal.y >= 0) ? max.y : min.y;
        pVertex.z = (plane.normal.z >= 0) ? max.z : min.z;

        // If the positive vertex is outside, the entire AABB is outside
        if (plane.signedDistance(pVertex) < 0)
        {
            return false;
        }
    }

    return true;
}

bool Frustum::testPoint(const Vector3& point) const
{
    for (int i = 0; i < kPlaneCount; ++i)
    {
        if (planes[i].signedDistance(point) < 0)
        {
            return false;
        }
    }

    return true;
}

} // namespace vesper
