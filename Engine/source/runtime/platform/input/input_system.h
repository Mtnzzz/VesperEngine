#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vesper {

/// @brief Game command bitmask for gameplay input
enum class GameCommand : uint32_t
{
    None     = 0,
    Forward  = 1 << 0,
    Backward = 1 << 1,
    Left     = 1 << 2,
    Right    = 1 << 3,
    Jump     = 1 << 4,
    Crouch   = 1 << 5,
    Sprint   = 1 << 6,
};

inline GameCommand operator|(GameCommand a, GameCommand b)
{
    return static_cast<GameCommand>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline GameCommand& operator|=(GameCommand& a, GameCommand b)
{
    return a = a | b;
}

inline GameCommand operator&(GameCommand a, GameCommand b)
{
    return static_cast<GameCommand>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline GameCommand operator~(GameCommand a)
{
    return static_cast<GameCommand>(~static_cast<uint32_t>(a));
}

inline GameCommand& operator&=(GameCommand& a, GameCommand b)
{
    return a = a & b;
}

/// @brief Input system handling keyboard, mouse, and other input devices
/// This is a low-level system that does not directly interact with GLFW callbacks.
/// WindowSystem is responsible for forwarding input events to this system.
class InputSystem
{
public:
    InputSystem() = default;
    ~InputSystem();

    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// @brief Initialize input system
    /// @param window Pointer to the GLFW window (for cursor queries)
    /// @return true if initialization successful
    bool initialize(GLFWwindow* window);

    /// @brief Shutdown and cleanup
    void shutdown();

    /// @brief Called once per frame to reset per-frame input state
    void tick();

    // =========================================================================
    // Input Event Handlers (called by WindowSystem)
    // =========================================================================

    /// @brief Handle key event
    void onKey(int key, int scancode, int action, int mods);

    /// @brief Handle character input event
    void onChar(uint32_t codepoint);

    /// @brief Handle mouse button event
    void onMouseButton(int button, int action, int mods);

    /// @brief Handle cursor position event
    void onCursorPos(double xpos, double ypos);

    /// @brief Handle cursor enter/leave event
    void onCursorEnter(bool entered);

    /// @brief Handle scroll event
    void onScroll(double xoffset, double yoffset);

    /// @brief Handle file drop event
    void onDrop(int count, const char** paths);

    // =========================================================================
    // Keyboard Input Queries
    // =========================================================================

    /// @brief Check if a key is currently pressed
    bool isKeyPressed(int key) const;

    /// @brief Check if a key was just pressed this frame
    bool isKeyDown(int key) const;

    /// @brief Check if a key was just released this frame
    bool isKeyUp(int key) const;

    // =========================================================================
    // Mouse Input Queries
    // =========================================================================

    /// @brief Check if a mouse button is currently pressed
    bool isMouseButtonPressed(int button) const;

    /// @brief Check if a mouse button was just pressed this frame
    bool isMouseButtonDown(int button) const;

    /// @brief Check if a mouse button was just released this frame
    bool isMouseButtonUp(int button) const;

    /// @brief Get current cursor position
    std::array<double, 2> getCursorPosition() const;

    /// @brief Get cursor movement delta since last frame
    std::array<double, 2> getCursorDelta() const { return {m_cursorDeltaX, m_cursorDeltaY}; }

    /// @brief Get scroll wheel delta since last frame
    std::array<double, 2> getScrollDelta() const { return {m_scrollDeltaX, m_scrollDeltaY}; }

    // =========================================================================
    // Focus Mode (FPS-style cursor capture)
    // =========================================================================

    /// @brief Get focus mode state
    bool getFocusMode() const { return m_focusMode; }

    /// @brief Set focus mode (captures cursor for FPS-style control)
    void setFocusMode(bool enabled);

    /// @brief Set cursor mode directly (GLFW_CURSOR_NORMAL, GLFW_CURSOR_HIDDEN, GLFW_CURSOR_DISABLED)
    void setCursorMode(int mode);

    // =========================================================================
    // Game Commands
    // =========================================================================

    /// @brief Get current game command state
    GameCommand getGameCommand() const { return m_gameCommand; }

    /// @brief Get cursor delta for yaw rotation (horizontal)
    float getCursorDeltaYaw() const { return static_cast<float>(m_cursorDeltaX); }

    /// @brief Get cursor delta for pitch rotation (vertical)
    float getCursorDeltaPitch() const { return static_cast<float>(m_cursorDeltaY); }

    // =========================================================================
    // Callback Registration (for external listeners)
    // =========================================================================

    using KeyCallback         = std::function<void(int key, int scancode, int action, int mods)>;
    using CharCallback        = std::function<void(uint32_t codepoint)>;
    using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
    using CursorPosCallback   = std::function<void(double xpos, double ypos)>;
    using CursorEnterCallback = std::function<void(bool entered)>;
    using ScrollCallback      = std::function<void(double xoffset, double yoffset)>;
    /// @note Paths are copied to std::vector<std::string> to ensure safe lifetime beyond callback
    using DropCallback        = std::function<void(const std::vector<std::string>& paths)>;

    void registerKeyCallback(KeyCallback callback) { m_keyCallbacks.push_back(std::move(callback)); }
    void registerCharCallback(CharCallback callback) { m_charCallbacks.push_back(std::move(callback)); }
    void registerMouseButtonCallback(MouseButtonCallback callback) { m_mouseButtonCallbacks.push_back(std::move(callback)); }
    void registerCursorPosCallback(CursorPosCallback callback) { m_cursorPosCallbacks.push_back(std::move(callback)); }
    void registerCursorEnterCallback(CursorEnterCallback callback) { m_cursorEnterCallbacks.push_back(std::move(callback)); }
    void registerScrollCallback(ScrollCallback callback) { m_scrollCallbacks.push_back(std::move(callback)); }
    void registerDropCallback(DropCallback callback) { m_dropCallbacks.push_back(std::move(callback)); }

private:
    // Update game command based on key state
    void updateGameCommand(int key, int action);

private:
    GLFWwindow* m_window{nullptr};
    bool        m_initialized{false};
    bool        m_focusMode{false};

    // Cursor state
    double m_cursorX{0.0};
    double m_cursorY{0.0};
    double m_lastCursorX{0.0};
    double m_lastCursorY{0.0};
    double m_cursorDeltaX{0.0};
    double m_cursorDeltaY{0.0};
    bool   m_firstCursorUpdate{true};

    // Scroll state
    double m_scrollDeltaX{0.0};
    double m_scrollDeltaY{0.0};

    // Key state tracking (for isKeyDown/isKeyUp)
    static constexpr int MAX_KEYS = GLFW_KEY_LAST + 1;
    bool m_keyPressed[MAX_KEYS]{};
    bool m_keyDown[MAX_KEYS]{};
    bool m_keyUp[MAX_KEYS]{};

    // Mouse button state tracking
    static constexpr int MAX_MOUSE_BUTTONS = GLFW_MOUSE_BUTTON_LAST + 1;
    bool m_mouseButtonPressed[MAX_MOUSE_BUTTONS]{};
    bool m_mouseButtonDown[MAX_MOUSE_BUTTONS]{};
    bool m_mouseButtonUp[MAX_MOUSE_BUTTONS]{};

    // Game command state
    GameCommand m_gameCommand{GameCommand::None};

    // Callback storage
    std::vector<KeyCallback>         m_keyCallbacks;
    std::vector<CharCallback>        m_charCallbacks;
    std::vector<MouseButtonCallback> m_mouseButtonCallbacks;
    std::vector<CursorPosCallback>   m_cursorPosCallbacks;
    std::vector<CursorEnterCallback> m_cursorEnterCallbacks;
    std::vector<ScrollCallback>      m_scrollCallbacks;
    std::vector<DropCallback>        m_dropCallbacks;
};

} // namespace vesper
