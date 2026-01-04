#include <gtest/gtest.h>

#include "runtime/function/window/window_system.h"
#include "runtime/platform/input/input_system.h"

namespace vesper {
namespace test {

TEST(WindowSystemTest, DefaultState) {
    WindowSystem windowSystem;
    EXPECT_TRUE(windowSystem.shouldClose());
    EXPECT_TRUE(windowSystem.isMinimized());

    auto framebufferSize = windowSystem.getFramebufferSize();
    EXPECT_EQ(framebufferSize[0], 0);
    EXPECT_EQ(framebufferSize[1], 0);

    EXPECT_FALSE(windowSystem.isFocused());
}

TEST(WindowSystemTest, InitializeAndShutdownWhenSupported) {
    if (!glfwInit()) {
        GTEST_SKIP() << "GLFW init failed";
    }
    bool vulkanSupported = glfwVulkanSupported();
    glfwTerminate();

    if (!vulkanSupported) {
        GTEST_SKIP() << "Vulkan not supported";
    }

    WindowSystem windowSystem;
    WindowCreateInfo info;
    info.width = 320;
    info.height = 240;
    info.title = "WindowSystemTest";
    info.resizable = false;
    info.fullscreen = false;

    ASSERT_TRUE(windowSystem.initialize(info));
    EXPECT_EQ(windowSystem.getWidth(), info.width);
    EXPECT_EQ(windowSystem.getHeight(), info.height);
    EXPECT_FALSE(windowSystem.isMinimized());
    EXPECT_FALSE(windowSystem.shouldClose());
    EXPECT_NEAR(
        windowSystem.getAspectRatio(),
        static_cast<float>(info.width) / static_cast<float>(info.height),
        1e-6f
    );

    windowSystem.shutdown();
}

TEST(WindowSystemTest, ForwardsKeyEventsToInputSystemWhenSupported) {
    if (!glfwInit()) {
        GTEST_SKIP() << "GLFW init failed";
    }
    bool vulkanSupported = glfwVulkanSupported();
    glfwTerminate();

    if (!vulkanSupported) {
        GTEST_SKIP() << "Vulkan not supported";
    }

    InputSystem input;
    WindowSystem windowSystem;
    WindowCreateInfo info;
    info.width = 320;
    info.height = 240;
    info.title = "WindowSystemForwardTest";
    info.resizable = false;
    info.fullscreen = false;

    ASSERT_TRUE(windowSystem.initialize(info, &input));
    ASSERT_TRUE(input.initialize(windowSystem.getNativeWindow()));

    GLFWwindow* window = windowSystem.getNativeWindow();
    ASSERT_NE(window, nullptr);

    GLFWkeyfun keyCallback = glfwSetKeyCallback(window, nullptr);
    ASSERT_NE(keyCallback, nullptr);

    input.tick();
    keyCallback(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);

    EXPECT_TRUE(input.isKeyPressed(GLFW_KEY_W));
    EXPECT_TRUE(input.isKeyDown(GLFW_KEY_W));
    EXPECT_EQ(input.getGameCommand(), GameCommand::Forward);

    keyCallback(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);

    EXPECT_FALSE(input.isKeyPressed(GLFW_KEY_W));
    EXPECT_TRUE(input.isKeyUp(GLFW_KEY_W));
    EXPECT_EQ(input.getGameCommand(), GameCommand::None);

    windowSystem.shutdown();
}

} // namespace test
} // namespace vesper
