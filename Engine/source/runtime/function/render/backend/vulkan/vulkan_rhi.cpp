#include "vulkan_rhi.h"

#include <algorithm>
#include <cstring>
#include <set>

namespace vesper {

// ============================================================================
// Debug Callback
// ============================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG_ERROR("Vulkan Validation: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN("Vulkan Validation: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LOG_DEBUG("Vulkan Validation: {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

VulkanRHI::VulkanRHI() = default;

VulkanRHI::~VulkanRHI()
{
    if (m_device != VK_NULL_HANDLE) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool VulkanRHI::initialize(const RHIConfig& config)
{
    LOG_INFO("Initializing Vulkan RHI...");

    // Load global Vulkan functions
    if (!vk.loadGlobalFunctions()) {
        LOG_ERROR("Failed to load Vulkan global functions");
        return false;
    }

    m_validationEnabled = config.enableValidation;
    m_debugMarkersEnabled = config.enableGpuDebugMarkers;

    // Create instance
    if (!createInstance(config)) {
        LOG_ERROR("Failed to create Vulkan instance");
        return false;
    }

    // Load instance functions
    if (!vk.loadInstanceFunctions(m_instance)) {
        LOG_ERROR("Failed to load Vulkan instance functions");
        return false;
    }

    // Setup debug messenger
    if (m_validationEnabled) {
        if (!setupDebugMessenger()) {
            LOG_WARN("Failed to setup debug messenger");
        }
    }

    // Select physical device
    if (!selectPhysicalDevice(config.preferredGpuIndex)) {
        LOG_ERROR("Failed to select physical device");
        return false;
    }

    // Create logical device
    if (!createLogicalDevice()) {
        LOG_ERROR("Failed to create logical device");
        return false;
    }

    // Load device functions
    LOG_INFO("Loading Vulkan device functions...");
    if (!vk.loadDeviceFunctions(m_instance, m_device)) {
        LOG_ERROR("Failed to load Vulkan device functions");
        return false;
    }
    LOG_INFO("Vulkan device functions loaded successfully");

    // Retrieve queues (must be after loadDeviceFunctions)
    retrieveQueues();

    // Create VMA allocator
    LOG_INFO("Creating VMA allocator...");
    if (!createVmaAllocator()) {
        LOG_ERROR("Failed to create VMA allocator");
        return false;
    }
    LOG_INFO("VMA allocator created successfully");

    // Create descriptor pool
    createDescriptorPool();

    // Fill GPU info
    fillGpuInfo();

    LOG_INFO("Vulkan RHI initialized successfully");
    LOG_INFO("  GPU: {}", m_gpuInfo.deviceName);
    LOG_INFO("  VRAM: {} MB", m_gpuInfo.dedicatedMemory / (1024 * 1024));
    LOG_INFO("  Dynamic Rendering: {}", m_gpuInfo.dynamicRendering ? "Yes" : "No");

    return true;
}

void VulkanRHI::shutdown()
{
    LOG_INFO("Shutting down Vulkan RHI...");

    waitIdle();

    // Destroy descriptor pool
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vk.vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    // Destroy VMA allocator
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }

    // Clear queues
    m_graphicsQueue.reset();
    m_computeQueue.reset();
    m_transferQueue.reset();

    // Destroy device
    if (m_device != VK_NULL_HANDLE) {
        vk.vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // Destroy debug messenger
    if (m_debugMessenger != VK_NULL_HANDLE && vk.vkDestroyDebugUtilsMessengerEXT) {
        vk.vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    // Destroy instance
    if (m_instance != VK_NULL_HANDLE) {
        vk.vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    LOG_INFO("Vulkan RHI shutdown complete");
}

// ============================================================================
// Initialization Helpers
// ============================================================================

bool VulkanRHI::createInstance(const RHIConfig& config)
{
    // Check Vulkan version
    uint32_t apiVersion = 0;
    if (vk.vkEnumerateInstanceVersion) {
        vk.vkEnumerateInstanceVersion(&apiVersion);
    } else {
        apiVersion = VK_API_VERSION_1_0;
    }

    uint32_t major = VK_VERSION_MAJOR(apiVersion);
    uint32_t minor = VK_VERSION_MINOR(apiVersion);
    LOG_INFO("Vulkan API version: {}.{}", major, minor);

    // We require Vulkan 1.3 for dynamic rendering
    if (apiVersion < VK_API_VERSION_1_3) {
        LOG_WARN("Vulkan 1.3 not available, some features may be disabled");
    }

    // Get required extensions from GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    m_instanceExtensions.clear();
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        m_instanceExtensions.push_back(glfwExtensions[i]);
    }

    // Add debug extension if validation is enabled
    if (m_validationEnabled) {
        m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // Verify layers are available
    if (m_validationEnabled) {
        uint32_t layerCount;
        vk.vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vk.vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : m_validationLayers) {
            bool found = false;
            for (const auto& layer : availableLayers) {
                if (strcmp(layerName, layer.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LOG_WARN("Validation layer {} not available", layerName);
                m_validationEnabled = false;
                m_validationLayers.clear();
                break;
            }
        }
    }

    // Application info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = config.applicationName;
    appInfo.applicationVersion = config.applicationVersion;
    appInfo.pEngineName = "VesperEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Instance create info
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_instanceExtensions.data();

    if (m_validationEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();

        // Debug messenger for instance creation/destruction
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    }

    VK_CHECK_RETURN(vk.vkCreateInstance(&createInfo, nullptr, &m_instance), false);
    return true;
}

bool VulkanRHI::setupDebugMessenger()
{
    if (!m_validationEnabled) return true;

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    if (!vk.vkCreateDebugUtilsMessengerEXT) {
        return false;
    }

    VK_CHECK_RETURN(vk.vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger), false);
    return true;
}

bool VulkanRHI::selectPhysicalDevice(uint32_t preferredGpuIndex)
{
    uint32_t deviceCount = 0;
    vk.vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        LOG_ERROR("No GPUs with Vulkan support found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vk.vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Score devices
    std::vector<std::pair<VkPhysicalDevice, int>> scoredDevices;
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vk.vkGetPhysicalDeviceProperties(device, &props);

        int score = 0;

        // Prefer discrete GPUs
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        // Score by max texture size (rough capability indicator)
        score += props.limits.maxImageDimension2D / 100;

        scoredDevices.push_back({device, score});
        LOG_INFO("Found GPU: {} (score: {})", props.deviceName, score);
    }

    // Sort by score
    std::sort(scoredDevices.begin(), scoredDevices.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Use preferred index if valid
    if (preferredGpuIndex > 0 && preferredGpuIndex <= deviceCount) {
        m_physicalDevice = devices[preferredGpuIndex - 1];
    } else {
        m_physicalDevice = scoredDevices[0].first;
    }

    // Get device properties
    vk.vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
    vk.vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);

    LOG_INFO("Selected GPU: {}", m_deviceProperties.deviceName);

    return true;
}

bool VulkanRHI::createLogicalDevice()
{
    // Find queue families (need a temporary surface to check present support)
    // For now, find without surface - we'll verify present support when creating swapchain
    m_queueFamilies = findQueueFamilies(m_physicalDevice, VK_NULL_HANDLE);

    if (!m_queueFamilies.graphics.has_value()) {
        LOG_ERROR("GPU does not support graphics queue");
        return false;
    }

    // Queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        m_queueFamilies.graphics.value()
    };
    if (m_queueFamilies.compute.has_value())
        uniqueQueueFamilies.insert(m_queueFamilies.compute.value());
    if (m_queueFamilies.transfer.has_value())
        uniqueQueueFamilies.insert(m_queueFamilies.transfer.value());

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Required extensions
    m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    // Check for optional extensions
    uint32_t extensionCount;
    vk.vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vk.vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    auto hasExtension = [&](const char* name) {
        return std::any_of(availableExtensions.begin(), availableExtensions.end(),
                           [name](const VkExtensionProperties& ext) {
                               return strcmp(ext.extensionName, name) == 0;
                           });
    };

    // Dynamic rendering (Vulkan 1.3 core, but can be extension)
    bool hasDynamicRendering = false;
    if (m_deviceProperties.apiVersion >= VK_API_VERSION_1_3) {
        hasDynamicRendering = true;
    } else if (hasExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
        m_deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        hasDynamicRendering = true;
    }

    // Synchronization2 (Vulkan 1.3 core)
    if (hasExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) &&
        m_deviceProperties.apiVersion < VK_API_VERSION_1_3) {
        m_deviceExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }

    // Query available Vulkan 1.2/1.3 features (for reference)
    m_vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    m_vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    m_vulkan12Features.pNext = &m_vulkan11Features;
    m_vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    m_vulkan13Features.pNext = &m_vulkan12Features;

    VkPhysicalDeviceFeatures2 queriedFeatures2 = {};
    queriedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    queriedFeatures2.pNext = &m_vulkan13Features;

    if (vk.vkGetPhysicalDeviceFeatures2) {
        vk.vkGetPhysicalDeviceFeatures2(m_physicalDevice, &queriedFeatures2);
    }

    // Create clean feature structures with only the features we need
    // This avoids enabling features that require extensions we haven't enabled
    VkPhysicalDeviceVulkan11Features enabledVulkan11Features = {};
    enabledVulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

    VkPhysicalDeviceVulkan12Features enabledVulkan12Features = {};
    enabledVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enabledVulkan12Features.pNext = &enabledVulkan11Features;
    // Enable commonly needed Vulkan 1.2 features if available
    enabledVulkan12Features.descriptorIndexing = m_vulkan12Features.descriptorIndexing;
    enabledVulkan12Features.bufferDeviceAddress = m_vulkan12Features.bufferDeviceAddress;
    enabledVulkan12Features.timelineSemaphore = m_vulkan12Features.timelineSemaphore;
    enabledVulkan12Features.hostQueryReset = m_vulkan12Features.hostQueryReset;

    VkPhysicalDeviceVulkan13Features enabledVulkan13Features = {};
    enabledVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enabledVulkan13Features.pNext = &enabledVulkan12Features;
    // Only enable Vulkan 1.3 features if API version supports them
    if (m_deviceProperties.apiVersion >= VK_API_VERSION_1_3) {
        enabledVulkan13Features.dynamicRendering = m_vulkan13Features.dynamicRendering;
        enabledVulkan13Features.synchronization2 = m_vulkan13Features.synchronization2;
        enabledVulkan13Features.maintenance4 = m_vulkan13Features.maintenance4;
    }

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &enabledVulkan13Features;
    // Copy basic features
    features2.features = queriedFeatures2.features;

    // Create device
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    // Enable features via pNext chain for Vulkan 1.1+
    if (m_deviceProperties.apiVersion >= VK_API_VERSION_1_1) {
        createInfo.pNext = &features2;
    } else {
        createInfo.pEnabledFeatures = &m_deviceFeatures;
    }

    LOG_INFO("Creating Vulkan device...");
    VkResult deviceResult = vk.vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (deviceResult != VK_SUCCESS) {
        LOG_ERROR("vkCreateDevice failed with error: {}", static_cast<int>(deviceResult));
        return false;
    }
    LOG_INFO("Vulkan device created successfully");

    // Note: Queue retrieval is done in initialize() after loadDeviceFunctions()
    return true;
}

void VulkanRHI::retrieveQueues()
{
    // Get queues (must be called after loadDeviceFunctions)
    m_graphicsQueue = std::make_shared<VulkanQueue>();
    m_graphicsQueue->type = RHIQueueType::Graphics;
    m_graphicsQueue->familyIndex = m_queueFamilies.graphics.value();
    vk.vkGetDeviceQueue(m_device, m_queueFamilies.graphics.value(), 0, &m_graphicsQueue->queue);

    if (m_queueFamilies.compute.has_value()) {
        m_computeQueue = std::make_shared<VulkanQueue>();
        m_computeQueue->type = RHIQueueType::Compute;
        m_computeQueue->familyIndex = m_queueFamilies.compute.value();
        vk.vkGetDeviceQueue(m_device, m_queueFamilies.compute.value(), 0, &m_computeQueue->queue);
    } else {
        m_computeQueue = m_graphicsQueue;
    }

    if (m_queueFamilies.transfer.has_value()) {
        m_transferQueue = std::make_shared<VulkanQueue>();
        m_transferQueue->type = RHIQueueType::Transfer;
        m_transferQueue->familyIndex = m_queueFamilies.transfer.value();
        vk.vkGetDeviceQueue(m_device, m_queueFamilies.transfer.value(), 0, &m_transferQueue->queue);
    } else {
        m_transferQueue = m_graphicsQueue;
    }

    LOG_INFO("Vulkan queues retrieved successfully");
}

bool VulkanRHI::createVmaAllocator()
{
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vk.vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        vk.vkGetInstanceProcAddr(m_instance, "vkGetDeviceProcAddr"));
    vulkanFunctions.vkGetPhysicalDeviceProperties = vk.vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vk.vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vk.vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vk.vkFreeMemory;
    vulkanFunctions.vkMapMemory = vk.vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vk.vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vk.vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vk.vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vk.vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vk.vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vk.vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vk.vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vk.vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vk.vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vk.vkCreateImage;
    vulkanFunctions.vkDestroyImage = vk.vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vk.vkCmdCopyBuffer;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    // Enable buffer device address if available
    if (m_vulkan12Features.bufferDeviceAddress) {
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    VK_CHECK_RETURN(vmaCreateAllocator(&allocatorInfo, &m_allocator), false);
    return true;
}

void VulkanRHI::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 10000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VK_CHECK(vk.vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool));
}

