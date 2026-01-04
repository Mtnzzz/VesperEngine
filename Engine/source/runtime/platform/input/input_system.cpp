#include "runtime/platform/input/input_system.h"
#include "runtime/core/log/log_system.h"

#include <cstring>

namespace vesper {

InputSystem::~InputSystem()
{
    shutdown();
}

bool InputSystem::initialize(GLFWwindow* window)
{
    if (m_initialized)
    {
        LOG_WARN("InputSystem already initialized");
        return true;
    }

    if (!window)
    {
        LOG_ERROR("InputSystem::initialize - Invalid window pointer");
        return false;
    }

    m_window = window;

    // Initialize state arrays
    std::memset(m_keyPressed, 0, sizeof(m_keyPressed));
    std::memset(m_keyDown, 0, sizeof(m_keyDown));
    std::memset(m_keyUp, 0, sizeof(m_keyUp));
    std::memset(m_mouseButtonPressed, 0, sizeof(m_mouseButtonPressed));
    std::memset(m_mouseButtonDown, 0, sizeof(m_mouseButtonDown));
    std::memset(m_mouseButtonUp, 0, sizeof(m_mouseButtonUp));

    // Get initial cursor position
    glfwGetCursorPos(m_window, &m_cursorX, &m_cursorY);
    m_lastCursorX = m_cursorX;
    m_lastCursorY = m_cursorY;
    m_firstCursorUpdate = true;

    m_initialized = true;

    LOG_INFO("InputSystem initialized");

    return true;
}

void InputSystem::shutdown()
{
    if (!m_initialized)
        return;

    // Clear callback storage
    m_keyCallbacks.clear();
    m_charCallbacks.clear();
    m_mouseButtonCallbacks.clear();
    m_cursorPosCallbacks.clear();
    m_cursorEnterCallbacks.clear();
    m_scrollCallbacks.clear();
    m_dropCallbacks.clear();

    m_window = nullptr;
    m_initialized = false;

    LOG_INFO("InputSystem shutdown");
}

void InputSystem::tick()
{
    // Reset per-frame state
    std::memset(m_keyDown, 0, sizeof(m_keyDown));
    std::memset(m_keyUp, 0, sizeof(m_keyUp));
    std::memset(m_mouseButtonDown, 0, sizeof(m_mouseButtonDown));
    std::memset(m_mouseButtonUp, 0, sizeof(m_mouseButtonUp));

    // Reset deltas (they accumulate during the frame from callbacks)
    m_cursorDeltaX = 0.0;
    m_cursorDeltaY = 0.0;
    m_scrollDeltaX = 0.0;
    m_scrollDeltaY = 0.0;
}

// =============================================================================
// Keyboard Input
// =============================================================================

bool InputSystem::isKeyPressed(int key) const
{
    if (key < 0 || key >= MAX_KEYS)
        return false;
    return m_keyPressed[key];
}

bool InputSystem::isKeyDown(int key) const
{
    if (key < 0 || key >= MAX_KEYS)
        return false;
    return m_keyDown[key];
}

bool InputSystem::isKeyUp(int key) const
{
    if (key < 0 || key >= MAX_KEYS)
        return false;
    return m_keyUp[key];
}

// =============================================================================
// Mouse Input
// =============================================================================

bool InputSystem::isMouseButtonPressed(int button) const
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return false;
    return m_mouseButtonPressed[button];
}

bool InputSystem::isMouseButtonDown(int button) const
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return false;
    return m_mouseButtonDown[button];
}

bool InputSystem::isMouseButtonUp(int button) const
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return false;
    return m_mouseButtonUp[button];
}

std::array<double, 2> InputSystem::getCursorPosition() const
{
    return {m_cursorX, m_cursorY};
}

void InputSystem::setFocusMode(bool enabled)
{
    m_focusMode = enabled;
    if (m_window)
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        // Reset cursor delta when entering focus mode to avoid jump
        if (enabled)
        {
            m_firstCursorUpdate = true;
        }
    }
}

void InputSystem::setCursorMode(int mode)
{
    if (m_window)
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, mode);
    }
}

