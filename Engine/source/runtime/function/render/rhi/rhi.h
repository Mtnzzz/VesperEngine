#pragma once

#include "runtime/function/render/rhi/rhi_types.h"

#include <memory>
#include <functional>
#include <span>

namespace vesper {

// ============================================================================
// RHI Base Resource Types
// ============================================================================

struct RHIBuffer
{
    virtual ~RHIBuffer() = default;

    uint64_t        size        = 0;
    RHIBufferUsage  usage       = RHIBufferUsage::None;
    RHIMemoryUsage  memoryUsage = RHIMemoryUsage::GpuOnly;
    void*           mappedData  = nullptr;
};

struct RHITexture
{
    virtual ~RHITexture() = default;

    RHIExtent3D         extent      = {};
    uint32_t            mipLevels   = 1;
    uint32_t            arrayLayers = 1;
    RHIFormat           format      = RHIFormat::Undefined;
    RHITextureDimension dimension   = RHITextureDimension::Tex2D;
    RHISampleCount      sampleCount = RHISampleCount::Count1;
    RHITextureUsage     usage       = RHITextureUsage::Sampled;
    RHIResourceState    currentState = RHIResourceState::Undefined;
};

struct RHISampler
{
    virtual ~RHISampler() = default;
};

struct RHIShader
{
    virtual ~RHIShader() = default;

    RHIShaderStage stage = RHIShaderStage::None;
};

struct RHIPipeline
{
    virtual ~RHIPipeline() = default;

    bool isCompute = false;
};

struct RHIDescriptorSetLayout
{
    virtual ~RHIDescriptorSetLayout() = default;
};

struct RHIDescriptorSet
{
    virtual ~RHIDescriptorSet() = default;
};

struct RHICommandPool
{
    virtual ~RHICommandPool() = default;

    RHIQueueType queueType = RHIQueueType::Graphics;
};

struct RHICommandBuffer
{
    virtual ~RHICommandBuffer() = default;

    bool isRecording = false;
    bool isSecondary = false;
};

struct RHIFence
{
    virtual ~RHIFence() = default;

    bool signaled = false;
};

struct RHISemaphore
{
    virtual ~RHISemaphore() = default;
};

struct RHIQueue
{
    virtual ~RHIQueue() = default;

    RHIQueueType type       = RHIQueueType::Graphics;
    uint32_t     familyIndex = 0;
    uint32_t     queueIndex  = 0;
};

struct RHISwapChain
{
    virtual ~RHISwapChain() = default;

    uint32_t            width       = 0;
    uint32_t            height      = 0;
    uint32_t            imageCount  = 0;
    RHIFormat           format      = RHIFormat::Undefined;
    uint32_t            currentImageIndex = 0;
};

// ============================================================================
// RHI Interface - Abstract Backend Interface
// ============================================================================

class RHI
{
public:
    virtual ~RHI() = default;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    virtual bool initialize(const RHIConfig& config) = 0;
    virtual void shutdown() = 0;

    // ========================================================================
    // Device Information
    // ========================================================================

    virtual RHIBackendType getBackendType() const = 0;
    virtual const RHIGpuInfo& getGpuInfo() const = 0;

    // ========================================================================
    // Queue Management
    // ========================================================================

    virtual RHIQueueHandle getQueue(RHIQueueType type) = 0;

    // ========================================================================
    // SwapChain
    // ========================================================================

    virtual RHISwapChainHandle createSwapChain(const RHISwapChainDesc& desc) = 0;
    virtual void destroySwapChain(RHISwapChainHandle swapChain) = 0;
    virtual void resizeSwapChain(RHISwapChainHandle swapChain, uint32_t width, uint32_t height) = 0;
    virtual RHITextureHandle getSwapChainImage(RHISwapChainHandle swapChain, uint32_t index) = 0;
    virtual uint32_t getSwapChainImageCount(RHISwapChainHandle swapChain) = 0;

    // ========================================================================
    // Resource Creation
    // ========================================================================

    virtual RHIBufferHandle createBuffer(const RHIBufferDesc& desc) = 0;
    virtual void destroyBuffer(RHIBufferHandle buffer) = 0;

    virtual RHITextureHandle createTexture(const RHITextureDesc& desc) = 0;
    virtual void destroyTexture(RHITextureHandle texture) = 0;