void VulkanRHI::fillGpuInfo()
{
    std::memcpy(m_gpuInfo.deviceName, m_deviceProperties.deviceName, sizeof(m_gpuInfo.deviceName));
    m_gpuInfo.vendorId = m_deviceProperties.vendorID;
    m_gpuInfo.deviceId = m_deviceProperties.deviceID;
    m_gpuInfo.discreteGpu = m_deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    // Get memory info
    VkPhysicalDeviceMemoryProperties memProps;
    vk.vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            m_gpuInfo.dedicatedMemory = memProps.memoryHeaps[i].size;
            break;
        }
    }

    // Capabilities
    m_gpuInfo.dynamicRendering = m_vulkan13Features.dynamicRendering || m_deviceProperties.apiVersion >= VK_API_VERSION_1_3;
    m_gpuInfo.descriptorIndexing = m_vulkan12Features.descriptorIndexing;
    m_gpuInfo.bufferDeviceAddress = m_vulkan12Features.bufferDeviceAddress;
    m_gpuInfo.synchronization2 = m_vulkan13Features.synchronization2 || m_deviceProperties.apiVersion >= VK_API_VERSION_1_3;
    m_gpuInfo.timelineSemaphore = m_vulkan12Features.timelineSemaphore;

    // Limits
    m_gpuInfo.maxTextureSize = m_deviceProperties.limits.maxImageDimension2D;
    m_gpuInfo.maxUniformBufferSize = m_deviceProperties.limits.maxUniformBufferRange;
    m_gpuInfo.maxStorageBufferSize = m_deviceProperties.limits.maxStorageBufferRange;
    m_gpuInfo.maxPushConstantSize = m_deviceProperties.limits.maxPushConstantsSize;
    m_gpuInfo.maxBoundDescriptorSets = m_deviceProperties.limits.maxBoundDescriptorSets;
    m_gpuInfo.maxColorAttachments = m_deviceProperties.limits.maxColorAttachments;

    m_gpuInfo.maxComputeWorkGroupCount[0] = m_deviceProperties.limits.maxComputeWorkGroupCount[0];
    m_gpuInfo.maxComputeWorkGroupCount[1] = m_deviceProperties.limits.maxComputeWorkGroupCount[1];
    m_gpuInfo.maxComputeWorkGroupCount[2] = m_deviceProperties.limits.maxComputeWorkGroupCount[2];
    m_gpuInfo.maxComputeWorkGroupSize[0] = m_deviceProperties.limits.maxComputeWorkGroupSize[0];
    m_gpuInfo.maxComputeWorkGroupSize[1] = m_deviceProperties.limits.maxComputeWorkGroupSize[1];
    m_gpuInfo.maxComputeWorkGroupSize[2] = m_deviceProperties.limits.maxComputeWorkGroupSize[2];
}

// ============================================================================
// Queue Management
// ============================================================================

RHIQueueHandle VulkanRHI::getQueue(RHIQueueType type)
{
    switch (type) {
        case RHIQueueType::Graphics: return m_graphicsQueue;
        case RHIQueueType::Compute:  return m_computeQueue;
        case RHIQueueType::Transfer: return m_transferQueue;
        default:                     return m_graphicsQueue;
    }
}

// ============================================================================
// SwapChain
// ============================================================================

RHISwapChainHandle VulkanRHI::createSwapChain(const RHISwapChainDesc& desc)
{
    auto swapchain = std::make_shared<VulkanSwapChain>();

    // Create surface from GLFW window
    GLFWwindow* window = static_cast<GLFWwindow*>(desc.windowHandle);
    VK_CHECK_RETURN(glfwCreateWindowSurface(m_instance, window, nullptr, &swapchain->surface), nullptr);

    // Verify present support
    VkBool32 presentSupport = false;
    vk.vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_queueFamilies.graphics.value(),
                                            swapchain->surface, &presentSupport);
    if (!presentSupport) {
        LOG_ERROR("Graphics queue does not support presentation");
        vk.vkDestroySurfaceKHR(m_instance, swapchain->surface, nullptr);
        return nullptr;
    }

    // Store present queue family
    m_queueFamilies.present = m_queueFamilies.graphics;

    swapchain->width = desc.width;
    swapchain->height = desc.height;
    swapchain->format = desc.format;

    createSwapChainInternal(swapchain.get(), desc.width, desc.height);

    // Check if swapchain creation succeeded
    if (swapchain->swapchain == VK_NULL_HANDLE) {
        LOG_ERROR("createSwapChain: Internal swapchain creation failed");
        vk.vkDestroySurfaceKHR(m_instance, swapchain->surface, nullptr);
        return nullptr;
    }

    if (desc.debugName) {
        setVkObjectName(m_device, swapchain->swapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR, desc.debugName);
    }

    return swapchain;
}

