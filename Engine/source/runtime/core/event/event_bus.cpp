#include "runtime/core/event/event_bus.h"
#include "runtime/core/log/log_system.h"

namespace vesper {

EventBus::EventBus()
    : m_windowInputChannel(EventChannelConfig{
        .name = "WindowInput",
        .categories = EventCategory::Window | EventCategory::Input,
        .overflowPolicy = OverflowPolicy::DropNewest,
        .enableStats = true
    })
    , m_resourceChannel(EventChannelConfig{
        .name = "Resource",
        .categories = EventCategory::Resource,
        .overflowPolicy = OverflowPolicy::Block,
        .enableStats = true
    })
    , m_gpuChannel(EventChannelConfig{
        .name = "GPU",
        .categories = EventCategory::GPU,
        .overflowPolicy = OverflowPolicy::Block,
        .enableStats = true
    })
    , m_diagnosticChannel(EventChannelConfig{
        .name = "Diagnostic",
        .categories = EventCategory::Diagnostic,
        .overflowPolicy = OverflowPolicy::DropNewest,
        .enableStats = true
    })
{
}

EventBus::~EventBus()
{
    shutdown();
}

void EventBus::shutdown()
{
    m_windowInputChannel.shutdown();
    m_resourceChannel.shutdown();
    m_gpuChannel.shutdown();
    m_diagnosticChannel.shutdown();
}

void EventBus::unsubscribe(uint64_t handlerId)
{
    std::lock_guard<std::mutex> lock(m_handlerMutex);

    for (auto& [typeIndex, handlers] : m_handlers)
    {
        auto it = std::remove_if(handlers.begin(), handlers.end(),
            [handlerId](const HandlerEntry& entry) {
                return entry.first == handlerId;
            });

        if (it != handlers.end())
        {
            handlers.erase(it, handlers.end());
            return;
        }
    }
}

void EventBus::dispatchByType(const Event& event, std::type_index typeIndex)
{
    // Copy handlers under lock, then invoke outside lock to avoid deadlock
    std::vector<GenericEventHandler> handlersCopy;
    {
        std::lock_guard<std::mutex> lock(m_handlerMutex);
        auto it = m_handlers.find(typeIndex);
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

void EventBus::processAllChannels()
{
    // Process window/input events
    m_windowInputChannel.consumeAll([this](const Event& event, std::type_index typeIndex) {
        dispatchByType(event, typeIndex);
    });

    // Process resource events
    m_resourceChannel.consumeAll([this](const Event& event, std::type_index typeIndex) {
        dispatchByType(event, typeIndex);
    });

    // Process GPU events
    m_gpuChannel.consumeAll([this](const Event& event, std::type_index typeIndex) {
        dispatchByType(event, typeIndex);
    });

    // Process diagnostic events
    m_diagnosticChannel.consumeAll([this](const Event& event, std::type_index typeIndex) {
        dispatchByType(event, typeIndex);
    });
}

void EventBus::logStats() const
{
    auto logChannelStats = [](const std::string& name, const QueueStats& stats) {
        LOG_DEBUG("EventChannel [{}]: enqueue={}, dequeue={}, drops={}, peak={}",
            name,
            stats.enqueue_count.load(std::memory_order_relaxed),
            stats.dequeue_count.load(std::memory_order_relaxed),
            stats.drop_count.load(std::memory_order_relaxed),
            stats.peak_depth.load(std::memory_order_relaxed));
    };

    logChannelStats(m_windowInputChannel.name(), m_windowInputChannel.stats());
    logChannelStats(m_resourceChannel.name(), m_resourceChannel.stats());
    logChannelStats(m_gpuChannel.name(), m_gpuChannel.stats());
    logChannelStats(m_diagnosticChannel.name(), m_diagnosticChannel.stats());
}

} // namespace vesper