// =============================================================================
// Input Event Handlers (called by WindowSystem)
// =============================================================================

void InputSystem::onKey(int key, int scancode, int action, int mods)
{
    // Update internal state
    if (key >= 0 && key < MAX_KEYS)
    {
        if (action == GLFW_PRESS)
        {
            m_keyPressed[key] = true;
            m_keyDown[key] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            m_keyPressed[key] = false;
            m_keyUp[key] = true;
        }
        // GLFW_REPEAT doesn't change pressed state
    }

    // Update game command
    updateGameCommand(key, action);

    // Dispatch to registered callbacks
    for (auto& callback : m_keyCallbacks)
    {
        callback(key, scancode, action, mods);
    }
}

void InputSystem::onChar(uint32_t codepoint)
{
    for (auto& callback : m_charCallbacks)
    {
        callback(codepoint);
    }
}

void InputSystem::onMouseButton(int button, int action, int mods)
{
    // Update internal state
    if (button >= 0 && button < MAX_MOUSE_BUTTONS)
    {
        if (action == GLFW_PRESS)
        {
            m_mouseButtonPressed[button] = true;
            m_mouseButtonDown[button] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            m_mouseButtonPressed[button] = false;
            m_mouseButtonUp[button] = true;
        }
    }

    // Dispatch to registered callbacks
    for (auto& callback : m_mouseButtonCallbacks)
    {
        callback(button, action, mods);
    }
}

void InputSystem::onCursorPos(double xpos, double ypos)
{
    // Update cursor position
    m_cursorX = xpos;
    m_cursorY = ypos;

    // Calculate delta
    if (m_firstCursorUpdate)
    {
        m_lastCursorX = xpos;
        m_lastCursorY = ypos;
        m_firstCursorUpdate = false;
    }

    m_cursorDeltaX += xpos - m_lastCursorX;
    m_cursorDeltaY += ypos - m_lastCursorY;

    m_lastCursorX = xpos;
    m_lastCursorY = ypos;

    // Dispatch to registered callbacks
    for (auto& callback : m_cursorPosCallbacks)
    {
        callback(xpos, ypos);
    }
}

void InputSystem::onCursorEnter(bool entered)
{
    for (auto& callback : m_cursorEnterCallbacks)
    {
        callback(entered);
    }
}

void InputSystem::onScroll(double xoffset, double yoffset)
{
    // Accumulate scroll delta
    m_scrollDeltaX += xoffset;
    m_scrollDeltaY += yoffset;

    // Dispatch to registered callbacks
    for (auto& callback : m_scrollCallbacks)
    {
        callback(xoffset, yoffset);
    }
}

void InputSystem::onDrop(int count, const char** paths)
{
    // Copy paths to vector to ensure safe lifetime beyond GLFW callback
    std::vector<std::string> pathsCopy;
    pathsCopy.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        pathsCopy.emplace_back(paths[i]);
    }

    for (auto& callback : m_dropCallbacks)
    {
        callback(pathsCopy);
    }
}

void InputSystem::updateGameCommand(int key, int action)
{
    bool pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);
    bool released = (action == GLFW_RELEASE);

    auto updateCommand = [this, pressed, released](GameCommand cmd) {
        if (pressed)
            m_gameCommand |= cmd;
        else if (released)
            m_gameCommand &= ~cmd;
    };

    switch (key)
    {
    case GLFW_KEY_W:
        updateCommand(GameCommand::Forward);
        break;
    case GLFW_KEY_S:
        updateCommand(GameCommand::Backward);
        break;
    case GLFW_KEY_A:
        updateCommand(GameCommand::Left);
        break;
    case GLFW_KEY_D:
        updateCommand(GameCommand::Right);
        break;
    case GLFW_KEY_SPACE:
        updateCommand(GameCommand::Jump);
        break;
    case GLFW_KEY_LEFT_CONTROL:
        updateCommand(GameCommand::Crouch);
        break;
    case GLFW_KEY_LEFT_SHIFT:
        updateCommand(GameCommand::Sprint);
        break;
    default:
        break;
    }
}

} // namespace vesper
