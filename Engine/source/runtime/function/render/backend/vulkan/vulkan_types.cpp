// VMA implementation - must be defined in exactly one .cpp file
#define VMA_IMPLEMENTATION
#include "vulkan_types.h"

#ifdef VESPER_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace vesper {

// Global Vulkan function table
VulkanFunctions vk;

// ============================================================================
// Vulkan Function Loader Implementation
// ============================================================================

bool VulkanFunctions::loadGlobalFunctions()
{
#ifdef VESPER_PLATFORM_WINDOWS
    HMODULE vulkanLib = LoadLibraryA("vulkan-1.dll");
    if (!vulkanLib) {
        LOG_ERROR("Failed to load vulkan-1.dll");
        return false;
    }
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(vulkanLib, "vkGetInstanceProcAddr"));
#else
    // Linux/macOS: use dlopen
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr"));
#endif

    if (!vkGetInstanceProcAddr) {
        LOG_ERROR("Failed to load vkGetInstanceProcAddr");
        return false;
    }

    // Load global functions
    vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    vkEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties"));
    vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceExtensionProperties"));

    return vkCreateInstance != nullptr;
}

bool VulkanFunctions::loadInstanceFunctions(VkInstance instance)
{
    if (!instance) return false;

#define LOAD_INSTANCE_FUNC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name))

    LOAD_INSTANCE_FUNC(vkDestroyInstance);
    LOAD_INSTANCE_FUNC(vkEnumeratePhysicalDevices);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceProperties);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceProperties2);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceFeatures);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceFeatures2);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceFormatProperties);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR);
    LOAD_INSTANCE_FUNC(vkEnumerateDeviceExtensionProperties);
    LOAD_INSTANCE_FUNC(vkCreateDevice);
    LOAD_INSTANCE_FUNC(vkDestroySurfaceKHR);

    // Debug extension
    LOAD_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);
    LOAD_INSTANCE_FUNC(vkDestroyDebugUtilsMessengerEXT);

#undef LOAD_INSTANCE_FUNC

    return vkDestroyInstance != nullptr && vkEnumeratePhysicalDevices != nullptr;
}