    virtual RHISamplerHandle createSampler(const RHISamplerDesc& desc) = 0;
    virtual void destroySampler(RHISamplerHandle sampler) = 0;

    // ========================================================================
    // Buffer Operations
    // ========================================================================

    virtual void* mapBuffer(RHIBufferHandle buffer) = 0;
    virtual void unmapBuffer(RHIBufferHandle buffer) = 0;
    virtual void updateBuffer(RHIBufferHandle buffer, const void* data, uint64_t size, uint64_t offset = 0) = 0;

    // ========================================================================
    // Shader and Pipeline
    // ========================================================================

    virtual RHIShaderHandle createShader(const RHIShaderDesc& desc) = 0;
    virtual void destroyShader(RHIShaderHandle shader) = 0;

    virtual RHIDescriptorSetLayoutHandle createDescriptorSetLayout(const RHIDescriptorSetLayoutDesc& desc) = 0;
    virtual void destroyDescriptorSetLayout(RHIDescriptorSetLayoutHandle layout) = 0;

    virtual RHIPipelineHandle createGraphicsPipeline(const RHIGraphicsPipelineDesc& desc) = 0;
    virtual RHIPipelineHandle createComputePipeline(const RHIComputePipelineDesc& desc) = 0;
    virtual void destroyPipeline(RHIPipelineHandle pipeline) = 0;

    // ========================================================================
    // Descriptor Sets
    // ========================================================================

    virtual RHIDescriptorSetHandle createDescriptorSet(RHIDescriptorSetLayoutHandle layout) = 0;
    virtual void destroyDescriptorSet(RHIDescriptorSetHandle set) = 0;
    virtual void updateDescriptorSet(RHIDescriptorSetHandle set, std::span<const RHIDescriptorWrite> writes) = 0;

    // ========================================================================
    // Command Pools and Buffers
    // ========================================================================

    virtual RHICommandPoolHandle createCommandPool(const RHICommandPoolDesc& desc) = 0;
    virtual void destroyCommandPool(RHICommandPoolHandle pool) = 0;
    virtual void resetCommandPool(RHICommandPoolHandle pool) = 0;

    virtual RHICommandBufferHandle allocateCommandBuffer(RHICommandPoolHandle pool, const RHICommandBufferDesc& desc = {}) = 0;
    virtual void freeCommandBuffer(RHICommandPoolHandle pool, RHICommandBufferHandle cmd) = 0;

    // ========================================================================
    // Synchronization
    // ========================================================================

    virtual RHIFenceHandle createFence(bool signaled = false) = 0;
    virtual void destroyFence(RHIFenceHandle fence) = 0;
    virtual void waitForFence(RHIFenceHandle fence, uint64_t timeout = UINT64_MAX) = 0;
    virtual void resetFence(RHIFenceHandle fence) = 0;
    virtual bool isFenceSignaled(RHIFenceHandle fence) = 0;

    virtual RHISemaphoreHandle createSemaphore() = 0;
    virtual void destroySemaphore(RHISemaphoreHandle semaphore) = 0;

    // ========================================================================
    // Command Recording
    // ========================================================================

    virtual void beginCommandBuffer(RHICommandBufferHandle cmd) = 0;
    virtual void endCommandBuffer(RHICommandBufferHandle cmd) = 0;

    // Dynamic Rendering (Vulkan 1.3+ / VK_KHR_dynamic_rendering)
    virtual void cmdBeginRendering(RHICommandBufferHandle cmd, const RHIRenderingInfo& info) = 0;
    virtual void cmdEndRendering(RHICommandBufferHandle cmd) = 0;

    // Viewport and Scissor
    virtual void cmdSetViewport(RHICommandBufferHandle cmd, const RHIViewport& viewport) = 0;
    virtual void cmdSetScissor(RHICommandBufferHandle cmd, const RHIRect2D& scissor) = 0;

    // Pipeline Binding
    virtual void cmdBindPipeline(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline) = 0;
    virtual void cmdBindDescriptorSets(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline,
                                       uint32_t firstSet, std::span<const RHIDescriptorSetHandle> sets,
                                       std::span<const uint32_t> dynamicOffsets = {}) = 0;

