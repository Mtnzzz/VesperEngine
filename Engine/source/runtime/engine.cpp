#include "runtime/engine.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/threading/worker_pool.h"
#include "runtime/core/event/event_bus.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/window/window_system.h"
#include "runtime/function/render/render_packet.h"
#include "runtime/function/render/render_system.h"
#include "runtime/platform/input/input_system.h"

#include <atomic>
#include <chrono>

// Platform-specific includes for thread naming
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace vesper {

VesperEngine::VesperEngine() = default;

VesperEngine::~VesperEngine()
{
    if (m_initialized)
    {
        shutdown();
    }
}

bool VesperEngine::initialize(const EngineConfig& config)
{
    if (m_initialized)
    {
        LOG_WARN("VesperEngine already initialized");
        return true;
    }

    LOG_INFO("===========================================");
    LOG_INFO("        Vesper Engine Starting...          ");
    LOG_INFO("===========================================");

    m_multithreadingEnabled = config.enableMultithreading;

    // Initialize threading and pipeline systems via GlobalContext
    // Pass configuration to control worker pool creation
    RuntimeSystemsConfig systemsConfig;
    systemsConfig.enableWorkerPool = config.enableMultithreading;
    systemsConfig.workerThreadCount = config.workerThreadCount;
    systemsConfig.enableHelpRun = true;

    g_runtime_global_context.startSystems(systemsConfig);

    // Create InputSystem first (it doesn't need window yet)
    m_inputSystem = std::make_unique<InputSystem>();

    // Create and initialize window system with InputSystem pointer
    m_windowSystem = std::make_unique<WindowSystem>();

    WindowCreateInfo windowInfo;
    windowInfo.width = config.windowWidth;
    windowInfo.height = config.windowHeight;
    windowInfo.title = config.windowTitle;
    windowInfo.resizable = config.resizable;
    windowInfo.fullscreen = config.fullscreen;

    // Initialize WindowSystem with InputSystem for event forwarding
    if (!m_windowSystem->initialize(windowInfo, m_inputSystem.get()))
    {
        LOG_ERROR("Failed to initialize WindowSystem");
        m_inputSystem.reset();
        g_runtime_global_context.shutdownSystems();
        return false;
    }

    LOG_INFO("Window created: {}x{}", config.windowWidth, config.windowHeight);

    // Now initialize InputSystem with the window handle
    if (!m_inputSystem->initialize(m_windowSystem->getNativeWindow()))
    {
        LOG_ERROR("Failed to initialize InputSystem");
        m_windowSystem->shutdown();
        m_windowSystem.reset();
        m_inputSystem.reset();
        g_runtime_global_context.shutdownSystems();
        return false;
    }

    // Initialize RenderSystem
    g_runtime_global_context.m_render_system = std::make_shared<RenderSystem>();

    RenderSystemConfig renderConfig{};
    renderConfig.windowSystem = m_windowSystem.get();
    renderConfig.enableValidation = true;
    renderConfig.enableDebugMarkers = true;
    renderConfig.presentMode = RHIPresentMode::Fifo;
    renderConfig.framesInFlight = 3;  // Must match swapchain image count (3 for triple buffering)

    if (!g_runtime_global_context.m_render_system->initialize(renderConfig))
    {
        LOG_ERROR("Failed to initialize RenderSystem");
        g_runtime_global_context.m_render_system.reset();
        m_inputSystem->shutdown();
        m_inputSystem.reset();
        m_windowSystem->shutdown();
        m_windowSystem.reset();
        g_runtime_global_context.shutdownSystems();
        return false;
    }

    // Register callbacks
    registerInputCallbacks();

    m_initialized = true;
    m_running.store(false, std::memory_order_release);
    m_lastFrameTime = std::chrono::steady_clock::now();

    LOG_INFO("VesperEngine initialized successfully");
    LOG_INFO("Multithreading: {}", m_multithreadingEnabled ? "enabled" : "disabled");

    return true;
}

void VesperEngine::registerInputCallbacks()
{
    // Window callbacks - publish to event bus
    m_windowSystem->registerWindowSizeCallback([this](uint32_t width, uint32_t height) {
        LOG_INFO("[Window] Resized to {}x{}", width, height);

        // Publish resize event
        if (auto* eventBus = getEventBus())
        {
            WindowResizeEvent event;
            event.width = width;
            event.height = height;
            eventBus->windowInputChannel().publish(event);
        }
    });

    m_windowSystem->registerWindowCloseCallback([this]() {
        m_running.store(false, std::memory_order_release);

        // Publish close event
        if (auto* eventBus = getEventBus())
        {
            eventBus->windowInputChannel().publish(WindowCloseEvent{});
        }
    });

    // ESC to quit
    m_inputSystem->registerKeyCallback([this](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            m_running.store(false, std::memory_order_release);
        }

        // Publish key event
        if (auto* eventBus = getEventBus())
        {
            KeyEvent event;
            event.key = key;
            event.scancode = scancode;
            event.action = action;
            event.mods = mods;
            eventBus->windowInputChannel().publish(event);
        }
    });
}

