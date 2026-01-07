#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <string>

namespace vesper {

// ===========================================================================
// EnTT Type Aliases
// ===========================================================================

/// @brief Entity registry - manages all entities and components
using EntityRegistry = entt::registry;

/// @brief Entity handle
using Entity = entt::entity;

/// @brief Entity ID type for serialization
using EntityId = entt::id_type;

/// @brief Null entity constant
constexpr Entity NullEntity = entt::null;

// ===========================================================================
// Helper Functions
// ===========================================================================

/// @brief Check if an entity is valid (not null)
inline bool isValidEntity(Entity entity)
{
    return entity != entt::null;
}

/// @brief Convert entity to integral ID
inline uint32_t entityToId(Entity entity)
{
    return static_cast<uint32_t>(entt::to_integral(entity));
}

/// @brief Convert integral ID to entity
inline Entity idToEntity(uint32_t id)
{
    return static_cast<Entity>(id);
}

// ===========================================================================
// Name Component (optional, for debugging)
// ===========================================================================

/// @brief Name tag for entities
struct NameComponent
{
    std::string name;

    NameComponent() = default;
    explicit NameComponent(const std::string& n) : name(n) {}
    explicit NameComponent(std::string&& n) : name(std::move(n)) {}
};

} // namespace vesper
