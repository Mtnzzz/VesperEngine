#include "runtime/function/framework/ecs/world.h"

namespace vesper {

// =============================================================================
// Entity Management
// =============================================================================

Entity World::createEntity()
{
    return m_registry.create();
}

Entity World::createEntity(const std::string& name)
{
    Entity entity = m_registry.create();
    m_registry.emplace<NameComponent>(entity, name);
    return entity;
}

void World::destroyEntity(Entity entity)
{
    if (!isValid(entity))
        return;

    // Remove from hierarchy first
    if (auto* hierarchy = tryGetComponent<HierarchyComponent>(entity))
    {
        // Unlink from parent
        if (hierarchy->hasParent())
        {
            removeParent(entity);
        }

        // Destroy all children
        Entity child = hierarchy->firstChild;
        while (child != NullEntity)
        {
            Entity next = NullEntity;
            if (auto* childHierarchy = tryGetComponent<HierarchyComponent>(child))
            {
                next = childHierarchy->nextSibling;
            }
            destroyEntity(child);
            child = next;
        }
    }

    m_registry.destroy(entity);
}

bool World::isValid(Entity entity) const
{
    return entity != NullEntity && m_registry.valid(entity);
}

size_t World::entityCount() const
{
    return m_registry.storage<Entity>()->size();
}

// =============================================================================
// System Updates
// =============================================================================

void World::updateTransforms()
{
    // First pass: update root entities (those without parents or with clean parents)
    auto view = m_registry.view<TransformComponent>();

    for (auto [entity, transform] : view.each())
    {
        if (!transform.dirty)
            continue;

        auto* hierarchy = tryGetComponent<HierarchyComponent>(entity);

        // If no hierarchy or no parent, this is a root entity
        if (!hierarchy || !hierarchy->hasParent())
        {
            Matrix4x4 localMatrix = transform.getLocalMatrix();
            transform.worldMatrix = localMatrix;
            transform.clearDirty();

            // Update children recursively
            if (hierarchy && hierarchy->hasChildren())
            {
                updateTransformHierarchy(hierarchy->firstChild, transform.worldMatrix);
            }
        }
    }

    // Second pass: handle remaining dirty transforms (orphaned hierarchies)
    for (auto [entity, transform] : view.each())
    {
        if (transform.dirty)
        {
            transform.worldMatrix = transform.getLocalMatrix();
            transform.clearDirty();
        }
    }
}

void World::updateTransformHierarchy(Entity entity, const Matrix4x4& parentWorld)
{
    while (entity != NullEntity)
    {
        auto* transform = tryGetComponent<TransformComponent>(entity);
        auto* hierarchy = tryGetComponent<HierarchyComponent>(entity);

        if (transform)
        {
            Matrix4x4 localMatrix = transform->getLocalMatrix();
            transform->worldMatrix = localMatrix * parentWorld;
            transform->clearDirty();

            // Recurse to children
            if (hierarchy && hierarchy->hasChildren())
            {
                updateTransformHierarchy(hierarchy->firstChild, transform->worldMatrix);
            }
        }

        // Move to next sibling
        entity = hierarchy ? hierarchy->nextSibling : NullEntity;
    }
}

void World::updateBounds()
{
    auto view = m_registry.view<TransformComponent, BoundingComponent>();

    for (auto [entity, transform, bounds] : view.each())
    {
        bounds.updateWorldBounds(transform.worldMatrix);
    }
}

// =============================================================================
// Hierarchy Operations
// =============================================================================

void World::setParent(Entity child, Entity parent)
{
    if (!isValid(child) || child == parent)
        return;

    // Ensure both have hierarchy components
    if (!hasComponent<HierarchyComponent>(child))
    {
        addComponent<HierarchyComponent>(child);
    }
    if (parent != NullEntity && !hasComponent<HierarchyComponent>(parent))
    {
        addComponent<HierarchyComponent>(parent);
    }

    auto& childHierarchy = getComponent<HierarchyComponent>(child);

    // Remove from old parent first
    if (childHierarchy.hasParent())
    {
        removeParent(child);
    }

    // Set new parent
    childHierarchy.parent = parent;

    if (parent != NullEntity)
    {
        auto& parentHierarchy = getComponent<HierarchyComponent>(parent);

        // Add as first child
        childHierarchy.nextSibling = parentHierarchy.firstChild;
        childHierarchy.prevSibling = NullEntity;

        if (parentHierarchy.firstChild != NullEntity)
        {
            auto& oldFirstChild = getComponent<HierarchyComponent>(parentHierarchy.firstChild);
            oldFirstChild.prevSibling = child;
        }

        parentHierarchy.firstChild = child;
    }

    // Mark transform dirty
    if (auto* transform = tryGetComponent<TransformComponent>(child))
    {
        transform->markDirty();
    }
}

void World::removeParent(Entity child)
{
    if (!isValid(child))
        return;

    auto* childHierarchy = tryGetComponent<HierarchyComponent>(child);
    if (!childHierarchy || !childHierarchy->hasParent())
        return;

    Entity parent = childHierarchy->parent;

    // Unlink from sibling chain
    if (childHierarchy->prevSibling != NullEntity)
    {
        auto& prev = getComponent<HierarchyComponent>(childHierarchy->prevSibling);
        prev.nextSibling = childHierarchy->nextSibling;
    }
    else if (parent != NullEntity)
    {
        // Was first child
        auto& parentHierarchy = getComponent<HierarchyComponent>(parent);
        parentHierarchy.firstChild = childHierarchy->nextSibling;
    }

    if (childHierarchy->nextSibling != NullEntity)
    {
        auto& next = getComponent<HierarchyComponent>(childHierarchy->nextSibling);
        next.prevSibling = childHierarchy->prevSibling;
    }

    childHierarchy->parent = NullEntity;
    childHierarchy->prevSibling = NullEntity;
    childHierarchy->nextSibling = NullEntity;

    // Mark transform dirty
    if (auto* transform = tryGetComponent<TransformComponent>(child))
    {
        transform->markDirty();
    }
}

std::vector<Entity> World::getChildren(Entity parent) const
{
    std::vector<Entity> children;

    auto* hierarchy = tryGetComponent<HierarchyComponent>(parent);
    if (!hierarchy)
        return children;

    Entity child = hierarchy->firstChild;
    while (child != NullEntity)
    {
        children.push_back(child);
        auto* childHierarchy = tryGetComponent<HierarchyComponent>(child);
        child = childHierarchy ? childHierarchy->nextSibling : NullEntity;
    }

    return children;
}

} // namespace vesper