void VulkanRHI::destroySwapChain(RHISwapChainHandle swapChain)
{
    if (!swapChain) return;

    waitIdle();

    auto vkSwapchain = std::static_pointer_cast<VulkanSwapChain>(swapChain);
    destroySwapChainInternal(vkSwapchain.get());

    if (vkSwapchain->surface != VK_NULL_HANDLE) {
        vk.vkDestroySurfaceKHR(m_instance, vkSwapchain->surface, nullptr);
        vkSwapchain->surface = VK_NULL_HANDLE;
    }
}

void VulkanRHI::resizeSwapChain(RHISwapChainHandle swapChain, uint32_t width, uint32_t height)
{
    if (!swapChain || width == 0 || height == 0) return;

    waitIdle();

    auto vkSwapchain = std::static_pointer_cast<VulkanSwapChain>(swapChain);
    destroySwapChainInternal(vkSwapchain.get());
    createSwapChainInternal(vkSwapchain.get(), width, height);

    if (vkSwapchain->swapchain == VK_NULL_HANDLE) {
        LOG_ERROR("resizeSwapChain: Failed to recreate swapchain");
    }
}

RHITextureHandle VulkanRHI::getSwapChainImage(RHISwapChainHandle swapChain, uint32_t index)
{
    auto vkSwapchain = std::static_pointer_cast<VulkanSwapChain>(swapChain);
    if (index >= vkSwapchain->imageTextures.size()) return nullptr;
    return vkSwapchain->imageTextures[index];
}

uint32_t VulkanRHI::getSwapChainImageCount(RHISwapChainHandle swapChain)
{
    auto vkSwapchain = std::static_pointer_cast<VulkanSwapChain>(swapChain);
    return vkSwapchain->imageCount;
}

void VulkanRHI::createSwapChainInternal(VulkanSwapChain* swapchain, uint32_t width, uint32_t height)
{
    // Query surface capabilities with error checking
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult capsResult = vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, swapchain->surface, &capabilities);
    if (capsResult != VK_SUCCESS) {
        LOG_ERROR("Failed to query surface capabilities: {}", static_cast<int>(capsResult));
        swapchain->swapchain = VK_NULL_HANDLE;
        return;
    }

    uint32_t formatCount = 0;
    vk.vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, swapchain->surface, &formatCount, nullptr);
    if (formatCount == 0) {
        LOG_ERROR("No surface formats available");
        swapchain->swapchain = VK_NULL_HANDLE;
        return;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vk.vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, swapchain->surface, &formatCount, formats.data());

    uint32_t presentModeCount = 0;
    vk.vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, swapchain->surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) {
        LOG_ERROR("No present modes available");
        swapchain->swapchain = VK_NULL_HANDLE;
        return;
    }
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vk.vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, swapchain->surface, &presentModeCount, presentModes.data());

    // Choose surface format
    swapchain->surfaceFormat = chooseSwapSurfaceFormat(formats, swapchain->format);

    // Choose present mode
    swapchain->presentMode = VK_PRESENT_MODE_FIFO_KHR;  // Always available, vsync
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            swapchain->presentMode = mode;
            break;
        }
    }

    // Choose extent
    VkExtent2D extent = chooseSwapExtent(capabilities, width, height);

    // Validate extent - cannot create swapchain with zero size
    if (extent.width == 0 || extent.height == 0) {
        LOG_WARN("Cannot create swapchain with zero extent ({}x{}), window may be minimized",
                 extent.width, extent.height);
        swapchain->swapchain = VK_NULL_HANDLE;
        return;
    }

    // Image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Determine image usage - only add TRANSFER_DST if supported
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    // Choose a supported composite alpha mode
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = swapchain->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = swapchain->surfaceFormat.format;
    createInfo.imageColorSpace = swapchain->surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = imageUsage;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = swapchain->presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult swapchainResult = vk.vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &swapchain->swapchain);
    if (swapchainResult != VK_SUCCESS) {
        LOG_ERROR("Failed to create swapchain: {}", static_cast<int>(swapchainResult));
        swapchain->swapchain = VK_NULL_HANDLE;
        return;
    }

    // Get swapchain images
    vk.vkGetSwapchainImagesKHR(m_device, swapchain->swapchain, &imageCount, nullptr);
    swapchain->images.resize(imageCount);
    vk.vkGetSwapchainImagesKHR(m_device, swapchain->swapchain, &imageCount, swapchain->images.data());

    swapchain->width = extent.width;
    swapchain->height = extent.height;
    swapchain->imageCount = imageCount;
    swapchain->format = fromVkFormat(swapchain->surfaceFormat.format);

    // Create image views and wrap as textures
    swapchain->imageTextures.clear();
    for (uint32_t i = 0; i < imageCount; i++) {
        auto texture = std::make_shared<VulkanTexture>();
        texture->image = swapchain->images[i];
        texture->isSwapchainImage = true;
        texture->extent = {extent.width, extent.height, 1};
        texture->format = swapchain->format;
        texture->usage = RHITextureUsage::ColorAttachment;
        texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
        texture->aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchain->images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchain->surfaceFormat.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vk.vkCreateImageView(m_device, &viewInfo, nullptr, &texture->imageView));

        swapchain->imageTextures.push_back(texture);
    }
}

void VulkanRHI::destroySwapChainInternal(VulkanSwapChain* swapchain)
{
    // Destroy image views
    for (auto& texture : swapchain->imageTextures) {
        auto vkTexture = std::static_pointer_cast<VulkanTexture>(texture);
        if (vkTexture->imageView != VK_NULL_HANDLE) {
            vk.vkDestroyImageView(m_device, vkTexture->imageView, nullptr);
        }
    }
    swapchain->imageTextures.clear();
    swapchain->images.clear();

    // Destroy swapchain
    if (swapchain->swapchain != VK_NULL_HANDLE) {
        vk.vkDestroySwapchainKHR(m_device, swapchain->swapchain, nullptr);
        swapchain->swapchain = VK_NULL_HANDLE;
    }
}

