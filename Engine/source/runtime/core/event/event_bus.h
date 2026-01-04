#pragma once

#include "runtime/core/base/macro.h"
#include "runtime/core/event/event_types.h"
#include "runtime/core/threading/spsc_queue.h"
#include "runtime/core/threading/mpsc_queue.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace vesper {

/// @brief Event channel configuration
struct EventChannelConfig
{
    std::string name;
    EventCategory categories{EventCategory::None};
    OverflowPolicy overflowPolicy{OverflowPolicy::DropNewest};
    bool enableStats{true};
};

/// @brief Type-erased event wrapper for queue storage
struct EventWrapper
{
    std::shared_ptr<Event> event;
    std::type_index typeIndex;

    EventWrapper()
        : typeIndex(typeid(void))
    {}

    template<typename T>
    explicit EventWrapper(std::shared_ptr<T> evt)
        : event(std::move(evt))
        , typeIndex(typeid(T))
    {}
};

/// @brief Event handler function type
template<typename EventType>
using EventHandler = std::function<void(const EventType&)>;

/// @brief Type-erased event handler
using GenericEventHandler = std::function<void(const Event&)>;

/// @brief Event channel for SPSC communication between threads
/// Each producer-consumer pair should have its own channel
template<size_t Capacity = 256>
class EventChannel
{
public:
    explicit EventChannel(EventChannelConfig config = {})
        : m_config(std::move(config))
    {}

    ~EventChannel()
    {
        shutdown();
    }

    VESPER_DISABLE_COPY_AND_MOVE(EventChannel)

    /// @brief Signal shutdown to unblock any waiting producers
    void shutdown()
    {
        m_queue.shutdown();
    }

    /// @brief Publish an event to the channel
    /// @param event Event to publish
    /// @return true if event was queued, false if dropped
    template<typename T>
    bool publish(std::shared_ptr<T> event)
    {
        static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");

        // Set timestamp if not set
        if (event->timestamp == 0)
        {
            event->timestamp = getTimestamp();
        }

        EventWrapper wrapper(std::move(event));
        return m_queue.enqueue(std::move(wrapper), m_config.overflowPolicy);
    }

    /// @brief Publish an event (convenience overload)
    template<typename T>
    bool publish(T event)
    {
        return publish(std::make_shared<T>(std::move(event)));
    }

    /// @brief Try to consume an event from the channel
    /// @return Event if available
    std::optional<EventWrapper> tryConsume()
    {
        return m_queue.tryDequeue();
    }

    /// @brief Consume all available events and dispatch to handler
    /// @param handler Function to call for each event
    /// @return Number of events processed
    template<typename Handler>
    size_t consumeAll(Handler&& handler)
    {
        size_t count = 0;
        while (auto wrapper = tryConsume())
        {
            if (wrapper->event)
            {
                handler(*wrapper->event, wrapper->typeIndex);
            }
            ++count;
        }
        return count;
    }

    /// @brief Check if channel is empty
    [[nodiscard]] bool isEmpty() const { return m_queue.isEmpty(); }

    /// @brief Get channel name
    [[nodiscard]] const std::string& name() const { return m_config.name; }

    /// @brief Get channel statistics
    [[nodiscard]] const QueueStats& stats() const { return m_queue.stats(); }

    /// @brief Get configuration
    [[nodiscard]] const EventChannelConfig& config() const { return m_config; }

private:
    static uint64_t getTimestamp()
    {
        using namespace std::chrono;
        return duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }

private:
    SPSCQueue<EventWrapper, Capacity> m_queue;
    EventChannelConfig m_config;
};

/// @brief Multi-producer event channel using MPSC queue
template<size_t Capacity = 256>
class MPEventChannel
{
public:
    explicit MPEventChannel(EventChannelConfig config = {})
        : m_config(std::move(config))
    {}

    ~MPEventChannel()
    {
        shutdown();
    }

    VESPER_DISABLE_COPY_AND_MOVE(MPEventChannel)

    /// @brief Signal shutdown to unblock any waiting producers
    void shutdown()
    {
        m_queue.shutdown();
    }

    /// @brief Publish an event to the channel (thread-safe)
    template<typename T>
    bool publish(std::shared_ptr<T> event)
    {
        static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");

        if (event->timestamp == 0)
        {
            event->timestamp = getTimestamp();
        }

        EventWrapper wrapper(std::move(event));
        return m_queue.enqueue(std::move(wrapper), m_config.overflowPolicy);
    }

    template<typename T>
    bool publish(T event)
    {
        return publish(std::make_shared<T>(std::move(event)));
    }

