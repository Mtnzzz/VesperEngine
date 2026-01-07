#pragma once

#include "runtime/function/framework/ecs/world.h"
#include "runtime/function/framework/component/camera/camera_component.h"
#include "runtime/function/framework/component/mesh/renderable_component.h"

#include <string>

namespace vesper {

// Forward declarations
class RenderPacketBuffer;
struct RenderPacket;
class Mesh;
class Material;

/// @brief Scene class - high-level scene management
class Scene
{
public:
    Scene();
    explicit Scene(const std::string& name);
    ~Scene() = default;

    // Non-copyable
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Movable
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

    // =========================================================================
    // Entity Creation Helpers
    // =========================================================================

    /// @brief Create an empty named entity
    Entity createEntity(const std::string& name = "Entity");

    /// @brief Create a renderable entity with mesh and material
    Entity createRenderableEntity(const std::string& name,
                                   std::shared_ptr<Mesh> mesh,
                                   MaterialPtr material);

    /// @brief Create a camera entity
    Entity createCameraEntity(const std::string& name,
                               const CameraConfig& config = CameraConfig{});

    /// @brief Destroy an entity
    void destroyEntity(Entity entity);

    // =========================================================================
    // Camera Management
    // =========================================================================

    /// @brief Set the main camera entity
    void setMainCamera(Entity cameraEntity);

    /// @brief Get the main camera entity
    Entity getMainCamera() const { return m_mainCamera; }

    /// @brief Get the main camera component
    CameraComponent* getMainCameraComponent();

    /// @brief Get the main camera instance
    Camera* getMainCameraInstance();

    // =========================================================================
    // Scene Update
    // =========================================================================

    /// @brief Update scene (transforms, bounds, etc.)
    void update(float deltaTime);

    /// @brief Prepare scene data for rendering (fills RenderPacket)
    void prepareForRendering(RenderPacketBuffer* buffer, uint64_t frameIndex);

    // =========================================================================
    // Accessors
    // =========================================================================

    /// @brief Get the underlying World
    World& world() { return m_world; }
    const World& world() const { return m_world; }

    /// @brief Get scene name
    const std::string& name() const { return m_name; }

    /// @brief Set scene name
    void setName(const std::string& name) { m_name = name; }

private:
    /// @brief Fill camera parameters into RenderPacket
    void fillCameraParams(RenderPacket* packet);

    /// @brief Fill visible objects into RenderPacket
    void fillVisibleObjects(RenderPacket* packet);

private:
    std::string m_name;
    World m_world;
    Entity m_mainCamera{NullEntity};
};

} // namespace vesper
