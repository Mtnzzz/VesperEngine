#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vesper {

// Forward declaration
class InputSystem;

/// @brief Window creation configuration
struct WindowCreateInfo
{
    uint32_t    width{1920};
    uint32_t    height{1080};
    std::string title{"Vesper Engine"};
    bool        fullscreen{false};
    bool        resizable{true};
};

/// @brief Window system wrapper using GLFW
/// Handles window creation, lifecycle, and all GLFW callbacks.
/// Input events are forwarded to InputSystem.
class WindowSystem
{
public:
    WindowSystem() = default;
    ~WindowSystem();

    WindowSystem(const WindowSystem&) = delete;
    WindowSystem& operator=(const WindowSystem&) = delete;

    /// @brief Initialize the window system
    /// @param createInfo Window creation parameters
    /// @param inputSystem Pointer to InputSystem for forwarding input events (can be nullptr)
    /// @return true if initialization successful
    bool initialize(const WindowCreateInfo& createInfo, InputSystem* inputSystem = nullptr);

    /// @brief Shutdown and cleanup the window system
    void shutdown();

    /// @brief Poll window events (call once per frame)
    void pollEvents() const;

    /// @brief Check if window should close
    bool shouldClose() const;

    /// @brief Set window title
    void setTitle(const std::string& title);

    /// @brief Get native GLFW window handle
    GLFWwindow* getNativeWindow() const { return m_window; }

    /// @brief Get window dimensions
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    std::array<uint32_t, 2> getSize() const { return {m_width, m_height}; }

    /// @brief Get aspect ratio (returns 1.0f if height is 0 to avoid divide-by-zero)
    float getAspectRatio() const
    {
        return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    }

    /// @brief Get framebuffer size (may differ from window size on high-DPI displays)
    std::array<int, 2> getFramebufferSize() const;

    /// @brief Check if window is minimized
    bool isMinimized() const { return m_width == 0 || m_height == 0; }

    /// @brief Check if window has focus
    bool isFocused() const;

    /// @brief Set the input system to forward events to
    void setInputSystem(InputSystem* inputSystem) { m_inputSystem = inputSystem; }

    // =========================================================================
    // Callback Registration (Window-related only)
    // =========================================================================

    using WindowSizeCallback      = std::function<void(uint32_t width, uint32_t height)>;
    using FramebufferSizeCallback = std::function<void(int width, int height)>;
    using WindowCloseCallback     = std::function<void()>;

    void registerWindowSizeCallback(WindowSizeCallback callback) { m_windowSizeCallbacks.push_back(std::move(callback)); }
    void registerFramebufferSizeCallback(FramebufferSizeCallback callback) { m_framebufferSizeCallbacks.push_back(std::move(callback)); }
    void registerWindowCloseCallback(WindowCloseCallback callback) { m_windowCloseCallbacks.push_back(std::move(callback)); }

private:
    // GLFW callback functions (static)
    // Window callbacks
    static void glfwWindowSizeCallback(GLFWwindow* window, int width, int height);
    static void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void glfwWindowCloseCallback(GLFWwindow* window);
    static void glfwErrorCallback(int error, const char* description);

    // Input callbacks (forwarded to InputSystem)
    static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfwCharCallback(GLFWwindow* window, uint32_t codepoint);
    static void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void glfwCursorEnterCallback(GLFWwindow* window, int entered);
    static void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void glfwDropCallback(GLFWwindow* window, int count, const char** paths);

    // Dispatch callbacks to registered handlers
    void onWindowSize(uint32_t width, uint32_t height);
    void onFramebufferSize(int width, int height);
    void onWindowClose();

private:
    GLFWwindow*  m_window{nullptr};
    InputSystem* m_inputSystem{nullptr};

    uint32_t m_width{0};
    uint32_t m_height{0};
    bool     m_initialized{false};

    // Callback storage (window-related only)
    std::vector<WindowSizeCallback>      m_windowSizeCallbacks;
    std::vector<FramebufferSizeCallback> m_framebufferSizeCallbacks;
    std::vector<WindowCloseCallback>     m_windowCloseCallbacks;
};

} // namespace vesper
