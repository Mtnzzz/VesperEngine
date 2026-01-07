#pragma once

#include "runtime/core/math/vector3.h"
#include "runtime/core/math/quaternion.h"
#include "runtime/core/math/matrix4x4.h"
#include "runtime/function/framework/ecs/ecs_types.h"

namespace vesper {

/// @brief Transform component - stores local and world space transforms
struct TransformComponent
{
    // =========================================================================
    // Local Space Transform
    // =========================================================================

    Vector3    localPosition{0.0f, 0.0f, 0.0f};
    Quaternion localRotation;  // Identity by default
    Vector3    localScale{1.0f, 1.0f, 1.0f};

    // =========================================================================
    // Cached World Space Transform
    // =========================================================================

    /// @brief World matrix (updated by TransformSystem)
    Matrix4x4 worldMatrix;

    /// @brief Dirty flag - set to true when local transform changes
    bool dirty{true};

    // =========================================================================
    // Convenience Setters (auto-mark dirty)
    // =========================================================================

    void setPosition(const Vector3& pos)
    {
        localPosition = pos;
        dirty = true;
    }

    void setRotation(const Quaternion& rot)
    {
        localRotation = rot;
        dirty = true;
    }

    void setScale(const Vector3& scale)
    {
        localScale = scale;
        dirty = true;
    }

    void setScale(float uniformScale)
    {
        localScale = Vector3(uniformScale, uniformScale, uniformScale);
        dirty = true;
    }

    void setTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale)
    {
        localPosition = pos;
        localRotation = rot;
        localScale = scale;
        dirty = true;
    }

    // =========================================================================
    // Matrix Computation
    // =========================================================================

    /// @brief Compute local transform matrix (TRS order)
    Matrix4x4 getLocalMatrix() const
    {
        // Scale -> Rotate -> Translate
        Matrix4x4 scaleMatrix = Matrix4x4::scaling(localScale.x, localScale.y, localScale.z);
        Matrix4x4 rotationMatrix = Matrix4x4::rotationQuaternion(localRotation);
        Matrix4x4 translationMatrix = Matrix4x4::translation(localPosition.x, localPosition.y, localPosition.z);

        // TRS = T * R * S (right-to-left application)
        return scaleMatrix * rotationMatrix * translationMatrix;
    }

    /// @brief Mark as needing update
    void markDirty() { dirty = true; }

    /// @brief Clear dirty flag (called after update)
    void clearDirty() { dirty = false; }
};

/// @brief Hierarchy component - parent-child relationships
struct HierarchyComponent
{
    Entity parent{NullEntity};
    Entity firstChild{NullEntity};
    Entity nextSibling{NullEntity};
    Entity prevSibling{NullEntity};

    /// @brief Check if this entity has a parent
    bool hasParent() const { return parent != NullEntity; }

    /// @brief Check if this entity has children
    bool hasChildren() const { return firstChild != NullEntity; }
};

} // namespace vesper
