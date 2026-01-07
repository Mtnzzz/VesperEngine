#pragma once

#include "runtime/function/framework/ecs/ecs_types.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/component/common/bounding_component.h"

#include <vector>

namespace vesper {

/// @brief World class - manages ECS registry and entity operations
class World
{
public:
    World() = default;
    ~World() = default;

    // Non-copyable
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Movable
    World(World&&) = default;
    World& operator=(World&&) = default;

    // =========================================================================
    // Entity Management
    // =========================================================================

    /// @brief Create a new entity
    Entity createEntity();

    /// @brief Create a named entity
    Entity createEntity(const std::string& name);

    /// @brief Destroy an entity and all its components
    void destroyEntity(Entity entity);

    /// @brief Check if entity is valid and exists
    bool isValid(Entity entity) const;

    /// @brief Get total entity count
    size_t entityCount() const;

    // =========================================================================
    // Component Operations
    // =========================================================================

    /// @brief Add a component to entity
    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args)
    {
        return m_registry.emplace<T>(entity, std::forward<Args>(args)...);
    }

    /// @brief Get a component from entity
    template<typename T>
    T& getComponent(Entity entity)
    {
        return m_registry.get<T>(entity);
    }

    /// @brief Get a component from entity (const)
    template<typename T>
    const T& getComponent(Entity entity) const
    {
        return m_registry.get<T>(entity);
    }

    /// @brief Try to get a component (returns nullptr if not exists)
    template<typename T>
    T* tryGetComponent(Entity entity)
    {
        return m_registry.try_get<T>(entity);
    }

    /// @brief Try to get a component (const, returns nullptr if not exists)
    template<typename T>
    const T* tryGetComponent(Entity entity) const
    {
        return m_registry.try_get<T>(entity);
    }

    /// @brief Check if entity has a component
    template<typename T>
    bool hasComponent(Entity entity) const
    {
        return m_registry.all_of<T>(entity);
    }

    /// @brief Remove a component from entity
    template<typename T>
    void removeComponent(Entity entity)
    {
        m_registry.remove<T>(entity);
    }

    // =========================================================================
    // System Updates
    // =========================================================================

    /// @brief Update all dirty transforms (compute world matrices)
    void updateTransforms();

    /// @brief Update world bounds for all entities with BoundingComponent
    void updateBounds();

    // =========================================================================
    // Hierarchy Operations
    // =========================================================================

    /// @brief Set parent-child relationship
    void setParent(Entity child, Entity parent);

    /// @brief Remove parent (make entity root)
    void removeParent(Entity child);

    /// @brief Get all children of an entity
    std::vector<Entity> getChildren(Entity parent) const;

    // =========================================================================
    // Registry Access
    // =========================================================================

    /// @brief Get the underlying registry
    EntityRegistry& registry() { return m_registry; }

    /// @brief Get the underlying registry (const)
    const EntityRegistry& registry() const { return m_registry; }

private:
    /// @brief Recursively update transforms for hierarchy
    void updateTransformHierarchy(Entity entity, const Matrix4x4& parentWorld);

private:
    EntityRegistry m_registry;
};

} // namespace vesper