void VesperEngine::run()
{
    if (!m_initialized)
    {
        LOG_ERROR("VesperEngine not initialized, cannot run");
        return;
    }

    m_running.store(true, std::memory_order_release);

    // Register main thread as the logic thread
    ThreadContext::registerThread(ThreadType::Main);

    // Process pending window events before starting render
    // This ensures the window is fully ready for Vulkan rendering on Windows
    m_windowSystem->pollEvents();

    if (m_multithreadingEnabled)
    {
        // Start render thread
        m_renderThreadRunning.store(true, std::memory_order_release);
        m_renderThread = std::make_unique<std::thread>(&VesperEngine::renderThreadLoop, this);

        LOG_INFO("Render thread started");

        // Main/Logic thread loop
        while (m_running.load(std::memory_order_acquire) && !m_windowSystem->shouldClose())
        {
            auto now = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
            m_lastFrameTime = now;

            logicTick(deltaTime);
        }

        // Stop render thread
        m_renderThreadRunning.store(false, std::memory_order_release);
        if (m_renderThread && m_renderThread->joinable())
        {
            m_renderThread->join();
        }
        m_renderThread.reset();

        LOG_INFO("Render thread stopped");
    }
    else
    {
        // Single-threaded fallback
        while (m_running.load(std::memory_order_acquire) && !m_windowSystem->shouldClose())
        {
            tick();
        }
    }

    // Unregister main thread
    ThreadContext::unregisterThread();
}

void VesperEngine::logicTick(float deltaTime)
{
    // Reset per-frame input state
    m_inputSystem->tick();

    // Poll window events (must be on main thread for GLFW)
    m_windowSystem->pollEvents();

    // Process events from event bus
    if (auto* eventBus = getEventBus())
    {
        eventBus->processAllChannels();
    }

    // Process all pending LogicOnly tasks
    // These are tasks that must run on the main/logic thread
    if (auto* pool = getWorkerPool())
    {
        pool->processAllLogicTasks();
    }

    // Process camera input
    auto* renderSystem = g_runtime_global_context.m_render_system.get();
    if (renderSystem && renderSystem->isInitialized())
    {
        renderSystem->processCameraInput(m_inputSystem.get(), deltaTime);
    }

    // TODO: Update game logic (physics, AI, animation, etc.)
    // These could be parallelized via worker pool

    // Example of submitting parallel tasks:
    // if (auto* pool = getWorkerPool())
    // {
    //     auto wg = pool->submit([](){ /* parallel work */ });
    //     pool->waitFor(wg, true); // Wait with help-run
    // }

    // Prepare and publish render packet
    auto* packetBuffer = g_runtime_global_context.m_render_packet_buffer.get();
    if (packetBuffer)
    {
        RenderPacket* packet = packetBuffer->acquireForWrite();
        if (packet)
        {
            prepareRenderPacket(packet);
            packetBuffer->releaseWrite();
        }
    }

    ++m_frameIndex;
}

void VesperEngine::renderThreadLoop()
{
    // Register this thread as the render thread using RAII guard
    ScopedThreadRegistration threadReg(ThreadType::Render);

    // Set render thread name for debugging
#ifdef _WIN32
    std::wstring threadName = L"VesperRender";
    SetThreadDescription(GetCurrentThread(), threadName.c_str());
#endif

    LOG_INFO("Render thread: Starting loop");

    // Backoff state for when no work is available
    uint32_t idleCount = 0;
    constexpr uint32_t kSpinBeforeYield = 16;
    constexpr uint32_t kYieldBeforeSleep = 32;
    constexpr uint32_t kMaxSleepUs = 1000;

    while (m_renderThreadRunning.load(std::memory_order_acquire))
    {
        bool didWork = renderTick();

        if (didWork)
        {
            idleCount = 0;
        }
        else
        {
            ++idleCount;

            if (idleCount < kSpinBeforeYield)
            {
                // Spin-wait briefly
                std::atomic_signal_fence(std::memory_order_seq_cst);
            }
            else if (idleCount < kSpinBeforeYield + kYieldBeforeSleep)
            {
                std::this_thread::yield();
            }
            else
            {
                // Sleep with capped backoff
                uint32_t sleepUs = std::min(
                    1U << (idleCount - kSpinBeforeYield - kYieldBeforeSleep),
                    kMaxSleepUs
                );
                std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
            }
        }
    }

    LOG_INFO("Render thread: Exiting loop");
}

