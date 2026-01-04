#pragma once

#include "runtime/core/base/macro.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

namespace vesper {

/// @brief Deferred deletion queue for GPU resources
/// Resources are queued with a fence/timeline value and deleted when GPU has completed
class DeferredDeletionQueue
{
public:
    /// @brief Deletion entry with fence value
    struct DeletionEntry
    {
        uint64_t fenceValue;        // GPU fence/timeline value when safe to delete
        std::function<void()> deletionCallback;
    };

    DeferredDeletionQueue() = default;
    ~DeferredDeletionQueue();

    VESPER_DISABLE_COPY_AND_MOVE(DeferredDeletionQueue)

    /// @brief Queue a resource for deferred deletion
    /// @param fenceValue The fence value after which the resource can be deleted
    /// @param callback Deletion callback (usually destroy/free operation)
    void queue(uint64_t fenceValue, std::function<void()> callback);

    /// @brief Process completed deletions
    /// @param completedFenceValue Current completed fence value from GPU
    /// @return Number of resources deleted
    uint32_t processCompleted(uint64_t completedFenceValue);

    /// @brief Flush all pending deletions (for shutdown)
    /// WARNING: Only call when GPU is idle
    void flushAll();

    /// @brief Get number of pending deletions
    [[nodiscard]] size_t pendingCount() const;

    /// @brief Get statistics
    [[nodiscard]] uint64_t totalDeleted() const { return m_totalDeleted; }

private:
    struct EntryComparator
    {
        bool operator()(const DeletionEntry& a, const DeletionEntry& b) const
        {
            // Min-heap based on fence value
            return a.fenceValue > b.fenceValue;
        }
    };

    std::priority_queue<DeletionEntry, std::vector<DeletionEntry>, EntryComparator> m_queue;
    mutable std::mutex m_mutex;
    uint64_t m_totalDeleted{0};
};

/// @brief Typed deletion helper for specific resource types
template<typename T>
class TypedDeferredDeletion
{
public:
    TypedDeferredDeletion(DeferredDeletionQueue& queue)
        : m_queue(queue)
    {}

    /// @brief Queue a resource for deletion
    void queue(uint64_t fenceValue, T resource, std::function<void(T)> deleter)
    {
        m_queue.queue(fenceValue, [resource = std::move(resource), deleter = std::move(deleter)]() {
            deleter(resource);
        });
    }

private:
    DeferredDeletionQueue& m_queue;
};

} // namespace vesper
