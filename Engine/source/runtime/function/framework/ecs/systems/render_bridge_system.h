#pragma once

#include "runtime/function/framework/ecs/ecs_types.h"
#include "runtime/function/framework/ecs/systems/frustum.h"

namespace vesper {

// Forward declarations
struct RenderPacket;
class Camera;

/// @brief RenderBridgeSystem - converts ECS data to RenderPacket for rendering
class RenderBridgeSystem
{
public:
    /// @brief Fill a RenderPacket from ECS data with frustum culling
    /// @param registry The entity registry containing scene data
    /// @param packet The packet to fill
    /// @param mainCamera The main camera entity
    static void fillRenderPacket(EntityRegistry& registry,
                                  RenderPacket* packet,
                                  Entity mainCamera);

    /// @brief Fill only camera parameters
    /// @param camera Camera to extract parameters from
    /// @param packet The packet to fill
    static void fillCameraParams(Camera* camera, RenderPacket* packet);

    /// @brief Fill visible objects with frustum culling
    /// @param registry The entity registry
    /// @param packet The packet to fill
    /// @param frustum Frustum for culling
    static void fillVisibleObjects(EntityRegistry& registry,
                                    RenderPacket* packet,
                                    const Frustum& frustum);

    /// @brief Fill visible objects without frustum culling
    /// @param registry The entity registry
    /// @param packet The packet to fill
    static void fillVisibleObjectsNoClip(EntityRegistry& registry,
                                          RenderPacket* packet);
};

} // namespace vesper
