#include "render_system.h"
#include "camera.h"
#include "mesh.h"
#include "mesh_primitives.h"
#include "model.h"
#include "texture.h"
#include "texture_manager.h"
#include "model_loader.h"
#include "shader_reflector.h"

#include "runtime/function/window/window_system.h"
#include "runtime/platform/input/input_system.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/math/matrix4x4.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <filesystem>
#include <cmath>

namespace vesper
{

RenderSystem::RenderSystem()  = default;
RenderSystem::~RenderSystem() = default;

bool RenderSystem::initialize(const RenderSystemConfig& config)
{
    if (m_initialized)
    {
        LOG_WARN("RenderSystem already initialized");
        return true;
    }

    m_windowSystem   = config.windowSystem;
    m_workerPool     = config.workerPool;
    m_framesInFlight = config.framesInFlight;

    if (!m_windowSystem)
    {
        LOG_ERROR("RenderSystem: WindowSystem is required for initialization");
        return false;
    }

    // Initialize RHI backend
    if (!initializeRHI(config))
    {
        LOG_ERROR("RenderSystem: Failed to initialize RHI");
        return false;
    }

    // Get graphics queue
    m_graphicsQueue = m_rhi->getQueue(RHIQueueType::Graphics);
    if (!m_graphicsQueue)
    {
        LOG_ERROR("RenderSystem: Failed to get graphics queue");
        return false;
    }

    // Create swapchain
    GLFWwindow* window = m_windowSystem->getNativeWindow();
    uint32_t width  = m_windowSystem->getWidth();
    uint32_t height = m_windowSystem->getHeight();

    if (!createSwapChain(window, width, height, config.presentMode))
    {
        LOG_ERROR("RenderSystem: Failed to create swapchain");
        return false;
    }

    // Create depth buffer
    if (!createDepthResources())
    {
        LOG_ERROR("RenderSystem: Failed to create depth resources");
        return false;
    }

    // Create per-frame resources
    if (!createFrameResources())
    {
        LOG_ERROR("RenderSystem: Failed to create frame resources");
        return false;
    }

    // Register window resize callback
    m_windowSystem->registerFramebufferSizeCallback(
        [this](int width, int height) {
            onWindowResize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
    );

    // Create minimal validation resources (triangle)
    if (!createMinimalResources())
    {
        LOG_ERROR("RenderSystem: Failed to create minimal validation resources");
        return false;
    }

    // Initialize texture manager
    m_textureManager = std::make_unique<TextureManager>();
    if (!m_textureManager->initialize(m_rhi.get(), m_workerPool))
    {
        LOG_ERROR("RenderSystem: Failed to initialize TextureManager");
        return false;
    }

    // Initialize model loader
    m_modelLoader = std::make_unique<ModelLoader>();
    if (!m_modelLoader->initialize(m_rhi.get(), m_textureManager.get(), m_workerPool))
    {
        LOG_ERROR("RenderSystem: Failed to initialize ModelLoader");
        return false;
    }

    // Load test model and create model pipeline
    if (!createModelResources())
    {
        LOG_WARN("RenderSystem: Failed to create model resources (model rendering disabled)");
        // Not a fatal error - we can still render the cube
    }

    m_initialized = true;
    LOG_INFO("RenderSystem: Initialized successfully");
    LOG_INFO("RenderSystem: GPU = {}", m_rhi->getGpuInfo().deviceName);
    LOG_INFO("RenderSystem: SwapChain = {}x{}, {} images",
             m_swapChainWidth, m_swapChainHeight, m_rhi->getSwapChainImageCount(m_swapChain));

    return true;
}

void RenderSystem::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    LOG_INFO("RenderSystem: Shutting down...");

    // Wait for GPU to finish all work
    if (m_rhi)
    {
        m_rhi->waitIdle();
    }

    // Shutdown model loader
    if (m_modelLoader)
    {
        m_modelLoader->shutdown();
        m_modelLoader.reset();
    }

    // Shutdown texture manager
    if (m_textureManager)
    {
        m_textureManager->shutdown();
        m_textureManager.reset();
    }

    // Destroy model resources
    destroyModelResources();

    // Destroy minimal validation resources
    destroyMinimalResources();

    // Destroy per-frame resources
    for (auto& frame : m_frameResources)
    {
        if (frame.commandPool)
        {
            m_rhi->freeCommandBuffer(frame.commandPool, frame.commandBuffer);
            m_rhi->destroyCommandPool(frame.commandPool);
        }
        if (frame.inFlightFence)
        {
            m_rhi->destroyFence(frame.inFlightFence);
        }
        if (frame.imageAvailableSemaphore)
        {
            m_rhi->destroySemaphore(frame.imageAvailableSemaphore);
        }
        if (frame.renderFinishedSemaphore)
        {
            m_rhi->destroySemaphore(frame.renderFinishedSemaphore);
        }
    }
    m_frameResources.clear();

    // Destroy depth texture
    if (m_depthTexture)
    {
        m_rhi->destroyTexture(m_depthTexture);
        m_depthTexture = nullptr;
    }

    // Destroy swapchain
    if (m_swapChain)
    {
        m_rhi->destroySwapChain(m_swapChain);
        m_swapChain = nullptr;
    }

    // Shutdown RHI
    if (m_rhi)
    {
        m_rhi->shutdown();
        m_rhi.reset();
    }

    m_initialized = false;
    LOG_INFO("RenderSystem: Shutdown complete");
}

void RenderSystem::tick(float deltaTime)
{
    if (!m_initialized || m_minimized)
    {
        return;
    }

    // Update rotation time for cube animation
    m_rotationTime += deltaTime;

    // Handle swapchain resize if needed
    if (m_swapChainNeedsResize)
    {
        recreateSwapChain();
        m_swapChainNeedsResize = false;
        return;
    }

    // Begin frame - acquire swapchain image
    uint32_t imageIndex = 0;
    if (!beginFrame(imageIndex))
    {
        return;
    }

    // Get current frame resources
    FrameResources& frame = m_frameResources[m_currentFrame];

    // Reset and begin command buffer
    m_rhi->resetCommandPool(frame.commandPool);
    m_rhi->beginCommandBuffer(frame.commandBuffer);

    // Record rendering commands
    recordCommands(frame.commandBuffer, imageIndex);

    // End command buffer
    m_rhi->endCommandBuffer(frame.commandBuffer);

    // Submit and present
    endFrame(imageIndex);

    // Advance to next frame
    m_currentFrame = (m_currentFrame + 1) % m_framesInFlight;
}

void RenderSystem::onWindowResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_minimized = true;
        return;
    }

    m_minimized = false;
    m_swapChainNeedsResize = true;
}

