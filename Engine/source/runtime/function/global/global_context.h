#pragma once

#include <cstdint>
#include <memory>

namespace vesper {

// Forward declarations
class LogSystem;
class WindowSystem;
class InputSystem;
class RenderSystem;
class PhysicsManager;
class AssetManager;
class ConfigManager;

// Threading and event systems
class WorkerPool;
class EventBus;
class RenderPacketBuffer;

/// @brief Configuration for runtime systems initialization
struct RuntimeSystemsConfig
{
    bool enableWorkerPool{true};        // Enable worker thread pool
    uint32_t workerThreadCount{0};      // 0 = auto-detect
    bool enableHelpRun{true};           // Allow main/render thread to help run tasks
};

// Global runtime context - Service Locator pattern
class RuntimeGlobalContext {
public:
    /// @brief Start runtime systems with configuration
    /// @param config Configuration options
    void startSystems(const RuntimeSystemsConfig& config = {});

    void shutdownSystems();

public:
    // Core systems
    std::shared_ptr<LogSystem>      m_log_system;
    std::shared_ptr<WindowSystem>   m_window_system;
    std::shared_ptr<InputSystem>    m_input_system;
    std::shared_ptr<RenderSystem>   m_render_system;
    std::shared_ptr<PhysicsManager> m_physics_manager;
    std::shared_ptr<AssetManager>   m_asset_manager;
    std::shared_ptr<ConfigManager>  m_config_manager;

    // Threading and pipeline systems
    std::shared_ptr<WorkerPool>          m_worker_pool;
    std::shared_ptr<EventBus>            m_event_bus;
    std::shared_ptr<RenderPacketBuffer>  m_render_packet_buffer;
};

extern RuntimeGlobalContext g_runtime_global_context;

} // namespace vesper