#pragma once

#include "runtime/function/render/mesh.h"
#include "runtime/function/render/material.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>

namespace vesper {

/// @brief Standard vertex format for loaded models
struct ModelVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec4 tangent;    // xyz = tangent, w = handedness

    /// @brief Get vertex input state for this vertex type
    static RHIVertexInputState getVertexInputState();
};

/// @brief Sub-mesh within a model (one draw call)
struct SubMesh
{
    std::shared_ptr<Mesh> mesh;
    MaterialPtr material;
    std::string name;

    // Local bounding box (in mesh space)
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};

    bool isValid() const { return mesh && mesh->isValid(); }
};

/// @brief Node in the model hierarchy
struct ModelNode
{
    std::string name;
    glm::mat4 localTransform{1.0f};
    std::vector<uint32_t> meshIndices;   // Indices into Model::m_subMeshes
    std::vector<uint32_t> childIndices;  // Indices into Model::m_nodes
    int32_t parentIndex = -1;            // -1 for root nodes
};

/// @brief Complete 3D model loaded from file
///
/// A Model contains:
/// - Multiple SubMeshes (each with its own geometry and material)
/// - Optional node hierarchy for scene graph support
/// - Bounding volume for culling
class Model
{
public:
    Model() = default;
    ~Model() = default;

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // =========================================================================
    // SubMesh Access
    // =========================================================================

    /// @brief Add a submesh to the model
    void addSubMesh(SubMesh submesh);

    /// @brief Get number of submeshes
    size_t getSubMeshCount() const { return m_subMeshes.size(); }

    /// @brief Get submesh by index
    const SubMesh& getSubMesh(size_t index) const { return m_subMeshes[index]; }
    SubMesh& getSubMesh(size_t index) { return m_subMeshes[index]; }

    /// @brief Get all submeshes
    const std::vector<SubMesh>& getSubMeshes() const { return m_subMeshes; }

    // =========================================================================
    // Node Hierarchy
    // =========================================================================

    /// @brief Add a node to the hierarchy
    void addNode(ModelNode node);

    /// @brief Get number of nodes
    size_t getNodeCount() const { return m_nodes.size(); }

    /// @brief Get node by index
    const ModelNode& getNode(size_t index) const { return m_nodes[index]; }
    ModelNode& getNode(size_t index) { return m_nodes[index]; }

    /// @brief Get all nodes
    const std::vector<ModelNode>& getNodes() const { return m_nodes; }

    /// @brief Set root node indices
    void setRootNodes(std::vector<uint32_t> rootIndices);

    /// @brief Get root node indices
    const std::vector<uint32_t>& getRootNodes() const { return m_rootNodes; }

    // =========================================================================
    // Bounds
    // =========================================================================

    /// @brief Recalculate bounds from all submeshes
    void recalculateBounds();

    /// @brief Get model-space bounding box minimum
    glm::vec3 getBoundsMin() const { return m_boundsMin; }

    /// @brief Get model-space bounding box maximum
    glm::vec3 getBoundsMax() const { return m_boundsMax; }

    /// @brief Get bounding box center
    glm::vec3 getBoundsCenter() const { return (m_boundsMin + m_boundsMax) * 0.5f; }

    /// @brief Get bounding box extents (half-size)
    glm::vec3 getBoundsExtents() const { return (m_boundsMax - m_boundsMin) * 0.5f; }

    // =========================================================================
    // Metadata
    // =========================================================================

    /// @brief Set model name
    void setName(const std::string& name) { m_name = name; }

    /// @brief Get model name
    const std::string& getName() const { return m_name; }

    /// @brief Set source file path
    void setSourcePath(const std::string& path) { m_sourcePath = path; }

    /// @brief Get source file path
    const std::string& getSourcePath() const { return m_sourcePath; }

    /// @brief Check if model is valid (has at least one valid submesh)
    bool isValid() const;

    // =========================================================================
    // Rendering Helpers
    // =========================================================================

    /// @brief Draw all submeshes (simple rendering, no hierarchy)
    /// @param rhi RHI instance
    /// @param cmd Command buffer
    void drawAll(RHI* rhi, RHICommandBufferHandle cmd) const;

    /// @brief Get total vertex count across all submeshes
    uint32_t getTotalVertexCount() const;

    /// @brief Get total index/triangle count
    uint32_t getTotalIndexCount() const;

private:
    std::string m_name;
    std::string m_sourcePath;

    // Geometry and materials
    std::vector<SubMesh> m_subMeshes;

    // Optional node hierarchy
    std::vector<ModelNode> m_nodes;
    std::vector<uint32_t> m_rootNodes;

    // Model-space bounding box
    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{0.0f};
};

using ModelPtr = std::shared_ptr<Model>;

} // namespace vesper
