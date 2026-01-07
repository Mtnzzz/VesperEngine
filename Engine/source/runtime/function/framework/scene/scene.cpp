#include "runtime/function/framework/scene/scene.h"
#include "runtime/function/framework/ecs/systems/render_bridge_system.h"
#include "runtime/function/render/render_packet.h"
#include "runtime/function/render/mesh.h"
#include "runtime/function/render/material.h"

#include <cstring>

namespace vesper {

Scene::Scene()
    : m_name("Untitled Scene")
{
}

Scene::Scene(const std::string& name)
    : m_name(name)
{
}

// =============================================================================
// Entity Creation
// =============================================================================

Entity Scene::createEntity(const std::string& name)
{
    Entity entity = m_world.createEntity(name);
    m_world.addComponent<TransformComponent>(entity);
    return entity;
}

Entity Scene::createRenderableEntity(const std::string& name,
                                      std::shared_ptr<Mesh> mesh,
                                      MaterialPtr material)
{
    Entity entity = createEntity(name);

    // Add renderable component
    auto& renderable = m_world.addComponent<RenderableComponent>(entity);
    renderable.mesh = std::move(mesh);
    renderable.material = std::move(material);

    // Add bounding component (initialized from mesh if possible)
    auto& bounds = m_world.addComponent<BoundingComponent>(entity);
    if (renderable.mesh)
    {
        // TODO: Get actual bounds from mesh
        // For now, use default sphere
        bounds.initFromSphere(Vector3(0, 0, 0), 1.0f);
    }

    return entity;
}

Entity Scene::createCameraEntity(const std::string& name, const CameraConfig& config)
{
    Entity entity = createEntity(name);

    // Add camera component
    auto& cameraComp = m_world.addComponent<CameraComponent>(entity);
    cameraComp.camera = std::make_shared<Camera>(config);
    cameraComp.active = true;

    // If no main camera set, use this one
    if (m_mainCamera == NullEntity)
    {
        m_mainCamera = entity;
    }

    return entity;
}

void Scene::destroyEntity(Entity entity)
{
    // If destroying main camera, clear it
    if (entity == m_mainCamera)
    {
        m_mainCamera = NullEntity;
    }

    m_world.destroyEntity(entity);
}

// =============================================================================
// Camera Management
// =============================================================================

void Scene::setMainCamera(Entity cameraEntity)
{
    if (m_world.isValid(cameraEntity) && m_world.hasComponent<CameraComponent>(cameraEntity))
    {
        m_mainCamera = cameraEntity;
    }
}

CameraComponent* Scene::getMainCameraComponent()
{
    if (m_mainCamera == NullEntity)
        return nullptr;

    return m_world.tryGetComponent<CameraComponent>(m_mainCamera);
}

Camera* Scene::getMainCameraInstance()
{
    auto* comp = getMainCameraComponent();
    return comp ? comp->camera.get() : nullptr;
}

// =============================================================================
// Scene Update
// =============================================================================

void Scene::update(float /*deltaTime*/)
{
    // Update transform hierarchy
    m_world.updateTransforms();

    // Update world-space bounding volumes
    m_world.updateBounds();
}

void Scene::prepareForRendering(RenderPacketBuffer* buffer, uint64_t frameIndex)
{
    if (!buffer)
        return;

    RenderPacket* packet = buffer->acquireForWrite();
    if (!packet)
        return;

    // Clear previous frame data
    packet->clear();
    packet->frameIndex = frameIndex;

    // Use RenderBridgeSystem for frustum-culled rendering
    RenderBridgeSystem::fillRenderPacket(m_world.registry(), packet, m_mainCamera);

    buffer->releaseWrite();
}

// =============================================================================
// Private Methods
// =============================================================================

void Scene::fillCameraParams(RenderPacket* packet)
{
    Camera* camera = getMainCameraInstance();
    if (!camera)
        return;

    // Get matrices
    Matrix4x4 viewMatrix = camera->getViewMatrix();
    Matrix4x4 projMatrix = camera->getProjectionMatrix();

    // Copy to CameraParams (float arrays)
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

void Scene::fillVisibleObjects(RenderPacket* packet)
{
    // Iterate all renderable entities
    auto view = m_world.registry().view<TransformComponent, RenderableComponent>();

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
        if (auto* bounds = m_world.tryGetComponent<BoundingComponent>(entity))
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