// =============================================================================
// Initialization Helpers
// =============================================================================

bool RenderSystem::initializeRHI(const RenderSystemConfig& config)
{
    m_rhi = createRHI(RHIBackendType::Vulkan);
    if (!m_rhi)
    {
        LOG_ERROR("RenderSystem: Failed to create RHI backend");
        return false;
    }

    RHIConfig rhiConfig{};
    rhiConfig.applicationName       = "VesperEngine";
    rhiConfig.applicationVersion    = 1;
    rhiConfig.enableValidation      = config.enableValidation;
    rhiConfig.enableGpuDebugMarkers = config.enableDebugMarkers;
    rhiConfig.preferredGpuIndex     = config.preferredGpuIndex;

    if (!m_rhi->initialize(rhiConfig))
    {
        LOG_ERROR("RenderSystem: Failed to initialize RHI");
        return false;
    }

    return true;
}

bool RenderSystem::createSwapChain(GLFWwindow* window, uint32_t width, uint32_t height, RHIPresentMode presentMode)
{
    RHISwapChainDesc desc{};
    desc.windowHandle = window;
    desc.width        = width;
    desc.height       = height;
    desc.imageCount   = 3;  // Triple buffering
    desc.format       = RHIFormat::BGRA8_SRGB;
    desc.presentMode  = presentMode;
    desc.vsync        = (presentMode == RHIPresentMode::Fifo || presentMode == RHIPresentMode::FifoRelaxed);
    desc.debugName    = "MainSwapChain";

    m_swapChain = m_rhi->createSwapChain(desc);
    if (!m_swapChain)
    {
        return false;
    }

    m_swapChainWidth  = m_swapChain->width;
    m_swapChainHeight = m_swapChain->height;

    return true;
}

bool RenderSystem::createFrameResources()
{
    m_frameResources.resize(m_framesInFlight);

    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
        FrameResources& frame = m_frameResources[i];

        // Create command pool
        RHICommandPoolDesc poolDesc{};
        poolDesc.queueType     = RHIQueueType::Graphics;
        poolDesc.transient     = true;
        poolDesc.resetCommands = true;

        frame.commandPool = m_rhi->createCommandPool(poolDesc);
        if (!frame.commandPool)
        {
            LOG_ERROR("RenderSystem: Failed to create command pool for frame {}", i);
            return false;
        }

        // Allocate command buffer
        frame.commandBuffer = m_rhi->allocateCommandBuffer(frame.commandPool);
        if (!frame.commandBuffer)
        {
            LOG_ERROR("RenderSystem: Failed to allocate command buffer for frame {}", i);
            return false;
        }

        // Create fence (signaled initially so first wait succeeds)
        frame.inFlightFence = m_rhi->createFence(true);
        if (!frame.inFlightFence)
        {
            LOG_ERROR("RenderSystem: Failed to create fence for frame {}", i);
            return false;
        }

        // Create semaphores
        frame.imageAvailableSemaphore = m_rhi->createSemaphore();
        if (!frame.imageAvailableSemaphore)
        {
            LOG_ERROR("RenderSystem: Failed to create image available semaphore for frame {}", i);
            return false;
        }

        frame.renderFinishedSemaphore = m_rhi->createSemaphore();
        if (!frame.renderFinishedSemaphore)
        {
            LOG_ERROR("RenderSystem: Failed to create render finished semaphore for frame {}", i);
            return false;
        }
    }

    return true;
}

bool RenderSystem::createDepthResources()
{
    RHITextureDesc depthDesc{};
    depthDesc.extent      = {m_swapChainWidth, m_swapChainHeight, 1};
    depthDesc.format      = RHIFormat::D32_FLOAT;
    depthDesc.dimension   = RHITextureDimension::Tex2D;
    depthDesc.usage       = RHITextureUsage::DepthStencil;
    depthDesc.memoryUsage = RHIMemoryUsage::GpuOnly;
    depthDesc.debugName   = "DepthBuffer";

    m_depthTexture = m_rhi->createTexture(depthDesc);
    if (!m_depthTexture)
    {
        LOG_ERROR("RenderSystem: Failed to create depth texture");
        return false;
    }

    return true;
}

void RenderSystem::destroySwapChainResources()
{
    // Destroy depth texture
    if (m_depthTexture)
    {
        m_rhi->destroyTexture(m_depthTexture);
        m_depthTexture = nullptr;
    }
}