VkSurfaceFormatKHR VulkanRHI::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats, RHIFormat preferredFormat)
{
    VkFormat preferred = toVkFormat(preferredFormat);

    for (const auto& format : availableFormats) {
        if (format.format == preferred) {
            return format;
        }
    }

    // Fallback to SRGB
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanRHI::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availableModes, RHIPresentMode preferredMode)
{
    VkPresentModeKHR preferred = toVkPresentMode(preferredMode);

    for (const auto& mode : availableModes) {
        if (mode == preferred) {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRHI::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
{
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {width, height};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

// ============================================================================
// Resource Creation
// ============================================================================

RHIBufferHandle VulkanRHI::createBuffer(const RHIBufferDesc& desc)
{
    auto buffer = std::make_shared<VulkanBuffer>();
    buffer->size = desc.size;
    buffer->usage = desc.usage;
    buffer->memoryUsage = desc.memoryUsage;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.size;
    bufferInfo.usage = toVkBufferUsage(desc.usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = toVmaMemoryUsage(desc.memoryUsage);

    if (desc.memoryUsage == RHIMemoryUsage::CpuToGpu || desc.memoryUsage == RHIMemoryUsage::CpuOnly) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VK_CHECK_RETURN(vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                                    &buffer->buffer, &buffer->allocation, &buffer->allocInfo), nullptr);

    if (allocInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        buffer->mappedData = buffer->allocInfo.pMappedData;
    }

    if (desc.debugName) {
        setVkObjectName(m_device, buffer->buffer, VK_OBJECT_TYPE_BUFFER, desc.debugName);
    }

    return buffer;
}

void VulkanRHI::destroyBuffer(RHIBufferHandle buffer)
{
    if (!buffer) return;

    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    if (vkBuffer->buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, vkBuffer->buffer, vkBuffer->allocation);
        vkBuffer->buffer = VK_NULL_HANDLE;
        vkBuffer->allocation = VK_NULL_HANDLE;
    }
}

RHITextureHandle VulkanRHI::createTexture(const RHITextureDesc& desc)
{
    auto texture = std::make_shared<VulkanTexture>();
    texture->extent = desc.extent;
    texture->mipLevels = desc.mipLevels;
    texture->arrayLayers = desc.arrayLayers;
    texture->format = desc.format;
    texture->dimension = desc.dimension;
    texture->sampleCount = desc.sampleCount;
    texture->usage = desc.usage;
    texture->currentState = desc.initialState;

    bool isDepth = isDepthFormat(desc.format);
    texture->aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    if (isStencilFormat(desc.format)) {
        texture->aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = toVkImageType(desc.dimension);
    imageInfo.format = toVkFormat(desc.format);
    imageInfo.extent = {desc.extent.width, desc.extent.height, desc.extent.depth};
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = desc.arrayLayers;
    imageInfo.samples = toVkSampleCount(desc.sampleCount);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = toVkImageUsage(desc.usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (desc.dimension == RHITextureDimension::TexCube || desc.dimension == RHITextureDimension::TexCubeArray) {
        imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = toVmaMemoryUsage(desc.memoryUsage);

    VK_CHECK_RETURN(vmaCreateImage(m_allocator, &imageInfo, &allocInfo,
                                   &texture->image, &texture->allocation, nullptr), nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture->image;
    viewInfo.viewType = toVkImageViewType(desc.dimension, desc.arrayLayers);
    viewInfo.format = toVkFormat(desc.format);
    viewInfo.subresourceRange.aspectMask = texture->aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = desc.arrayLayers;

    VK_CHECK_RETURN(vk.vkCreateImageView(m_device, &viewInfo, nullptr, &texture->imageView), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, texture->image, VK_OBJECT_TYPE_IMAGE, desc.debugName);
    }

    return texture;
}

void VulkanRHI::destroyTexture(RHITextureHandle texture)
{
    if (!texture) return;

    auto vkTexture = std::static_pointer_cast<VulkanTexture>(texture);

    if (vkTexture->imageView != VK_NULL_HANDLE) {
        vk.vkDestroyImageView(m_device, vkTexture->imageView, nullptr);
        vkTexture->imageView = VK_NULL_HANDLE;
    }

    if (!vkTexture->isSwapchainImage && vkTexture->image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, vkTexture->image, vkTexture->allocation);
        vkTexture->image = VK_NULL_HANDLE;
        vkTexture->allocation = VK_NULL_HANDLE;
    }
}

RHISamplerHandle VulkanRHI::createSampler(const RHISamplerDesc& desc)
{
    auto sampler = std::make_shared<VulkanSampler>();

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = toVkFilter(desc.magFilter);
    samplerInfo.minFilter = toVkFilter(desc.minFilter);
    samplerInfo.mipmapMode = toVkMipmapMode(desc.mipmapMode);
    samplerInfo.addressModeU = toVkAddressMode(desc.addressModeU);
    samplerInfo.addressModeV = toVkAddressMode(desc.addressModeV);
    samplerInfo.addressModeW = toVkAddressMode(desc.addressModeW);
    samplerInfo.mipLodBias = desc.mipLodBias;
    samplerInfo.anisotropyEnable = desc.anisotropyEnable ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = desc.maxAnisotropy;
    samplerInfo.compareEnable = desc.compareEnable ? VK_TRUE : VK_FALSE;
    samplerInfo.compareOp = toVkCompareOp(desc.compareOp);
    samplerInfo.minLod = desc.minLod;
    samplerInfo.maxLod = desc.maxLod;
    samplerInfo.borderColor = toVkBorderColor(desc.borderColor);
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK_RETURN(vk.vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler->sampler), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, sampler->sampler, VK_OBJECT_TYPE_SAMPLER, desc.debugName);
    }

    return sampler;
}

void VulkanRHI::destroySampler(RHISamplerHandle sampler)
{
    if (!sampler) return;

    auto vkSampler = std::static_pointer_cast<VulkanSampler>(sampler);
    if (vkSampler->sampler != VK_NULL_HANDLE) {
        vk.vkDestroySampler(m_device, vkSampler->sampler, nullptr);
        vkSampler->sampler = VK_NULL_HANDLE;
    }
}

// ============================================================================
// Buffer Operations
// ============================================================================

void* VulkanRHI::mapBuffer(RHIBufferHandle buffer)
{
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    if (vkBuffer->mappedData) {
        return vkBuffer->mappedData;
    }

    void* data;
    VK_CHECK_RETURN(vmaMapMemory(m_allocator, vkBuffer->allocation, &data), nullptr);
    vkBuffer->mappedData = data;
    return data;
}

void VulkanRHI::unmapBuffer(RHIBufferHandle buffer)
{
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    // Don't unmap persistently mapped buffers (CpuToGpu and CpuOnly are created with VMA_ALLOCATION_CREATE_MAPPED_BIT)
    if (vkBuffer->mappedData &&
        vkBuffer->memoryUsage != RHIMemoryUsage::CpuToGpu &&
        vkBuffer->memoryUsage != RHIMemoryUsage::CpuOnly) {
        vmaUnmapMemory(m_allocator, vkBuffer->allocation);
        vkBuffer->mappedData = nullptr;
    }
}

void VulkanRHI::updateBuffer(RHIBufferHandle buffer, const void* data, uint64_t size, uint64_t offset)
{
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    void* mapped = mapBuffer(buffer);
    if (mapped) {
        std::memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
        unmapBuffer(buffer);
    }
}

// ============================================================================
// Shader and Pipeline
// ============================================================================

RHIShaderHandle VulkanRHI::createShader(const RHIShaderDesc& desc)
{
    auto shader = std::make_shared<VulkanShader>();
    shader->stage = desc.stage;
    shader->entryPoint = desc.entryPoint ? desc.entryPoint : "main";

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = desc.codeSize;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(desc.code);

    VK_CHECK_RETURN(vk.vkCreateShaderModule(m_device, &createInfo, nullptr, &shader->module), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, shader->module, VK_OBJECT_TYPE_SHADER_MODULE, desc.debugName);
    }

    return shader;
}

void VulkanRHI::destroyShader(RHIShaderHandle shader)
{
    if (!shader) return;

    auto vkShader = std::static_pointer_cast<VulkanShader>(shader);
    if (vkShader->module != VK_NULL_HANDLE) {
        vk.vkDestroyShaderModule(m_device, vkShader->module, nullptr);
        vkShader->module = VK_NULL_HANDLE;
    }
}

RHIDescriptorSetLayoutHandle VulkanRHI::createDescriptorSetLayout(const RHIDescriptorSetLayoutDesc& desc)
{
    auto layout = std::make_shared<VulkanDescriptorSetLayout>();
    layout->bindings = desc.bindings;

    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(desc.bindings.size());

    for (const auto& binding : desc.bindings) {
        VkDescriptorSetLayoutBinding vkBinding = {};
        vkBinding.binding = binding.binding;
        vkBinding.descriptorType = toVkDescriptorType(binding.descriptorType);
        vkBinding.descriptorCount = binding.descriptorCount;
        vkBinding.stageFlags = toVkShaderStageFlags(binding.stageFlags);
        vkBinding.pImmutableSamplers = nullptr;
        vkBindings.push_back(vkBinding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    layoutInfo.pBindings = vkBindings.data();

    VK_CHECK_RETURN(vk.vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &layout->layout), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, layout->layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, desc.debugName);
    }

    return layout;
}

void VulkanRHI::destroyDescriptorSetLayout(RHIDescriptorSetLayoutHandle layout)
{
    if (!layout) return;

    auto vkLayout = std::static_pointer_cast<VulkanDescriptorSetLayout>(layout);
    if (vkLayout->layout != VK_NULL_HANDLE) {
        vk.vkDestroyDescriptorSetLayout(m_device, vkLayout->layout, nullptr);
        vkLayout->layout = VK_NULL_HANDLE;
    }
}

RHIPipelineHandle VulkanRHI::createGraphicsPipeline(const RHIGraphicsPipelineDesc& desc)
{
    auto pipeline = std::make_shared<VulkanPipeline>();
    pipeline->isCompute = false;
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Shader stages
    // Store VulkanShader pointers to keep entryPoint strings alive
    std::vector<std::shared_ptr<VulkanShader>> vkShaders;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (const auto& shader : desc.shaders) {
        auto vkShader = std::static_pointer_cast<VulkanShader>(shader);
        vkShaders.push_back(vkShader);

        VkPipelineShaderStageCreateInfo stageInfo = {};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = toVkShaderStage(vkShader->stage);
        stageInfo.module = vkShader->module;
        stageInfo.pName = vkShader->entryPoint.c_str();
        shaderStages.push_back(stageInfo);
    }

    // Vertex input
    std::vector<VkVertexInputBindingDescription> bindingDescs;
    std::vector<VkVertexInputAttributeDescription> attrDescs;

    for (const auto& binding : desc.vertexInput.bindings) {
        VkVertexInputBindingDescription bindingDesc = {};
        bindingDesc.binding = binding.binding;
        bindingDesc.stride = binding.stride;
        bindingDesc.inputRate = toVkVertexInputRate(binding.inputRate);
        bindingDescs.push_back(bindingDesc);
    }

    for (const auto& attr : desc.vertexInput.attributes) {
        VkVertexInputAttributeDescription attrDesc = {};
        attrDesc.location = attr.location;
        attrDesc.binding = attr.binding;
        attrDesc.format = toVkFormat(attr.format);
        attrDesc.offset = attr.offset;
        attrDescs.push_back(attrDesc);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescs.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescs.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = toVkPrimitiveTopology(desc.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = desc.rasterization.depthClampEnable ? VK_TRUE : VK_FALSE;
    rasterizer.rasterizerDiscardEnable = desc.rasterization.rasterizerDiscardEnable ? VK_TRUE : VK_FALSE;
    rasterizer.polygonMode = toVkPolygonMode(desc.rasterization.polygonMode);
    rasterizer.cullMode = toVkCullMode(desc.rasterization.cullMode);
    rasterizer.frontFace = toVkFrontFace(desc.rasterization.frontFace);
    rasterizer.depthBiasEnable = desc.rasterization.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterizer.depthBiasConstantFactor = desc.rasterization.depthBiasConstant;
    rasterizer.depthBiasClamp = desc.rasterization.depthBiasClamp;
    rasterizer.depthBiasSlopeFactor = desc.rasterization.depthBiasSlope;
    rasterizer.lineWidth = desc.rasterization.lineWidth;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = desc.multisample.sampleShadingEnable ? VK_TRUE : VK_FALSE;
    multisampling.rasterizationSamples = toVkSampleCount(desc.multisample.sampleCount);
    multisampling.minSampleShading = desc.multisample.minSampleShading;
    multisampling.alphaToCoverageEnable = desc.multisample.alphaToCoverageEnable ? VK_TRUE : VK_FALSE;
    multisampling.alphaToOneEnable = desc.multisample.alphaToOneEnable ? VK_TRUE : VK_FALSE;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = toVkCompareOp(desc.depthStencil.depthCompareOp);
    depthStencil.depthBoundsTestEnable = desc.depthStencil.depthBoundsEnable ? VK_TRUE : VK_FALSE;
    depthStencil.stencilTestEnable = desc.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.minDepthBounds = desc.depthStencil.minDepthBounds;
    depthStencil.maxDepthBounds = desc.depthStencil.maxDepthBounds;

    // Color blending
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    for (const auto& attachment : desc.colorBlend.attachments) {
        VkPipelineColorBlendAttachmentState blendAttachment = {};
        blendAttachment.blendEnable = attachment.blendEnable ? VK_TRUE : VK_FALSE;
        blendAttachment.srcColorBlendFactor = toVkBlendFactor(attachment.srcColorFactor);
        blendAttachment.dstColorBlendFactor = toVkBlendFactor(attachment.dstColorFactor);
        blendAttachment.colorBlendOp = toVkBlendOp(attachment.colorBlendOp);
        blendAttachment.srcAlphaBlendFactor = toVkBlendFactor(attachment.srcAlphaFactor);
        blendAttachment.dstAlphaBlendFactor = toVkBlendFactor(attachment.dstAlphaFactor);
        blendAttachment.alphaBlendOp = toVkBlendOp(attachment.alphaBlendOp);
        blendAttachment.colorWriteMask = attachment.colorWriteMask;
        colorBlendAttachments.push_back(blendAttachment);
    }

    // If no attachments specified but we have color formats, create default
    if (colorBlendAttachments.empty() && !desc.colorFormats.empty()) {
        for (size_t i = 0; i < desc.colorFormats.size(); i++) {
            VkPipelineColorBlendAttachmentState blendAttachment = {};
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachments.push_back(blendAttachment);
        }
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = desc.colorBlend.logicOpEnable ? VK_TRUE : VK_FALSE;
    colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.data();
    std::memcpy(colorBlending.blendConstants, desc.colorBlend.blendConstants, sizeof(float) * 4);

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Pipeline layout
    std::vector<VkDescriptorSetLayout> setLayouts;
    for (const auto& layout : desc.descriptorLayouts) {
        auto vkLayout = std::static_pointer_cast<VulkanDescriptorSetLayout>(layout);
        setLayouts.push_back(vkLayout->layout);
    }
    pipeline->descriptorSetLayouts = setLayouts;

    // Push constant ranges
    std::vector<VkPushConstantRange> pushConstantRanges;
    for (const auto& range : desc.pushConstantRanges) {
        VkPushConstantRange vkRange = {};
        vkRange.stageFlags = toVkShaderStageFlags(range.stages);
        vkRange.offset = range.offset;
        vkRange.size = range.size;
        pushConstantRanges.push_back(vkRange);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

    VK_CHECK_RETURN(vk.vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipeline->pipelineLayout), nullptr);

    // Dynamic rendering format info (Vulkan 1.3)
    std::vector<VkFormat> colorFormats;
    for (const auto& format : desc.colorFormats) {
        colorFormats.push_back(toVkFormat(format));
    }

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorFormats.size());
    renderingInfo.pColorAttachmentFormats = colorFormats.data();
    renderingInfo.depthAttachmentFormat = toVkFormat(desc.depthFormat);
    renderingInfo.stencilAttachmentFormat = toVkFormat(desc.stencilFormat);

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;  // Dynamic rendering
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeline->pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;  // Using dynamic rendering
    pipelineInfo.subpass = 0;

    VK_CHECK_RETURN(vk.vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline->pipeline), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, pipeline->pipeline, VK_OBJECT_TYPE_PIPELINE, desc.debugName);
    }

    return pipeline;
}

RHIPipelineHandle VulkanRHI::createComputePipeline(const RHIComputePipelineDesc& desc)
{
    auto pipeline = std::make_shared<VulkanPipeline>();
    pipeline->isCompute = true;
    pipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

    auto vkShader = std::static_pointer_cast<VulkanShader>(desc.shader);

    // Pipeline layout
    std::vector<VkDescriptorSetLayout> setLayouts;
    for (const auto& layout : desc.descriptorLayouts) {
        auto vkLayout = std::static_pointer_cast<VulkanDescriptorSetLayout>(layout);
        setLayouts.push_back(vkLayout->layout);
    }
    pipeline->descriptorSetLayouts = setLayouts;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();

    VK_CHECK_RETURN(vk.vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipeline->pipelineLayout), nullptr);

    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = vkShader->module;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pipeline->pipelineLayout;

    VK_CHECK_RETURN(vk.vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline->pipeline), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, pipeline->pipeline, VK_OBJECT_TYPE_PIPELINE, desc.debugName);
    }

    return pipeline;
}

