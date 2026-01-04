#include "mesh.h"
#include "runtime/core/log/log_system.h"

namespace vesper {

Mesh::~Mesh()
{
    if (m_rhi)
    {
        if (m_indexBuffer)
        {
            m_rhi->destroyBuffer(m_indexBuffer);
        }
        if (m_vertexBuffer)
        {
            m_rhi->destroyBuffer(m_vertexBuffer);
        }
    }
}

std::shared_ptr<Mesh> Mesh::create(RHI* rhi, const MeshData& data, const char* debugName)
{
    if (!rhi)
    {
        LOG_ERROR("Mesh::create: RHI is null");
        return nullptr;
    }

    if (!data.isValid())
    {
        LOG_ERROR("Mesh::create: Invalid mesh data");
        return nullptr;
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->m_rhi = rhi;
    mesh->m_vertexLayout = data.vertexLayout;
    mesh->m_vertexCount = data.vertexCount;
    mesh->m_vertexStride = data.vertexStride;
    mesh->m_indexCount = data.getIndexCount();

    // Create vertex buffer
    RHIBufferDesc vbDesc{};
    vbDesc.size = data.getVertexDataSize();
    vbDesc.usage = RHIBufferUsage::Vertex;
    vbDesc.memoryUsage = RHIMemoryUsage::CpuToGpu;

    std::string vbName = debugName ? std::string(debugName) + "_VB" : "MeshVertexBuffer";
    vbDesc.debugName = vbName.c_str();

    mesh->m_vertexBuffer = rhi->createBuffer(vbDesc);
    if (!mesh->m_vertexBuffer)
    {
        LOG_ERROR("Mesh::create: Failed to create vertex buffer");
        return nullptr;
    }

    // Upload vertex data
    rhi->updateBuffer(mesh->m_vertexBuffer, data.vertexData.data(), data.getVertexDataSize(), 0);

    // Create index buffer
    RHIBufferDesc ibDesc{};
    ibDesc.size = data.getIndexDataSize();
    ibDesc.usage = RHIBufferUsage::Index;
    ibDesc.memoryUsage = RHIMemoryUsage::CpuToGpu;

    std::string ibName = debugName ? std::string(debugName) + "_IB" : "MeshIndexBuffer";
    ibDesc.debugName = ibName.c_str();

    mesh->m_indexBuffer = rhi->createBuffer(ibDesc);
    if (!mesh->m_indexBuffer)
    {
        LOG_ERROR("Mesh::create: Failed to create index buffer");
        return nullptr;
    }

    // Upload index data
    rhi->updateBuffer(mesh->m_indexBuffer, data.indices.data(), data.getIndexDataSize(), 0);

    LOG_DEBUG("Mesh::create: Created mesh '{}' with {} vertices, {} indices",
              debugName ? debugName : "unnamed", mesh->m_vertexCount, mesh->m_indexCount);

    return mesh;
}

void Mesh::bind(RHI* rhi, RHICommandBufferHandle cmd) const
{
    if (!isValid())
    {
        return;
    }

    rhi->cmdBindVertexBuffer(cmd, 0, m_vertexBuffer, 0);
    rhi->cmdBindIndexBuffer(cmd, m_indexBuffer, 0, false);  // false = 32-bit indices
}

void Mesh::draw(RHI* rhi, RHICommandBufferHandle cmd, uint32_t instanceCount) const
{
    if (!isValid())
    {
        return;
    }

    rhi->cmdDrawIndexed(cmd, m_indexCount, instanceCount, 0, 0, 0);
}

void Mesh::bindAndDraw(RHI* rhi, RHICommandBufferHandle cmd, uint32_t instanceCount) const
{
    bind(rhi, cmd);
    draw(rhi, cmd, instanceCount);
}

} // namespace vesper