void RenderSystem::recreateSwapChain()
{
    // Wait for GPU
    m_rhi->waitIdle();

    // Get new dimensions
    uint32_t width  = m_windowSystem->getWidth();
    uint32_t height = m_windowSystem->getHeight();

    if (width == 0 || height == 0)
    {
        m_minimized = true;
        return;
    }

    LOG_INFO("RenderSystem: Recreating swapchain {}x{}", width, height);

    // Destroy old resources
    destroySwapChainResources();

    // Resize swapchain
    m_rhi->resizeSwapChain(m_swapChain, width, height);

    // Check if swapchain recreation succeeded
    if (m_swapChain->width == 0 || m_swapChain->height == 0)
    {
        LOG_WARN("RenderSystem: Swapchain recreation resulted in zero size, will retry later");
        m_minimized = true;
        return;
    }

    m_swapChainWidth  = m_swapChain->width;
    m_swapChainHeight = m_swapChain->height;
    m_minimized = false;

    // Recreate depth buffer
    createDepthResources();
}

// =============================================================================
// Frame Rendering
// =============================================================================

bool RenderSystem::beginFrame(uint32_t& imageIndex)
{
    FrameResources& frame = m_frameResources[m_currentFrame];

    // Wait for this frame's previous work to complete
    m_rhi->waitForFence(frame.inFlightFence);

    // Acquire next swapchain image
    auto result = m_rhi->acquireNextImage(
        m_swapChain,
        frame.imageAvailableSemaphore,
        nullptr,
        UINT64_MAX,
        &imageIndex
    );

    if (result == RHI::AcquireResult::OutOfDate || result == RHI::AcquireResult::SurfaceLost)
    {
        // Swapchain needs to be recreated
        m_swapChainNeedsResize = true;
        if (result == RHI::AcquireResult::SurfaceLost)
        {
            LOG_WARN("Surface lost, will recreate swapchain");
        }
        return false;
    }
    else if (result == RHI::AcquireResult::Error)
    {
        LOG_ERROR("RenderSystem: Failed to acquire swapchain image");
        return false;
    }

    // Reset fence for this frame
    m_rhi->resetFence(frame.inFlightFence);

    return true;
}

void RenderSystem::recordCommands(RHICommandBufferHandle cmd, uint32_t imageIndex)
{
    // Get swapchain image for this frame
    RHITextureHandle swapChainImage = m_rhi->getSwapChainImage(m_swapChain, imageIndex);

    // Transition resources for rendering
    {
        std::vector<RHITextureBarrier> barriers;

        // Transition swapchain image to render target
        // Use tracked currentState (Undefined on first frame, Present on subsequent frames)
        RHITextureBarrier colorBarrier{};
        colorBarrier.texture         = swapChainImage;
        colorBarrier.srcState        = swapChainImage->currentState;
        colorBarrier.dstState        = RHIResourceState::RenderTarget;
        colorBarrier.baseMipLevel    = 0;
        colorBarrier.mipLevelCount   = 1;
        colorBarrier.baseArrayLayer  = 0;
        colorBarrier.arrayLayerCount = 1;
        barriers.push_back(colorBarrier);

        // Transition depth texture to DepthWrite
        // We clear each frame so treat as Undefined -> DepthWrite
        RHITextureBarrier depthBarrier{};
        depthBarrier.texture         = m_depthTexture;
        depthBarrier.srcState        = RHIResourceState::Undefined;
        depthBarrier.dstState        = RHIResourceState::DepthWrite;
        depthBarrier.baseMipLevel    = 0;
        depthBarrier.mipLevelCount   = 1;
        depthBarrier.baseArrayLayer  = 0;
        depthBarrier.arrayLayerCount = 1;
        barriers.push_back(depthBarrier);

        m_rhi->cmdPipelineBarrier(cmd, {}, barriers);
    }

    // Setup rendering info
    RHIRenderingAttachmentInfo colorAttachment{};
    colorAttachment.texture    = swapChainImage;
    colorAttachment.loadOp     = RHILoadOp::Clear;
    colorAttachment.storeOp    = RHIStoreOp::Store;
    colorAttachment.clearValue = RHIClearValue::Color(0.1f, 0.1f, 0.15f, 1.0f);

    RHIRenderingAttachmentInfo depthAttachment{};
    depthAttachment.texture    = m_depthTexture;
    depthAttachment.loadOp     = RHILoadOp::Clear;
    depthAttachment.storeOp    = RHIStoreOp::DontCare;
    depthAttachment.clearValue = RHIClearValue::DepthStencil(1.0f, 0);

    RHIRenderingInfo renderingInfo{};
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = {m_swapChainWidth, m_swapChainHeight};
    renderingInfo.layerCount        = 1;
    renderingInfo.colorAttachments.push_back(colorAttachment);
    renderingInfo.depthAttachment   = &depthAttachment;

    // Begin dynamic rendering
    m_rhi->cmdBeginRendering(cmd, renderingInfo);

    // Set viewport and scissor
    RHIViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_swapChainWidth);
    viewport.height   = static_cast<float>(m_swapChainHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    m_rhi->cmdSetViewport(cmd, viewport);

    RHIRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_swapChainWidth, m_swapChainHeight};
    m_rhi->cmdSetScissor(cmd, scissor);

    // Update camera aspect ratio
    float aspectRatio = static_cast<float>(m_swapChainWidth) / static_cast<float>(m_swapChainHeight);
    if (m_mainCamera)
    {
        m_mainCamera->setAspectRatio(aspectRatio);
    }

    // Debug: Log rendering state once
    static bool loggedOnce = false;
    if (!loggedOnce)
    {
        LOG_INFO("RenderSystem: Render state - modelPipeline:{}, loadedModel:{}, cubePipeline:{}, cubeMesh:{}, camera:{}",
                 m_modelPipeline != nullptr,
                 m_loadedModel != nullptr,
                 m_pipeline != nullptr,
                 m_cubeMesh != nullptr,
                 m_mainCamera != nullptr);
        loggedOnce = true;
    }

    // Draw loaded model (if available)
    if (m_modelPipeline && m_loadedModel && m_mainCamera)
    {
        m_rhi->cmdBindPipeline(cmd, m_modelPipeline);

        // Build Model matrix: rotate model to stand upright (-90 degrees around X axis)
        constexpr float PI_OVER_2 = 1.5707963267948966f;
        Matrix4x4 modelMatrix = Matrix4x4::rotationX(-PI_OVER_2);

        // Get View-Projection matrix from camera
        Matrix4x4 viewMatrix = m_mainCamera->getViewMatrix();
        Matrix4x4 projMatrix = m_mainCamera->getProjectionMatrix();

        // MVP = Model * View * Projection (row-major, left-to-right multiplication)
        Matrix4x4 mvpMatrix = modelMatrix * viewMatrix * projMatrix;

        // Push constants: MVP matrix + Model matrix
        // NOTE: Using v * M order in shader (row-vector convention) to match DirectXMath
        // No transpose needed!
        struct PushConstantData {
            Matrix4x4 mvp;
            Matrix4x4 model;
        } pushData;
        pushData.mvp = mvpMatrix;
        pushData.model = modelMatrix;

        m_rhi->cmdPushConstants(cmd, m_modelPipeline, RHIShaderStage::Vertex,
                                0, sizeof(PushConstantData), &pushData);

        // Draw all submeshes with their respective materials
        for (size_t i = 0; i < m_loadedModel->getSubMeshCount(); ++i)
        {
            const auto& submesh = m_loadedModel->getSubMesh(i);
            if (submesh.mesh && submesh.mesh->isValid())
            {
                // Bind material's descriptor set if available, otherwise use fallback
                if (submesh.material && submesh.material->hasDescriptorSet())
                {
                    RHIDescriptorSetHandle descSet = submesh.material->getDescriptorSet();
                    m_rhi->cmdBindDescriptorSets(cmd, m_modelPipeline, 0, std::span(&descSet, 1));
                }
                else if (m_modelDescriptorSet)
                {
                    // Fallback to default descriptor set
                    m_rhi->cmdBindDescriptorSets(cmd, m_modelPipeline, 0, std::span(&m_modelDescriptorSet, 1));
                }

                submesh.mesh->bindAndDraw(m_rhi.get(), cmd);
            }
        }
    }
    // Fallback: Draw rotating cube if no model loaded
    else if (m_pipeline && m_cubeMesh && m_mainCamera)
    {
        m_rhi->cmdBindPipeline(cmd, m_pipeline);

        // Build Model matrix: rotate cube around Y axis
        Matrix4x4 modelMatrix = Matrix4x4::rotationY(m_rotationTime);

        // Get View-Projection matrix from camera
        Matrix4x4 viewMatrix = m_mainCamera->getViewMatrix();
        Matrix4x4 projMatrix = m_mainCamera->getProjectionMatrix();

        // MVP = Model * View * Projection (DirectXMath row-major, left-to-right)
        Matrix4x4 mvpMatrix = modelMatrix * viewMatrix * projMatrix;

        // NOTE: Using v * M order in shader (row-vector convention) to match DirectXMath
        // No transpose needed!

        // Push MVP matrix to shader
        m_rhi->cmdPushConstants(cmd, m_pipeline, RHIShaderStage::Vertex,
                                0, sizeof(Matrix4x4), &mvpMatrix);

        // Bind and draw the cube mesh
        m_cubeMesh->bindAndDraw(m_rhi.get(), cmd);
    }

    // End dynamic rendering
    m_rhi->cmdEndRendering(cmd);

    // Transition swapchain image to present
    {
        RHITextureBarrier barrier{};
        barrier.texture         = swapChainImage;
        barrier.srcState        = RHIResourceState::RenderTarget;
        barrier.dstState        = RHIResourceState::Present;
        barrier.baseMipLevel    = 0;
        barrier.mipLevelCount   = 1;
        barrier.baseArrayLayer  = 0;
        barrier.arrayLayerCount = 1;

        m_rhi->cmdPipelineBarrier(cmd, {}, std::span(&barrier, 1));
    }
}

