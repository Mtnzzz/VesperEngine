#pragma once

#include "runtime/function/render/rhi/rhi.h"
#include "runtime/function/render/backend/vulkan/vulkan_types.h"
#include "runtime/function/render/deferred_deletion.h"

#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace vesper {

class VulkanRHI : public RHI
{
public:
    VulkanRHI();
    ~VulkanRHI() override;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool initialize(const RHIConfig& config) override;
    void shutdown() override;

    // ========================================================================
    // Device Information
    // ========================================================================

    RHIBackendType getBackendType() const override { return RHIBackendType::Vulkan; }
    const RHIGpuInfo& getGpuInfo() const override { return m_gpuInfo; }

    // ========================================================================
    // Queue Management
    // ========================================================================

    RHIQueueHandle getQueue(RHIQueueType type) override;

    // ========================================================================
    // SwapChain
    // ========================================================================

    RHISwapChainHandle createSwapChain(const RHISwapChainDesc& desc) override;
    void destroySwapChain(RHISwapChainHandle swapChain) override;
    void resizeSwapChain(RHISwapChainHandle swapChain, uint32_t width, uint32_t height) override;
    RHITextureHandle getSwapChainImage(RHISwapChainHandle swapChain, uint32_t index) override;
    uint32_t getSwapChainImageCount(RHISwapChainHandle swapChain) override;

    // ========================================================================
    // Resource Creation
    // ========================================================================

    RHIBufferHandle createBuffer(const RHIBufferDesc& desc) override;
    void destroyBuffer(RHIBufferHandle buffer) override;

    RHITextureHandle createTexture(const RHITextureDesc& desc) override;
    void destroyTexture(RHITextureHandle texture) override;

    RHISamplerHandle createSampler(const RHISamplerDesc& desc) override;
    void destroySampler(RHISamplerHandle sampler) override;

    // ========================================================================
    // Buffer Operations
    // ========================================================================

    void* mapBuffer(RHIBufferHandle buffer) override;
    void unmapBuffer(RHIBufferHandle buffer) override;
    void updateBuffer(RHIBufferHandle buffer, const void* data, uint64_t size, uint64_t offset = 0) override;

    // ========================================================================
    // Shader and Pipeline
    // ========================================================================

    RHIShaderHandle createShader(const RHIShaderDesc& desc) override;
    void destroyShader(RHIShaderHandle shader) override;

    RHIDescriptorSetLayoutHandle createDescriptorSetLayout(const RHIDescriptorSetLayoutDesc& desc) override;
    void destroyDescriptorSetLayout(RHIDescriptorSetLayoutHandle layout) override;

    RHIPipelineHandle createGraphicsPipeline(const RHIGraphicsPipelineDesc& desc) override;
    RHIPipelineHandle createComputePipeline(const RHIComputePipelineDesc& desc) override;
    void destroyPipeline(RHIPipelineHandle pipeline) override;

    // ========================================================================
    // Descriptor Sets
    // ========================================================================

    RHIDescriptorSetHandle createDescriptorSet(RHIDescriptorSetLayoutHandle layout) override;
    void destroyDescriptorSet(RHIDescriptorSetHandle set) override;
    void updateDescriptorSet(RHIDescriptorSetHandle set, std::span<const RHIDescriptorWrite> writes) override;

    // ========================================================================
    // Command Pools and Buffers
    // ========================================================================

    RHICommandPoolHandle createCommandPool(const RHICommandPoolDesc& desc) override;
    void destroyCommandPool(RHICommandPoolHandle pool) override;
    void resetCommandPool(RHICommandPoolHandle pool) override;

    RHICommandBufferHandle allocateCommandBuffer(RHICommandPoolHandle pool, const RHICommandBufferDesc& desc = {}) override;
    void freeCommandBuffer(RHICommandPoolHandle pool, RHICommandBufferHandle cmd) override;

    // ========================================================================
    // Synchronization
    // ========================================================================

    RHIFenceHandle createFence(bool signaled = false) override;
    void destroyFence(RHIFenceHandle fence) override;
    void waitForFence(RHIFenceHandle fence, uint64_t timeout = UINT64_MAX) override;
    void resetFence(RHIFenceHandle fence) override;
    bool isFenceSignaled(RHIFenceHandle fence) override;

    RHISemaphoreHandle createSemaphore() override;
    void destroySemaphore(RHISemaphoreHandle semaphore) override;

    // ========================================================================
    // Command Recording
    // ========================================================================

    void beginCommandBuffer(RHICommandBufferHandle cmd) override;
    void endCommandBuffer(RHICommandBufferHandle cmd) override;

    void cmdBeginRendering(RHICommandBufferHandle cmd, const RHIRenderingInfo& info) override;
    void cmdEndRendering(RHICommandBufferHandle cmd) override;

    void cmdSetViewport(RHICommandBufferHandle cmd, const RHIViewport& viewport) override;
    void cmdSetScissor(RHICommandBufferHandle cmd, const RHIRect2D& scissor) override;

    void cmdBindPipeline(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline) override;
    void cmdBindDescriptorSets(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline,
                               uint32_t firstSet, std::span<const RHIDescriptorSetHandle> sets,
                               std::span<const uint32_t> dynamicOffsets = {}) override;

    void cmdBindVertexBuffer(RHICommandBufferHandle cmd, uint32_t binding,
                             RHIBufferHandle buffer, uint64_t offset = 0) override;
    void cmdBindIndexBuffer(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                            uint64_t offset = 0, bool use16Bit = false) override;

