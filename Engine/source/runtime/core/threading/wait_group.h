#pragma once

#include "runtime/core/base/macro.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>

namespace vesper {

/// @brief A synchronization primitive for waiting on a group of tasks to complete
/// Similar to Go's sync.WaitGroup
class WaitGroup
{
public:
    WaitGroup() = default;
    ~WaitGroup() = default;

    VESPER_DISABLE_COPY(WaitGroup)

    // Allow move for returning from functions
    WaitGroup(WaitGroup&& other) noexcept
        : m_counter(other.m_counter.load(std::memory_order_relaxed))
    {
        other.m_counter.store(0, std::memory_order_relaxed);
    }

    WaitGroup& operator=(WaitGroup&& other) noexcept
    {
        if (this != &other)
        {
            m_counter.store(other.m_counter.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
            other.m_counter.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    /// @brief Add delta to the counter (can be negative)
    /// @param delta Amount to add (default 1)
    void add(int32_t delta = 1)
    {
        const int32_t newValue = m_counter.fetch_add(delta, std::memory_order_acq_rel) + delta;

        if (newValue < 0)
        {
            // This is a programming error - counter went negative
            std::terminate();
        }

        if (newValue == 0)
        {
            // Wake all waiters
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cv.notify_all();
        }
    }

    /// @brief Decrement the counter by 1 (called when a task completes)
    void done()
    {
        add(-1);
    }

    /// @brief Block until counter reaches zero (blocking wait, no help-run)
    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]() {
            return m_counter.load(std::memory_order_acquire) == 0;
        });
    }

    /// @brief Non-blocking check if all tasks are done
    [[nodiscard]] bool isDone() const
    {
        return m_counter.load(std::memory_order_acquire) == 0;
    }

    /// @brief Get current counter value
    [[nodiscard]] int32_t count() const
    {
        return m_counter.load(std::memory_order_acquire);
    }

    /// @brief Reset the counter (only safe when no waiters)
    void reset()
    {
        m_counter.store(0, std::memory_order_release);
    }

private:
    std::atomic<int32_t> m_counter{0};
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

/// @brief Shared pointer wrapper for WaitGroup to allow multiple owners
using WaitGroupPtr = std::shared_ptr<WaitGroup>;

/// @brief Create a new WaitGroup with initial count
inline WaitGroupPtr makeWaitGroup(int32_t initialCount = 0)
{
    auto wg = std::make_shared<WaitGroup>();
    if (initialCount > 0)
    {
        wg->add(initialCount);
    }
    return wg;
}

} // namespace vesper
