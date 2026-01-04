#pragma once

#include "runtime/core/base/macro.h"
#include "runtime/core/threading/wait_group.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace vesper {

/// @brief Thread affinity for task execution
enum class TaskAffinity : uint8_t
{
    AnyThread,      // Can run on any worker thread (pure computation, no shared writes)
    RenderOnly,     // Must run on render thread (GPU API, resource create/destroy)
    LogicOnly,      // Must run on logic/main thread (game state, scripts)
};

/// @brief Task priority levels
enum class TaskPriority : uint8_t
{
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3,   // Highest priority, for time-critical tasks
};

/// @brief Function type for task execution (heap-allocating, flexible)
using TaskFunction = std::function<void()>;

/// @brief Lightweight function pointer type (no heap allocation)
/// @param userData User-provided context pointer
using LightTaskFunc = void(*)(void* userData);

// Forward declaration for LightTask::toTask()
struct Task;

/// @brief Lightweight task for high-frequency scenarios
/// Uses function pointer instead of std::function to avoid heap allocation
struct LightTask
{
    LightTaskFunc       function{nullptr};   // Function pointer (no allocation)
    void*               userData{nullptr};   // User context
    TaskAffinity        affinity{TaskAffinity::AnyThread};
    TaskPriority        priority{TaskPriority::Normal};

    LightTask() = default;

    LightTask(LightTaskFunc fn, void* data = nullptr,
              TaskAffinity aff = TaskAffinity::AnyThread,
              TaskPriority prio = TaskPriority::Normal)
        : function(fn)
        , userData(data)
        , affinity(aff)
        , priority(prio)
    {}

    /// @brief Check if task is valid
    [[nodiscard]] bool isValid() const { return function != nullptr; }

    /// @brief Execute the task
    void execute() const
    {
        if (function)
        {
            function(userData);
        }
    }

    /// @brief Convert to regular Task (for unified processing)
    /// Defined after Task declaration
    [[nodiscard]] inline Task toTask() const;
};

/// @brief A unit of work to be executed by the task system
struct Task
{
    TaskFunction    function;           // The work to execute
    TaskAffinity    affinity{TaskAffinity::AnyThread};
    TaskPriority    priority{TaskPriority::Normal};
    WaitGroupPtr    waitGroup;          // Optional wait group to signal on completion
    std::string     debugName;          // Optional name for debugging/profiling

    Task() = default;

    Task(TaskFunction fn,
         TaskAffinity aff = TaskAffinity::AnyThread,
         TaskPriority prio = TaskPriority::Normal,
         WaitGroupPtr wg = nullptr,
         std::string name = "")
        : function(std::move(fn))
        , affinity(aff)
        , priority(prio)
        , waitGroup(std::move(wg))
        , debugName(std::move(name))
    {}

    /// @brief Execute the task and signal completion
    void execute()
    {
        if (function)
        {
            function();
        }

        if (waitGroup)
        {
            waitGroup->done();
        }
    }

    /// @brief Check if task is valid (has a function)
    [[nodiscard]] bool isValid() const
    {
        return function != nullptr;
    }

    /// @brief Check if task can run on worker threads
    [[nodiscard]] bool canRunOnWorker() const
    {
        return affinity == TaskAffinity::AnyThread;
    }
};

// Implementation of LightTask::toTask() (must be after Task definition)
inline Task LightTask::toTask() const
{
    Task t;
    if (function)
    {
        // Capture by value to avoid dangling references
        LightTaskFunc fn = function;
        void* data = userData;
        t.function = [fn, data]() { fn(data); };
    }
    t.affinity = affinity;
    t.priority = priority;
    return t;
}

/// @brief Builder pattern for creating tasks
class TaskBuilder
{
public:
    TaskBuilder() = default;

    TaskBuilder& withFunction(TaskFunction fn)
    {
        m_task.function = std::move(fn);
        return *this;
    }

    TaskBuilder& withAffinity(TaskAffinity affinity)
    {
        m_task.affinity = affinity;
        return *this;
    }

    TaskBuilder& withPriority(TaskPriority priority)
    {
        m_task.priority = priority;
        return *this;
    }

    TaskBuilder& withWaitGroup(WaitGroupPtr wg)
    {
        m_task.waitGroup = std::move(wg);
        return *this;
    }

    TaskBuilder& withDebugName(std::string name)
    {
        m_task.debugName = std::move(name);
        return *this;
    }

    Task build()
    {
        return std::move(m_task);
    }

private:
    Task m_task;
};

/// @brief Create a simple task with a function
inline Task makeTask(TaskFunction fn,
                     TaskAffinity affinity = TaskAffinity::AnyThread,
                     TaskPriority priority = TaskPriority::Normal)
{
    return Task(std::move(fn), affinity, priority);
}

/// @brief Create a task with a wait group
inline Task makeTask(TaskFunction fn,
                     WaitGroupPtr wg,
                     TaskAffinity affinity = TaskAffinity::AnyThread,
                     TaskPriority priority = TaskPriority::Normal)
{
    return Task(std::move(fn), affinity, priority, std::move(wg));
}

/// @brief Create a lightweight task
inline LightTask makeLightTask(LightTaskFunc fn,
                               void* userData = nullptr,
                               TaskAffinity affinity = TaskAffinity::AnyThread,
                               TaskPriority priority = TaskPriority::Normal)
{
    return LightTask(fn, userData, affinity, priority);
}

/// @brief Batch task data for array-style submission (Forge-like)
/// Minimizes allocations by using arrays instead of vectors
template<size_t N>
struct TaskBatch
{
    LightTaskFunc functions[N]{};
    void* userData[N]{};
    TaskAffinity affinity{TaskAffinity::AnyThread};
    TaskPriority priority{TaskPriority::Normal};
    size_t count{0};

    void add(LightTaskFunc fn, void* data = nullptr)
    {
        if (count < N)
        {
            functions[count] = fn;
            userData[count] = data;
            ++count;
        }
    }

    void clear()
    {
        count = 0;
    }
};

} // namespace vesper
