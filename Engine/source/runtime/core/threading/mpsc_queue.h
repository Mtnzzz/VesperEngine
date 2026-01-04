#pragma once

#include "runtime/core/base/macro.h"
#include "runtime/core/threading/spsc_queue.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace vesper {

/// @brief Lock-free Multi Producer Single Consumer (MPSC) bounded queue
/// Uses a mutex for producers and lock-free consumer reads
/// @tparam T Element type
/// @tparam Capacity Maximum number of elements (must be power of 2)
template<typename T, size_t Capacity>
class MPSCQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    MPSCQueue() = default;
    ~MPSCQueue() = default;

    VESPER_DISABLE_COPY_AND_MOVE(MPSCQueue)

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

    /// @brief Try to enqueue an item (thread-safe for multiple producers)
    /// @param item Item to enqueue
    /// @return true if successful, false if queue is full
    bool tryEnqueue(const T& item)
    {
        std::lock_guard<std::mutex> lock(m_producerMutex);
        return tryEnqueueImpl(item);
    }

    /// @brief Try to enqueue an item (thread-safe for multiple producers, move version)
    /// @param item Item to enqueue
    /// @return true if successful, false if queue is full
    bool tryEnqueue(T&& item)
    {
        std::lock_guard<std::mutex> lock(m_producerMutex);
        return tryEnqueueImpl(std::move(item));
    }

    /// @brief Enqueue with specified overflow policy (thread-safe)
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
                std::unique_lock<std::mutex> lock(m_producerMutex);

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
            {
                std::lock_guard<std::mutex> lock(m_producerMutex);
                if (!tryEnqueueImpl(std::forward<U>(item)))
                {
                    m_stats.drop_count.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
                return true;
            }
        }
        return false;
    }

    /// @brief Try to dequeue an item (single consumer, no lock needed)
    /// @param item Output item
    /// @return true if successful, false if queue is empty
    bool tryDequeue(T& item)
    {
        return tryDequeueImpl(item);
    }

    /// @brief Try to dequeue an item (single consumer)
    /// @return Optional containing the item, or empty if queue is empty
    std::optional<T> tryDequeue()
    {
        T item;
        if (tryDequeueImpl(item))
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

    bool tryDequeueImpl(T& item)
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

private:
    static constexpr size_t kIndexMask = Capacity - 1;
    static constexpr size_t kCacheLineSize = 64;

    alignas(kCacheLineSize) std::atomic<size_t> m_head{0};
    alignas(kCacheLineSize) std::atomic<size_t> m_tail{0};
    alignas(kCacheLineSize) std::array<T, Capacity> m_buffer{};

    // Synchronization for producers and Block policy
    std::mutex m_producerMutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_shutdown{false};

    QueueStats m_stats;
};

} // namespace vesper
