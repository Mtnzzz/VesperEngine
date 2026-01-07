#pragma once

#include "runtime/function/framework/ecs/systems/frustum.h"
#include "runtime/function/framework/ecs/ecs_types.h"

#include <vector>

namespace vesper {

/// @brief System for frustum culling entities
class FrustumCullSystem
{
public:
    /// @brief Cull entities against frustum, outputting visible entities
    /// @param registry The entity registry to query
    /// @param frustum The view frustum to test against
    /// @param outVisible Output vector of visible entities
    static void cullEntities(EntityRegistry& registry,
                             const Frustum& frustum,
                             std::vector<Entity>& outVisible);

    /// @brief Cull entities and return count (without storing entities)
    /// @param registry The entity registry to query
    /// @param frustum The view frustum to test against
    /// @return Number of visible entities
    static size_t countVisible(EntityRegistry& registry,
                               const Frustum& frustum);

    /// @brief Build frustum from camera matrices
    /// @param viewMatrix Camera view matrix
    /// @param projMatrix Camera projection matrix
    /// @return Constructed frustum
    static Frustum buildFrustum(const Matrix4x4& viewMatrix,
                                const Matrix4x4& projMatrix);
};

} // namespace vesper
