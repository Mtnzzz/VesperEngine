#pragma once

#include "runtime/function/render/rhi/rhi.h"
#include "runtime/core/log/log_system.h"

// Vulkan headers
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// VMA header
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>

// GLFW for surface creation
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace vesper {

// ============================================================================
// Vulkan Function Loader
// ============================================================================

struct VulkanFunctions
{
    // Instance functions
    PFN_vkGetInstanceProcAddr                       vkGetInstanceProcAddr = nullptr;
    PFN_vkCreateInstance                            vkCreateInstance = nullptr;
    PFN_vkEnumerateInstanceVersion                  vkEnumerateInstanceVersion = nullptr;
    PFN_vkEnumerateInstanceLayerProperties          vkEnumerateInstanceLayerProperties = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties      vkEnumerateInstanceExtensionProperties = nullptr;

    // Instance-level functions
    PFN_vkDestroyInstance                           vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices                  vkEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceProperties               vkGetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceProperties2              vkGetPhysicalDeviceProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceFeatures                 vkGetPhysicalDeviceFeatures = nullptr;
    PFN_vkGetPhysicalDeviceFeatures2                vkGetPhysicalDeviceFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties    vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties         vkGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceFormatProperties         vkGetPhysicalDeviceFormatProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR        vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR   vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR        vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR   vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties        vkEnumerateDeviceExtensionProperties = nullptr;
    PFN_vkCreateDevice                              vkCreateDevice = nullptr;
    PFN_vkDestroySurfaceKHR                         vkDestroySurfaceKHR = nullptr;

    // Debug
    PFN_vkCreateDebugUtilsMessengerEXT              vkCreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT             vkDestroyDebugUtilsMessengerEXT = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT                vkSetDebugUtilsObjectNameEXT = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT                vkCmdBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT                  vkCmdEndDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT               vkCmdInsertDebugUtilsLabelEXT = nullptr;

    // Device-level functions
    PFN_vkDestroyDevice                             vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue                            vkGetDeviceQueue = nullptr;
    PFN_vkDeviceWaitIdle                            vkDeviceWaitIdle = nullptr;
    PFN_vkQueueWaitIdle                             vkQueueWaitIdle = nullptr;
    PFN_vkQueueSubmit                               vkQueueSubmit = nullptr;

    // Swapchain
    PFN_vkCreateSwapchainKHR                        vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR                       vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR                     vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR                       vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR                           vkQueuePresentKHR = nullptr;

    // Command pool/buffer
    PFN_vkCreateCommandPool                         vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool                        vkDestroyCommandPool = nullptr;
    PFN_vkResetCommandPool                          vkResetCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers                    vkAllocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers                        vkFreeCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer                        vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer                          vkEndCommandBuffer = nullptr;
    PFN_vkResetCommandBuffer                        vkResetCommandBuffer = nullptr;

    // Synchronization
    PFN_vkCreateSemaphore                           vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore                          vkDestroySemaphore = nullptr;
    PFN_vkCreateFence                               vkCreateFence = nullptr;
    PFN_vkDestroyFence                              vkDestroyFence = nullptr;
    PFN_vkWaitForFences                             vkWaitForFences = nullptr;
    PFN_vkResetFences                               vkResetFences = nullptr;
    PFN_vkGetFenceStatus                            vkGetFenceStatus = nullptr;

    // Buffer
    PFN_vkCreateBuffer                              vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer                             vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements               vkGetBufferMemoryRequirements = nullptr;
    PFN_vkBindBufferMemory                          vkBindBufferMemory = nullptr;

    // Image
    PFN_vkCreateImage                               vkCreateImage = nullptr;
    PFN_vkDestroyImage                              vkDestroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements                vkGetImageMemoryRequirements = nullptr;
    PFN_vkBindImageMemory                           vkBindImageMemory = nullptr;
    PFN_vkCreateImageView                           vkCreateImageView = nullptr;
    PFN_vkDestroyImageView                          vkDestroyImageView = nullptr;

    // Sampler
    PFN_vkCreateSampler                             vkCreateSampler = nullptr;
    PFN_vkDestroySampler                            vkDestroySampler = nullptr;

    // Memory
    PFN_vkAllocateMemory                            vkAllocateMemory = nullptr;
    PFN_vkFreeMemory                                vkFreeMemory = nullptr;
    PFN_vkMapMemory                                 vkMapMemory = nullptr;
    PFN_vkUnmapMemory                               vkUnmapMemory = nullptr;
    PFN_vkFlushMappedMemoryRanges                   vkFlushMappedMemoryRanges = nullptr;
    PFN_vkInvalidateMappedMemoryRanges              vkInvalidateMappedMemoryRanges = nullptr;

    // Shader
    PFN_vkCreateShaderModule                        vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule                       vkDestroyShaderModule = nullptr;

    // Pipeline
    PFN_vkCreatePipelineLayout                      vkCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout                     vkDestroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines                   vkCreateGraphicsPipelines = nullptr;
    PFN_vkCreateComputePipelines                    vkCreateComputePipelines = nullptr;
    PFN_vkDestroyPipeline                           vkDestroyPipeline = nullptr;

    // Descriptor
    PFN_vkCreateDescriptorSetLayout                 vkCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout                vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool                      vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool                     vkDestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets                    vkAllocateDescriptorSets = nullptr;
    PFN_vkFreeDescriptorSets                        vkFreeDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets                      vkUpdateDescriptorSets = nullptr;

    // Commands - Drawing
    PFN_vkCmdBindPipeline                           vkCmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets                     vkCmdBindDescriptorSets = nullptr;
    PFN_vkCmdBindVertexBuffers                      vkCmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer                        vkCmdBindIndexBuffer = nullptr;
    PFN_vkCmdSetViewport                            vkCmdSetViewport = nullptr;
    PFN_vkCmdSetScissor                             vkCmdSetScissor = nullptr;
    PFN_vkCmdDraw                                   vkCmdDraw = nullptr;
    PFN_vkCmdDrawIndexed                            vkCmdDrawIndexed = nullptr;
    PFN_vkCmdDrawIndirect                           vkCmdDrawIndirect = nullptr;
    PFN_vkCmdDrawIndexedIndirect                    vkCmdDrawIndexedIndirect = nullptr;
    PFN_vkCmdDispatch                               vkCmdDispatch = nullptr;
    PFN_vkCmdDispatchIndirect                       vkCmdDispatchIndirect = nullptr;
    PFN_vkCmdPushConstants                          vkCmdPushConstants = nullptr;

    // Commands - Copy
    PFN_vkCmdCopyBuffer                             vkCmdCopyBuffer = nullptr;
    PFN_vkCmdCopyImage                              vkCmdCopyImage = nullptr;
    PFN_vkCmdCopyBufferToImage                      vkCmdCopyBufferToImage = nullptr;
    PFN_vkCmdCopyImageToBuffer                      vkCmdCopyImageToBuffer = nullptr;
    PFN_vkCmdBlitImage                              vkCmdBlitImage = nullptr;

    // Commands - Barriers
    PFN_vkCmdPipelineBarrier                        vkCmdPipelineBarrier = nullptr;
    PFN_vkCmdPipelineBarrier2                       vkCmdPipelineBarrier2 = nullptr;

    // Dynamic rendering (Vulkan 1.3)
    PFN_vkCmdBeginRendering                         vkCmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering                           vkCmdEndRendering = nullptr;

    bool loadGlobalFunctions();
    bool loadInstanceFunctions(VkInstance instance);
    bool loadDeviceFunctions(VkInstance instance, VkDevice device);
};

extern VulkanFunctions vk;

// ============================================================================
// Vulkan Error Handling
// ============================================================================

#define VK_CHECK(call) \
    do { \
        VkResult result = (call); \
        if (result != VK_SUCCESS) { \
            LOG_ERROR("Vulkan error: {} at {}:{}", static_cast<int>(result), __FILE__, __LINE__); \
        } \
    } while(0)

#define VK_CHECK_RETURN(call, retval) \
    do { \
        VkResult result = (call); \
        if (result != VK_SUCCESS) { \
            LOG_ERROR("Vulkan error: {} at {}:{}", static_cast<int>(result), __FILE__, __LINE__); \
            return retval; \
        } \
    } while(0)

// ============================================================================
// Vulkan Resource Types
// ============================================================================

struct VulkanBuffer : public RHIBuffer
{
    VkBuffer        buffer     = VK_NULL_HANDLE;
    VmaAllocation   allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo = {};
};

struct VulkanTexture : public RHITexture
{
    VkImage         image      = VK_NULL_HANDLE;
    VkImageView     imageView  = VK_NULL_HANDLE;
    VmaAllocation   allocation = VK_NULL_HANDLE;
    VkImageLayout   layout     = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    // For swapchain images (not owned)
    bool isSwapchainImage = false;
};

struct VulkanSampler : public RHISampler
{
    VkSampler sampler = VK_NULL_HANDLE;
};

struct VulkanShader : public RHIShader
{
    VkShaderModule module = VK_NULL_HANDLE;
    std::string entryPoint = "main";
};

struct VulkanDescriptorSetLayout : public RHIDescriptorSetLayout
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    std::vector<RHIDescriptorBinding> bindings;
};

