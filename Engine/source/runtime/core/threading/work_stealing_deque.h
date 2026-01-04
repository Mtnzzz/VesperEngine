#pragma once

#include "runtime/core/base/macro.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace vesper {

/// @brief Lock-free work-stealing deque
/// Owner thread pushes/pops from bottom, thieves steal from top
/// Based on "Dynamic Circular Work-Stealing Deque" by Chase and Lev
/// @tparam T Element type
template<typename T>
class WorkStealingDeque
{
public:
    static constexpr size_t kInitialCapacity = 1024;

    WorkStealingDeque()
        : m_buffer(new CircularBuffer(kInitialCapacity))
        , m_top(0)
        , m_bottom(0)
    {}

    ~WorkStealingDeque() = default;

    VESPER_DISABLE_COPY_AND_MOVE(WorkStealingDeque)

    /// @brief Push an item to the bottom (owner thread only)
    void push(T item)
    {
        int64_t bottom = m_bottom.load(std::memory_order_relaxed);
        int64_t top = m_top.load(std::memory_order_acquire);
        CircularBuffer* buffer = m_buffer.load(std::memory_order_relaxed);

        if (bottom - top >= static_cast<int64_t>(buffer->capacity()) - 1)
        {
            // Grow the buffer
            buffer = resize(buffer, bottom, top);
            m_buffer.store(buffer, std::memory_order_relaxed);
        }

        buffer->put(bottom, std::move(item));
        std::atomic_thread_fence(std::memory_order_release);
        m_bottom.store(bottom + 1, std::memory_order_relaxed);
    }

    /// @brief Pop an item from the bottom (owner thread only)
    /// @return The item if available
    std::optional<T> pop()
    {
        int64_t bottom = m_bottom.load(std::memory_order_relaxed) - 1;
        CircularBuffer* buffer = m_buffer.load(std::memory_order_relaxed);
        m_bottom.store(bottom, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);

        int64_t top = m_top.load(std::memory_order_relaxed);

        if (top <= bottom)
        {
            // Non-empty queue
            T item = buffer->get(bottom);

            if (top == bottom)
            {
                // Last element - need CAS to prevent race with steal
                if (!m_top.compare_exchange_strong(top, top + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed))
                {
                    // Lost race to thief
                    m_bottom.store(bottom + 1, std::memory_order_relaxed);
                    return std::nullopt;
                }
                m_bottom.store(bottom + 1, std::memory_order_relaxed);
            }
            return item;
        }
        else
        {
            // Empty queue
            m_bottom.store(bottom + 1, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    /// @brief Steal an item from the top (thief threads)
    /// @return The item if available
    std::optional<T> steal()
    {
        int64_t top = m_top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int64_t bottom = m_bottom.load(std::memory_order_acquire);

        if (top < bottom)
        {
            // Non-empty queue
            CircularBuffer* buffer = m_buffer.load(std::memory_order_consume);
            T item = buffer->get(top);

            if (!m_top.compare_exchange_strong(top, top + 1,
                std::memory_order_seq_cst, std::memory_order_relaxed))
            {
                // Lost race to another thief or owner
                return std::nullopt;
            }
            return item;
        }

        return std::nullopt;
    }

    /// @brief Check if deque is empty (approximate)
    [[nodiscard]] bool isEmpty() const
    {
        int64_t bottom = m_bottom.load(std::memory_order_relaxed);
        int64_t top = m_top.load(std::memory_order_relaxed);
        return bottom <= top;
    }

    /// @brief Get approximate size
    [[nodiscard]] size_t size() const
    {
        int64_t bottom = m_bottom.load(std::memory_order_relaxed);
        int64_t top = m_top.load(std::memory_order_relaxed);
        return static_cast<size_t>(std::max(bottom - top, int64_t(0)));
    }

private:
    /// @brief Circular buffer with power-of-2 capacity
    struct CircularBuffer
    {
        std::unique_ptr<T[]> data;
        size_t mask;

        explicit CircularBuffer(size_t cap)
            : data(std::make_unique<T[]>(cap))
            , mask(cap - 1)
        {}

        [[nodiscard]] size_t capacity() const { return mask + 1; }

        void put(int64_t index, T item)
        {
            data[index & mask] = std::move(item);
        }

        T get(int64_t index)
        {
            return data[index & mask];
        }
    };

    CircularBuffer* resize(CircularBuffer* oldBuffer, int64_t bottom, int64_t top)
    {
        size_t newCapacity = oldBuffer->capacity() * 2;
        auto* newBuffer = new CircularBuffer(newCapacity);

        // Copy elements
        for (int64_t i = top; i < bottom; ++i)
        {
            newBuffer->put(i, oldBuffer->get(i));
        }

        // Store old buffer for deferred cleanup
        m_oldBuffers.push_back(std::unique_ptr<CircularBuffer>(oldBuffer));

        return newBuffer;
    }

private:
    std::atomic<CircularBuffer*> m_buffer;
    std::atomic<int64_t> m_top;
    std::atomic<int64_t> m_bottom;

    // Keep old buffers alive until destruction to avoid ABA
    std::vector<std::unique_ptr<CircularBuffer>> m_oldBuffers;
};

} // namespace vesper
