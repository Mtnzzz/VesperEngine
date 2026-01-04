#pragma once

#include "runtime/core/base/macro.h"
#include "runtime/core/threading/task.h"
#include "runtime/core/threading/wait_group.h"
#include "runtime/core/threading/work_stealing_deque.h"
#include "runtime/core/threading/mpsc_queue.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace vesper {

/// @brief Thread type identification for the task system
/// Similar to Unreal Engine's Named Threads concept
enum class ThreadType : uint8_t
{
    Unknown,        // Unregistered thread
    Main,           // Main/Logic thread
    Render,         // Render thread
    Worker,         // Worker pool thread
};

/// @brief Thread-local context for thread identification
/// Uses RAII pattern for automatic registration/unregistration
class ThreadContext
{
public:
    /// @brief Register current thread with a specific type
    static void registerThread(ThreadType type)
    {
        s_threadType = type;
    }

    /// @brief Unregister current thread
    static void unregisterThread()
    {
        s_threadType = ThreadType::Unknown;
    }

    /// @brief Get current thread type
    [[nodiscard]] static ThreadType currentThreadType()
    {
        return s_threadType;
    }

    /// @brief Check if current thread is the main/logic thread
    [[nodiscard]] static bool isMainThread()
    {
        return s_threadType == ThreadType::Main;
    }

    /// @brief Check if current thread is the render thread
    [[nodiscard]] static bool isRenderThread()
    {
        return s_threadType == ThreadType::Render;
    }

    /// @brief Check if current thread is a worker thread
    [[nodiscard]] static bool isWorkerThread()
    {
        return s_threadType == ThreadType::Worker;
    }

private:
    inline static thread_local ThreadType s_threadType = ThreadType::Unknown;
};

/// @brief RAII guard for thread registration
/// Automatically registers on construction and unregisters on destruction
class ScopedThreadRegistration
{
public:
    explicit ScopedThreadRegistration(ThreadType type)
    {
        ThreadContext::registerThread(type);
    }

    ~ScopedThreadRegistration()
    {
        ThreadContext::unregisterThread();
    }

    VESPER_DISABLE_COPY_AND_MOVE(ScopedThreadRegistration)
};

/// @brief Policy when task queue is full
enum class QueueOverflowPolicy : uint8_t
{
    Drop,           // Drop the task and log warning (current behavior)
    Block,          // Block until space is available
    ExecuteInline,  // Execute the task immediately on calling thread
};

/// @brief Thread configuration for worker threads
struct ThreadConfig
{
    std::string namePrefix{"VesperWorker"};  // Thread name prefix (e.g., "VesperWorker-0")
    int32_t priority{0};                      // 0 = normal, positive = higher, negative = lower
    uint64_t affinityMask{0};                 // 0 = no affinity (OS decides)
};

/// @brief Worker pool configuration
struct WorkerPoolConfig
{
    uint32_t numWorkers{0};                             // 0 = auto-detect (hardware_concurrency - 2)
    bool enableHelpRun{true};                           // Allow main/render thread to help run tasks
    QueueOverflowPolicy overflowPolicy{QueueOverflowPolicy::Block};  // What to do when queue is full
    ThreadConfig threadConfig{};                        // Thread naming/affinity configuration
    bool syncFallback{true};                            // If true and numWorkers ends up 0, execute tasks synchronously
    // Note: Queue capacities are compile-time constants for performance
    // See kGlobalQueueCapacity and kAffinityQueueCapacity below
};

/// @brief Statistics for worker pool monitoring
struct WorkerPoolStats
{
    std::atomic<uint64_t> tasksSubmitted{0};
    std::atomic<uint64_t> tasksCompleted{0};
    std::atomic<uint64_t> tasksStolen{0};
    std::atomic<uint64_t> helpRunCount{0};
    std::atomic<uint64_t> tasksDropped{0};          // Tasks dropped due to queue full (Drop policy)
    std::atomic<uint64_t> tasksExecutedInline{0};   // Tasks executed inline (ExecuteInline policy)
    std::atomic<uint64_t> tasksBlocked{0};          // Times blocked waiting for queue space

    void reset()
    {
        tasksSubmitted.store(0, std::memory_order_relaxed);
        tasksCompleted.store(0, std::memory_order_relaxed);
        tasksStolen.store(0, std::memory_order_relaxed);
        helpRunCount.store(0, std::memory_order_relaxed);
        tasksDropped.store(0, std::memory_order_relaxed);
        tasksExecutedInline.store(0, std::memory_order_relaxed);
        tasksBlocked.store(0, std::memory_order_relaxed);
    }
};

/// @brief Work-stealing thread pool for task execution
/// Supports task affinity routing: AnyThread, RenderOnly, LogicOnly
class WorkerPool
{
public:
    WorkerPool();
    ~WorkerPool();

    VESPER_DISABLE_COPY_AND_MOVE(WorkerPool)

    /// @brief Initialize the worker pool
    /// @param config Configuration options
    /// @return true if successful
    bool initialize(const WorkerPoolConfig& config = {});

    /// @brief Shutdown the worker pool and wait for all tasks to complete
    void shutdown();

    /// @brief Submit a task to the pool
    /// Routes task based on affinity:
    /// - AnyThread: worker pool
    /// - RenderOnly: render thread queue
    /// - LogicOnly: logic thread queue
    /// @param task Task to execute
    /// @return WaitGroup that will be signaled when task completes
    WaitGroupPtr submit(Task task);

    /// @brief Submit a function as a task
    /// @param fn Function to execute
    /// @param affinity Thread affinity
    /// @param priority Task priority
    /// @return WaitGroup that will be signaled when task completes
    WaitGroupPtr submit(TaskFunction fn,
                        TaskAffinity affinity = TaskAffinity::AnyThread,
                        TaskPriority priority = TaskPriority::Normal);

