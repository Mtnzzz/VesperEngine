#include "runtime/function/render/deferred_deletion.h"
#include "runtime/core/log/log_system.h"

namespace vesper {

DeferredDeletionQueue::~DeferredDeletionQueue()
{
    if (!m_queue.empty())
    {
        LOG_WARN("DeferredDeletionQueue: {} pending deletions at destruction, forcing flush",
                 m_queue.size());
        flushAll();
    }
}

void DeferredDeletionQueue::queue(uint64_t fenceValue, std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(DeletionEntry{fenceValue, std::move(callback)});
}

uint32_t DeferredDeletionQueue::processCompleted(uint64_t completedFenceValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t count = 0;

    // Process all entries with fence value <= completedFenceValue
    while (!m_queue.empty())
    {
        const auto& top = m_queue.top();
        if (top.fenceValue > completedFenceValue)
        {
            break; // Not yet safe to delete
        }

        // Execute deletion callback
        if (top.deletionCallback)
        {
            top.deletionCallback();
        }

        m_queue.pop();
        ++count;
        ++m_totalDeleted;
    }

    return count;
}

void DeferredDeletionQueue::flushAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    while (!m_queue.empty())
    {
        const auto& top = m_queue.top();
        if (top.deletionCallback)
        {
            top.deletionCallback();
        }
        m_queue.pop();
        ++m_totalDeleted;
    }
}

size_t DeferredDeletionQueue::pendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

} // namespace vesper