    void cmdDraw(RHICommandBufferHandle cmd, uint32_t vertexCount, uint32_t instanceCount = 1,
                 uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
    void cmdDrawIndexed(RHICommandBufferHandle cmd, uint32_t indexCount, uint32_t instanceCount = 1,
                        uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) override;
    void cmdDrawIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                         uint64_t offset, uint32_t drawCount, uint32_t stride) override;
    void cmdDrawIndexedIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                                uint64_t offset, uint32_t drawCount, uint32_t stride) override;

    void cmdDispatch(RHICommandBufferHandle cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void cmdDispatchIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer, uint64_t offset) override;

    void cmdPipelineBarrier(RHICommandBufferHandle cmd,
                            std::span<const RHIBufferBarrier> bufferBarriers,
                            std::span<const RHITextureBarrier> textureBarriers) override;

    void cmdCopyBuffer(RHICommandBufferHandle cmd, RHIBufferHandle src, RHIBufferHandle dst,
                       uint64_t srcOffset, uint64_t dstOffset, uint64_t size) override;
    void cmdCopyBufferToTexture(RHICommandBufferHandle cmd, RHIBufferHandle src, RHITextureHandle dst,
                                uint64_t bufferOffset, uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;
    void cmdCopyTextureToBuffer(RHICommandBufferHandle cmd, RHITextureHandle src, RHIBufferHandle dst,
                                uint32_t mipLevel, uint32_t arrayLayer, uint64_t bufferOffset) override;

    void cmdPushConstants(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline,
                          RHIShaderStage stages, uint32_t offset, uint32_t size, const void* data) override;

    void cmdBeginDebugLabel(RHICommandBufferHandle cmd, const char* label, float color[4] = nullptr) override;
    void cmdEndDebugLabel(RHICommandBufferHandle cmd) override;
    void cmdInsertDebugLabel(RHICommandBufferHandle cmd, const char* label, float color[4] = nullptr) override;

    // ========================================================================
    // Queue Submission
    // ========================================================================

    void queueSubmit(RHIQueueHandle queue, const SubmitInfo& submitInfo) override;
    void queueWaitIdle(RHIQueueHandle queue) override;

    // ========================================================================
    // SwapChain Present
    // ========================================================================

    AcquireResult acquireNextImage(RHISwapChainHandle swapChain, RHISemaphoreHandle semaphore,
                                   RHIFenceHandle fence, uint64_t timeout, uint32_t* imageIndex) override;
    bool queuePresent(RHIQueueHandle queue, const PresentInfo& presentInfo) override;

    // ========================================================================
    // Utility
    // ========================================================================

    void waitIdle() override;
    uint64_t getCurrentFrameIndex() const override { return m_frameIndex; }
    uint64_t getCompletedFrameIndex() const override { return m_completedFrameIndex; }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    VkInstance getInstance() const { return m_instance; }
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VmaAllocator getAllocator() const { return m_allocator; }

private:
    // ========================================================================
    // Initialization Helpers
    // ========================================================================

    bool createInstance(const RHIConfig& config);
    bool setupDebugMessenger();
    bool selectPhysicalDevice(uint32_t preferredGpuIndex);
    bool createLogicalDevice();
    void retrieveQueues();
    bool createVmaAllocator();
    void createDescriptorPool();
    void fillGpuInfo();

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats, RHIFormat preferredFormat);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availableModes, RHIPresentMode preferredMode);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);

    void createSwapChainInternal(VulkanSwapChain* swapchain, uint32_t width, uint32_t height);
    void destroySwapChainInternal(VulkanSwapChain* swapchain);

    bool isDepthFormat(RHIFormat format) const;
    bool isStencilFormat(RHIFormat format) const;

private:
    // ========================================================================
    // Core Vulkan Objects
    // ========================================================================

    VkInstance                      m_instance          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT        m_debugMessenger    = VK_NULL_HANDLE;
    VkPhysicalDevice                m_physicalDevice    = VK_NULL_HANDLE;
    VkDevice                        m_device            = VK_NULL_HANDLE;
    VmaAllocator                    m_allocator         = VK_NULL_HANDLE;

    // ========================================================================
    // Queues
    // ========================================================================

    QueueFamilyIndices              m_queueFamilies;
    std::shared_ptr<VulkanQueue>    m_graphicsQueue;
    std::shared_ptr<VulkanQueue>    m_computeQueue;
    std::shared_ptr<VulkanQueue>    m_transferQueue;

    // ========================================================================
    // Descriptor Pool
    // ========================================================================

    VkDescriptorPool                m_descriptorPool    = VK_NULL_HANDLE;
    std::mutex                      m_descriptorPoolMutex;

    // ========================================================================
    // GPU Information
    // ========================================================================

    RHIGpuInfo                      m_gpuInfo;
    VkPhysicalDeviceProperties      m_deviceProperties;
    VkPhysicalDeviceFeatures        m_deviceFeatures;

    // Vulkan 1.2+ features
    VkPhysicalDeviceVulkan11Features m_vulkan11Features = {};
    VkPhysicalDeviceVulkan12Features m_vulkan12Features = {};
    VkPhysicalDeviceVulkan13Features m_vulkan13Features = {};

    // ========================================================================
    // Frame Tracking
    // ========================================================================

    std::atomic<uint64_t>           m_frameIndex{0};
    std::atomic<uint64_t>           m_completedFrameIndex{0};

    // ========================================================================
    // Configuration
    // ========================================================================

    bool                            m_validationEnabled = false;
    bool                            m_debugMarkersEnabled = false;

    // Required extensions
    std::vector<const char*>        m_instanceExtensions;
    std::vector<const char*>        m_deviceExtensions;
    std::vector<const char*>        m_validationLayers;
};

} // namespace vesper
