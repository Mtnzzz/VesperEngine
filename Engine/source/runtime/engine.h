
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace vesper
{

    // Forward declarations
    class WindowSystem;
    class InputSystem;
    class WorkerPool;
    class EventBus;
    class RenderPacketBuffer;
    struct RenderPacket;

    /// @brief Engine initialization configuration
    struct EngineConfig {
        uint32_t windowWidth{1920};
        uint32_t windowHeight{1080};
        std::string windowTitle{"Vesper Engine"};
        bool fullscreen{false};
        bool resizable{true};
        uint32_t workerThreadCount{0}; // 0 = auto-detect
        bool enableMultithreading{true};
    };

    /// @brief Core engine class managing all subsystems
    /// Implements the three-thread architecture: Main/Logic, Render, Workers
    class VesperEngine
    {
    public:
        VesperEngine();
        ~VesperEngine();

        VesperEngine(const VesperEngine &)            = delete;
        VesperEngine &operator=(const VesperEngine &) = delete;

        /// @brief Initialize all engine subsystems
        /// @param config Engine configuration
        /// @return true if initialization successful
        bool initialize(const EngineConfig &config = {});

        /// @brief Run the main engine loop
        void run();

        /// @brief Shutdown all engine subsystems
        void shutdown();

        /// @brief Check if engine is running
        bool isRunning() const { return m_running.load(std::memory_order_acquire); }

        /// @brief Request engine to stop
        void requestQuit() { m_running.store(false, std::memory_order_release); }

        /// @brief Get window system
        WindowSystem *getWindowSystem() const { return m_windowSystem.get(); }

        /// @brief Get input system
        InputSystem *getInputSystem() const { return m_inputSystem.get(); }

        /// @brief Get worker pool
        WorkerPool *getWorkerPool() const;

        /// @brief Get event bus
        EventBus *getEventBus() const;

    private:
        /// @brief Register default input callbacks for logging
        void registerInputCallbacks();

        /// @brief Main/Logic thread tick - runs game logic
        void logicTick(float deltaTime);

        /// @brief Render thread main loop
        void renderThreadLoop();

        /// @brief Render thread tick - consumes RenderPackets
        /// @return true if work was performed (packet processed or tasks run)
        bool renderTick();

        /// @brief Process one frame (single-threaded fallback)
        void tick();

        /// @brief Prepare render packet from current game state
        void prepareRenderPacket(RenderPacket *packet);

    private:
        std::unique_ptr<WindowSystem> m_windowSystem;
        std::unique_ptr<InputSystem> m_inputSystem;

        // Threading systems (managed via GlobalContext)
        std::unique_ptr<std::thread> m_renderThread;

        // Frame timing
        uint64_t m_frameIndex{0};
        std::chrono::steady_clock::time_point m_lastFrameTime;

        // State flags
        std::atomic<bool> m_running{false};
        std::atomic<bool> m_renderThreadRunning{false};
        bool m_initialized{false};
        bool m_multithreadingEnabled{true};
    };

} // namespace vesper