void VulkanRHI::destroyPipeline(RHIPipelineHandle pipeline)
{
    if (!pipeline) return;

    auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);

    if (vkPipeline->pipeline != VK_NULL_HANDLE) {
        vk.vkDestroyPipeline(m_device, vkPipeline->pipeline, nullptr);
        vkPipeline->pipeline = VK_NULL_HANDLE;
    }

    if (vkPipeline->pipelineLayout != VK_NULL_HANDLE) {
        vk.vkDestroyPipelineLayout(m_device, vkPipeline->pipelineLayout, nullptr);
        vkPipeline->pipelineLayout = VK_NULL_HANDLE;
    }
}

// ============================================================================
// Descriptor Sets
// ============================================================================

RHIDescriptorSetHandle VulkanRHI::createDescriptorSet(RHIDescriptorSetLayoutHandle layout)
{
    auto vkLayout = std::static_pointer_cast<VulkanDescriptorSetLayout>(layout);

    auto set = std::make_shared<VulkanDescriptorSet>();
    set->pool = m_descriptorPool;

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vkLayout->layout;

    std::lock_guard<std::mutex> lock(m_descriptorPoolMutex);
    VK_CHECK_RETURN(vk.vkAllocateDescriptorSets(m_device, &allocInfo, &set->set), nullptr);

    return set;
}

void VulkanRHI::destroyDescriptorSet(RHIDescriptorSetHandle set)
{
    if (!set) return;

    auto vkSet = std::static_pointer_cast<VulkanDescriptorSet>(set);
    if (vkSet->set != VK_NULL_HANDLE) {
        std::lock_guard<std::mutex> lock(m_descriptorPoolMutex);
        vk.vkFreeDescriptorSets(m_device, vkSet->pool, 1, &vkSet->set);
        vkSet->set = VK_NULL_HANDLE;
    }
}

