#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace vesper {

/// @brief Event categories for routing and filtering
enum class EventCategory : uint32_t
{
    None = 0,
    Window          = 1 << 0,   // Window resize, close, focus, etc.
    Input           = 1 << 1,   // Keyboard, mouse, gamepad
    Resource        = 1 << 2,   // Asset loading, hot reload
    GPU             = 1 << 3,   // GPU resource lifecycle, fence completion
    Physics         = 1 << 4,   // Collision, triggers
    Diagnostic      = 1 << 5,   // Profiling, statistics
    Application     = 1 << 6,   // Application-level events
};

inline EventCategory operator|(EventCategory a, EventCategory b)
{
    return static_cast<EventCategory>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EventCategory operator&(EventCategory a, EventCategory b)
{
    return static_cast<EventCategory>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasCategory(EventCategory value, EventCategory category)
{
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(category)) != 0;
}

/// @brief Base event structure
struct Event
{
    EventCategory category{EventCategory::None};
    uint64_t timestamp{0};  // Monotonic timestamp when event was generated

    virtual ~Event() = default;
};

// ============================================================================
// Window Events
// ============================================================================

struct WindowResizeEvent : Event
{
    uint32_t width{0};
    uint32_t height{0};

    WindowResizeEvent()
    {
        category = EventCategory::Window;
    }
};

struct WindowCloseEvent : Event
{
    WindowCloseEvent()
    {
        category = EventCategory::Window;
    }
};

struct WindowFocusEvent : Event
{
    bool focused{false};

    WindowFocusEvent()
    {
        category = EventCategory::Window;
    }
};

// ============================================================================
// Input Events
// ============================================================================

struct KeyEvent : Event
{
    int32_t key{0};
    int32_t scancode{0};
    int32_t action{0};  // 0 = release, 1 = press, 2 = repeat
    int32_t mods{0};

    KeyEvent()
    {
        category = EventCategory::Input;
    }
};

struct MouseButtonEvent : Event
{
    int32_t button{0};
    int32_t action{0};
    int32_t mods{0};

    MouseButtonEvent()
    {
        category = EventCategory::Input;
    }
};

struct MouseMoveEvent : Event
{
    double x{0.0};
    double y{0.0};

    MouseMoveEvent()
    {
        category = EventCategory::Input;
    }
};

struct MouseScrollEvent : Event
{
    double xOffset{0.0};
    double yOffset{0.0};

    MouseScrollEvent()
    {
        category = EventCategory::Input;
    }
};

// ============================================================================
// Resource Events
// ============================================================================

struct ResourceLoadedEvent : Event
{
    uint32_t resourceId{0};
    std::string resourcePath;
    bool success{false};

    ResourceLoadedEvent()
    {
        category = EventCategory::Resource;
    }
};

struct HotReloadEvent : Event
{
    std::string filePath;
    uint32_t resourceType{0};

    HotReloadEvent()
    {
        category = EventCategory::Resource;
    }
};

// ============================================================================
// GPU Events
// ============================================================================

struct GPUResourceReleasedEvent : Event
{
    uint64_t resourceHandle{0};
    uint32_t resourceType{0};  // Buffer, Image, etc.

    GPUResourceReleasedEvent()
    {
        category = EventCategory::GPU;
    }
};

struct GPUFenceCompletedEvent : Event
{
    uint64_t fenceValue{0};
    uint32_t queueIndex{0};

    GPUFenceCompletedEvent()
    {
        category = EventCategory::GPU;
    }
};

// ============================================================================
// Diagnostic Events
// ============================================================================

struct FrameTimingEvent : Event
{
    uint64_t frameIndex{0};
    float cpuTimeMs{0.0f};
    float gpuTimeMs{0.0f};
    float frameTimeMs{0.0f};

    FrameTimingEvent()
    {
        category = EventCategory::Diagnostic;
    }
};

struct QueueStatsEvent : Event
{
    std::string queueName;
    uint64_t enqueueCount{0};
    uint64_t dequeueCount{0};
    uint64_t dropCount{0};
    uint32_t peakDepth{0};

    QueueStatsEvent()
    {
        category = EventCategory::Diagnostic;
    }
};

} // namespace vesper
