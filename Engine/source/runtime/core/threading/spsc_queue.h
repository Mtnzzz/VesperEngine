#pragma once

#include "runtime/core/base/macro.h"

#include <atomic>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <new>

namespace vesper {

/// @brief Overflow policy for bounded queues
enum class OverflowPolicy : uint8_t
{
    Block,          // Block producer until space available (uses condition variable)
    DropNewest,     // Drop the new item if queue is full
};

/// @brief Statistics for queue monitoring
struct QueueStats
{
    std::atomic<uint64_t> enqueue_count{0};
    std::atomic<uint64_t> dequeue_count{0};
    std::atomic<uint64_t> drop_count{0};
    std::atomic<uint32_t> peak_depth{0};

    void reset()
    {
        enqueue_count.store(0, std::memory_order_relaxed);
        dequeue_count.store(0, std::memory_order_relaxed);
        drop_count.store(0, std::memory_order_relaxed);
        peak_depth.store(0, std::memory_order_relaxed);
    }
};

/// @brief Lock-free Single Producer Single Consumer (SPSC) bounded queue
/// @tparam T Element type
/// @tparam Capacity Maximum number of elements (must be power of 2)
template<typename T, size_t Capacity>
class SPSCQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    SPSCQueue() = default;
    ~SPSCQueue() = default;

    VESPER_DISABLE_COPY_AND_MOVE(SPSCQueue)

    /// @brief Signal shutdown to unblock waiting producers
    void shutdown()
    {
        m_shutdown.store(true, std::memory_order_release);
        m_cv.notify_all();
    }

    /// @brief Check if queue is in shutdown state
    [[nodiscard]] bool isShutdown() const
    {
        return m_shutdown.load(std::memory_order_acquire);
    }

    /// @brief Try to enqueue an item (non-blocking)
    /// @param item Item to enqueue
    /// @return true if successful, false if queue is full
    bool tryEnqueue(const T& item)
    {
        return tryEnqueueImpl(item);
    }

    /// @brief Try to enqueue an item (non-blocking, move version)
    /// @param item Item to enqueue
    /// @return true if successful, false if queue is full
    bool tryEnqueue(T&& item)
    {
        return tryEnqueueImpl(std::move(item));
    }

    /// @brief Enqueue with specified overflow policy
    /// @param item Item to enqueue
    /// @param policy How to handle overflow
    /// @return true if item was enqueued, false if dropped (DropNewest) or shutdown (Block)
    template<typename U>
    bool enqueue(U&& item, OverflowPolicy policy)
    {
        switch (policy)
        {
            case OverflowPolicy::Block:
            {
                std::unique_lock<std::mutex> lock(m_mutex);

                // Wait until space available or shutdown
                m_cv.wait(lock, [this]() {
                    return !isFull() || m_shutdown.load(std::memory_order_acquire);
                });

                // Check if we woke up due to shutdown
                if (m_shutdown.load(std::memory_order_acquire))
                {
                    m_stats.drop_count.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }

                // Now we have space, enqueue the item
                return tryEnqueueImpl(std::forward<U>(item));
            }

            case OverflowPolicy::DropNewest:
                if (!tryEnqueue(std::forward<U>(item)))
                {
                    m_stats.drop_count.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
                return true;
        }
        return false;
    }

    /// @brief Try to dequeue an item (non-blocking)
    /// @param item Output item
    /// @return true if successful, false if queue is empty
    bool tryDequeue(T& item)
    {
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        const size_t head = m_head.load(std::memory_order_acquire);

        if (tail == head)
        {
            return false; // Queue is empty
        }

        item = std::move(m_buffer[tail & kIndexMask]);
        m_tail.store(tail + 1, std::memory_order_release);

        m_stats.dequeue_count.fetch_add(1, std::memory_order_relaxed);

        // Notify waiting producers that space is available
        m_cv.notify_one();

        return true;
    }

    /// @brief Try to dequeue an item (non-blocking)
    /// @return Optional containing the item, or empty if queue is empty
    std::optional<T> tryDequeue()
    {
        T item;
        if (tryDequeue(item))
        {
            return std::move(item);
        }
        return std::nullopt;
    }

    /// @brief Check if queue is empty
    [[nodiscard]] bool isEmpty() const
    {
        return m_head.load(std::memory_order_acquire) ==
               m_tail.load(std::memory_order_acquire);
    }

    /// @brief Check if queue is full
    [[nodiscard]] bool isFull() const
    {
        return size() >= Capacity;
    }

    /// @brief Get current number of elements in queue
    [[nodiscard]] size_t size() const
    {
        const size_t head = m_head.load(std::memory_order_acquire);
        const size_t tail = m_tail.load(std::memory_order_acquire);
        return head - tail;
    }

    /// @brief Get queue capacity
    [[nodiscard]] static constexpr size_t capacity() { return Capacity; }

    /// @brief Get queue statistics
    [[nodiscard]] const QueueStats& stats() const { return m_stats; }

    /// @brief Reset statistics
    void resetStats() { m_stats.reset(); }

private:
    template<typename U>
    bool tryEnqueueImpl(U&& item)
    {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t tail = m_tail.load(std::memory_order_acquire);

        if (head - tail >= Capacity)
        {
            return false; // Queue is full
        }

        m_buffer[head & kIndexMask] = std::forward<U>(item);
        m_head.store(head + 1, std::memory_order_release);

        m_stats.enqueue_count.fetch_add(1, std::memory_order_relaxed);

        // Update peak depth
        const uint32_t currentDepth = static_cast<uint32_t>(head + 1 - tail);
        uint32_t peak = m_stats.peak_depth.load(std::memory_order_relaxed);
        while (currentDepth > peak)
        {
            if (m_stats.peak_depth.compare_exchange_weak(peak, currentDepth,
                std::memory_order_relaxed))
            {
                break;
            }
        }

        return true;
    }

private:
    static constexpr size_t kIndexMask = Capacity - 1;

    // Cache line padding to prevent false sharing
    static constexpr size_t kCacheLineSize = 64;

    alignas(kCacheLineSize) std::atomic<size_t> m_head{0};
    alignas(kCacheLineSize) std::atomic<size_t> m_tail{0};
    alignas(kCacheLineSize) std::array<T, Capacity> m_buffer{};

    // Synchronization for Block policy
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_shutdown{false};

    QueueStats m_stats;
};

} // namespace vesper
