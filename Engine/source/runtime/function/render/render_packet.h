#pragma once

#include "runtime/core/base/macro.h"
#include "runtime/function/render/render_swap_context.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace vesper {

/// @brief Buffer mode for RenderPacket management
enum class RenderPacketBufferMode : uint8_t
{
    LatestOnly,     // Only keep latest packet, skip intermediate (low latency, may drop frames)
    Sequential,     // Process all packets in order (for recording/validation)
};

/// @brief A complete render packet containing all data for one frame
struct RenderPacket
{
    // Frame identification
    uint64_t frameIndex{0};
    uint64_t timestamp{0};      // Monotonic timestamp when packet was created

    // Camera data
    CameraParams camera{};

    // Visible objects for this frame
    std::vector<RenderObjectDesc> visibleObjects;

    // Object updates
    std::vector<RenderObjectDesc> objectsToAdd;
    std::vector<RenderObjectDesc> objectsToUpdate;
    std::vector<uint32_t> objectsToDelete;

    // Animation data
    std::vector<SkinningMatrices> skinningData;

    // Particle requests
    std::vector<ParticleRequest> particleRequests;

    // Resource loading requests
    bool levelResourceChanged{false};
    std::vector<uint32_t> meshesToLoad;
    std::vector<uint32_t> texturesToLoad;

    /// @brief Clear all data for reuse
    void clear()
    {
        visibleObjects.clear();
        objectsToAdd.clear();
        objectsToUpdate.clear();
        objectsToDelete.clear();
        skinningData.clear();
        particleRequests.clear();
        levelResourceChanged = false;
        meshesToLoad.clear();
        texturesToLoad.clear();
    }

    /// @brief Reserve capacity for expected data sizes
    void reserve(size_t objectCount, size_t skinCount = 0, size_t particleCount = 0)
    {
        visibleObjects.reserve(objectCount);
        objectsToAdd.reserve(objectCount / 10);     // Expect ~10% new per frame max
        objectsToUpdate.reserve(objectCount / 2);   // Expect ~50% updates per frame
        objectsToDelete.reserve(objectCount / 20);  // Expect ~5% deletes per frame
        skinningData.reserve(skinCount);
        particleRequests.reserve(particleCount);
    }
};

/// @brief Thread-safe double/triple buffer for RenderPacket exchange between logic and render threads
/// Logic thread writes, render thread reads
class RenderPacketBuffer
{
public:
    static constexpr uint32_t kDoubleBuffer = 2;
    static constexpr uint32_t kTripleBuffer = 3;

    explicit RenderPacketBuffer(uint32_t bufferCount = kTripleBuffer,
                                 RenderPacketBufferMode mode = RenderPacketBufferMode::LatestOnly);
    ~RenderPacketBuffer() = default;

    VESPER_DISABLE_COPY_AND_MOVE(RenderPacketBuffer)

    // ========================================================================
    // Logic Thread Interface (Producer)
    // ========================================================================

    /// @brief Acquire a packet for writing (logic thread)
    /// @return Pointer to packet for writing, or nullptr if no buffer available
    RenderPacket* acquireForWrite();

    /// @brief Release the packet after writing is complete (logic thread)
    /// This publishes the packet for the render thread to consume
    void releaseWrite();

    /// @brief Check if a write buffer is available
    [[nodiscard]] bool canWrite() const;

    // ========================================================================
    // Render Thread Interface (Consumer)
    // ========================================================================

    /// @brief Acquire the latest packet for reading (render thread)
    /// @return Pointer to packet for reading, or nullptr if none available
    RenderPacket* acquireForRead();

    /// @brief Release the packet after reading is complete (render thread)
    void releaseRead();

    /// @brief Check if a packet is available for reading
    [[nodiscard]] bool canRead() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    /// @brief Set buffer mode
    void setMode(RenderPacketBufferMode mode);

    /// @brief Get current mode
    [[nodiscard]] RenderPacketBufferMode mode() const { return m_mode; }

    /// @brief Get buffer count
    [[nodiscard]] uint32_t bufferCount() const { return m_bufferCount; }

    // ========================================================================
    // Statistics
    // ========================================================================

    /// @brief Get number of packets written
    [[nodiscard]] uint64_t packetsWritten() const
    {
        return m_packetsWritten.load(std::memory_order_relaxed);
    }

    /// @brief Get number of packets read
    [[nodiscard]] uint64_t packetsRead() const
    {
        return m_packetsRead.load(std::memory_order_relaxed);
    }

    /// @brief Get number of packets skipped (in LatestOnly mode)
    [[nodiscard]] uint64_t packetsSkipped() const
    {
        return m_packetsSkipped.load(std::memory_order_relaxed);
    }

private:
    // Buffer states
    enum class BufferState : uint32_t
    {
        Empty,          // Available for writing
        Writing,        // Currently being written by logic thread
        Ready,          // Written and ready for reading
        Reading,        // Currently being read by render thread
    };

    struct BufferSlot
    {
        RenderPacket packet;
        std::atomic<BufferState> state{BufferState::Empty};
        uint64_t sequenceNumber{0};

        BufferSlot() = default;
        ~BufferSlot() = default;

        // Non-copyable, non-movable due to atomic
        BufferSlot(const BufferSlot&) = delete;
        BufferSlot& operator=(const BufferSlot&) = delete;
        BufferSlot(BufferSlot&&) = delete;
        BufferSlot& operator=(BufferSlot&&) = delete;
    };

    uint32_t findWriteBuffer();
    uint32_t findReadBuffer();

private:
    std::vector<std::unique_ptr<BufferSlot>> m_buffers;
    uint32_t m_bufferCount;
    RenderPacketBufferMode m_mode;

    // Current write/read indices
    std::atomic<uint32_t> m_writeIndex{0};
    std::atomic<uint32_t> m_readIndex{0};

    // Sequence numbers for ordering
    std::atomic<uint64_t> m_writeSequence{0};
    std::atomic<uint64_t> m_readSequence{0};

    // Statistics
    std::atomic<uint64_t> m_packetsWritten{0};
    std::atomic<uint64_t> m_packetsRead{0};
    std::atomic<uint64_t> m_packetsSkipped{0};

    // Synchronization for blocking operations
    mutable std::mutex m_mutex;
    std::condition_variable m_writeCV;   // Notified when a buffer becomes empty (for writers)
    std::condition_variable m_readCV;    // Notified when a buffer becomes ready (for readers)
};

} // namespace vesper
