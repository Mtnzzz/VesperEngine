#include "runtime/function/render/render_packet.h"
#include "runtime/core/log/log_system.h"

#include <chrono>
#include <limits>
#include <thread>

namespace vesper {

RenderPacketBuffer::RenderPacketBuffer(uint32_t bufferCount, RenderPacketBufferMode mode)
    : m_bufferCount(std::max(bufferCount, kDoubleBuffer))
    , m_mode(mode)
{
    m_buffers.reserve(m_bufferCount);
    for (uint32_t i = 0; i < m_bufferCount; ++i)
    {
        m_buffers.push_back(std::make_unique<BufferSlot>());
    }

    LOG_INFO("RenderPacketBuffer: Created with {} buffers, mode={}",
             m_bufferCount,
             mode == RenderPacketBufferMode::LatestOnly ? "LatestOnly" : "Sequential");
}

RenderPacket* RenderPacketBuffer::acquireForWrite()
{
    uint32_t index = findWriteBuffer();

    if (index == UINT32_MAX)
    {
        if (m_mode == RenderPacketBufferMode::Sequential)
        {
            // Block until a buffer becomes available using condition variable
            std::unique_lock<std::mutex> lock(m_mutex);
            m_writeCV.wait(lock, [this, &index]() {
                index = findWriteBuffer();
                return index != UINT32_MAX;
            });
        }
        else
        {
            // LatestOnly: shouldn't happen with proper buffer count
            // Only log once to avoid flooding the console
            static bool warnedOnce = false;
            if (!warnedOnce)
            {
                LOG_WARN("RenderPacketBuffer: No write buffer available (this warning will only appear once)");
                warnedOnce = true;
            }
            return nullptr;
        }
    }

    // Mark as writing
    BufferState expected = BufferState::Empty;
    if (!m_buffers[index]->state.compare_exchange_strong(expected, BufferState::Writing,
        std::memory_order_acq_rel))
    {
        // Race condition, try again
        return acquireForWrite();
    }

    m_writeIndex.store(index, std::memory_order_release);

    // Clear the packet for reuse
    m_buffers[index]->packet.clear();

    return &m_buffers[index]->packet;
}

void RenderPacketBuffer::releaseWrite()
{
    uint32_t index = m_writeIndex.load(std::memory_order_acquire);

    auto& slot = *m_buffers[index];

    // Set frame info
    slot.sequenceNumber = m_writeSequence.fetch_add(1, std::memory_order_relaxed);
    slot.packet.frameIndex = slot.sequenceNumber;
    slot.packet.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    // Publish the packet
    slot.state.store(BufferState::Ready, std::memory_order_release);

    m_packetsWritten.fetch_add(1, std::memory_order_relaxed);

    // Notify waiting readers that a packet is ready
    m_readCV.notify_one();
}

bool RenderPacketBuffer::canWrite() const
{
    return const_cast<RenderPacketBuffer*>(this)->findWriteBuffer() != UINT32_MAX;
}

RenderPacket* RenderPacketBuffer::acquireForRead()
{
    uint32_t index = findReadBuffer();

    if (index == UINT32_MAX)
    {
        // No packet available
        return nullptr;
    }

    // Mark as reading
    BufferState expected = BufferState::Ready;
    if (!m_buffers[index]->state.compare_exchange_strong(expected, BufferState::Reading,
        std::memory_order_acq_rel))
    {
        // Race condition or state changed, try again
        return acquireForRead();
    }

    m_readIndex.store(index, std::memory_order_release);

    return &m_buffers[index]->packet;
}

void RenderPacketBuffer::releaseRead()
{
    uint32_t index = m_readIndex.load(std::memory_order_acquire);

    // Update read sequence
    m_readSequence.store(m_buffers[index]->sequenceNumber + 1, std::memory_order_release);

    // Mark as empty (available for writing again)
    m_buffers[index]->state.store(BufferState::Empty, std::memory_order_release);

    m_packetsRead.fetch_add(1, std::memory_order_relaxed);

    // Notify waiting writers that a buffer is available
    m_writeCV.notify_one();
}

bool RenderPacketBuffer::canRead() const
{
    return const_cast<RenderPacketBuffer*>(this)->findReadBuffer() != UINT32_MAX;
}

void RenderPacketBuffer::setMode(RenderPacketBufferMode mode)
{
    m_mode = mode;
}

uint32_t RenderPacketBuffer::findWriteBuffer()
{
    // Find an empty buffer
    for (uint32_t i = 0; i < m_bufferCount; ++i)
    {
        if (m_buffers[i]->state.load(std::memory_order_acquire) == BufferState::Empty)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t RenderPacketBuffer::findReadBuffer()
{
    if (m_mode == RenderPacketBufferMode::LatestOnly)
    {
        // Find the ready buffer with highest sequence number
        uint32_t bestIndex = UINT32_MAX;
        uint64_t bestSequence = 0;

        for (uint32_t i = 0; i < m_bufferCount; ++i)
        {
            if (m_buffers[i]->state.load(std::memory_order_acquire) == BufferState::Ready)
            {
                if (bestIndex == UINT32_MAX || m_buffers[i]->sequenceNumber > bestSequence)
                {
                    // Skip old packets - mark them as empty
                    if (bestIndex != UINT32_MAX)
                    {
                        BufferState expected = BufferState::Ready;
                        if (m_buffers[bestIndex]->state.compare_exchange_strong(
                            expected, BufferState::Empty, std::memory_order_acq_rel))
                        {
                            m_packetsSkipped.fetch_add(1, std::memory_order_relaxed);
                        }
                    }

                    bestIndex = i;
                    bestSequence = m_buffers[i]->sequenceNumber;
                }
                else
                {
                    // This packet is older, skip it
                    BufferState expected = BufferState::Ready;
                    if (m_buffers[i]->state.compare_exchange_strong(
                        expected, BufferState::Empty, std::memory_order_acq_rel))
                    {
                        m_packetsSkipped.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }

        return bestIndex;
    }
    else // Sequential mode
    {
        // Find the ready buffer with the expected sequence number
        uint64_t expectedSeq = m_readSequence.load(std::memory_order_acquire);

        for (uint32_t i = 0; i < m_bufferCount; ++i)
        {
            if (m_buffers[i]->state.load(std::memory_order_acquire) == BufferState::Ready &&
                m_buffers[i]->sequenceNumber == expectedSeq)
            {
                return i;
            }
        }

        return UINT32_MAX;
    }
}

} // namespace vesper