void VulkanRHI::updateDescriptorSet(RHIDescriptorSetHandle set, std::span<const RHIDescriptorWrite> writes)
{
    auto vkSet = std::static_pointer_cast<VulkanDescriptorSet>(set);

    std::vector<VkWriteDescriptorSet> vkWrites;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

    bufferInfos.reserve(writes.size());
    imageInfos.reserve(writes.size());

    for (const auto& write : writes) {
        VkWriteDescriptorSet vkWrite = {};
        vkWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vkWrite.dstSet = vkSet->set;
        vkWrite.dstBinding = write.binding;
        vkWrite.dstArrayElement = write.arrayElement;
        vkWrite.descriptorCount = 1;
        vkWrite.descriptorType = toVkDescriptorType(write.type);

        if (write.buffer) {
            auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(write.buffer);
            VkDescriptorBufferInfo bufferInfo = {};
            bufferInfo.buffer = vkBuffer->buffer;
            bufferInfo.offset = write.bufferOffset;
            bufferInfo.range = write.bufferRange == 0 ? VK_WHOLE_SIZE : write.bufferRange;
            bufferInfos.push_back(bufferInfo);
            vkWrite.pBufferInfo = &bufferInfos.back();
        }

        if (write.texture || write.sampler) {
            VkDescriptorImageInfo imageInfo = {};
            if (write.texture) {
                auto vkTexture = std::static_pointer_cast<VulkanTexture>(write.texture);
                imageInfo.imageView = vkTexture->imageView;
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            if (write.sampler) {
                auto vkSampler = std::static_pointer_cast<VulkanSampler>(write.sampler);
                imageInfo.sampler = vkSampler->sampler;
            }
            imageInfos.push_back(imageInfo);
            vkWrite.pImageInfo = &imageInfos.back();
        }

        vkWrites.push_back(vkWrite);
    }

    vk.vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(vkWrites.size()), vkWrites.data(), 0, nullptr);
}

// ============================================================================
// Command Pools and Buffers
// ============================================================================

RHICommandPoolHandle VulkanRHI::createCommandPool(const RHICommandPoolDesc& desc)
{
    auto pool = std::make_shared<VulkanCommandPool>();
    pool->queueType = desc.queueType;

    uint32_t queueFamilyIndex = m_queueFamilies.graphics.value();
    switch (desc.queueType) {
        case RHIQueueType::Compute:  queueFamilyIndex = m_queueFamilies.compute.value_or(queueFamilyIndex); break;
        case RHIQueueType::Transfer: queueFamilyIndex = m_queueFamilies.transfer.value_or(queueFamilyIndex); break;
        default: break;
    }

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    if (desc.transient) {
        poolInfo.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    }
    if (desc.resetCommands) {
        poolInfo.flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    }

    VK_CHECK_RETURN(vk.vkCreateCommandPool(m_device, &poolInfo, nullptr, &pool->pool), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, pool->pool, VK_OBJECT_TYPE_COMMAND_POOL, desc.debugName);
    }

    return pool;
}

void VulkanRHI::destroyCommandPool(RHICommandPoolHandle pool)
{
    if (!pool) return;

    auto vkPool = std::static_pointer_cast<VulkanCommandPool>(pool);
    if (vkPool->pool != VK_NULL_HANDLE) {
        vk.vkDestroyCommandPool(m_device, vkPool->pool, nullptr);
        vkPool->pool = VK_NULL_HANDLE;
    }
}

void VulkanRHI::resetCommandPool(RHICommandPoolHandle pool)
{
    auto vkPool = std::static_pointer_cast<VulkanCommandPool>(pool);
    vk.vkResetCommandPool(m_device, vkPool->pool, 0);
}

RHICommandBufferHandle VulkanRHI::allocateCommandBuffer(RHICommandPoolHandle pool, const RHICommandBufferDesc& desc)
{
    auto vkPool = std::static_pointer_cast<VulkanCommandPool>(pool);
    auto cmd = std::make_shared<VulkanCommandBuffer>();
    cmd->isSecondary = desc.secondary;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkPool->pool;
    allocInfo.level = desc.secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK_RETURN(vk.vkAllocateCommandBuffers(m_device, &allocInfo, &cmd->commandBuffer), nullptr);

    if (desc.debugName) {
        setVkObjectName(m_device, cmd->commandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, desc.debugName);
    }

    return cmd;
}

void VulkanRHI::freeCommandBuffer(RHICommandPoolHandle pool, RHICommandBufferHandle cmd)
{
    if (!pool || !cmd) return;

    auto vkPool = std::static_pointer_cast<VulkanCommandPool>(pool);
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    if (vkCmd->commandBuffer != VK_NULL_HANDLE) {
        vk.vkFreeCommandBuffers(m_device, vkPool->pool, 1, &vkCmd->commandBuffer);
        vkCmd->commandBuffer = VK_NULL_HANDLE;
    }
}

// ============================================================================
// Synchronization
// ============================================================================

RHIFenceHandle VulkanRHI::createFence(bool signaled)
{
    auto fence = std::make_shared<VulkanFence>();
    fence->signaled = signaled;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (signaled) {
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VK_CHECK_RETURN(vk.vkCreateFence(m_device, &fenceInfo, nullptr, &fence->fence), nullptr);

    return fence;
}

void VulkanRHI::destroyFence(RHIFenceHandle fence)
{
    if (!fence) return;

    auto vkFence = std::static_pointer_cast<VulkanFence>(fence);
    if (vkFence->fence != VK_NULL_HANDLE) {
        vk.vkDestroyFence(m_device, vkFence->fence, nullptr);
        vkFence->fence = VK_NULL_HANDLE;
    }
}

void VulkanRHI::waitForFence(RHIFenceHandle fence, uint64_t timeout)
{
    auto vkFence = std::static_pointer_cast<VulkanFence>(fence);
    vk.vkWaitForFences(m_device, 1, &vkFence->fence, VK_TRUE, timeout);
    vkFence->signaled = true;
}

void VulkanRHI::resetFence(RHIFenceHandle fence)
{
    auto vkFence = std::static_pointer_cast<VulkanFence>(fence);
    vk.vkResetFences(m_device, 1, &vkFence->fence);
    vkFence->signaled = false;
}

bool VulkanRHI::isFenceSignaled(RHIFenceHandle fence)
{
    auto vkFence = std::static_pointer_cast<VulkanFence>(fence);
    return vk.vkGetFenceStatus(m_device, vkFence->fence) == VK_SUCCESS;
}

RHISemaphoreHandle VulkanRHI::createSemaphore()
{
    auto semaphore = std::make_shared<VulkanSemaphore>();

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK_RETURN(vk.vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &semaphore->semaphore), nullptr);

    return semaphore;
}

void VulkanRHI::destroySemaphore(RHISemaphoreHandle semaphore)
{
    if (!semaphore) return;

    auto vkSemaphore = std::static_pointer_cast<VulkanSemaphore>(semaphore);
    if (vkSemaphore->semaphore != VK_NULL_HANDLE) {
        vk.vkDestroySemaphore(m_device, vkSemaphore->semaphore, nullptr);
        vkSemaphore->semaphore = VK_NULL_HANDLE;
    }
}

// ============================================================================
// Command Recording
// ============================================================================

void VulkanRHI::beginCommandBuffer(RHICommandBufferHandle cmd)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vk.vkBeginCommandBuffer(vkCmd->commandBuffer, &beginInfo));
    vkCmd->isRecording = true;
}

void VulkanRHI::endCommandBuffer(RHICommandBufferHandle cmd)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    VK_CHECK(vk.vkEndCommandBuffer(vkCmd->commandBuffer));
    vkCmd->isRecording = false;
}

void VulkanRHI::cmdBeginRendering(RHICommandBufferHandle cmd, const RHIRenderingInfo& info)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    for (const auto& attachment : info.colorAttachments) {
        auto vkTexture = std::static_pointer_cast<VulkanTexture>(attachment.texture);

        VkRenderingAttachmentInfo attachmentInfo = {};
        attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachmentInfo.imageView = vkTexture->imageView;
        attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentInfo.loadOp = toVkLoadOp(attachment.loadOp);
        attachmentInfo.storeOp = toVkStoreOp(attachment.storeOp);
        attachmentInfo.clearValue.color = {{
            attachment.clearValue.color[0],
            attachment.clearValue.color[1],
            attachment.clearValue.color[2],
            attachment.clearValue.color[3]
        }};

        if (attachment.resolveTexture) {
            auto vkResolve = std::static_pointer_cast<VulkanTexture>(attachment.resolveTexture);
            attachmentInfo.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            attachmentInfo.resolveImageView = vkResolve->imageView;
            attachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        colorAttachments.push_back(attachmentInfo);
    }

    VkRenderingAttachmentInfo depthAttachment = {};
    if (info.depthAttachment && info.depthAttachment->texture) {
        auto vkTexture = std::static_pointer_cast<VulkanTexture>(info.depthAttachment->texture);

        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = vkTexture->imageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = toVkLoadOp(info.depthAttachment->loadOp);
        depthAttachment.storeOp = toVkStoreOp(info.depthAttachment->storeOp);
        depthAttachment.clearValue.depthStencil = {
            info.depthAttachment->clearValue.depthStencil.depth,
            info.depthAttachment->clearValue.depthStencil.stencil
        };
    }

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {info.renderArea.offset.x, info.renderArea.offset.y};
    renderingInfo.renderArea.extent = {info.renderArea.extent.width, info.renderArea.extent.height};
    renderingInfo.layerCount = info.layerCount;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = (info.depthAttachment && info.depthAttachment->texture) ? &depthAttachment : nullptr;
    renderingInfo.pStencilAttachment = nullptr;  // TODO: stencil support

    if (!vk.vkCmdBeginRendering) {
        LOG_ERROR("vkCmdBeginRendering is null - dynamic rendering not available");
        return;
    }
    vk.vkCmdBeginRendering(vkCmd->commandBuffer, &renderingInfo);
}