void RenderSystem::endFrame(uint32_t imageIndex)
{
    FrameResources& frame = m_frameResources[m_currentFrame];

    // Submit command buffer
    RHI::SubmitInfo submitInfo{};
    submitInfo.commandBuffers   = std::span(&frame.commandBuffer, 1);
    submitInfo.waitSemaphores   = std::span(&frame.imageAvailableSemaphore, 1);
    submitInfo.signalSemaphores = std::span(&frame.renderFinishedSemaphore, 1);
    submitInfo.fence            = frame.inFlightFence;

    m_rhi->queueSubmit(m_graphicsQueue, submitInfo);

    // Present
    RHI::PresentInfo presentInfo{};
    presentInfo.swapChain      = m_swapChain;
    presentInfo.imageIndex     = imageIndex;
    presentInfo.waitSemaphores = std::span(&frame.renderFinishedSemaphore, 1);

    bool presentSuccess = m_rhi->queuePresent(m_graphicsQueue, presentInfo);
    if (!presentSuccess)
    {
        m_swapChainNeedsResize = true;
    }
}

// =============================================================================
// Minimal Validation Resources
// =============================================================================

namespace
{
    // Helper to load SPIR-V file
    std::vector<uint8_t> loadSpirv(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return {};
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(buffer.data()), size);

        return buffer;
    }
}

