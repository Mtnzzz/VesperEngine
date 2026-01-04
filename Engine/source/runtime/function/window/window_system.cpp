#include "runtime/function/window/window_system.h"
#include "runtime/platform/input/input_system.h"
#include "runtime/core/log/log_system.h"

namespace vesper {

WindowSystem::~WindowSystem()
{
    shutdown();
}

bool WindowSystem::initialize(const WindowCreateInfo& createInfo, InputSystem* inputSystem)
{
    if (m_initialized)
    {
        LOG_WARN("WindowSystem already initialized");
        return true;
    }

    m_inputSystem = inputSystem;

    // Set error callback before glfwInit
    glfwSetErrorCallback(glfwErrorCallback);

    // Initialize GLFW
    if (!glfwInit())
    {
        LOG_ERROR("Failed to initialize GLFW");
        return false;
    }

    // Check Vulkan support
    if (!glfwVulkanSupported())
    {
        LOG_ERROR("Vulkan is not supported on this system");
        glfwTerminate();
        return false;
    }

    m_width = createInfo.width;
    m_height = createInfo.height;

    // Window hints for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, createInfo.resizable ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    GLFWmonitor* monitor = nullptr;
    if (createInfo.fullscreen)
    {
        monitor = glfwGetPrimaryMonitor();
        if (monitor)
        {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode)
            {
                m_width = mode->width;
                m_height = mode->height;
            }
            else
            {
                // Failed to get video mode, fall back to windowed mode
                LOG_WARN("Failed to get video mode for primary monitor, falling back to windowed mode");
                monitor = nullptr;
            }
        }
        else
        {
            // No monitor available (headless/remote environment), fall back to windowed mode
            LOG_WARN("No primary monitor detected (headless/remote environment?), falling back to windowed mode");
        }
    }

    m_window = glfwCreateWindow(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        createInfo.title.c_str(),
        monitor,
        nullptr
    );

    if (!m_window)
    {
        LOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    // Store this pointer for callbacks
    glfwSetWindowUserPointer(m_window, this);

    // Setup window callbacks
    glfwSetWindowSizeCallback(m_window, glfwWindowSizeCallback);
    glfwSetFramebufferSizeCallback(m_window, glfwFramebufferSizeCallback);
    glfwSetWindowCloseCallback(m_window, glfwWindowCloseCallback);

    // Setup input callbacks (forwarded to InputSystem)
    glfwSetKeyCallback(m_window, glfwKeyCallback);
    glfwSetCharCallback(m_window, glfwCharCallback);
    glfwSetMouseButtonCallback(m_window, glfwMouseButtonCallback);
    glfwSetCursorPosCallback(m_window, glfwCursorPosCallback);
    glfwSetCursorEnterCallback(m_window, glfwCursorEnterCallback);
    glfwSetScrollCallback(m_window, glfwScrollCallback);
    glfwSetDropCallback(m_window, glfwDropCallback);

    // Enable raw mouse motion if supported
    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    m_initialized = true;

    LOG_INFO("WindowSystem initialized: {}x{} \"{}\"", m_width, m_height, createInfo.title);

    return true;
}

void WindowSystem::shutdown()
{
    if (!m_initialized)
        return;

    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();

    // Clear callbacks
    m_windowSizeCallbacks.clear();
    m_framebufferSizeCallbacks.clear();
    m_windowCloseCallbacks.clear();

    m_inputSystem = nullptr;
    m_initialized = false;

    LOG_INFO("WindowSystem shutdown");
}

void WindowSystem::pollEvents() const
{
    glfwPollEvents();
}

bool WindowSystem::shouldClose() const
{
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void WindowSystem::setTitle(const std::string& title)
{
    if (m_window)
    {
        glfwSetWindowTitle(m_window, title.c_str());
    }
}

std::array<int, 2> WindowSystem::getFramebufferSize() const
{
    int width = 0, height = 0;
    if (m_window)
    {
        glfwGetFramebufferSize(m_window, &width, &height);
    }
    return {width, height};
}

bool WindowSystem::isFocused() const
{
    return m_window ? glfwGetWindowAttrib(m_window, GLFW_FOCUSED) : false;
}

// =============================================================================
// GLFW Static Callbacks - Window
// =============================================================================

void WindowSystem::glfwWindowSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->m_width = static_cast<uint32_t>(width);
        self->m_height = static_cast<uint32_t>(height);
        self->onWindowSize(self->m_width, self->m_height);
    }
}

void WindowSystem::glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->onFramebufferSize(width, height);
    }
}

void WindowSystem::glfwWindowCloseCallback(GLFWwindow* window)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->onWindowClose();
    }
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void WindowSystem::glfwErrorCallback(int error, const char* description)
{
    LOG_ERROR("GLFW Error ({}): {}", error, description);
}

// =============================================================================
// GLFW Static Callbacks - Input (forwarded to InputSystem)
// =============================================================================

void WindowSystem::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self && self->m_inputSystem)
    {
        self->m_inputSystem->onKey(key, scancode, action, mods);
    }
}

void WindowSystem::glfwCharCallback(GLFWwindow* window, uint32_t codepoint)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self && self->m_inputSystem)
    {
        self->m_inputSystem->onChar(codepoint);
    }
}

void WindowSystem::glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self && self->m_inputSystem)
    {
        self->m_inputSystem->onMouseButton(button, action, mods);
    }
}

void WindowSystem::glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self && self->m_inputSystem)
    {
        self->m_inputSystem->onCursorPos(xpos, ypos);
    }
}

void WindowSystem::glfwCursorEnterCallback(GLFWwindow* window, int entered)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self && self->m_inputSystem)
    {
        self->m_inputSystem->onCursorEnter(entered == GLFW_TRUE);
    }
}

void WindowSystem::glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self && self->m_inputSystem)
    {
        self->m_inputSystem->onScroll(xoffset, yoffset);
    }
}

void WindowSystem::glfwDropCallback(GLFWwindow* window, int count, const char** paths)
{
    auto* self = static_cast<WindowSystem*>(glfwGetWindowUserPointer(window));
    if (self && self->m_inputSystem)
    {
        self->m_inputSystem->onDrop(count, paths);
    }
}

// =============================================================================
// Callback Dispatchers
// =============================================================================

void WindowSystem::onWindowSize(uint32_t width, uint32_t height)
{
    for (auto& callback : m_windowSizeCallbacks)
    {
        callback(width, height);
    }
}

void WindowSystem::onFramebufferSize(int width, int height)
{
    for (auto& callback : m_framebufferSizeCallbacks)
    {
        callback(width, height);
    }
}

void WindowSystem::onWindowClose()
{
    for (auto& callback : m_windowCloseCallbacks)
    {
        callback();
    }
}

} // namespace vesper