struct VulkanPipeline : public RHIPipeline
{
    VkPipeline          pipeline        = VK_NULL_HANDLE;
    VkPipelineLayout    pipelineLayout  = VK_NULL_HANDLE;
    VkPipelineBindPoint bindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Keep track of descriptor set layouts for binding
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
};

struct VulkanDescriptorSet : public RHIDescriptorSet
{
    VkDescriptorSet     set     = VK_NULL_HANDLE;
    VkDescriptorPool    pool    = VK_NULL_HANDLE;  // Pool this set was allocated from
};

struct VulkanCommandPool : public RHICommandPool
{
    VkCommandPool pool = VK_NULL_HANDLE;
};

struct VulkanCommandBuffer : public RHICommandBuffer
{
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

struct VulkanFence : public RHIFence
{
    VkFence fence = VK_NULL_HANDLE;
};

struct VulkanSemaphore : public RHISemaphore
{
    VkSemaphore semaphore = VK_NULL_HANDLE;
};

struct VulkanQueue : public RHIQueue
{
    VkQueue         queue           = VK_NULL_HANDLE;
    std::mutex      submitMutex;    // Thread-safe queue submission
};

struct VulkanSwapChain : public RHISwapChain
{
    VkSwapchainKHR                  swapchain   = VK_NULL_HANDLE;
    VkSurfaceKHR                    surface     = VK_NULL_HANDLE;
    std::vector<VkImage>            images;
    std::vector<RHITextureHandle>   imageTextures;  // Wrapped textures
    VkSurfaceFormatKHR              surfaceFormat   = {};
    VkPresentModeKHR                presentMode     = VK_PRESENT_MODE_FIFO_KHR;
};

// ============================================================================
// Format Conversion Utilities
// ============================================================================

VkFormat toVkFormat(RHIFormat format);
RHIFormat fromVkFormat(VkFormat format);

VkBufferUsageFlags toVkBufferUsage(RHIBufferUsage usage);
VkImageUsageFlags toVkImageUsage(RHITextureUsage usage);
VkImageType toVkImageType(RHITextureDimension dim);
VkImageViewType toVkImageViewType(RHITextureDimension dim, uint32_t arrayLayers);
VkSampleCountFlagBits toVkSampleCount(RHISampleCount count);
VkShaderStageFlagBits toVkShaderStage(RHIShaderStage stage);
VkShaderStageFlags toVkShaderStageFlags(RHIShaderStage stages);
VkDescriptorType toVkDescriptorType(RHIDescriptorType type);
VkFilter toVkFilter(RHIFilter filter);
VkSamplerMipmapMode toVkMipmapMode(RHIFilter filter);
VkSamplerAddressMode toVkAddressMode(RHISamplerAddressMode mode);
VkBorderColor toVkBorderColor(RHIBorderColor color);
VkCompareOp toVkCompareOp(RHICompareOp op);
VkPrimitiveTopology toVkPrimitiveTopology(RHIPrimitiveTopology topology);
VkPolygonMode toVkPolygonMode(RHIPolygonMode mode);
VkCullModeFlags toVkCullMode(RHICullMode mode);
VkFrontFace toVkFrontFace(RHIFrontFace face);
VkBlendFactor toVkBlendFactor(RHIBlendFactor factor);
VkBlendOp toVkBlendOp(RHIBlendOp op);
VkAttachmentLoadOp toVkLoadOp(RHILoadOp op);
VkAttachmentStoreOp toVkStoreOp(RHIStoreOp op);
VkPresentModeKHR toVkPresentMode(RHIPresentMode mode);
VkVertexInputRate toVkVertexInputRate(RHIVertexInputRate rate);

VmaMemoryUsage toVmaMemoryUsage(RHIMemoryUsage usage);

// Resource state to Vulkan access/layout/stage
struct VkResourceStateInfo
{
    VkAccessFlags       accessMask;
    VkImageLayout       imageLayout;
    VkPipelineStageFlags stageMask;
};

VkResourceStateInfo toVkResourceState(RHIResourceState state, bool isDepth = false);

// ============================================================================
// Debug Utilities
// ============================================================================

void setVkObjectName(VkDevice device, uint64_t object, VkObjectType type, const char* name);

template<typename T>
void setVkObjectName(VkDevice device, T handle, VkObjectType type, const char* name)
{
    setVkObjectName(device, reinterpret_cast<uint64_t>(handle), type, name);
}

// ============================================================================
// Queue Family Indices
// ============================================================================

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> transfer;
    std::optional<uint32_t> present;

    bool isComplete() const {
        return graphics.has_value() && present.has_value();
    }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

} // namespace vesper
