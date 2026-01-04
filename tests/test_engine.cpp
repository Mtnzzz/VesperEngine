#include <gtest/gtest.h>

#include "runtime/engine.h"

#include <GLFW/glfw3.h>

namespace vesper {
namespace test {

TEST(EngineTest, InitializeAndShutdownWhenSupported) {
    if (!glfwInit()) {
        GTEST_SKIP() << "GLFW init failed";
    }
    bool vulkanSupported = glfwVulkanSupported();
    glfwTerminate();

    if (!vulkanSupported) {
        GTEST_SKIP() << "Vulkan not supported";
    }

    VesperEngine engine;
    EngineConfig config;
    config.windowWidth = 320;
    config.windowHeight = 240;
    config.windowTitle = "EngineTest";
    config.resizable = false;
    config.fullscreen = false;

    ASSERT_TRUE(engine.initialize(config));
    EXPECT_FALSE(engine.isRunning());
    EXPECT_NE(engine.getWindowSystem(), nullptr);
    EXPECT_NE(engine.getInputSystem(), nullptr);

    engine.shutdown();

    EXPECT_EQ(engine.getWindowSystem(), nullptr);
    EXPECT_EQ(engine.getInputSystem(), nullptr);
    EXPECT_FALSE(engine.isRunning());
}

} // namespace test
} // namespace vesper
