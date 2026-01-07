#include "runtime/function/framework/ecs/systems/render_bridge_system.h"
#include "runtime/function/framework/ecs/systems/frustum_cull_system.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/component/mesh/renderable_component.h"
#include "runtime/function/framework/component/camera/camera_component.h"
#include "runtime/function/framework/component/common/bounding_component.h"
#include "runtime/function/render/render_packet.h"
#include "runtime/function/render/camera.h"

#include <cstring>

namespace vesper {

void RenderBridgeSystem::fillRenderPacket(EntityRegistry& registry,
                                           RenderPacket* packet,
                                           Entity mainCamera)
{
    if (!packet)
        return;

    packet->clear();

    // Fill camera parameters
    auto* cameraComp = registry.try_get<CameraComponent>(mainCamera);
    if (cameraComp && cameraComp->camera)
    {
        fillCameraParams(cameraComp->camera.get(), packet);

        // Build frustum from camera
        Frustum frustum = FrustumCullSystem::buildFrustum(
            cameraComp->camera->getViewMatrix(),
            cameraComp->camera->getProjectionMatrix()
        );

        // Fill visible objects with frustum culling
        fillVisibleObjects(registry, packet, frustum);
    }
    else
    {
        // No camera - fill all objects without culling
        fillVisibleObjectsNoClip(registry, packet);
    }
}

void RenderBridgeSystem::fillCameraParams(Camera* camera, RenderPacket* packet)
{
    if (!camera || !packet)
        return;

    // Get matrices
    Matrix4x4 viewMatrix = camera->getViewMatrix();
    Matrix4x4 projMatrix = camera->getProjectionMatrix();

    // Copy to CameraParams
    std::memcpy(packet->camera.view_matrix, viewMatrix.ptr(), sizeof(float) * 16);
    std::memcpy(packet->camera.proj_matrix, projMatrix.ptr(), sizeof(float) * 16);

    // Copy position
    Vector3 pos = camera->getPosition();
    packet->camera.position[0] = pos.x;
    packet->camera.position[1] = pos.y;
    packet->camera.position[2] = pos.z;

    // Copy camera parameters
    packet->camera.fov = camera->getFovY();
    packet->camera.near_plane = camera->getNearPlane();
    packet->camera.far_plane = camera->getFarPlane();
}

void RenderBridgeSystem::fillVisibleObjects(EntityRegistry& registry,
                                             RenderPacket* packet,
                                             const Frustum& frustum)
{
    if (!packet)
        return;

    auto view = registry.view<TransformComponent, RenderableComponent>();

    for (auto [entity, transform, renderable] : view.each())
    {
        // Skip invisible
        if (!renderable.visible)
            continue;

        // Skip invalid renderables
        if (!renderable.isValid())
            continue;

        // Frustum culling
        auto* bounds = registry.try_get<BoundingComponent>(entity);
        if (bounds)
        {
            if (!frustum.testSphere(bounds->worldCenter, bounds->worldRadius))
            {
                continue; // Culled
            }
        }

        // Create render object descriptor
        RenderObjectDesc desc;
        desc.object_id = entityToId(entity);
        desc.mesh_id = renderable.meshId;
        desc.material_id = renderable.materialId;

        // Copy world transform
        std::memcpy(desc.transform, transform.worldMatrix.ptr(), sizeof(float) * 16);

        // Copy bounding sphere
        if (bounds)
        {
            desc.bounding_sphere[0] = bounds->worldCenter.x;
            desc.bounding_sphere[1] = bounds->worldCenter.y;
            desc.bounding_sphere[2] = bounds->worldCenter.z;
            desc.bounding_sphere[3] = bounds->worldRadius;
        }

        packet->visibleObjects.push_back(desc);
    }
}

void RenderBridgeSystem::fillVisibleObjectsNoClip(EntityRegistry& registry,
                                                   RenderPacket* packet)
{
    if (!packet)
        return;

    auto view = registry.view<TransformComponent, RenderableComponent>();

    for (auto [entity, transform, renderable] : view.each())
    {
        // Skip invisible
        if (!renderable.visible)
            continue;

        // Skip invalid renderables
        if (!renderable.isValid())
            continue;

        // Create render object descriptor
        RenderObjectDesc desc;
        desc.object_id = entityToId(entity);
        desc.mesh_id = renderable.meshId;
        desc.material_id = renderable.materialId;

        // Copy world transform
        std::memcpy(desc.transform, transform.worldMatrix.ptr(), sizeof(float) * 16);

        // Copy bounding sphere if available
        if (auto* bounds = registry.try_get<BoundingComponent>(entity))
        {
            desc.bounding_sphere[0] = bounds->worldCenter.x;
            desc.bounding_sphere[1] = bounds->worldCenter.y;
            desc.bounding_sphere[2] = bounds->worldCenter.z;
            desc.bounding_sphere[3] = bounds->worldRadius;
        }

        packet->visibleObjects.push_back(desc);
    }
}

} // namespace vesper
