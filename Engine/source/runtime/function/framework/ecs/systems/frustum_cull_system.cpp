#include "runtime/function/framework/ecs/systems/frustum_cull_system.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/component/mesh/renderable_component.h"
#include "runtime/function/framework/component/common/bounding_component.h"

namespace vesper {

void FrustumCullSystem::cullEntities(EntityRegistry& registry,
                                      const Frustum& frustum,
                                      std::vector<Entity>& outVisible)
{
    outVisible.clear();

    // Query all renderable entities with transforms
    auto view = registry.view<TransformComponent, RenderableComponent>();

    for (auto entity : view)
    {
        auto& renderable = view.get<RenderableComponent>(entity);

        // Skip invisible entities
        if (!renderable.visible)
            continue;

        // Skip invalid renderables
        if (!renderable.isValid())
            continue;

        // Check bounding component for frustum test
        auto* bounds = registry.try_get<BoundingComponent>(entity);

        if (bounds)
        {
            // Test world-space bounding sphere against frustum
            if (!frustum.testSphere(bounds->worldCenter, bounds->worldRadius))
            {
                continue; // Culled
            }
        }
        // If no bounds, assume visible (conservative approach)

        outVisible.push_back(entity);
    }
}

size_t FrustumCullSystem::countVisible(EntityRegistry& registry,
                                        const Frustum& frustum)
{
    size_t count = 0;

    auto view = registry.view<TransformComponent, RenderableComponent>();

    for (auto entity : view)
    {
        auto& renderable = view.get<RenderableComponent>(entity);

        if (!renderable.visible || !renderable.isValid())
            continue;

        auto* bounds = registry.try_get<BoundingComponent>(entity);

        if (bounds)
        {
            if (!frustum.testSphere(bounds->worldCenter, bounds->worldRadius))
            {
                continue;
            }
        }

        ++count;
    }

    return count;
}

Frustum FrustumCullSystem::buildFrustum(const Matrix4x4& viewMatrix,
                                         const Matrix4x4& projMatrix)
{
    Frustum frustum;
    Matrix4x4 viewProj = viewMatrix * projMatrix;
    frustum.extractFromViewProjection(viewProj);
    return frustum;
}

} // namespace vesper
