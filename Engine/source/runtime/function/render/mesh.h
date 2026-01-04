#pragma once

#include "runtime/function/render/rhi/rhi.h"
#include "runtime/function/render/rhi/rhi_types.h"

#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>

namespace vesper {

/// @brief CPU-side mesh data with flexible vertex format support
struct MeshData
{
    std::vector<uint8_t>  vertexData;    // Raw vertex byte data
    std::vector<uint32_t> indices;       // Index data (32-bit)
    RHIVertexInputState   vertexLayout;  // Vertex layout description
    uint32_t              vertexStride = 0;  // Bytes per vertex
    uint32_t              vertexCount = 0;   // Number of vertices

    /// @brief Set vertex data from a typed vertex array
    template<typename VertexType>
    void setVertices(const std::vector<VertexType>& vertices)
    {
        setVertices(vertices.data(), vertices.size());
    }

    /// @brief Set vertex data from a typed vertex pointer
    template<typename VertexType>
    void setVertices(const VertexType* data, size_t count)
    {
        vertexStride = sizeof(VertexType);
        vertexCount = static_cast<uint32_t>(count);
        vertexData.resize(vertexStride * count);
        std::memcpy(vertexData.data(), data, vertexData.size());
    }

    /// @brief Set vertex data with explicit layout
    template<typename VertexType>
    void setVerticesWithLayout(const std::vector<VertexType>& vertices, const RHIVertexInputState& layout)
    {
        setVertices(vertices);
        vertexLayout = layout;
    }

    /// @brief Get vertex data size in bytes
    size_t getVertexDataSize() const { return vertexData.size(); }

    /// @brief Get index data size in bytes
    size_t getIndexDataSize() const { return indices.size() * sizeof(uint32_t); }

    /// @brief Get index count
    uint32_t getIndexCount() const { return static_cast<uint32_t>(indices.size()); }

    /// @brief Check if mesh data is valid
    bool isValid() const
    {
        return !vertexData.empty() && !indices.empty() && vertexStride > 0 && vertexCount > 0;
    }
};

/// @brief GPU-side mesh resource
class Mesh
{
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    /// @brief Create mesh from MeshData
    /// @param rhi RHI instance for GPU resource creation
    /// @param data CPU-side mesh data
    /// @param debugName Optional debug name for GPU resources
    /// @return Shared pointer to created mesh, nullptr on failure
    static std::shared_ptr<Mesh> create(RHI* rhi, const MeshData& data, const char* debugName = nullptr);

    /// @brief Bind vertex and index buffers to command buffer
    /// @param rhi RHI instance
    /// @param cmd Command buffer to bind to
    void bind(RHI* rhi, RHICommandBufferHandle cmd) const;

    /// @brief Draw the mesh
    /// @param rhi RHI instance
    /// @param cmd Command buffer
    /// @param instanceCount Number of instances to draw
    void draw(RHI* rhi, RHICommandBufferHandle cmd, uint32_t instanceCount = 1) const;

    /// @brief Bind and draw in one call
    void bindAndDraw(RHI* rhi, RHICommandBufferHandle cmd, uint32_t instanceCount = 1) const;

    // Accessors
    RHIBufferHandle getVertexBuffer() const { return m_vertexBuffer; }
    RHIBufferHandle getIndexBuffer() const { return m_indexBuffer; }
    uint32_t getIndexCount() const { return m_indexCount; }
    uint32_t getVertexCount() const { return m_vertexCount; }
    uint32_t getVertexStride() const { return m_vertexStride; }
    const RHIVertexInputState& getVertexLayout() const { return m_vertexLayout; }

    /// @brief Check if mesh is valid and ready for rendering
    bool isValid() const { return m_vertexBuffer && m_indexBuffer && m_indexCount > 0; }

private:
    RHI*                m_rhi = nullptr;
    RHIBufferHandle     m_vertexBuffer;
    RHIBufferHandle     m_indexBuffer;
    RHIVertexInputState m_vertexLayout;
    uint32_t            m_indexCount = 0;
    uint32_t            m_vertexCount = 0;
    uint32_t            m_vertexStride = 0;
};

} // namespace vesper