bool VulkanFunctions::loadDeviceFunctions(VkInstance instance, VkDevice device)
{
    LOG_DEBUG("loadDeviceFunctions called");
    if (!instance || !device) {
        LOG_ERROR("loadDeviceFunctions: null instance or device");
        return false;
    }

    LOG_DEBUG("Loading vkGetDeviceProcAddr...");
    auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    if (!getDeviceProcAddr) {
        LOG_ERROR("Failed to load vkGetDeviceProcAddr");
        return false;
    }
    LOG_DEBUG("vkGetDeviceProcAddr loaded successfully");

#define LOAD_DEVICE_FUNC(name) \
    name = reinterpret_cast<PFN_##name>(getDeviceProcAddr(device, #name))

    LOAD_DEVICE_FUNC(vkDestroyDevice);
    LOAD_DEVICE_FUNC(vkGetDeviceQueue);
    LOAD_DEVICE_FUNC(vkDeviceWaitIdle);
    LOAD_DEVICE_FUNC(vkQueueWaitIdle);
    LOAD_DEVICE_FUNC(vkQueueSubmit);

    // Swapchain
    LOAD_DEVICE_FUNC(vkCreateSwapchainKHR);
    LOAD_DEVICE_FUNC(vkDestroySwapchainKHR);
    LOAD_DEVICE_FUNC(vkGetSwapchainImagesKHR);
    LOAD_DEVICE_FUNC(vkAcquireNextImageKHR);
    LOAD_DEVICE_FUNC(vkQueuePresentKHR);

    // Command pool/buffer
    LOAD_DEVICE_FUNC(vkCreateCommandPool);
    LOAD_DEVICE_FUNC(vkDestroyCommandPool);
    LOAD_DEVICE_FUNC(vkResetCommandPool);
    LOAD_DEVICE_FUNC(vkAllocateCommandBuffers);
    LOAD_DEVICE_FUNC(vkFreeCommandBuffers);
    LOAD_DEVICE_FUNC(vkBeginCommandBuffer);
    LOAD_DEVICE_FUNC(vkEndCommandBuffer);
    LOAD_DEVICE_FUNC(vkResetCommandBuffer);

    // Synchronization
    LOAD_DEVICE_FUNC(vkCreateSemaphore);
    LOAD_DEVICE_FUNC(vkDestroySemaphore);
    LOAD_DEVICE_FUNC(vkCreateFence);
    LOAD_DEVICE_FUNC(vkDestroyFence);
    LOAD_DEVICE_FUNC(vkWaitForFences);
    LOAD_DEVICE_FUNC(vkResetFences);
    LOAD_DEVICE_FUNC(vkGetFenceStatus);

    // Buffer
    LOAD_DEVICE_FUNC(vkCreateBuffer);
    LOAD_DEVICE_FUNC(vkDestroyBuffer);
    LOAD_DEVICE_FUNC(vkGetBufferMemoryRequirements);
    LOAD_DEVICE_FUNC(vkBindBufferMemory);

    // Image
    LOAD_DEVICE_FUNC(vkCreateImage);
    LOAD_DEVICE_FUNC(vkDestroyImage);
    LOAD_DEVICE_FUNC(vkGetImageMemoryRequirements);
    LOAD_DEVICE_FUNC(vkBindImageMemory);
    LOAD_DEVICE_FUNC(vkCreateImageView);
    LOAD_DEVICE_FUNC(vkDestroyImageView);

    // Sampler
    LOAD_DEVICE_FUNC(vkCreateSampler);
    LOAD_DEVICE_FUNC(vkDestroySampler);

    // Memory
    LOAD_DEVICE_FUNC(vkAllocateMemory);
    LOAD_DEVICE_FUNC(vkFreeMemory);
    LOAD_DEVICE_FUNC(vkMapMemory);
    LOAD_DEVICE_FUNC(vkUnmapMemory);
    LOAD_DEVICE_FUNC(vkFlushMappedMemoryRanges);
    LOAD_DEVICE_FUNC(vkInvalidateMappedMemoryRanges);

    // Shader
    LOAD_DEVICE_FUNC(vkCreateShaderModule);
    LOAD_DEVICE_FUNC(vkDestroyShaderModule);

    // Pipeline
    LOAD_DEVICE_FUNC(vkCreatePipelineLayout);
    LOAD_DEVICE_FUNC(vkDestroyPipelineLayout);
    LOAD_DEVICE_FUNC(vkCreateGraphicsPipelines);
    LOAD_DEVICE_FUNC(vkCreateComputePipelines);
    LOAD_DEVICE_FUNC(vkDestroyPipeline);

    // Descriptor
    LOAD_DEVICE_FUNC(vkCreateDescriptorSetLayout);
    LOAD_DEVICE_FUNC(vkDestroyDescriptorSetLayout);
    LOAD_DEVICE_FUNC(vkCreateDescriptorPool);
    LOAD_DEVICE_FUNC(vkDestroyDescriptorPool);
    LOAD_DEVICE_FUNC(vkAllocateDescriptorSets);
    LOAD_DEVICE_FUNC(vkFreeDescriptorSets);
    LOAD_DEVICE_FUNC(vkUpdateDescriptorSets);

    // Commands
    LOAD_DEVICE_FUNC(vkCmdBindPipeline);
    LOAD_DEVICE_FUNC(vkCmdBindDescriptorSets);
    LOAD_DEVICE_FUNC(vkCmdBindVertexBuffers);
    LOAD_DEVICE_FUNC(vkCmdBindIndexBuffer);
    LOAD_DEVICE_FUNC(vkCmdSetViewport);
    LOAD_DEVICE_FUNC(vkCmdSetScissor);
    LOAD_DEVICE_FUNC(vkCmdDraw);
    LOAD_DEVICE_FUNC(vkCmdDrawIndexed);
    LOAD_DEVICE_FUNC(vkCmdDrawIndirect);
    LOAD_DEVICE_FUNC(vkCmdDrawIndexedIndirect);
    LOAD_DEVICE_FUNC(vkCmdDispatch);
    LOAD_DEVICE_FUNC(vkCmdDispatchIndirect);
    LOAD_DEVICE_FUNC(vkCmdPushConstants);
    LOAD_DEVICE_FUNC(vkCmdCopyBuffer);
    LOAD_DEVICE_FUNC(vkCmdCopyImage);
    LOAD_DEVICE_FUNC(vkCmdCopyBufferToImage);
    LOAD_DEVICE_FUNC(vkCmdCopyImageToBuffer);
    LOAD_DEVICE_FUNC(vkCmdBlitImage);
    LOAD_DEVICE_FUNC(vkCmdPipelineBarrier);
    LOAD_DEVICE_FUNC(vkCmdPipelineBarrier2);

    // Dynamic rendering (Vulkan 1.3)
    LOAD_DEVICE_FUNC(vkCmdBeginRendering);
    LOAD_DEVICE_FUNC(vkCmdEndRendering);

    // Debug
    LOAD_DEVICE_FUNC(vkSetDebugUtilsObjectNameEXT);
    LOAD_DEVICE_FUNC(vkCmdBeginDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNC(vkCmdEndDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNC(vkCmdInsertDebugUtilsLabelEXT);

#undef LOAD_DEVICE_FUNC

    return vkDestroyDevice != nullptr;
}

// ============================================================================
// Format Conversion
// ============================================================================

VkFormat toVkFormat(RHIFormat format)
{
    switch (format) {
        case RHIFormat::Undefined:          return VK_FORMAT_UNDEFINED;
        case RHIFormat::R8_UNORM:           return VK_FORMAT_R8_UNORM;
        case RHIFormat::R8_SNORM:           return VK_FORMAT_R8_SNORM;
        case RHIFormat::R8_UINT:            return VK_FORMAT_R8_UINT;
        case RHIFormat::R8_SINT:            return VK_FORMAT_R8_SINT;
        case RHIFormat::RG8_UNORM:          return VK_FORMAT_R8G8_UNORM;
        case RHIFormat::RG8_SNORM:          return VK_FORMAT_R8G8_SNORM;
        case RHIFormat::RG8_UINT:           return VK_FORMAT_R8G8_UINT;
        case RHIFormat::RG8_SINT:           return VK_FORMAT_R8G8_SINT;
        case RHIFormat::RGBA8_UNORM:        return VK_FORMAT_R8G8B8A8_UNORM;
        case RHIFormat::RGBA8_SNORM:        return VK_FORMAT_R8G8B8A8_SNORM;
        case RHIFormat::RGBA8_UINT:         return VK_FORMAT_R8G8B8A8_UINT;
        case RHIFormat::RGBA8_SINT:         return VK_FORMAT_R8G8B8A8_SINT;
        case RHIFormat::RGBA8_SRGB:         return VK_FORMAT_R8G8B8A8_SRGB;
        case RHIFormat::BGRA8_UNORM:        return VK_FORMAT_B8G8R8A8_UNORM;
        case RHIFormat::BGRA8_SRGB:         return VK_FORMAT_B8G8R8A8_SRGB;
        case RHIFormat::R16_UNORM:          return VK_FORMAT_R16_UNORM;
        case RHIFormat::R16_SNORM:          return VK_FORMAT_R16_SNORM;
        case RHIFormat::R16_UINT:           return VK_FORMAT_R16_UINT;
        case RHIFormat::R16_SINT:           return VK_FORMAT_R16_SINT;
        case RHIFormat::R16_FLOAT:          return VK_FORMAT_R16_SFLOAT;
        case RHIFormat::RG16_UNORM:         return VK_FORMAT_R16G16_UNORM;
        case RHIFormat::RG16_SNORM:         return VK_FORMAT_R16G16_SNORM;
        case RHIFormat::RG16_UINT:          return VK_FORMAT_R16G16_UINT;
        case RHIFormat::RG16_SINT:          return VK_FORMAT_R16G16_SINT;
        case RHIFormat::RG16_FLOAT:         return VK_FORMAT_R16G16_SFLOAT;
        case RHIFormat::RGBA16_UNORM:       return VK_FORMAT_R16G16B16A16_UNORM;
        case RHIFormat::RGBA16_SNORM:       return VK_FORMAT_R16G16B16A16_SNORM;
        case RHIFormat::RGBA16_UINT:        return VK_FORMAT_R16G16B16A16_UINT;
        case RHIFormat::RGBA16_SINT:        return VK_FORMAT_R16G16B16A16_SINT;
        case RHIFormat::RGBA16_FLOAT:       return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RHIFormat::R32_UINT:           return VK_FORMAT_R32_UINT;
        case RHIFormat::R32_SINT:           return VK_FORMAT_R32_SINT;
        case RHIFormat::R32_FLOAT:          return VK_FORMAT_R32_SFLOAT;
        case RHIFormat::RG32_UINT:          return VK_FORMAT_R32G32_UINT;
        case RHIFormat::RG32_SINT:          return VK_FORMAT_R32G32_SINT;
        case RHIFormat::RG32_FLOAT:         return VK_FORMAT_R32G32_SFLOAT;
        case RHIFormat::RGB32_UINT:         return VK_FORMAT_R32G32B32_UINT;
        case RHIFormat::RGB32_SINT:         return VK_FORMAT_R32G32B32_SINT;
        case RHIFormat::RGB32_FLOAT:        return VK_FORMAT_R32G32B32_SFLOAT;
        case RHIFormat::RGBA32_UINT:        return VK_FORMAT_R32G32B32A32_UINT;
        case RHIFormat::RGBA32_SINT:        return VK_FORMAT_R32G32B32A32_SINT;
        case RHIFormat::RGBA32_FLOAT:       return VK_FORMAT_R32G32B32A32_SFLOAT;
        case RHIFormat::D16_UNORM:          return VK_FORMAT_D16_UNORM;
        case RHIFormat::D24_UNORM_S8_UINT:  return VK_FORMAT_D24_UNORM_S8_UINT;
        case RHIFormat::D32_FLOAT:          return VK_FORMAT_D32_SFLOAT;
        case RHIFormat::D32_FLOAT_S8_UINT:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case RHIFormat::BC1_RGB_UNORM:      return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case RHIFormat::BC1_RGB_SRGB:       return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case RHIFormat::BC1_RGBA_UNORM:     return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case RHIFormat::BC1_RGBA_SRGB:      return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case RHIFormat::BC2_UNORM:          return VK_FORMAT_BC2_UNORM_BLOCK;
        case RHIFormat::BC2_SRGB:           return VK_FORMAT_BC2_SRGB_BLOCK;
        case RHIFormat::BC3_UNORM:          return VK_FORMAT_BC3_UNORM_BLOCK;
        case RHIFormat::BC3_SRGB:           return VK_FORMAT_BC3_SRGB_BLOCK;
        case RHIFormat::BC4_UNORM:          return VK_FORMAT_BC4_UNORM_BLOCK;
        case RHIFormat::BC4_SNORM:          return VK_FORMAT_BC4_SNORM_BLOCK;
        case RHIFormat::BC5_UNORM:          return VK_FORMAT_BC5_UNORM_BLOCK;
        case RHIFormat::BC5_SNORM:          return VK_FORMAT_BC5_SNORM_BLOCK;
        case RHIFormat::BC6H_UFLOAT:        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case RHIFormat::BC6H_SFLOAT:        return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case RHIFormat::BC7_UNORM:          return VK_FORMAT_BC7_UNORM_BLOCK;
        case RHIFormat::BC7_SRGB:           return VK_FORMAT_BC7_SRGB_BLOCK;
        default:                            return VK_FORMAT_UNDEFINED;
    }
}

RHIFormat fromVkFormat(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_UNDEFINED:               return RHIFormat::Undefined;
        case VK_FORMAT_R8_UNORM:                return RHIFormat::R8_UNORM;
        case VK_FORMAT_R8G8_UNORM:              return RHIFormat::RG8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM:          return RHIFormat::RGBA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:           return RHIFormat::RGBA8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:          return RHIFormat::BGRA8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:           return RHIFormat::BGRA8_SRGB;
        case VK_FORMAT_R16_SFLOAT:              return RHIFormat::R16_FLOAT;
        case VK_FORMAT_R16G16_SFLOAT:           return RHIFormat::RG16_FLOAT;
        case VK_FORMAT_R16G16B16A16_SFLOAT:     return RHIFormat::RGBA16_FLOAT;
        case VK_FORMAT_R32_SFLOAT:              return RHIFormat::R32_FLOAT;
        case VK_FORMAT_R32G32_SFLOAT:           return RHIFormat::RG32_FLOAT;
        case VK_FORMAT_R32G32B32_SFLOAT:        return RHIFormat::RGB32_FLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:     return RHIFormat::RGBA32_FLOAT;
        case VK_FORMAT_D16_UNORM:               return RHIFormat::D16_UNORM;
        case VK_FORMAT_D24_UNORM_S8_UINT:       return RHIFormat::D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT:              return RHIFormat::D32_FLOAT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:      return RHIFormat::D32_FLOAT_S8_UINT;
        default:                                return RHIFormat::Undefined;
    }
}

VkBufferUsageFlags toVkBufferUsage(RHIBufferUsage usage)
{
    VkBufferUsageFlags flags = 0;
    if (hasFlag(usage, RHIBufferUsage::Vertex))       flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (hasFlag(usage, RHIBufferUsage::Index))        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (hasFlag(usage, RHIBufferUsage::Uniform))      flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (hasFlag(usage, RHIBufferUsage::Storage))      flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (hasFlag(usage, RHIBufferUsage::Indirect))     flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (hasFlag(usage, RHIBufferUsage::TransferSrc))  flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (hasFlag(usage, RHIBufferUsage::TransferDst))  flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkImageUsageFlags toVkImageUsage(RHITextureUsage usage)
{
    VkImageUsageFlags flags = 0;
    if (hasFlag(usage, RHITextureUsage::Sampled))         flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (hasFlag(usage, RHITextureUsage::Storage))         flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (hasFlag(usage, RHITextureUsage::ColorAttachment)) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (hasFlag(usage, RHITextureUsage::DepthStencil))    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (hasFlag(usage, RHITextureUsage::TransferSrc))     flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (hasFlag(usage, RHITextureUsage::TransferDst))     flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (hasFlag(usage, RHITextureUsage::InputAttachment)) flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return flags;
}

VkImageType toVkImageType(RHITextureDimension dim)
{
    switch (dim) {
        case RHITextureDimension::Tex1D:
        case RHITextureDimension::Tex1DArray:
            return VK_IMAGE_TYPE_1D;
        case RHITextureDimension::Tex2D:
        case RHITextureDimension::Tex2DArray:
        case RHITextureDimension::TexCube:
        case RHITextureDimension::TexCubeArray:
            return VK_IMAGE_TYPE_2D;
        case RHITextureDimension::Tex3D:
            return VK_IMAGE_TYPE_3D;
        default:
            return VK_IMAGE_TYPE_2D;
    }
}

VkImageViewType toVkImageViewType(RHITextureDimension dim, uint32_t arrayLayers)
{
    switch (dim) {
        case RHITextureDimension::Tex1D:
            return VK_IMAGE_VIEW_TYPE_1D;
        case RHITextureDimension::Tex1DArray:
            return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case RHITextureDimension::Tex2D:
            return arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case RHITextureDimension::Tex2DArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case RHITextureDimension::Tex3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case RHITextureDimension::TexCube:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        case RHITextureDimension::TexCubeArray:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        default:
            return VK_IMAGE_VIEW_TYPE_2D;
    }
}

VkSampleCountFlagBits toVkSampleCount(RHISampleCount count)
{
    return static_cast<VkSampleCountFlagBits>(static_cast<uint32_t>(count));
}

VkShaderStageFlagBits toVkShaderStage(RHIShaderStage stage)
{
    switch (stage) {
        case RHIShaderStage::Vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
        case RHIShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case RHIShaderStage::Compute:  return VK_SHADER_STAGE_COMPUTE_BIT;
        case RHIShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case RHIShaderStage::Hull:     return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case RHIShaderStage::Domain:   return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default:                       return VK_SHADER_STAGE_ALL;
    }
}

VkShaderStageFlags toVkShaderStageFlags(RHIShaderStage stages)
{
    VkShaderStageFlags flags = 0;
    if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(RHIShaderStage::Vertex)) != 0)
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(RHIShaderStage::Fragment)) != 0)
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(RHIShaderStage::Compute)) != 0)
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(RHIShaderStage::Geometry)) != 0)
        flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(RHIShaderStage::Hull)) != 0)
        flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if ((static_cast<uint32_t>(stages) & static_cast<uint32_t>(RHIShaderStage::Domain)) != 0)
        flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return flags;
}

