#pragma once

#include "runtime/function/render/mesh.h"
#include "runtime/function/render/material.h"
#include "runtime/function/render/model.h"

#include <memory>
#include <cstdint>

namespace vesper {

/// @brief Renderable component - associates mesh and material for rendering
struct RenderableComponent
{
    // =========================================================================
    // Resource References
    // =========================================================================

    /// @brief Mesh resource ID (for resource manager lookup)
    uint32_t meshId{0};

    /// @brief Material resource ID (for resource manager lookup)
    uint32_t materialId{0};

    /// @brief Direct mesh reference (alternative to ID)
    std::shared_ptr<Mesh> mesh;

    /// @brief Direct material reference (alternative to ID)
    MaterialPtr material;

    /// @brief Optional model reference (for multi-submesh models)
    std::shared_ptr<Model> model;

    /// @brief Submesh index within model (if using model)
    uint32_t submeshIndex{0};

    // =========================================================================
    // Rendering Flags
    // =========================================================================

    /// @brief Whether this entity should be rendered
    bool visible{true};

    /// @brief Whether this entity casts shadows
    bool castShadow{true};

    /// @brief Whether this entity receives shadows
    bool receiveShadow{true};

    // =========================================================================
    // Sorting and Layering
    // =========================================================================

    /// @brief Render layer (for grouping/filtering)
    uint32_t renderLayer{0};

    /// @brief Sort order within layer (lower = render first)
    int32_t sortOrder{0};

    // =========================================================================
    // Validation
    // =========================================================================

    /// @brief Check if this component has valid rendering resources
    bool isValid() const
    {
        // Valid if has direct mesh reference
        if (mesh && mesh->isValid())
        {
            return true;
        }

        // Valid if has model with submeshes
        if (model && model->isValid() && submeshIndex < model->getSubMeshCount())
        {
            return true;
        }

        // Valid if has mesh ID (will be resolved by resource manager)
        if (meshId != 0)
        {
            return true;
        }

        return false;
    }

    /// @brief Get the mesh to render (resolves model submesh if needed)
    std::shared_ptr<Mesh> getActiveMesh() const
    {
        if (mesh)
        {
            return mesh;
        }

        if (model && submeshIndex < model->getSubMeshCount())
        {
            return model->getSubMesh(submeshIndex).mesh;
        }

        return nullptr;
    }

    /// @brief Get the material to use (resolves model submesh if needed)
    MaterialPtr getActiveMaterial() const
    {
        if (material)
        {
            return material;
        }

        if (model && submeshIndex < model->getSubMeshCount())
        {
            return model->getSubMesh(submeshIndex).material;
        }

        return nullptr;
    }
};

} // namespace vesper