void VulkanRHI::cmdEndRendering(RHICommandBufferHandle cmd)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    if (!vk.vkCmdEndRendering) {
        LOG_ERROR("vkCmdEndRendering is null - dynamic rendering not available");
        return;
    }
    vk.vkCmdEndRendering(vkCmd->commandBuffer);
}

void VulkanRHI::cmdSetViewport(RHICommandBufferHandle cmd, const RHIViewport& viewport)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    VkViewport vkViewport = {};
    vkViewport.x = viewport.x;
    vkViewport.y = viewport.y;
    vkViewport.width = viewport.width;
    vkViewport.height = viewport.height;
    vkViewport.minDepth = viewport.minDepth;
    vkViewport.maxDepth = viewport.maxDepth;

    vk.vkCmdSetViewport(vkCmd->commandBuffer, 0, 1, &vkViewport);
}

void VulkanRHI::cmdSetScissor(RHICommandBufferHandle cmd, const RHIRect2D& scissor)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    VkRect2D vkScissor = {};
    vkScissor.offset = {scissor.offset.x, scissor.offset.y};
    vkScissor.extent = {scissor.extent.width, scissor.extent.height};

    vk.vkCmdSetScissor(vkCmd->commandBuffer, 0, 1, &vkScissor);
}

void VulkanRHI::cmdBindPipeline(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);

    vk.vkCmdBindPipeline(vkCmd->commandBuffer, vkPipeline->bindPoint, vkPipeline->pipeline);
}

void VulkanRHI::cmdBindDescriptorSets(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline,
                                      uint32_t firstSet, std::span<const RHIDescriptorSetHandle> sets,
                                      std::span<const uint32_t> dynamicOffsets)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);

    std::vector<VkDescriptorSet> vkSets;
    for (const auto& set : sets) {
        auto vkSet = std::static_pointer_cast<VulkanDescriptorSet>(set);
        vkSets.push_back(vkSet->set);
    }

    vk.vkCmdBindDescriptorSets(vkCmd->commandBuffer, vkPipeline->bindPoint, vkPipeline->pipelineLayout,
                               firstSet, static_cast<uint32_t>(vkSets.size()), vkSets.data(),
                               static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
}

void VulkanRHI::cmdBindVertexBuffer(RHICommandBufferHandle cmd, uint32_t binding,
                                    RHIBufferHandle buffer, uint64_t offset)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);

    VkBuffer buffers[] = {vkBuffer->buffer};
    VkDeviceSize offsets[] = {offset};

    vk.vkCmdBindVertexBuffers(vkCmd->commandBuffer, binding, 1, buffers, offsets);
}

void VulkanRHI::cmdBindIndexBuffer(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                                   uint64_t offset, bool use16Bit)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);

    vk.vkCmdBindIndexBuffer(vkCmd->commandBuffer, vkBuffer->buffer, offset,
                            use16Bit ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void VulkanRHI::cmdDraw(RHICommandBufferHandle cmd, uint32_t vertexCount, uint32_t instanceCount,
                        uint32_t firstVertex, uint32_t firstInstance)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    vk.vkCmdDraw(vkCmd->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanRHI::cmdDrawIndexed(RHICommandBufferHandle cmd, uint32_t indexCount, uint32_t instanceCount,
                               uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    vk.vkCmdDrawIndexed(vkCmd->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VulkanRHI::cmdDrawIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                                uint64_t offset, uint32_t drawCount, uint32_t stride)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);

    vk.vkCmdDrawIndirect(vkCmd->commandBuffer, vkBuffer->buffer, offset, drawCount, stride);
}

void VulkanRHI::cmdDrawIndexedIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer,
                                       uint64_t offset, uint32_t drawCount, uint32_t stride)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);

    vk.vkCmdDrawIndexedIndirect(vkCmd->commandBuffer, vkBuffer->buffer, offset, drawCount, stride);
}

void VulkanRHI::cmdDispatch(RHICommandBufferHandle cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    vk.vkCmdDispatch(vkCmd->commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void VulkanRHI::cmdDispatchIndirect(RHICommandBufferHandle cmd, RHIBufferHandle buffer, uint64_t offset)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);

    vk.vkCmdDispatchIndirect(vkCmd->commandBuffer, vkBuffer->buffer, offset);
}

void VulkanRHI::cmdPipelineBarrier(RHICommandBufferHandle cmd,
                                   std::span<const RHIBufferBarrier> bufferBarriers,
                                   std::span<const RHITextureBarrier> textureBarriers)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    std::vector<VkBufferMemoryBarrier> vkBufferBarriers;
    std::vector<VkImageMemoryBarrier> vkImageBarriers;

    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;

    for (const auto& barrier : bufferBarriers) {
        auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(barrier.buffer);
        auto srcInfo = toVkResourceState(barrier.srcState);
        auto dstInfo = toVkResourceState(barrier.dstState);

        VkBufferMemoryBarrier vkBarrier = {};
        vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vkBarrier.srcAccessMask = srcInfo.accessMask;
        vkBarrier.dstAccessMask = dstInfo.accessMask;
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.buffer = vkBuffer->buffer;
        vkBarrier.offset = barrier.offset;
        vkBarrier.size = barrier.size == 0 ? VK_WHOLE_SIZE : barrier.size;

        vkBufferBarriers.push_back(vkBarrier);
        srcStageMask |= srcInfo.stageMask;
        dstStageMask |= dstInfo.stageMask;
    }

    for (const auto& barrier : textureBarriers) {
        auto vkTexture = std::static_pointer_cast<VulkanTexture>(barrier.texture);
        bool isDepth = isDepthFormat(vkTexture->format);
        auto srcInfo = toVkResourceState(barrier.srcState, isDepth);
        auto dstInfo = toVkResourceState(barrier.dstState, isDepth);

        VkImageMemoryBarrier vkBarrier = {};
        vkBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vkBarrier.srcAccessMask = srcInfo.accessMask;
        vkBarrier.dstAccessMask = dstInfo.accessMask;
        vkBarrier.oldLayout = srcInfo.imageLayout;
        vkBarrier.newLayout = dstInfo.imageLayout;
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.image = vkTexture->image;
        vkBarrier.subresourceRange.aspectMask = vkTexture->aspectMask;
        vkBarrier.subresourceRange.baseMipLevel = barrier.baseMipLevel;
        vkBarrier.subresourceRange.levelCount = barrier.mipLevelCount;
        vkBarrier.subresourceRange.baseArrayLayer = barrier.baseArrayLayer;
        vkBarrier.subresourceRange.layerCount = barrier.arrayLayerCount;

        vkImageBarriers.push_back(vkBarrier);
        srcStageMask |= srcInfo.stageMask;
        dstStageMask |= dstInfo.stageMask;

        // Update tracked state
        vkTexture->layout = dstInfo.imageLayout;
        vkTexture->currentState = barrier.dstState;
    }

    if (srcStageMask == 0) srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (dstStageMask == 0) dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    vk.vkCmdPipelineBarrier(vkCmd->commandBuffer,
                            srcStageMask, dstStageMask,
                            0,
                            0, nullptr,
                            static_cast<uint32_t>(vkBufferBarriers.size()), vkBufferBarriers.data(),
                            static_cast<uint32_t>(vkImageBarriers.size()), vkImageBarriers.data());
}

void VulkanRHI::cmdCopyBuffer(RHICommandBufferHandle cmd, RHIBufferHandle src, RHIBufferHandle dst,
                              uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkSrc = std::static_pointer_cast<VulkanBuffer>(src);
    auto vkDst = std::static_pointer_cast<VulkanBuffer>(dst);

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;

    vk.vkCmdCopyBuffer(vkCmd->commandBuffer, vkSrc->buffer, vkDst->buffer, 1, &copyRegion);
}