bool RenderSystem::createMinimalResources()
{
    // -------------------------------------------------------------------------
    // 1. Create Camera
    // -------------------------------------------------------------------------

    CameraConfig cameraConfig{};
    cameraConfig.fovY = 45.0f * (3.14159f / 180.0f);  // 45 degrees in radians
    cameraConfig.nearPlane = 0.1f;
    cameraConfig.farPlane = 100.0f;
    cameraConfig.projection = CameraProjection::Perspective;

    m_mainCamera = std::make_shared<Camera>(cameraConfig);

    // Position camera to see the car from the side
    m_mainCamera->setPosition(Vector3(0.0f, 1.0f, 3.0f));
    m_mainCamera->lookAt(Vector3(0.0f, 0.5f, 0.0f));

    float aspectRatio = static_cast<float>(m_swapChainWidth) / static_cast<float>(m_swapChainHeight);
    m_mainCamera->setAspectRatio(aspectRatio);

    LOG_INFO("RenderSystem: Created main camera at (2, 2, -4)");

    // -------------------------------------------------------------------------
    // 2. Create Cube Mesh
    // -------------------------------------------------------------------------

    MeshData cubeMeshData = createColorCubeMesh(1.0f);
    m_cubeMesh = Mesh::create(m_rhi.get(), cubeMeshData, "ColorCube");
    if (!m_cubeMesh)
    {
        LOG_ERROR("RenderSystem: Failed to create cube mesh");
        return false;
    }

    LOG_INFO("RenderSystem: Created color cube mesh ({} vertices, {} indices)",
             m_cubeMesh->getVertexCount(), m_cubeMesh->getIndexCount());

    // -------------------------------------------------------------------------
    // 3. Load and create shaders
    // -------------------------------------------------------------------------

    // Find shader directory (relative to executable or source)
    std::filesystem::path shaderDir;

    // Try several possible paths (covers various build configurations)
    std::vector<std::filesystem::path> possiblePaths = {
        "Engine/shader/generated/spv",
        "../Engine/shader/generated/spv",
        "../../Engine/shader/generated/spv",
        "../../../Engine/shader/generated/spv",
        "../../../../Engine/shader/generated/spv",
        "../../../../../Engine/shader/generated/spv",
    };

    for (const auto& path : possiblePaths)
    {
        if (std::filesystem::exists(path / "minimal.vert.spv"))
        {
            shaderDir = path;
            break;
        }
    }

    if (shaderDir.empty())
    {
        LOG_ERROR("RenderSystem: Could not find shader directory");
        return false;
    }

    LOG_INFO("RenderSystem: Found shaders in {}", shaderDir.string());

    // Load vertex shader
    auto vsCode = loadSpirv(shaderDir / "minimal.vert.spv");
    if (vsCode.empty())
    {
        LOG_ERROR("RenderSystem: Failed to load minimal.vert.spv");
        return false;
    }

    RHIShaderDesc vsDesc{};
    vsDesc.code      = vsCode.data();
    vsDesc.codeSize  = vsCode.size();
    vsDesc.stage     = RHIShaderStage::Vertex;
    vsDesc.entryPoint = "main";  // Slang compiles all entry points to "main" in SPIR-V
    vsDesc.debugName = "MinimalVertexShader";

    m_vertexShader = m_rhi->createShader(vsDesc);
    if (!m_vertexShader)
    {
        LOG_ERROR("RenderSystem: Failed to create vertex shader");
        return false;
    }

    // Load fragment shader
    auto fsCode = loadSpirv(shaderDir / "minimal.frag.spv");
    if (fsCode.empty())
    {
        LOG_ERROR("RenderSystem: Failed to load minimal.frag.spv");
        return false;
    }

    RHIShaderDesc fsDesc{};
    fsDesc.code      = fsCode.data();
    fsDesc.codeSize  = fsCode.size();
    fsDesc.stage     = RHIShaderStage::Fragment;
    fsDesc.entryPoint = "main";  // Slang compiles all entry points to "main" in SPIR-V
    fsDesc.debugName = "MinimalFragmentShader";

    m_fragmentShader = m_rhi->createShader(fsDesc);
    if (!m_fragmentShader)
    {
        LOG_ERROR("RenderSystem: Failed to create fragment shader");
        return false;
    }

    LOG_INFO("RenderSystem: Loaded minimal shaders successfully");

    // -------------------------------------------------------------------------
    // 4. Create graphics pipeline
    // -------------------------------------------------------------------------

    RHIGraphicsPipelineDesc pipelineDesc{};

    // Shaders
    pipelineDesc.shaders.push_back(m_vertexShader);
    pipelineDesc.shaders.push_back(m_fragmentShader);

    // Vertex input layout - use layout from the cube mesh
    pipelineDesc.vertexInput = m_cubeMesh->getVertexLayout();

    // Topology
    pipelineDesc.topology = RHIPrimitiveTopology::TriangleList;

    // Push constants for MVP matrix (64 bytes = 4x4 float matrix)
    RHIPushConstantRange pushConstantRange{};
    pushConstantRange.stages = RHIShaderStage::Vertex;
    pushConstantRange.offset = 0;
    pushConstantRange.size   = sizeof(float) * 16;  // 4x4 matrix
    pipelineDesc.pushConstantRanges.push_back(pushConstantRange);

    // Rasterization - disable culling for now to ensure visibility
    pipelineDesc.rasterization.cullMode = RHICullMode::None;
    pipelineDesc.rasterization.frontFace = RHIFrontFace::Clockwise;
    pipelineDesc.rasterization.polygonMode = RHIPolygonMode::Fill;

    // Depth/stencil - enable for proper 3D rendering
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;
    pipelineDesc.depthStencil.depthCompareOp = RHICompareOp::Less;

    // Color blend - no blending, just write
    RHIColorBlendAttachment colorAttachment{};
    colorAttachment.blendEnable = false;
    colorAttachment.colorWriteMask = 0xF; // RGBA
    pipelineDesc.colorBlend.attachments.push_back(colorAttachment);

    // Dynamic rendering formats
    pipelineDesc.colorFormats.push_back(m_swapChain->format);
    pipelineDesc.depthFormat = RHIFormat::D32_FLOAT;

    pipelineDesc.debugName = "ColorCubePipeline";

    m_pipeline = m_rhi->createGraphicsPipeline(pipelineDesc);
    if (!m_pipeline)
    {
        LOG_ERROR("RenderSystem: Failed to create graphics pipeline");
        return false;
    }

    LOG_INFO("RenderSystem: Created 3D cube pipeline successfully");

    return true;
}

