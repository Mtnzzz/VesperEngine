#pragma once

#include "runtime/function/render/rhi/rhi.h"
#include "runtime/function/render/rhi/rhi_types.h"

#include <memory>
#include <vector>
#include <cstdint>

struct GLFWwindow;

namespace vesper
{

class WindowSystem;
class Camera;
class Mesh;
class Model;
class Texture;
class InputSystem;
class TextureManager;
class ModelLoader;
class WorkerPool;

/// @brief Configuration for RenderSystem initialization
struct RenderSystemConfig
{
    WindowSystem*   windowSystem        = nullptr;
    WorkerPool*     workerPool          = nullptr;  // For async texture/model loading
    bool            enableValidation    = true;
    bool            enableDebugMarkers  = true;
    uint32_t        preferredGpuIndex   = 0;
    RHIPresentMode  presentMode         = RHIPresentMode::Fifo;
    uint32_t        framesInFlight      = 3;  // Must match swapchain image count
};

/// @brief Per-frame rendering resources
struct FrameResources
{
    RHICommandPoolHandle    commandPool;
    RHICommandBufferHandle  commandBuffer;
    RHIFenceHandle          inFlightFence;
    RHISemaphoreHandle      imageAvailableSemaphore;
    RHISemaphoreHandle      renderFinishedSemaphore;
};

/// @brief Main rendering system - manages RHI, resources, and render loop
class RenderSystem
{
public:
    RenderSystem();
    ~RenderSystem();

    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    /// @brief Initialize the render system
    /// @param config Configuration parameters
    /// @return true if initialization successful
    bool initialize(const RenderSystemConfig& config);

    /// @brief Shutdown and cleanup all rendering resources
    void shutdown();

    /// @brief Execute one frame of rendering
    /// @param deltaTime Time since last frame in seconds
    void tick(float deltaTime);

    /// @brief Handle window resize
    /// @param width New window width
    /// @param height New window height
    void onWindowResize(uint32_t width, uint32_t height);

    // =========================================================================
    // Accessors
    // =========================================================================

    RHI* getRHI() const { return m_rhi.get(); }
    RHISwapChainHandle getSwapChain() const { return m_swapChain; }
    uint32_t getCurrentFrameIndex() const { return m_currentFrame; }
    uint32_t getFramesInFlight() const { return m_framesInFlight; }

    bool isInitialized() const { return m_initialized; }
    bool isMinimized() const { return m_minimized; }

    /// @brief Get the main camera
    Camera* getMainCamera() const { return m_mainCamera.get(); }

    /// @brief Get texture manager
    TextureManager* getTextureManager() const { return m_textureManager.get(); }

    /// @brief Get model loader
    ModelLoader* getModelLoader() const { return m_modelLoader.get(); }

    /// @brief Process camera input (WASD + mouse)
    /// @param input InputSystem for reading input state
    /// @param deltaTime Time since last frame
    void processCameraInput(InputSystem* input, float deltaTime);

    /// @brief Process pending async asset uploads (textures, etc.)
    /// Call this once per frame on render thread
    void processPendingAssets();

private:
    // =========================================================================
    // Initialization Helpers
    // =========================================================================

    bool initializeRHI(const RenderSystemConfig& config);
    bool createSwapChain(GLFWwindow* window, uint32_t width, uint32_t height, RHIPresentMode presentMode);
    bool createFrameResources();
    bool createDepthResources();

    void destroySwapChainResources();
    void recreateSwapChain();

    // =========================================================================
    // Frame Rendering
    // =========================================================================

    bool beginFrame(uint32_t& imageIndex);
    void recordCommands(RHICommandBufferHandle cmd, uint32_t imageIndex);
    void endFrame(uint32_t imageIndex);

private:
    // =========================================================================
    // Core Systems
    // =========================================================================

    std::unique_ptr<RHI>    m_rhi;
    WindowSystem*           m_windowSystem = nullptr;

    // =========================================================================
    // SwapChain
    // =========================================================================

    RHISwapChainHandle      m_swapChain;
    RHIQueueHandle          m_graphicsQueue;
    uint32_t                m_swapChainWidth  = 0;
    uint32_t                m_swapChainHeight = 0;

    // =========================================================================
    // Depth Buffer
    // =========================================================================

    RHITextureHandle        m_depthTexture;

    // =========================================================================
    // Per-Frame Resources
    // =========================================================================

    std::vector<FrameResources> m_frameResources;
    uint32_t                    m_framesInFlight = 3;
    uint32_t                    m_currentFrame   = 0;

    // =========================================================================
    // State
    // =========================================================================

    bool m_initialized          = false;
    bool m_minimized            = false;
    bool m_swapChainNeedsResize = false;

    // =========================================================================
    // Camera
    // =========================================================================

    std::shared_ptr<Camera> m_mainCamera;

    // =========================================================================
    // Asset Management
    // =========================================================================

    std::unique_ptr<TextureManager> m_textureManager;
    std::unique_ptr<ModelLoader>    m_modelLoader;
    WorkerPool*                     m_workerPool = nullptr;

    // =========================================================================
    // Mesh Resources
    // =========================================================================

    std::shared_ptr<Mesh>   m_cubeMesh;

    // =========================================================================
    // Pipeline Resources (Color Cube)
    // =========================================================================

    RHIShaderHandle     m_vertexShader;
    RHIShaderHandle     m_fragmentShader;
    RHIPipelineHandle   m_pipeline;
    float               m_rotationTime = 0.0f;

    // =========================================================================
    // Model Resources (Textured Model)
    // =========================================================================

    std::shared_ptr<Model>          m_loadedModel;
    std::shared_ptr<Texture>        m_modelTexture;
    RHIShaderHandle                 m_modelVertexShader;
    RHIShaderHandle                 m_modelFragmentShader;
    RHIPipelineHandle               m_modelPipeline;
    RHIDescriptorSetLayoutHandle    m_modelDescriptorSetLayout;
    RHIDescriptorSetHandle          m_modelDescriptorSet;

    bool createMinimalResources();
    void destroyMinimalResources();
    bool createModelResources();
    void destroyModelResources();
};

} // namespace vesper