void VulkanRHI::cmdCopyBufferToTexture(RHICommandBufferHandle cmd, RHIBufferHandle src, RHITextureHandle dst,
                                       uint64_t bufferOffset, uint32_t mipLevel, uint32_t arrayLayer)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkSrc = std::static_pointer_cast<VulkanBuffer>(src);
    auto vkDst = std::static_pointer_cast<VulkanTexture>(dst);

    VkBufferImageCopy region = {};
    region.bufferOffset = bufferOffset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vkDst->aspectMask;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = arrayLayer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {vkDst->extent.width >> mipLevel, vkDst->extent.height >> mipLevel, vkDst->extent.depth};

    vk.vkCmdCopyBufferToImage(vkCmd->commandBuffer, vkSrc->buffer, vkDst->image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void VulkanRHI::cmdCopyTextureToBuffer(RHICommandBufferHandle cmd, RHITextureHandle src, RHIBufferHandle dst,
                                       uint32_t mipLevel, uint32_t arrayLayer, uint64_t bufferOffset)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkSrc = std::static_pointer_cast<VulkanTexture>(src);
    auto vkDst = std::static_pointer_cast<VulkanBuffer>(dst);

    VkBufferImageCopy region = {};
    region.bufferOffset = bufferOffset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vkSrc->aspectMask;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = arrayLayer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {vkSrc->extent.width >> mipLevel, vkSrc->extent.height >> mipLevel, vkSrc->extent.depth};

    vk.vkCmdCopyImageToBuffer(vkCmd->commandBuffer, vkSrc->image,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkDst->buffer, 1, &region);
}

void VulkanRHI::cmdPushConstants(RHICommandBufferHandle cmd, RHIPipelineHandle pipeline,
                                 RHIShaderStage stages, uint32_t offset, uint32_t size, const void* data)
{
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);

    vk.vkCmdPushConstants(vkCmd->commandBuffer, vkPipeline->pipelineLayout,
                          toVkShaderStageFlags(stages), offset, size, data);
}

void VulkanRHI::cmdBeginDebugLabel(RHICommandBufferHandle cmd, const char* label, float color[4])
{
    if (!m_debugMarkersEnabled || !vk.vkCmdBeginDebugUtilsLabelEXT) return;

    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    VkDebugUtilsLabelEXT labelInfo = {};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = label;
    if (color) {
        std::memcpy(labelInfo.color, color, sizeof(float) * 4);
    } else {
        labelInfo.color[0] = 1.0f;
        labelInfo.color[1] = 1.0f;
        labelInfo.color[2] = 1.0f;
        labelInfo.color[3] = 1.0f;
    }

    vk.vkCmdBeginDebugUtilsLabelEXT(vkCmd->commandBuffer, &labelInfo);
}

void VulkanRHI::cmdEndDebugLabel(RHICommandBufferHandle cmd)
{
    if (!m_debugMarkersEnabled || !vk.vkCmdEndDebugUtilsLabelEXT) return;

    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    vk.vkCmdEndDebugUtilsLabelEXT(vkCmd->commandBuffer);
}

void VulkanRHI::cmdInsertDebugLabel(RHICommandBufferHandle cmd, const char* label, float color[4])
{
    if (!m_debugMarkersEnabled || !vk.vkCmdInsertDebugUtilsLabelEXT) return;

    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);

    VkDebugUtilsLabelEXT labelInfo = {};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = label;
    if (color) {
        std::memcpy(labelInfo.color, color, sizeof(float) * 4);
    }

    vk.vkCmdInsertDebugUtilsLabelEXT(vkCmd->commandBuffer, &labelInfo);
}

// ============================================================================
// Queue Submission
// ============================================================================

void VulkanRHI::queueSubmit(RHIQueueHandle queue, const SubmitInfo& submitInfo)
{
    auto vkQueue = std::static_pointer_cast<VulkanQueue>(queue);

    std::vector<VkCommandBuffer> commandBuffers;
    for (const auto& cmd : submitInfo.commandBuffers) {
        auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
        commandBuffers.push_back(vkCmd->commandBuffer);
    }

    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;
    for (const auto& sem : submitInfo.waitSemaphores) {
        auto vkSem = std::static_pointer_cast<VulkanSemaphore>(sem);
        waitSemaphores.push_back(vkSem->semaphore);
        waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    std::vector<VkSemaphore> signalSemaphores;
    for (const auto& sem : submitInfo.signalSemaphores) {
        auto vkSem = std::static_pointer_cast<VulkanSemaphore>(sem);
        signalSemaphores.push_back(vkSem->semaphore);
    }

    VkFence fence = VK_NULL_HANDLE;
    if (submitInfo.fence) {
        auto vkFence = std::static_pointer_cast<VulkanFence>(submitInfo.fence);
        fence = vkFence->fence;
    }

    VkSubmitInfo vkSubmitInfo = {};
    vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    vkSubmitInfo.pWaitSemaphores = waitSemaphores.data();
    vkSubmitInfo.pWaitDstStageMask = waitStages.data();
    vkSubmitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    vkSubmitInfo.pCommandBuffers = commandBuffers.data();
    vkSubmitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    vkSubmitInfo.pSignalSemaphores = signalSemaphores.data();

    std::lock_guard<std::mutex> lock(vkQueue->submitMutex);
    VK_CHECK(vk.vkQueueSubmit(vkQueue->queue, 1, &vkSubmitInfo, fence));
}

void VulkanRHI::queueWaitIdle(RHIQueueHandle queue)
{
    auto vkQueue = std::static_pointer_cast<VulkanQueue>(queue);
    vk.vkQueueWaitIdle(vkQueue->queue);
}

// ============================================================================
// SwapChain Present
// ============================================================================

RHI::AcquireResult VulkanRHI::acquireNextImage(RHISwapChainHandle swapChain, RHISemaphoreHandle semaphore,
                                               RHIFenceHandle fence, uint64_t timeout, uint32_t* imageIndex)
{
    auto vkSwapchain = std::static_pointer_cast<VulkanSwapChain>(swapChain);
    VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    VkFence vkFence = VK_NULL_HANDLE;

    if (semaphore) {
        auto sem = std::static_pointer_cast<VulkanSemaphore>(semaphore);
        vkSemaphore = sem->semaphore;
    }
    if (fence) {
        auto f = std::static_pointer_cast<VulkanFence>(fence);
        vkFence = f->fence;
    }

    VkResult result = vk.vkAcquireNextImageKHR(m_device, vkSwapchain->swapchain, timeout,
                                               vkSemaphore, vkFence, imageIndex);

    // Only update currentImageIndex on success
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        vkSwapchain->currentImageIndex = *imageIndex;
    }

    switch (result) {
        case VK_SUCCESS:                  return AcquireResult::Success;
        case VK_TIMEOUT:                  return AcquireResult::Timeout;
        case VK_SUBOPTIMAL_KHR:           return AcquireResult::Suboptimal;
        case VK_ERROR_OUT_OF_DATE_KHR:    return AcquireResult::OutOfDate;
        case VK_ERROR_SURFACE_LOST_KHR:   return AcquireResult::SurfaceLost;
        default:
            LOG_ERROR("acquireNextImage failed with VkResult: {}", static_cast<int>(result));
            return AcquireResult::Error;
    }
}

bool VulkanRHI::queuePresent(RHIQueueHandle queue, const PresentInfo& presentInfo)
{
    auto vkQueue = std::static_pointer_cast<VulkanQueue>(queue);
    auto vkSwapchain = std::static_pointer_cast<VulkanSwapChain>(presentInfo.swapChain);

    std::vector<VkSemaphore> waitSemaphores;
    for (const auto& sem : presentInfo.waitSemaphores) {
        auto vkSem = std::static_pointer_cast<VulkanSemaphore>(sem);
        waitSemaphores.push_back(vkSem->semaphore);
    }

    VkPresentInfoKHR vkPresentInfo = {};
    vkPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    vkPresentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    vkPresentInfo.pWaitSemaphores = waitSemaphores.data();
    vkPresentInfo.swapchainCount = 1;
    vkPresentInfo.pSwapchains = &vkSwapchain->swapchain;
    vkPresentInfo.pImageIndices = &presentInfo.imageIndex;

    std::lock_guard<std::mutex> lock(vkQueue->submitMutex);
    VkResult result = vk.vkQueuePresentKHR(vkQueue->queue, &vkPresentInfo);

    m_frameIndex++;

    return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

// ============================================================================
// Utility
// ============================================================================

void VulkanRHI::waitIdle()
{
    if (m_device != VK_NULL_HANDLE) {
        vk.vkDeviceWaitIdle(m_device);
    }
}

bool VulkanRHI::isDepthFormat(RHIFormat format) const
{
    switch (format) {
        case RHIFormat::D16_UNORM:
        case RHIFormat::D24_UNORM_S8_UINT:
        case RHIFormat::D32_FLOAT:
        case RHIFormat::D32_FLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

bool VulkanRHI::isStencilFormat(RHIFormat format) const
{
    switch (format) {
        case RHIFormat::D24_UNORM_S8_UINT:
        case RHIFormat::D32_FLOAT_S8_UINT:
            return true;
        default:
            return false;
    }
}

} // namespace vesper