    std::optional<EventWrapper> tryConsume()
    {
        return m_queue.tryDequeue();
    }

    template<typename Handler>
    size_t consumeAll(Handler&& handler)
    {
        size_t count = 0;
        while (auto wrapper = tryConsume())
        {
            if (wrapper->event)
            {
                handler(*wrapper->event, wrapper->typeIndex);
            }
            ++count;
        }
        return count;
    }

    [[nodiscard]] bool isEmpty() const { return m_queue.isEmpty(); }
    [[nodiscard]] const std::string& name() const { return m_config.name; }
    [[nodiscard]] const QueueStats& stats() const { return m_queue.stats(); }
    [[nodiscard]] const EventChannelConfig& config() const { return m_config; }

private:
    static uint64_t getTimestamp()
    {
        using namespace std::chrono;
        return duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }

private:
    MPSCQueue<EventWrapper, Capacity> m_queue;
    EventChannelConfig m_config;
};

/// @brief Central event bus managing multiple channels and handlers
class EventBus
{
public:
    /// @brief Default channel capacities
    static constexpr size_t kWindowInputCapacity = 256;
    static constexpr size_t kResourceCapacity = 64;
    static constexpr size_t kGPUCapacity = 32;
    static constexpr size_t kDiagnosticCapacity = 128;

    EventBus();
    ~EventBus();

    VESPER_DISABLE_COPY_AND_MOVE(EventBus)

    /// @brief Shutdown all channels to unblock waiting producers
    void shutdown();

    // ========================================================================
    // Pre-configured channels
    // ========================================================================

    /// @brief Get window/input event channel (single producer: main thread)
    auto& windowInputChannel() { return m_windowInputChannel; }

    /// @brief Get resource event channel (multi-producer: loader threads)
    auto& resourceChannel() { return m_resourceChannel; }

    /// @brief Get GPU event channel (single producer: render thread)
    auto& gpuChannel() { return m_gpuChannel; }

    /// @brief Get diagnostic event channel (multi-producer)
    auto& diagnosticChannel() { return m_diagnosticChannel; }

    // ========================================================================
    // Handler registration
    // ========================================================================

    /// @brief Register a typed event handler
    /// @param handler Function to call when event of type T is received
    /// @return Handler ID for unregistration
    template<typename T>
    uint64_t subscribe(EventHandler<T> handler)
    {
        static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");

        std::lock_guard<std::mutex> lock(m_handlerMutex);

        uint64_t id = m_nextHandlerId++;
        m_handlers[typeid(T)].emplace_back(
            id,
            [handler = std::move(handler)](const Event& evt) {
                handler(static_cast<const T&>(evt));
            }
        );

        return id;
    }

    /// @brief Unsubscribe a handler by ID
    void unsubscribe(uint64_t handlerId);

    /// @brief Dispatch an event to all registered handlers
    /// @note Handlers are copied before invocation to allow safe subscribe/unsubscribe during dispatch
    template<typename T>
    void dispatch(const T& event)
    {
        static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");

        // Copy handlers under lock, then invoke outside lock to avoid deadlock
        std::vector<GenericEventHandler> handlersCopy;
        {
            std::lock_guard<std::mutex> lock(m_handlerMutex);
            auto it = m_handlers.find(typeid(T));
            if (it != m_handlers.end())
            {
                handlersCopy.reserve(it->second.size());
                for (const auto& [id, handler] : it->second)
                {
                    handlersCopy.push_back(handler);
                }
            }
        }

        // Invoke handlers outside the lock
        for (const auto& handler : handlersCopy)
        {
            handler(event);
        }
    }

    /// @brief Dispatch an event by type index
    void dispatchByType(const Event& event, std::type_index typeIndex);

    // ========================================================================
    // Processing
    // ========================================================================

    /// @brief Process all pending events in all channels
    void processAllChannels();

    /// @brief Get combined statistics from all channels
    void logStats() const;

private:
    // Pre-configured channels for different event types
    EventChannel<kWindowInputCapacity> m_windowInputChannel;
    MPEventChannel<kResourceCapacity> m_resourceChannel;
    EventChannel<kGPUCapacity> m_gpuChannel;
    MPEventChannel<kDiagnosticCapacity> m_diagnosticChannel;

    // Handler storage
    using HandlerEntry = std::pair<uint64_t, GenericEventHandler>;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
    std::mutex m_handlerMutex;
    std::atomic<uint64_t> m_nextHandlerId{1};
};

} // namespace vesper