void RenderSystem::destroyMinimalResources()
{
    // Destroy pipeline
    if (m_pipeline)
    {
        m_rhi->destroyPipeline(m_pipeline);
        m_pipeline = nullptr;
    }

    // Destroy shaders
    if (m_fragmentShader)
    {
        m_rhi->destroyShader(m_fragmentShader);
        m_fragmentShader = nullptr;
    }

    if (m_vertexShader)
    {
        m_rhi->destroyShader(m_vertexShader);
        m_vertexShader = nullptr;
    }

    // Reset mesh (shared_ptr handles cleanup)
    m_cubeMesh.reset();

    // Reset camera
    m_mainCamera.reset();
}

void RenderSystem::processCameraInput(InputSystem* input, float deltaTime)
{
    if (!m_mainCamera || !input)
    {
        return;
    }

    // Speed modifiers
    float moveSpeed = 5.0f;
    float rotateSpeed = 2.0f;  // radians per second

    // Hold Left Ctrl for faster movement
    if (input->isKeyPressed(GLFW_KEY_LEFT_CONTROL))
    {
        moveSpeed *= 3.0f;
        rotateSpeed *= 2.0f;
    }

    // Hold Left Alt for slower movement
    if (input->isKeyPressed(GLFW_KEY_LEFT_ALT))
    {
        moveSpeed *= 0.2f;
        rotateSpeed *= 0.3f;
    }

    // WASD movement (using GLFW key codes)
    Vector3 moveDir(0, 0, 0);
    if (input->isKeyPressed(GLFW_KEY_W)) moveDir.z += 1.0f;  // Forward
    if (input->isKeyPressed(GLFW_KEY_S)) moveDir.z -= 1.0f;  // Backward
    if (input->isKeyPressed(GLFW_KEY_A)) moveDir.x -= 1.0f;  // Left
    if (input->isKeyPressed(GLFW_KEY_D)) moveDir.x += 1.0f;  // Right
    if (input->isKeyPressed(GLFW_KEY_SPACE)) moveDir.y += 1.0f;      // Up
    if (input->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) moveDir.y -= 1.0f; // Down

    if (moveDir.squaredLength() > 0.0f)
    {
        m_mainCamera->moveLocal(moveDir.normalized() * moveSpeed * deltaTime);
    }

    // Rotation controls
    float deltaPitch = 0.0f;
    float deltaYaw = 0.0f;

    // Q/E for yaw (left/right rotation)
    if (input->isKeyPressed(GLFW_KEY_Q)) deltaYaw += rotateSpeed * deltaTime;
    if (input->isKeyPressed(GLFW_KEY_E)) deltaYaw -= rotateSpeed * deltaTime;

    // R/F for pitch (up/down rotation)
    if (input->isKeyPressed(GLFW_KEY_R)) deltaPitch -= rotateSpeed * deltaTime;
    if (input->isKeyPressed(GLFW_KEY_F)) deltaPitch += rotateSpeed * deltaTime;

    // Arrow keys alternative
    if (input->isKeyPressed(GLFW_KEY_UP)) deltaPitch -= rotateSpeed * deltaTime;
    if (input->isKeyPressed(GLFW_KEY_DOWN)) deltaPitch += rotateSpeed * deltaTime;
    if (input->isKeyPressed(GLFW_KEY_LEFT)) deltaYaw += rotateSpeed * deltaTime;
    if (input->isKeyPressed(GLFW_KEY_RIGHT)) deltaYaw -= rotateSpeed * deltaTime;

    if (deltaPitch != 0.0f || deltaYaw != 0.0f)
    {
        m_mainCamera->rotate(deltaPitch, deltaYaw);
    }

    // Reset camera to default position (Home key) - side view of car
    if (input->isKeyDown(GLFW_KEY_HOME))
    {
        m_mainCamera->setPosition(Vector3(0.0f, 1.0f, 3.0f));
        m_mainCamera->lookAt(Vector3(0.0f, 0.5f, 0.0f));
    }
}

void RenderSystem::processPendingAssets()
{
    if (!m_initialized)
    {
        return;
    }

    // Process pending texture uploads (max 4 per frame to avoid stalls)
    if (m_textureManager)
    {
        m_textureManager->processPendingUploads(4);
    }
}

// =============================================================================
// Model Resources
// =============================================================================

