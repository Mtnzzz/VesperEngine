#include "runtime/core/threading/worker_pool.h"
#include "runtime/core/log/log_system.h"

#include <algorithm>
#include <chrono>

// Platform-specific includes for thread naming/affinity
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <processthreadsapi.h>
#else
#include <pthread.h>
#ifdef __linux__
#include <sched.h>
#endif
#endif

namespace vesper {

// ============================================================================
// Platform-specific thread utilities
// ============================================================================

void WorkerPool::setThreadName(const std::string& name)
{
#ifdef _WIN32
    // Windows 10 1607+ thread naming via SetThreadDescription
    std::wstring wname(name.begin(), name.end());
    SetThreadDescription(GetCurrentThread(), wname.c_str());
#elif defined(__APPLE__)
    pthread_setname_np(name.c_str());
#elif defined(__linux__)
    // Linux pthread naming (max 16 chars including null)
    pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#endif
}

void WorkerPool::setThreadAffinity(uint64_t mask)
{
    if (mask == 0) return;  // 0 means no affinity

#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(mask));
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < 64; ++i)
    {
        if (mask & (1ULL << i))
        {
            CPU_SET(i, &cpuset);
        }
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    // macOS doesn't support thread affinity in the same way
}

void WorkerPool::setThreadPriority(int32_t priority)
{
    if (priority == 0) return;  // 0 means normal priority

#ifdef _WIN32
    int winPriority = THREAD_PRIORITY_NORMAL;
    if (priority > 0)
    {
        winPriority = (priority >= 2) ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_ABOVE_NORMAL;
    }
    else
    {
        winPriority = (priority <= -2) ? THREAD_PRIORITY_LOWEST : THREAD_PRIORITY_BELOW_NORMAL;
    }
    SetThreadPriority(GetCurrentThread(), winPriority);
#else
    // POSIX thread priority (requires appropriate permissions)
    struct sched_param param;
    int policy;
    pthread_getschedparam(pthread_self(), &policy, &param);

    int minPrio = sched_get_priority_min(policy);
    int maxPrio = sched_get_priority_max(policy);
    int range = maxPrio - minPrio;

    // Map priority to range
    param.sched_priority = minPrio + (range / 2) + (priority * range / 4);
    param.sched_priority = std::clamp(param.sched_priority, minPrio, maxPrio);

    pthread_setschedparam(pthread_self(), policy, &param);
#endif
}

thread_local std::mt19937 WorkerPool::t_rng{std::random_device{}()};

WorkerPool::WorkerPool() = default;

WorkerPool::~WorkerPool()
{
    if (m_running.load(std::memory_order_acquire))
    {
        shutdown();
    }
}

bool WorkerPool::initialize(const WorkerPoolConfig& config)
{
    if (m_running.load(std::memory_order_acquire))
    {
        LOG_WARN("WorkerPool already initialized");
        return false;
    }

    m_config = config;

    // Determine number of workers
    uint32_t numWorkers = config.numWorkers;
    if (numWorkers == 0)
    {
        // Auto-detect: leave 2 cores for main and render threads
        uint32_t hwThreads = std::thread::hardware_concurrency();
        numWorkers = (hwThreads > 2) ? (hwThreads - 2) : 1;
    }

    // Check if we should run in sync mode (no worker threads)
    // This can happen when explicitly configured or on single-core systems
    if (numWorkers == 0 || (config.numWorkers == UINT32_MAX))
    {
        if (config.syncFallback)
        {
            m_syncMode = true;
            m_running.store(true, std::memory_order_release);
            LOG_INFO("WorkerPool: Initialized in sync mode (no worker threads, tasks execute inline)");
            return true;
        }
        else
        {
            LOG_WARN("WorkerPool: No workers available and syncFallback disabled");
            return false;
        }
    }

    LOG_INFO("WorkerPool: Initializing with {} worker threads", numWorkers);

    // Create local queues for each worker
    m_localQueues.reserve(numWorkers);
    for (uint32_t i = 0; i < numWorkers; ++i)
    {
        m_localQueues.push_back(std::make_unique<WorkStealingDeque<Task>>());
    }

    m_running.store(true, std::memory_order_release);

    // Create worker threads
    m_workers.reserve(numWorkers);
    for (uint32_t i = 0; i < numWorkers; ++i)
    {
        m_workers.emplace_back(&WorkerPool::workerLoop, this, i);
    }

    return true;
}

void WorkerPool::shutdown()
{
    if (!m_running.load(std::memory_order_acquire))
    {
        return;
    }

    LOG_INFO("WorkerPool: Shutting down...");

    // Signal shutdown
    m_running.store(false, std::memory_order_release);

    // Shutdown all queues to unblock any waiting producers
    for (size_t i = 0; i < kPriorityLevels; ++i)
    {
        m_priorityQueues[i].shutdown();
    }
    m_logicQueue.shutdown();
    m_renderQueue.shutdown();

    // Wake all workers
    m_cv.notify_all();

    // Wait for all workers to finish
    for (auto& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    m_workers.clear();
    m_localQueues.clear();

    // Drain remaining priority queues (AnyThread tasks)
    Task task;
    for (size_t i = 0; i < kPriorityLevels; ++i)
    {
        while (m_priorityQueues[i].tryDequeue(task))
        {
            LOG_WARN("WorkerPool: Dropping unprocessed AnyThread task (priority {})", i);
            if (task.waitGroup)
            {
                task.waitGroup->done();
            }
            m_pendingTasks.fetch_sub(1, std::memory_order_release);
        }
    }

    // Drain remaining affinity queues
    while (m_logicQueue.tryDequeue(task))
    {
        LOG_WARN("WorkerPool: Dropping unprocessed LogicOnly task");
        if (task.waitGroup)
        {
            task.waitGroup->done();
        }
    }
    while (m_renderQueue.tryDequeue(task))
    {
        LOG_WARN("WorkerPool: Dropping unprocessed RenderOnly task");
        if (task.waitGroup)
        {
            task.waitGroup->done();
        }
    }

    LOG_INFO("WorkerPool: Shutdown complete. Tasks completed: {}",
             m_stats.tasksCompleted.load(std::memory_order_relaxed));
}

template<typename Queue>
bool WorkerPool::enqueueWithPolicy(Queue& queue, Task& task, bool updatePendingCount)
{
    // Fast path: try to enqueue immediately
    if (queue.tryEnqueue(task))
    {
        if (updatePendingCount)
        {
            m_pendingTasks.fetch_add(1, std::memory_order_release);
        }
        return true;
    }

    // Check for potential deadlock scenarios:
    // 1. Render thread submitting RenderOnly task (sole consumer)
    // 2. Main thread submitting LogicOnly task (sole consumer)
    // 3. Worker thread submitting AnyThread task when no other worker is available to consume
    const ThreadType currentThread = ThreadContext::currentThreadType();
    bool wouldDeadlock = false;

    if (task.affinity == TaskAffinity::RenderOnly && currentThread == ThreadType::Render)
    {
        wouldDeadlock = true;
    }
    else if (task.affinity == TaskAffinity::LogicOnly && currentThread == ThreadType::Main)
    {
        wouldDeadlock = true;
    }
    else if (task.affinity == TaskAffinity::AnyThread && currentThread == ThreadType::Worker)
    {
        // Worker submitting to AnyThread queue
        // Deadlock occurs if blocking this worker would leave no active consumers
        // Active consumers = total workers - workers already blocked on enqueue - this worker (1)
        uint32_t blockedWorkers = m_workersBlockedOnEnqueue.load(std::memory_order_acquire);
        uint32_t totalWorkers = static_cast<uint32_t>(m_workers.size());

        // If blocking would leave zero active workers, we'd deadlock
        if (blockedWorkers + 1 >= totalWorkers)
        {
            wouldDeadlock = true;
        }
    }

    if (wouldDeadlock)
    {
        // Potential deadlock detected - respect the configured overflow policy
        switch (m_config.overflowPolicy)
        {
            case QueueOverflowPolicy::Block:
            case QueueOverflowPolicy::ExecuteInline:
            {
                // Execute inline to avoid deadlock
                LOG_DEBUG("WorkerPool: Avoiding deadlock by executing task inline (affinity={}, thread={})",
                          static_cast<int>(task.affinity), static_cast<int>(currentThread));
                m_stats.tasksExecutedInline.fetch_add(1, std::memory_order_relaxed);
                task.execute();
                m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            case QueueOverflowPolicy::Drop:
            default:
            {
                // User explicitly wants Drop - respect that even in deadlock scenario
                LOG_WARN("WorkerPool: Queue full in potential deadlock scenario, task dropped (affinity={}, policy=Drop)",
                         static_cast<int>(task.affinity));
                m_stats.tasksDropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }
    }

    // No deadlock risk, apply normal overflow policy
    switch (m_config.overflowPolicy)
    {
        case QueueOverflowPolicy::Block:
        {
            // Block with exponential backoff until space is available
            m_stats.tasksBlocked.fetch_add(1, std::memory_order_relaxed);

            // Track blocked workers only for AnyThread submissions
            // RenderOnly/LogicOnly deadlocks are detected by thread type, not by counting
            const bool trackBlocked = (currentThread == ThreadType::Worker) &&
                                      (task.affinity == TaskAffinity::AnyThread);

            if (trackBlocked)
            {
                m_workersBlockedOnEnqueue.fetch_add(1, std::memory_order_release);
            }

            uint32_t backoffUs = 1;
            constexpr uint32_t kMaxBackoffUs = 1000;  // 1ms max
            bool enqueued = false;

            while (!enqueued)
            {
                if (queue.tryEnqueue(task))
                {
                    enqueued = true;
                    break;
                }

                if (!m_running.load(std::memory_order_acquire))
                {
                    // Pool is shutting down, drop the task
                    if (trackBlocked)
                    {
                        m_workersBlockedOnEnqueue.fetch_sub(1, std::memory_order_release);
                    }
                    LOG_WARN("WorkerPool: Pool shutting down, task dropped");
                    m_stats.tasksDropped.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }

                // Re-check for deadlock: another worker might have become blocked while we were waiting
                if (trackBlocked)
                {
                    uint32_t blockedWorkers = m_workersBlockedOnEnqueue.load(std::memory_order_acquire);
                    uint32_t totalWorkers = static_cast<uint32_t>(m_workers.size());

                    if (blockedWorkers >= totalWorkers)
                    {
                        // All workers are now blocked on AnyThread - we must break out to avoid deadlock
                        m_workersBlockedOnEnqueue.fetch_sub(1, std::memory_order_release);

                        // Fall back based on policy intent
                        LOG_DEBUG("WorkerPool: All workers blocked on AnyThread, executing task inline to avoid deadlock");
                        m_stats.tasksExecutedInline.fetch_add(1, std::memory_order_relaxed);
                        task.execute();
                        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }
                }

                std::this_thread::sleep_for(std::chrono::microseconds(backoffUs));
                backoffUs = std::min(backoffUs * 2, kMaxBackoffUs);
            }

            if (trackBlocked)
            {
                m_workersBlockedOnEnqueue.fetch_sub(1, std::memory_order_release);
            }

            if (updatePendingCount)
            {
                m_pendingTasks.fetch_add(1, std::memory_order_release);
            }
            return true;
        }

        case QueueOverflowPolicy::ExecuteInline:
        {
            // Execute the task immediately on the calling thread
            m_stats.tasksExecutedInline.fetch_add(1, std::memory_order_relaxed);
            task.execute();
            m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
            return true;  // Task completed, no need to enqueue
        }

        case QueueOverflowPolicy::Drop:
        default:
        {
            LOG_WARN("WorkerPool: Queue full for affinity {}, task dropped",
                     static_cast<int>(task.affinity));
            m_stats.tasksDropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }
}

WaitGroupPtr WorkerPool::submit(Task task)
{
    if (!m_running.load(std::memory_order_acquire))
    {
        LOG_WARN("WorkerPool: Cannot submit task - pool is not running");
        return nullptr;
    }

    // Create wait group if not provided
    WaitGroupPtr wg = task.waitGroup;
    if (!wg)
    {
        wg = makeWaitGroup(1);
        task.waitGroup = wg;
    }
    else
    {
        wg->add(1);
    }

    // Sync mode: execute all tasks immediately on calling thread
    if (m_syncMode)
    {
        m_stats.tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
        m_stats.tasksExecutedInline.fetch_add(1, std::memory_order_relaxed);
        task.execute();
        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        return wg;
    }

    // Route based on affinity
    bool success = false;
    switch (task.affinity)
    {
        case TaskAffinity::AnyThread:
        {
            size_t queueIndex = priorityToIndex(task.priority);
            success = enqueueWithPolicy(m_priorityQueues[queueIndex], task, true);
            if (success && m_config.overflowPolicy != QueueOverflowPolicy::ExecuteInline)
            {
                m_cv.notify_one();
            }
            break;
        }

        case TaskAffinity::RenderOnly:
            success = enqueueWithPolicy(m_renderQueue, task, false);
            break;

        case TaskAffinity::LogicOnly:
            success = enqueueWithPolicy(m_logicQueue, task, false);
            break;
    }

    if (!success)
    {
        // Task was dropped (Drop policy)
        wg->done();
        return wg;
    }

    m_stats.tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
    return wg;
}

WaitGroupPtr WorkerPool::submit(TaskFunction fn, TaskAffinity affinity, TaskPriority priority)
{
    return submit(makeTask(std::move(fn), affinity, priority));
}

WaitGroupPtr WorkerPool::submit(const LightTask& lightTask)
{
    // Convert to regular task and submit
    // Note: This does allocate due to std::function, but keeps the API simple
    // For truly zero-allocation, use submitLightBatch with pre-allocated structures
    return submit(lightTask.toTask());
}

WaitGroupPtr WorkerPool::submitLightBatch(LightTaskFunc* functions,
                                          void** userData,
                                          size_t count,
                                          TaskAffinity affinity,
                                          TaskPriority priority)
{
    if (count == 0 || functions == nullptr)
    {
        return makeWaitGroup(0);
    }

    auto wg = makeWaitGroup(static_cast<int32_t>(count));

    // Sync mode: execute all tasks immediately
    if (m_syncMode)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (functions[i])
            {
                m_stats.tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
                m_stats.tasksExecutedInline.fetch_add(1, std::memory_order_relaxed);
                functions[i](userData ? userData[i] : nullptr);
                m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
            }
            wg->done();
        }
        return wg;
    }

    bool anyEnqueued = false;

    for (size_t i = 0; i < count; ++i)
    {
        if (!functions[i])
        {
            wg->done();
            continue;
        }

        // Create task with captured function pointer and userData
        // This is a small lambda that just calls the function pointer
        LightTaskFunc fn = functions[i];
        void* data = userData ? userData[i] : nullptr;

        Task task;
        task.function = [fn, data]() { fn(data); };
        task.affinity = affinity;
        task.priority = priority;
        task.waitGroup = wg;

        bool success = false;
        switch (affinity)
        {
            case TaskAffinity::AnyThread:
            {
                size_t queueIndex = priorityToIndex(priority);
                success = enqueueWithPolicy(m_priorityQueues[queueIndex], task, true);
                break;
            }

            case TaskAffinity::RenderOnly:
                success = enqueueWithPolicy(m_renderQueue, task, false);
                break;

            case TaskAffinity::LogicOnly:
                success = enqueueWithPolicy(m_logicQueue, task, false);
                break;
        }

        if (!success)
        {
            wg->done();
            continue;
        }

        anyEnqueued = true;
        m_stats.tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
    }

    if (anyEnqueued)
    {
        m_cv.notify_all();
    }

    return wg;
}

WaitGroupPtr WorkerPool::submitBatch(std::vector<Task>& tasks)
{
    if (tasks.empty())
    {
        return makeWaitGroup(0);
    }

    auto wg = makeWaitGroup(static_cast<int32_t>(tasks.size()));

    // Sync mode: execute all tasks immediately
    if (m_syncMode)
    {
        for (auto& task : tasks)
        {
            task.waitGroup = wg;
            m_stats.tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
            m_stats.tasksExecutedInline.fetch_add(1, std::memory_order_relaxed);
            task.execute();
            m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        }
        return wg;
    }

    bool anyEnqueued = false;

    for (auto& task : tasks)
    {
        task.waitGroup = wg;

        bool success = false;
        switch (task.affinity)
        {
            case TaskAffinity::AnyThread:
            {
                size_t queueIndex = priorityToIndex(task.priority);
                success = enqueueWithPolicy(m_priorityQueues[queueIndex], task, true);
                break;
            }

            case TaskAffinity::RenderOnly:
                success = enqueueWithPolicy(m_renderQueue, task, false);
                break;

            case TaskAffinity::LogicOnly:
                success = enqueueWithPolicy(m_logicQueue, task, false);
                break;
        }

        if (!success)
        {
            // Task was dropped (Drop policy)
            wg->done();
            continue;
        }

        anyEnqueued = true;
        m_stats.tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
    }

    if (anyEnqueued)
    {
        m_cv.notify_all();
    }

    return wg;
}

bool WorkerPool::helpRunOne()
{
    if (!m_config.enableHelpRun)
    {
        return false;
    }

    // Try priority queues from highest to lowest
    Task task;
    for (int i = static_cast<int>(kPriorityLevels) - 1; i >= 0; --i)
    {
        if (m_priorityQueues[i].tryDequeue(task))
        {
            task.execute();
            m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
            m_stats.helpRunCount.fetch_add(1, std::memory_order_relaxed);
            m_pendingTasks.fetch_sub(1, std::memory_order_release);
            return true;
        }
    }

    // Try stealing from workers
    for (auto& queue : m_localQueues)
    {
        if (auto stolen = queue->steal())
        {
            stolen->execute();
            m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
            m_stats.helpRunCount.fetch_add(1, std::memory_order_relaxed);
            m_stats.tasksStolen.fetch_add(1, std::memory_order_relaxed);
            m_pendingTasks.fetch_sub(1, std::memory_order_release);
            return true;
        }
    }

    return false;
}

bool WorkerPool::processLogicTask()
{
    Task task;
    if (m_logicQueue.tryDequeue(task))
    {
        task.execute();
        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

uint32_t WorkerPool::processAllLogicTasks()
{
    uint32_t count = 0;
    Task task;
    while (m_logicQueue.tryDequeue(task))
    {
        task.execute();
        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        ++count;
    }
    return count;
}

bool WorkerPool::processRenderTask()
{
    Task task;
    if (m_renderQueue.tryDequeue(task))
    {
        task.execute();
        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

uint32_t WorkerPool::processAllRenderTasks()
{
    uint32_t count = 0;
    Task task;
    while (m_renderQueue.tryDequeue(task))
    {
        task.execute();
        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        ++count;
    }
    return count;
}

void WorkerPool::waitFor(WaitGroupPtr wg, bool allowHelpRun)
{
    if (!wg || wg->isDone())
    {
        return;
    }

    if (allowHelpRun && m_config.enableHelpRun)
    {
        // Smart help-run based on current thread type
        // This prevents deadlock when waiting for affinity-specific tasks
        const ThreadType threadType = ThreadContext::currentThreadType();

        // Exponential backoff parameters
        uint32_t spinCount = 0;
        constexpr uint32_t kSpinBeforeYield = 32;       // Spin iterations before first yield
        constexpr uint32_t kYieldBeforeSleep = 64;      // Additional yields before sleeping
        constexpr uint32_t kMaxSleepUs = 1000;          // Max sleep time (1ms)

        while (!wg->isDone())
        {
            bool didWork = false;

            // Always try AnyThread tasks first (available to all threads)
            didWork = helpRunOne();

            // If on main thread, also process LogicOnly tasks
            // This prevents deadlock when waitFor contains LogicOnly tasks
            if (!didWork && threadType == ThreadType::Main)
            {
                didWork = processLogicTask();
            }

            // If on render thread, also process RenderOnly tasks
            // This prevents deadlock when waitFor contains RenderOnly tasks
            if (!didWork && threadType == ThreadType::Render)
            {
                didWork = processRenderTask();
            }

            if (didWork)
            {
                // Reset backoff when work is done
                spinCount = 0;
            }
            else
            {
                // No tasks available, apply exponential backoff
                ++spinCount;

                if (spinCount < kSpinBeforeYield)
                {
                    // Spin-wait (CPU pause instruction would be ideal here)
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                }
                else if (spinCount < kSpinBeforeYield + kYieldBeforeSleep)
                {
                    // Yield to other threads
                    std::this_thread::yield();
                }
                else
                {
                    // Sleep with exponential backoff
                    uint32_t sleepUs = std::min(
                        1U << (spinCount - kSpinBeforeYield - kYieldBeforeSleep),
                        kMaxSleepUs
                    );
                    std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
                }
            }
        }
    }
    else
    {
        // Blocking wait - use with caution, may deadlock if WaitGroup
        // contains affinity tasks that current thread should process
        wg->wait();
    }
}

void WorkerPool::workerLoop(uint32_t workerId)
{
    // Register this thread as a worker thread
    ScopedThreadRegistration threadReg(ThreadType::Worker);

    // Apply thread configuration
    std::string threadName = m_config.threadConfig.namePrefix + "-" + std::to_string(workerId);
    setThreadName(threadName);
    setThreadAffinity(m_config.threadConfig.affinityMask);
    setThreadPriority(m_config.threadConfig.priority);

    LOG_DEBUG("WorkerPool: Worker {} started (name: {})", workerId, threadName);

    auto& localQueue = *m_localQueues[workerId];

    while (m_running.load(std::memory_order_acquire))
    {
        // Try to get a task
        auto task = tryGetTask(workerId);

        if (task)
        {
            task->execute();
            m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
            m_pendingTasks.fetch_sub(1, std::memory_order_release);
        }
        else
        {
            // No task found, wait for notification
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(1), [this]() {
                return !m_running.load(std::memory_order_acquire) ||
                       m_pendingTasks.load(std::memory_order_acquire) > 0;
            });
        }
    }

    // Drain remaining tasks in local queue before exiting
    while (auto task = localQueue.pop())
    {
        task->execute();
        m_stats.tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        m_pendingTasks.fetch_sub(1, std::memory_order_release);
    }
}

std::optional<Task> WorkerPool::tryGetTask(uint32_t workerId)
{
    auto& localQueue = *m_localQueues[workerId];

    // 1. Try local queue first (LIFO for cache locality)
    if (auto task = localQueue.pop())
    {
        return task;
    }

    // 2. Try priority queues from highest to lowest
    Task globalTask;
    for (int i = static_cast<int>(kPriorityLevels) - 1; i >= 0; --i)
    {
        if (m_priorityQueues[i].tryDequeue(globalTask))
        {
            return globalTask;
        }
    }

    // 3. Try stealing from other workers
    return trySteal(workerId);
}

std::optional<Task> WorkerPool::trySteal(uint32_t thiefId)
{
    if (m_localQueues.size() <= 1)
    {
        return std::nullopt;
    }

    // Random victim selection
    std::uniform_int_distribution<uint32_t> dist(0,
        static_cast<uint32_t>(m_localQueues.size()) - 1);

    // Try a few random victims
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        uint32_t victimId = dist(t_rng);
        if (victimId == thiefId)
        {
            continue;
        }

        if (auto task = m_localQueues[victimId]->steal())
        {
            m_stats.tasksStolen.fetch_add(1, std::memory_order_relaxed);
            return task;
        }
    }

    return std::nullopt;
}

void WorkerPool::notifyWorkers()
{
    m_cv.notify_all();
}

} // namespace vesper