VkDescriptorType toVkDescriptorType(RHIDescriptorType type)
{
    switch (type) {
        case RHIDescriptorType::Sampler:              return VK_DESCRIPTOR_TYPE_SAMPLER;
        case RHIDescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case RHIDescriptorType::SampledImage:         return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case RHIDescriptorType::StorageImage:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case RHIDescriptorType::UniformTexelBuffer:   return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case RHIDescriptorType::StorageTexelBuffer:   return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case RHIDescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case RHIDescriptorType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RHIDescriptorType::UniformBufferDynamic: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case RHIDescriptorType::StorageBufferDynamic: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case RHIDescriptorType::InputAttachment:      return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        default:                                       return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

VkFilter toVkFilter(RHIFilter filter)
{
    return filter == RHIFilter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerMipmapMode toVkMipmapMode(RHIFilter filter)
{
    return filter == RHIFilter::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

VkSamplerAddressMode toVkAddressMode(RHISamplerAddressMode mode)
{
    switch (mode) {
        case RHISamplerAddressMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case RHISamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RHISamplerAddressMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case RHISamplerAddressMode::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:                                     return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkBorderColor toVkBorderColor(RHIBorderColor color)
{
    switch (color) {
        case RHIBorderColor::TransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case RHIBorderColor::OpaqueBlack:      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case RHIBorderColor::OpaqueWhite:      return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        default:                                return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }
}

VkCompareOp toVkCompareOp(RHICompareOp op)
{
    switch (op) {
        case RHICompareOp::Never:          return VK_COMPARE_OP_NEVER;
        case RHICompareOp::Less:           return VK_COMPARE_OP_LESS;
        case RHICompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        case RHICompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RHICompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        case RHICompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        case RHICompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case RHICompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
        default:                            return VK_COMPARE_OP_ALWAYS;
    }
}

VkPrimitiveTopology toVkPrimitiveTopology(RHIPrimitiveTopology topology)
{
    switch (topology) {
        case RHIPrimitiveTopology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case RHIPrimitiveTopology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case RHIPrimitiveTopology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case RHIPrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case RHIPrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case RHIPrimitiveTopology::TriangleFan:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case RHIPrimitiveTopology::PatchList:     return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        default:                                   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkPolygonMode toVkPolygonMode(RHIPolygonMode mode)
{
    switch (mode) {
        case RHIPolygonMode::Fill:  return VK_POLYGON_MODE_FILL;
        case RHIPolygonMode::Line:  return VK_POLYGON_MODE_LINE;
        case RHIPolygonMode::Point: return VK_POLYGON_MODE_POINT;
        default:                     return VK_POLYGON_MODE_FILL;
    }
}

VkCullModeFlags toVkCullMode(RHICullMode mode)
{
    switch (mode) {
        case RHICullMode::None:  return VK_CULL_MODE_NONE;
        case RHICullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case RHICullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        default:                  return VK_CULL_MODE_BACK_BIT;
    }
}

VkFrontFace toVkFrontFace(RHIFrontFace face)
{
    return face == RHIFrontFace::CounterClockwise ?
        VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

VkBlendFactor toVkBlendFactor(RHIBlendFactor factor)
{
    switch (factor) {
        case RHIBlendFactor::Zero:                  return VK_BLEND_FACTOR_ZERO;
        case RHIBlendFactor::One:                   return VK_BLEND_FACTOR_ONE;
        case RHIBlendFactor::SrcColor:              return VK_BLEND_FACTOR_SRC_COLOR;
        case RHIBlendFactor::OneMinusSrcColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case RHIBlendFactor::DstColor:              return VK_BLEND_FACTOR_DST_COLOR;
        case RHIBlendFactor::OneMinusDstColor:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case RHIBlendFactor::SrcAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
        case RHIBlendFactor::OneMinusSrcAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case RHIBlendFactor::DstAlpha:              return VK_BLEND_FACTOR_DST_ALPHA;
        case RHIBlendFactor::OneMinusDstAlpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case RHIBlendFactor::ConstantColor:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case RHIBlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case RHIBlendFactor::SrcAlphaSaturate:      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default:                                     return VK_BLEND_FACTOR_ONE;
    }
}

VkBlendOp toVkBlendOp(RHIBlendOp op)
{
    switch (op) {
        case RHIBlendOp::Add:             return VK_BLEND_OP_ADD;
        case RHIBlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case RHIBlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case RHIBlendOp::Min:             return VK_BLEND_OP_MIN;
        case RHIBlendOp::Max:             return VK_BLEND_OP_MAX;
        default:                           return VK_BLEND_OP_ADD;
    }
}

VkAttachmentLoadOp toVkLoadOp(RHILoadOp op)
{
    switch (op) {
        case RHILoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case RHILoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case RHILoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:                   return VK_ATTACHMENT_LOAD_OP_CLEAR;
    }
}

VkAttachmentStoreOp toVkStoreOp(RHIStoreOp op)
{
    switch (op) {
        case RHIStoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
        case RHIStoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:                    return VK_ATTACHMENT_STORE_OP_STORE;
    }
}

VkPresentModeKHR toVkPresentMode(RHIPresentMode mode)
{
    switch (mode) {
        case RHIPresentMode::Immediate:   return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case RHIPresentMode::Mailbox:     return VK_PRESENT_MODE_MAILBOX_KHR;
        case RHIPresentMode::Fifo:        return VK_PRESENT_MODE_FIFO_KHR;
        case RHIPresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        default:                           return VK_PRESENT_MODE_FIFO_KHR;
    }
}

VkVertexInputRate toVkVertexInputRate(RHIVertexInputRate rate)
{
    return rate == RHIVertexInputRate::Vertex ?
        VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
}

VmaMemoryUsage toVmaMemoryUsage(RHIMemoryUsage usage)
{
    switch (usage) {
        case RHIMemoryUsage::GpuOnly:  return VMA_MEMORY_USAGE_GPU_ONLY;
        case RHIMemoryUsage::CpuOnly:  return VMA_MEMORY_USAGE_CPU_ONLY;
        case RHIMemoryUsage::CpuToGpu: return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case RHIMemoryUsage::GpuToCpu: return VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:                        return VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

VkResourceStateInfo toVkResourceState(RHIResourceState state, bool isDepth)
{
    VkResourceStateInfo info = {};

    switch (state) {
        case RHIResourceState::Undefined:
            info.accessMask = 0;
            info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;

        case RHIResourceState::Common:
            info.accessMask = 0;
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            info.stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            break;

        case RHIResourceState::VertexBuffer:
            info.accessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.stageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;

        case RHIResourceState::IndexBuffer:
            info.accessMask = VK_ACCESS_INDEX_READ_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.stageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;

        case RHIResourceState::UniformBuffer:
            info.accessMask = VK_ACCESS_UNIFORM_READ_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.stageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case RHIResourceState::ShaderResource:
            info.accessMask = VK_ACCESS_SHADER_READ_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.stageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case RHIResourceState::UnorderedAccess:
            info.accessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            info.stageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;

        case RHIResourceState::RenderTarget:
            info.accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            info.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;

        case RHIResourceState::DepthWrite:
            info.accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            info.stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;

        case RHIResourceState::DepthRead:
            info.accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            info.stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;

        case RHIResourceState::CopySrc:
            info.accessMask = VK_ACCESS_TRANSFER_READ_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            info.stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case RHIResourceState::CopyDst:
            info.accessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            info.stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case RHIResourceState::Present:
            info.accessMask = 0;
            info.imageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            info.stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;

        case RHIResourceState::IndirectArgument:
            info.accessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.stageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            break;

        default:
            info.accessMask = 0;
            info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            info.stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
    }

    return info;
}

// ============================================================================
// Debug Utilities
// ============================================================================

void setVkObjectName(VkDevice device, uint64_t object, VkObjectType type, const char* name)
{
    if (!name || !vk.vkSetDebugUtilsObjectNameEXT) return;

    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = type;
    nameInfo.objectHandle = object;
    nameInfo.pObjectName = name;

    vk.vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}

// ============================================================================
// Queue Family Utilities
// ============================================================================

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vk.vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vk.vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        const auto& queueFamily = queueFamilies[i];

        // Graphics queue
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }

        // Compute queue (prefer dedicated)
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.compute = i;
        } else if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && !indices.compute.has_value()) {
            indices.compute = i;
        }

        // Transfer queue (prefer dedicated)
        if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.transfer = i;
        } else if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) && !indices.transfer.has_value()) {
            indices.transfer = i;
        }

        // Present queue
        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = false;
            vk.vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.present = i;
            }
        }
    }

    // Fallback: use graphics queue for compute/transfer if no dedicated queues
    if (!indices.compute.has_value() && indices.graphics.has_value()) {
        indices.compute = indices.graphics;
    }
    if (!indices.transfer.has_value() && indices.graphics.has_value()) {
        indices.transfer = indices.graphics;
    }

    return indices;
}

} // namespace vesper