bool VesperEngine::renderTick()
{
    bool didWork = false;

    // Process all pending RenderOnly tasks first
    // These are tasks that must run on the render thread (GPU operations, resource management)
    if (auto* pool = getWorkerPool())
    {
        didWork = pool->processAllRenderTasks() > 0;
    }

    // Execute RenderSystem tick (GPU work)
    auto* renderSystem = g_runtime_global_context.m_render_system.get();
    if (renderSystem && renderSystem->isInitialized())
    {
        // Calculate delta time for render system
        static auto lastRenderTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastRenderTime).count();
        lastRenderTime = now;

        renderSystem->tick(deltaTime);
        didWork = true;
    }

    auto* packetBuffer = g_runtime_global_context.m_render_packet_buffer.get();
    if (!packetBuffer)
    {
        return didWork;
    }

    // Try to acquire a render packet
    RenderPacket* packet = packetBuffer->acquireForRead();
    if (!packet)
    {
        // No packet available
        return didWork;
    }

    // TODO: Process render packet
    // - Setup camera from packet->camera
    // - Render visible objects from packet->visibleObjects
    // - Process object updates (add/update/delete)
    // - Handle resource loading requests

    // Release the packet
    packetBuffer->releaseRead();

    return true;  // We did work (processed a packet)
}

void VesperEngine::tick()
{
    // Single-threaded fallback - combines logic and render
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;

    // Reset per-frame input state
    m_inputSystem->tick();

    // Poll events
    m_windowSystem->pollEvents();

    // Process events
    if (auto* eventBus = getEventBus())
    {
        eventBus->processAllChannels();
    }

    // Process camera input
    auto* renderSystem = g_runtime_global_context.m_render_system.get();
    if (renderSystem && renderSystem->isInitialized())
    {
        renderSystem->processCameraInput(m_inputSystem.get(), deltaTime);
    }

    // TODO: Update game logic

    // Render frame
    if (renderSystem && renderSystem->isInitialized())
    {
        renderSystem->tick(deltaTime);
    }

    ++m_frameIndex;
}

void VesperEngine::prepareRenderPacket(RenderPacket* packet)
{
    // Fill render packet with current frame data
    packet->frameIndex = m_frameIndex;

    // TODO: Fill camera data from game camera
    // packet->camera = ...

    // TODO: Fill visible objects from culling results
    // packet->visibleObjects = ...

    // TODO: Fill object updates from game state changes
    // packet->objectsToAdd = ...
    // packet->objectsToUpdate = ...
    // packet->objectsToDelete = ...
}

void VesperEngine::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    LOG_INFO("Shutting down VesperEngine...");

    // 1. Stop running (signals threads to exit)
    m_running.store(false, std::memory_order_release);
    m_renderThreadRunning.store(false, std::memory_order_release);

    // 2. Wait for render thread to finish
    if (m_renderThread && m_renderThread->joinable())
    {
        m_renderThread->join();
    }
    m_renderThread.reset();

    // 3. Shutdown render system (must be before window system)
    if (g_runtime_global_context.m_render_system)
    {
        g_runtime_global_context.m_render_system->shutdown();
        g_runtime_global_context.m_render_system.reset();
    }

    // 4. Shutdown input system
    if (m_inputSystem)
    {
        m_inputSystem->shutdown();
        m_inputSystem.reset();
    }

    // 5. Shutdown window system (after render system stopped)
    if (m_windowSystem)
    {
        m_windowSystem->shutdown();
        m_windowSystem.reset();
    }

    // 6. Shutdown threading and pipeline systems
    g_runtime_global_context.shutdownSystems();

    m_initialized = false;

    LOG_INFO("===========================================");
    LOG_INFO("      Vesper Engine Shutdown Complete      ");
    LOG_INFO("===========================================");
}

WorkerPool* VesperEngine::getWorkerPool() const
{
    return g_runtime_global_context.m_worker_pool.get();
}

EventBus* VesperEngine::getEventBus() const
{
    return g_runtime_global_context.m_event_bus.get();
}

} // namespace vesper