    // Vertex/Index Buffers
    virtual void cmdBindVertexBuffer(RHICommandBufferHandle cmd, uint32_t binding,
                                     RHIBufferHandle buffer, uint64_t offset = 0) = 0;
    virtual void cmdBindIndexBuffer(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                                    uint64_t offset = 0, bool use16Bit = false) = 0;

    // Draw Commands
    virtual void cmdDraw(RHICommandBufferHandle cmd, uint32_t vertexCount, uint32_t instanceCount = 1,
                         uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
    virtual void cmdDrawIndexed(RHICommandBufferHandle cmd, uint32_t indexCount, uint32_t instanceCount = 1,
                                uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;
    virtual void cmdDrawIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                                 uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void cmdDrawIndexedIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                                        uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;

    // Compute Commands
    virtual void cmdDispatch(RHICommandBufferHandle cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    virtual void cmdDispatchIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer, uint64_t offset) = 0;

    // Resource Barriers
    virtual void cmdPipelineBarrier(RHICommandBufferHandle cmd,
                                    std::span<const RHIBufferBarrier> bufferBarriers,
                                    std::span<const RHITextureBarrier> textureBarriers) = 0;

    // Copy Commands
    virtual void cmdCopyBuffer(RHICommandBufferHandle cmd, RHIBufferHandle src, RHIBufferHandle dst,
                               uint64_t srcOffset, uint64_t dstOffset, uint64_t size) = 0;
    virtual void cmdCopyBufferToTexture(RHICommandBufferHandle cmd, RHIBufferHandle src, RHITextureHandle dst,
                                        uint64_t bufferOffset, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) = 0;
    virtual void cmdCopyTextureToBuffer(RHICommandBufferHandle cmd, RHITextureHandle src, RHIBufferHandle dst,
                                        uint32_t mipLevel, uint32_t arrayLayer, uint64_t bufferOffset) = 0;

    // Push Constants
    virtual void cmdPushConstants(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline,
                                  RHIShaderStage stages, uint32_t offset, uint32_t size, const void* data) = 0;

    // Debug Markers
    virtual void cmdBeginDebugLabel(RHICommandBufferHandle cmd, const char* label, float color[4] = nullptr) = 0;
    virtual void cmdEndDebugLabel(RHICommandBufferHandle cmd) = 0;
    virtual void cmdInsertDebugLabel(RHICommandBufferHandle cmd, const char* label, float color[4] = nullptr) = 0;

    // ========================================================================
    // Queue Submission
    // ========================================================================

    struct SubmitInfo
    {
        std::span<const RHICommandBufferHandle> commandBuffers;
        std::span<const RHISemaphoreHandle>     waitSemaphores;
        std::span<const RHISemaphoreHandle>     signalSemaphores;
        RHIFenceHandle                          fence;
    };

    virtual void queueSubmit(RHIQueueHandle queue, const SubmitInfo& submitInfo) = 0;
    virtual void queueWaitIdle(RHIQueueHandle queue) = 0;

    // ========================================================================
    // SwapChain Present
    // ========================================================================

    struct PresentInfo
    {
        RHISwapChainHandle                      swapChain;
        uint32_t                                imageIndex;
        std::span<const RHISemaphoreHandle>     waitSemaphores;
    };

    enum class AcquireResult
    {
        Success,
        Timeout,
        OutOfDate,
        Suboptimal,
        SurfaceLost,    // Surface is no longer valid, needs recreation
        Error,
    };

    virtual AcquireResult acquireNextImage(RHISwapChainHandle swapChain, RHISemaphoreHandle semaphore,
                                           RHIFenceHandle fence, uint64_t timeout, uint32_t* imageIndex) = 0;
    virtual bool queuePresent(RHIQueueHandle queue, const PresentInfo& presentInfo) = 0;

    // ========================================================================
    // Utility
    // ========================================================================

    virtual void waitIdle() = 0;

    // Get current frame fence value for deferred deletion
    virtual uint64_t getCurrentFrameIndex() const = 0;
    virtual uint64_t getCompletedFrameIndex() const = 0;
};

// Factory function for creating RHI backend
std::unique_ptr<RHI> createRHI(RHIBackendType type = RHIBackendType::Vulkan);

} // namespace vesper