    /// @brief Submit multiple tasks and get a single wait group
    /// @param tasks Vector of tasks to submit
    /// @return WaitGroup that will be signaled when all tasks complete
    WaitGroupPtr submitBatch(std::vector<Task>& tasks);

    /// @brief Submit a lightweight task (minimal allocation overhead)
    /// @param task LightTask to execute
    /// @return WaitGroup that will be signaled when task completes
    WaitGroupPtr submit(const LightTask& task);

    /// @brief Submit a batch of lightweight tasks (Forge-style array submission)
    /// @param functions Array of function pointers
    /// @param userData Array of user data pointers (parallel to functions)
    /// @param count Number of tasks
    /// @param affinity Thread affinity for all tasks
    /// @param priority Priority for all tasks
    /// @return WaitGroup that will be signaled when all tasks complete
    WaitGroupPtr submitLightBatch(LightTaskFunc* functions,
                                  void** userData,
                                  size_t count,
                                  TaskAffinity affinity = TaskAffinity::AnyThread,
                                  TaskPriority priority = TaskPriority::Normal);

    /// @brief Submit a TaskBatch (template version)
    template<size_t N>
    WaitGroupPtr submitBatch(const TaskBatch<N>& batch)
    {
        return submitLightBatch(
            const_cast<LightTaskFunc*>(batch.functions),
            const_cast<void**>(batch.userData),
            batch.count,
            batch.affinity,
            batch.priority
        );
    }

    /// @brief Try to execute one AnyThread task from the pool (help-run)
    /// Call this while waiting to avoid idle spinning
    /// @return true if a task was executed
    bool helpRunOne();

    /// @brief Process one LogicOnly task (call from main/logic thread)
    /// @return true if a task was executed
    bool processLogicTask();

    /// @brief Process all pending LogicOnly tasks (call from main/logic thread)
    /// @return Number of tasks executed
    uint32_t processAllLogicTasks();

    /// @brief Process one RenderOnly task (call from render thread)
    /// @return true if a task was executed
    bool processRenderTask();

    /// @brief Process all pending RenderOnly tasks (call from render thread)
    /// @return Number of tasks executed
    uint32_t processAllRenderTasks();

    /// @brief Wait for a wait group with help-run support
    /// @param wg Wait group to wait for
    /// @param allowHelpRun If true, execute tasks while waiting
    void waitFor(WaitGroupPtr wg, bool allowHelpRun = true);

    /// @brief Check if pool is running
    [[nodiscard]] bool isRunning() const { return m_running.load(std::memory_order_acquire); }

    /// @brief Get number of worker threads
    [[nodiscard]] uint32_t workerCount() const { return static_cast<uint32_t>(m_workers.size()); }

    /// @brief Get statistics
    [[nodiscard]] const WorkerPoolStats& stats() const { return m_stats; }

    /// @brief Reset statistics
    void resetStats() { m_stats.reset(); }

private:
    /// @brief Worker thread function
    void workerLoop(uint32_t workerId);

    /// @brief Try to get a task for execution
    /// @param workerId Worker requesting task
    /// @return Task if available
    std::optional<Task> tryGetTask(uint32_t workerId);

    /// @brief Try to steal from another worker
    /// @param thiefId Worker trying to steal
    /// @return Stolen task if available
    std::optional<Task> trySteal(uint32_t thiefId);

    /// @brief Wake up workers
    void notifyWorkers();

    /// @brief Enqueue task with overflow policy handling
    /// @return true if task was enqueued or executed inline
    template<typename Queue>
    bool enqueueWithPolicy(Queue& queue, Task& task, bool updatePendingCount);

    /// @brief Set thread name (platform-specific)
    static void setThreadName(const std::string& name);

    /// @brief Set thread affinity (platform-specific)
    static void setThreadAffinity(uint64_t mask);

    /// @brief Set thread priority (platform-specific)
    static void setThreadPriority(int32_t priority);

private:
    static constexpr size_t kGlobalQueueCapacity = 1024;
    static constexpr size_t kAffinityQueueCapacity = 512;
    static constexpr size_t kPriorityLevels = 4;  // Low, Normal, High, Critical

    // Worker threads
    std::vector<std::thread> m_workers;
    std::vector<std::unique_ptr<WorkStealingDeque<Task>>> m_localQueues;

    // Priority-based global queues for AnyThread tasks (index = priority level)
    std::array<MPSCQueue<Task, kGlobalQueueCapacity>, kPriorityLevels> m_priorityQueues;

    // Affinity-specific queues (MPSC: multiple producers, single consumer on target thread)
    MPSCQueue<Task, kAffinityQueueCapacity> m_renderQueue;  // RenderOnly tasks
    MPSCQueue<Task, kAffinityQueueCapacity> m_logicQueue;   // LogicOnly tasks

    // Helper to get queue index from priority
    static size_t priorityToIndex(TaskPriority priority)
    {
        return static_cast<size_t>(priority);
    }

    // Synchronization
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running{false};
    std::atomic<uint32_t> m_pendingTasks{0};
    std::atomic<uint32_t> m_workersBlockedOnEnqueue{0};  // Track workers blocked waiting to enqueue

    // Configuration
    WorkerPoolConfig m_config;

    // Sync mode flag (when no workers are available)
    bool m_syncMode{false};

    // Statistics
    WorkerPoolStats m_stats;

    // Random number generator for work stealing victim selection
    thread_local static std::mt19937 t_rng;
};

} // namespace vesper
