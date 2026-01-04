#include "model.h"

#include <limits>

namespace vesper {

// =============================================================================
// ModelVertex
// =============================================================================

RHIVertexInputState ModelVertex::getVertexInputState()
{
    RHIVertexInputState state;

    // Single binding for interleaved vertex data
    state.bindings.push_back(RHIVertexBinding{
        .binding = 0,
        .stride = sizeof(ModelVertex),
        .inputRate = RHIVertexInputRate::Vertex
    });

    // Position (location 0)
    state.attributes.push_back(RHIVertexAttribute{
        .location = 0,
        .binding = 0,
        .format = RHIFormat::RGB32_FLOAT,
        .offset = offsetof(ModelVertex, position)
    });

    // Normal (location 1)
    state.attributes.push_back(RHIVertexAttribute{
        .location = 1,
        .binding = 0,
        .format = RHIFormat::RGB32_FLOAT,
        .offset = offsetof(ModelVertex, normal)
    });

    // TexCoord (location 2)
    state.attributes.push_back(RHIVertexAttribute{
        .location = 2,
        .binding = 0,
        .format = RHIFormat::RG32_FLOAT,
        .offset = offsetof(ModelVertex, texCoord)
    });

    // Tangent (location 3)
    state.attributes.push_back(RHIVertexAttribute{
        .location = 3,
        .binding = 0,
        .format = RHIFormat::RGBA32_FLOAT,
        .offset = offsetof(ModelVertex, tangent)
    });

    return state;
}

// =============================================================================
// Model
// =============================================================================

void Model::addSubMesh(SubMesh submesh)
{
    m_subMeshes.push_back(std::move(submesh));
}

void Model::addNode(ModelNode node)
{
    m_nodes.push_back(std::move(node));
}

void Model::setRootNodes(std::vector<uint32_t> rootIndices)
{
    m_rootNodes = std::move(rootIndices);
}

void Model::recalculateBounds()
{
    if (m_subMeshes.empty())
    {
        m_boundsMin = glm::vec3(0.0f);
        m_boundsMax = glm::vec3(0.0f);
        return;
    }

    m_boundsMin = glm::vec3(std::numeric_limits<float>::max());
    m_boundsMax = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& submesh : m_subMeshes)
    {
        m_boundsMin = glm::min(m_boundsMin, submesh.boundsMin);
        m_boundsMax = glm::max(m_boundsMax, submesh.boundsMax);
    }
}

bool Model::isValid() const
{
    for (const auto& submesh : m_subMeshes)
    {
        if (submesh.isValid())
        {
            return true;
        }
    }
    return false;
}

void Model::drawAll(RHI* rhi, RHICommandBufferHandle cmd) const
{
    for (const auto& submesh : m_subMeshes)
    {
        if (submesh.mesh && submesh.mesh->isValid())
        {
            submesh.mesh->bindAndDraw(rhi, cmd);
        }
    }
}

uint32_t Model::getTotalVertexCount() const
{
    uint32_t total = 0;
    for (const auto& submesh : m_subMeshes)
    {
        if (submesh.mesh)
        {
            total += submesh.mesh->getVertexCount();
        }
    }
    return total;
}

uint32_t Model::getTotalIndexCount() const
{
    uint32_t total = 0;
    for (const auto& submesh : m_subMeshes)
    {
        if (submesh.mesh)
        {
            total += submesh.mesh->getIndexCount();
        }
    }
    return total;
}

} // namespace vesper
