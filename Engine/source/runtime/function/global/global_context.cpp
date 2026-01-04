#include "global_context.h"

#include "runtime/core/log/log_system.h"
#include "runtime/core/threading/worker_pool.h"
#include "runtime/core/event/event_bus.h"
#include "runtime/function/render/render_packet.h"

namespace vesper {

RuntimeGlobalContext g_runtime_global_context;

void RuntimeGlobalContext::startSystems(const RuntimeSystemsConfig& config) {
    // Initialize threading and pipeline systems first (order matters!)

    // 1. Worker pool - parallel task execution
    // Always create the worker pool for consistent API behavior
    // In single-threaded mode, it will execute tasks synchronously
    WorkerPoolConfig poolConfig;
    poolConfig.enableHelpRun = config.enableHelpRun;
    poolConfig.syncFallback = true;  // Enable sync fallback for single-thread mode

    if (config.enableWorkerPool)
    {
        poolConfig.numWorkers = config.workerThreadCount;  // 0 = auto-detect
    }
    else
    {
        // Force sync mode by setting numWorkers to a special value
        poolConfig.numWorkers = UINT32_MAX;  // Signals sync mode
    }

    m_worker_pool = std::make_shared<WorkerPool>();
    if (m_worker_pool->initialize(poolConfig))
    {
        if (m_worker_pool->workerCount() > 0)
        {
            LOG_INFO("RuntimeGlobalContext: Worker pool enabled with {} workers",
                     m_worker_pool->workerCount());
        }
        else
        {
            LOG_INFO("RuntimeGlobalContext: Worker pool in sync mode (single-threaded)");
        }
    }
    else
    {
        LOG_ERROR("RuntimeGlobalContext: Failed to initialize worker pool");
        m_worker_pool.reset();
    }

    // 2. Event bus - async event dispatch (always enabled)
    m_event_bus = std::make_shared<EventBus>();

    // 3. Render packet buffer - logic/render thread communication (always enabled)
    m_render_packet_buffer = std::make_shared<RenderPacketBuffer>(
        RenderPacketBuffer::kTripleBuffer,
        RenderPacketBufferMode::LatestOnly
    );

    LOG_INFO("RuntimeGlobalContext: Pipeline systems initialized");
}

void RuntimeGlobalContext::shutdownSystems() {
    LOG_INFO("RuntimeGlobalContext: Shutting down systems...");

    // Shutdown in reverse order of initialization

    // 1. Stop worker pool first (wait for in-flight tasks)
    if (m_worker_pool) {
        m_worker_pool->shutdown();
        m_worker_pool.reset();
    }

    // 2. Event bus - process remaining events
    if (m_event_bus) {
        m_event_bus->processAllChannels();
        m_event_bus.reset();
    }

    // 3. Render packet buffer
    m_render_packet_buffer.reset();

    // Reset other systems
    m_render_system.reset();
    m_physics_manager.reset();
    m_asset_manager.reset();
    m_input_system.reset();
    m_window_system.reset();
    m_config_manager.reset();

    LOG_INFO("RuntimeGlobalContext: All systems shut down");
}

} // namespace vesper