bool RenderSystem::createModelResources()
{
    LOG_INFO("RenderSystem: createModelResources() starting...");

    // -------------------------------------------------------------------------
    // 1. Load Model
    // -------------------------------------------------------------------------

    // Try to find the model file
    std::vector<std::filesystem::path> modelPaths = {
        "Engine/asset/models/FINAL_MODEL_25.fbx",
        "../Engine/asset/models/FINAL_MODEL_25.fbx",
        "../../Engine/asset/models/FINAL_MODEL_25.fbx",
        "../../../Engine/asset/models/FINAL_MODEL_25.fbx",
        "../../../../Engine/asset/models/FINAL_MODEL_25.fbx",
        "../../../../../Engine/asset/models/FINAL_MODEL_25.fbx",
    };

    std::filesystem::path modelPath;
    for (const auto& path : modelPaths)
    {
        if (std::filesystem::exists(path))
        {
            modelPath = path;
            break;
        }
    }

    if (modelPath.empty())
    {
        LOG_WARN("RenderSystem: Could not find model file");
        return false;
    }

    LOG_INFO("RenderSystem: Loading model from {}", modelPath.string());

    ModelLoadOptions loadOptions;
    loadOptions.flipUVs = true;
    loadOptions.calculateTangents = true;
    loadOptions.scaleFactor = 0.01f;  // FBX often uses cm, scale to meters

    auto result = m_modelLoader->loadSync(modelPath.string(), loadOptions);
    if (!result.success)
    {
        LOG_ERROR("RenderSystem: Failed to load model: {}", result.errorMessage);
        return false;
    }

    m_loadedModel = result.model;
    LOG_INFO("RenderSystem: Loaded model '{}' with {} submeshes",
             m_loadedModel->getName(), m_loadedModel->getSubMeshCount());

    // -------------------------------------------------------------------------
    // 2. Load a texture for testing
    // -------------------------------------------------------------------------

    std::vector<std::filesystem::path> texturePaths = {
        "Engine/asset/textures/Body_ao.png",
        "../Engine/asset/textures/Body_ao.png",
        "../../Engine/asset/textures/Body_ao.png",
        "../../../Engine/asset/textures/Body_ao.png",
        "../../../../Engine/asset/textures/Body_ao.png",
        "../../../../../Engine/asset/textures/Body_ao.png",
    };

    std::filesystem::path texturePath;
    for (const auto& path : texturePaths)
    {
        if (std::filesystem::exists(path))
        {
            texturePath = path;
            break;
        }
    }

    if (!texturePath.empty())
    {
        m_modelTexture = m_textureManager->loadTextureSync(texturePath.string(), true, "ModelTexture");
        LOG_INFO("RenderSystem: Loaded texture from {}", texturePath.string());
    }
    else
    {
        m_modelTexture = m_textureManager->getDefaultWhite();
        LOG_WARN("RenderSystem: Could not find texture, using default white");
    }

    // -------------------------------------------------------------------------
    // 3. Load Model Shaders
    // -------------------------------------------------------------------------

    std::filesystem::path shaderDir;
    std::vector<std::filesystem::path> possiblePaths = {
        "Engine/shader/generated/spv",
        "../Engine/shader/generated/spv",
        "../../Engine/shader/generated/spv",
        "../../../Engine/shader/generated/spv",
        "../../../../Engine/shader/generated/spv",
        "../../../../../Engine/shader/generated/spv",
    };

    for (const auto& path : possiblePaths)
    {
        if (std::filesystem::exists(path / "model.vert.spv"))
        {
            shaderDir = path;
            break;
        }
    }

    if (shaderDir.empty())
    {
        LOG_WARN("RenderSystem: Could not find model shaders - need to rebuild after adding model.slang");
        return false;
    }

    // Load vertex shader
    auto vsCode = [](const std::filesystem::path& path) -> std::vector<uint8_t> {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return {};
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }(shaderDir / "model.vert.spv");

    if (vsCode.empty())
    {
        LOG_ERROR("RenderSystem: Failed to load model.vert.spv");
        return false;
    }

    RHIShaderDesc vsDesc{};
    vsDesc.code = vsCode.data();
    vsDesc.codeSize = vsCode.size();
    vsDesc.stage = RHIShaderStage::Vertex;
    vsDesc.entryPoint = "main";
    vsDesc.debugName = "ModelVertexShader";

    m_modelVertexShader = m_rhi->createShader(vsDesc);
    if (!m_modelVertexShader)
    {
        LOG_ERROR("RenderSystem: Failed to create model vertex shader");
        return false;
    }

    // Load fragment shader
    auto fsCode = [](const std::filesystem::path& path) -> std::vector<uint8_t> {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return {};
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }(shaderDir / "model.frag.spv");

    if (fsCode.empty())
    {
        LOG_ERROR("RenderSystem: Failed to load model.frag.spv");
        return false;
    }

    RHIShaderDesc fsDesc{};
    fsDesc.code = fsCode.data();
    fsDesc.codeSize = fsCode.size();
    fsDesc.stage = RHIShaderStage::Fragment;
    fsDesc.entryPoint = "main";
    fsDesc.debugName = "ModelFragmentShader";

    m_modelFragmentShader = m_rhi->createShader(fsDesc);
    if (!m_modelFragmentShader)
    {
        LOG_ERROR("RenderSystem: Failed to create model fragment shader");
        return false;
    }

    LOG_INFO("RenderSystem: Loaded model shaders successfully");

    // -------------------------------------------------------------------------
    // 4. Reflect Shaders and Create Descriptor Set Layout
    // -------------------------------------------------------------------------

    // Use shader reflection to automatically create descriptor set layout
    auto shaderReflection = ShaderReflector::reflectProgram(
        (shaderDir / "model.vert.spv").string(),
        (shaderDir / "model.frag.spv").string()
    );

    if (shaderReflection.bindings.empty())
    {
        LOG_WARN("RenderSystem: No bindings found in shader reflection, using manual layout");
        // Fallback to manual layout if reflection fails
        RHIDescriptorSetLayoutDesc layoutDesc{};
        layoutDesc.bindings.push_back(RHIDescriptorBinding{
            .binding = 0,
            .descriptorType = RHIDescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = RHIShaderStage::Fragment
        });
        layoutDesc.bindings.push_back(RHIDescriptorBinding{
            .binding = 1,
            .descriptorType = RHIDescriptorType::Sampler,
            .descriptorCount = 1,
            .stageFlags = RHIShaderStage::Fragment
        });
        m_modelDescriptorSetLayout = m_rhi->createDescriptorSetLayout(layoutDesc);
    }
    else
    {
        LOG_INFO("RenderSystem: Shader reflection found {} bindings, {} push constant ranges",
                 shaderReflection.bindings.size(), shaderReflection.pushConstants.size());

        // Log reflected bindings for debugging
        for (const auto& binding : shaderReflection.bindings)
        {
            LOG_DEBUG("  Binding: set={}, binding={}, name='{}', type={}",
                      binding.set, binding.binding, binding.name,
                      static_cast<int>(binding.type));
        }

        // Create descriptor set layouts from reflection
        auto layouts = ShaderReflector::createDescriptorSetLayouts(m_rhi.get(), shaderReflection);
        if (!layouts.empty())
        {
            m_modelDescriptorSetLayout = layouts[0];
        }
    }
    if (!m_modelDescriptorSetLayout)
    {
        LOG_ERROR("RenderSystem: Failed to create model descriptor set layout");
        return false;
    }

    // -------------------------------------------------------------------------
    // 5. Create Descriptor Set
    // -------------------------------------------------------------------------

    m_modelDescriptorSet = m_rhi->createDescriptorSet(m_modelDescriptorSetLayout);
    if (!m_modelDescriptorSet)
    {
        LOG_ERROR("RenderSystem: Failed to create model descriptor set");
        return false;
    }

    // Update descriptor set with texture
    std::vector<RHIDescriptorWrite> writes;

    RHIDescriptorWrite texWrite{};
    texWrite.binding = 0;
    texWrite.type = RHIDescriptorType::SampledImage;
    texWrite.texture = m_modelTexture->getTexture();
    writes.push_back(texWrite);

    RHIDescriptorWrite samplerWrite{};
    samplerWrite.binding = 1;
    samplerWrite.type = RHIDescriptorType::Sampler;
    samplerWrite.sampler = m_modelTexture->getSampler();
    writes.push_back(samplerWrite);

    m_rhi->updateDescriptorSet(m_modelDescriptorSet, writes);

    // -------------------------------------------------------------------------
    // 6. Create Model Pipeline
    // -------------------------------------------------------------------------

    RHIGraphicsPipelineDesc pipelineDesc{};

    // Shaders
    pipelineDesc.shaders.push_back(m_modelVertexShader);
    pipelineDesc.shaders.push_back(m_modelFragmentShader);

    // Vertex input layout (ModelVertex)
    pipelineDesc.vertexInput = ModelVertex::getVertexInputState();

    // Topology
    pipelineDesc.topology = RHIPrimitiveTopology::TriangleList;

    // Push constants for MVP + Model matrix (2 matrices = 128 bytes)
    RHIPushConstantRange pushConstantRange{};
    pushConstantRange.stages = RHIShaderStage::Vertex;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 16 * 2;  // Two 4x4 matrices
    pipelineDesc.pushConstantRanges.push_back(pushConstantRange);

    // Descriptor set layouts
    pipelineDesc.descriptorLayouts.push_back(m_modelDescriptorSetLayout);

    // Rasterization - disable culling for now to ensure visibility
    pipelineDesc.rasterization.cullMode = RHICullMode::None;
    pipelineDesc.rasterization.frontFace = RHIFrontFace::Clockwise;
    pipelineDesc.rasterization.polygonMode = RHIPolygonMode::Fill;

    // Depth/stencil - enable for proper 3D rendering
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;
    pipelineDesc.depthStencil.depthCompareOp = RHICompareOp::Less;

    // Color blend
    RHIColorBlendAttachment colorAttachment{};
    colorAttachment.blendEnable = false;
    colorAttachment.colorWriteMask = 0xF;
    pipelineDesc.colorBlend.attachments.push_back(colorAttachment);

    // Dynamic rendering formats
    pipelineDesc.colorFormats.push_back(m_swapChain->format);
    pipelineDesc.depthFormat = RHIFormat::D32_FLOAT;

    pipelineDesc.debugName = "ModelPipeline";

    m_modelPipeline = m_rhi->createGraphicsPipeline(pipelineDesc);
    if (!m_modelPipeline)
    {
        LOG_ERROR("RenderSystem: Failed to create model pipeline");
        return false;
    }

    LOG_INFO("RenderSystem: Created model pipeline successfully");

    // -------------------------------------------------------------------------
    // 7. Create Descriptor Sets for Each Material
    // -------------------------------------------------------------------------

    int materialDescSetCount = 0;
    for (size_t i = 0; i < m_loadedModel->getSubMeshCount(); ++i)
    {
        auto& submesh = m_loadedModel->getSubMesh(i);
        if (submesh.material)
        {
            if (submesh.material->createDescriptorSet(m_modelDescriptorSetLayout))
            {
                materialDescSetCount++;
            }
        }
    }
    LOG_INFO("RenderSystem: Created {} material descriptor sets", materialDescSetCount);

    // Position camera at car's left side (car faces X direction)
    m_mainCamera->setPosition(Vector3(0.0f, 1.0f, 3.0f));
    m_mainCamera->lookAt(Vector3(0.0f, 0.5f, 0.0f));

    return true;
}

void RenderSystem::destroyModelResources()
{
    if (m_modelPipeline)
    {
        m_rhi->destroyPipeline(m_modelPipeline);
        m_modelPipeline = nullptr;
    }

    if (m_modelDescriptorSet)
    {
        m_rhi->destroyDescriptorSet(m_modelDescriptorSet);
        m_modelDescriptorSet = nullptr;
    }

    if (m_modelDescriptorSetLayout)
    {
        m_rhi->destroyDescriptorSetLayout(m_modelDescriptorSetLayout);
        m_modelDescriptorSetLayout = nullptr;
    }

    if (m_modelFragmentShader)
    {
        m_rhi->destroyShader(m_modelFragmentShader);
        m_modelFragmentShader = nullptr;
    }

    if (m_modelVertexShader)
    {
        m_rhi->destroyShader(m_modelVertexShader);
        m_modelVertexShader = nullptr;
    }

    m_modelTexture.reset();
    m_loadedModel.reset();
}

} // namespace vesper
