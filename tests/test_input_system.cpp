#include <gtest/gtest.h>

#include "runtime/platform/input/input_system.h"

namespace vesper {
namespace test {

TEST(InputSystemTest, GameCommandBitmaskOperators) {
    GameCommand command = GameCommand::Forward | GameCommand::Left;

    EXPECT_EQ(command & GameCommand::Forward, GameCommand::Forward);
    EXPECT_EQ(command & GameCommand::Left, GameCommand::Left);
    EXPECT_EQ(command & GameCommand::Right, GameCommand::None);
}

TEST(InputSystemTest, InvalidKeyQueriesReturnFalse) {
    InputSystem input;

    EXPECT_FALSE(input.isKeyPressed(-1));
    EXPECT_FALSE(input.isKeyDown(GLFW_KEY_LAST + 1));
    EXPECT_FALSE(input.isKeyUp(GLFW_KEY_LAST + 1));
}

TEST(InputSystemTest, InitializeFailsWithoutWindow) {
    InputSystem input;

    EXPECT_FALSE(input.initialize(nullptr));
}

TEST(InputSystemTest, KeyPressAndReleaseUpdatesState) {
    InputSystem input;

    input.tick();
    input.onKey(GLFW_KEY_W, 0, GLFW_PRESS, 0);

    EXPECT_TRUE(input.isKeyPressed(GLFW_KEY_W));
    EXPECT_TRUE(input.isKeyDown(GLFW_KEY_W));
    EXPECT_EQ(input.getGameCommand(), GameCommand::Forward);

    input.tick();
    EXPECT_FALSE(input.isKeyDown(GLFW_KEY_W));
    EXPECT_TRUE(input.isKeyPressed(GLFW_KEY_W));

    input.onKey(GLFW_KEY_W, 0, GLFW_RELEASE, 0);

    EXPECT_FALSE(input.isKeyPressed(GLFW_KEY_W));
    EXPECT_TRUE(input.isKeyUp(GLFW_KEY_W));
    EXPECT_EQ(input.getGameCommand(), GameCommand::None);
}

TEST(InputSystemTest, ScrollDeltaResetsOnTick) {
    InputSystem input;

    input.tick();
    input.onScroll(0.5, -1.0);

    auto delta = input.getScrollDelta();
    EXPECT_DOUBLE_EQ(delta[0], 0.5);
    EXPECT_DOUBLE_EQ(delta[1], -1.0);

    input.tick();

    delta = input.getScrollDelta();
    EXPECT_DOUBLE_EQ(delta[0], 0.0);
    EXPECT_DOUBLE_EQ(delta[1], 0.0);
}

TEST(InputSystemTest, CursorDeltaUpdatesOnMovement) {
    InputSystem input;

    input.tick();
    input.onCursorPos(10.0, 15.0);

    auto delta = input.getCursorDelta();
    EXPECT_DOUBLE_EQ(delta[0], 0.0);
    EXPECT_DOUBLE_EQ(delta[1], 0.0);

    input.onCursorPos(12.5, 9.0);
    delta = input.getCursorDelta();
    EXPECT_DOUBLE_EQ(delta[0], 2.5);
    EXPECT_DOUBLE_EQ(delta[1], -6.0);

    input.tick();
    delta = input.getCursorDelta();
    EXPECT_DOUBLE_EQ(delta[0], 0.0);
    EXPECT_DOUBLE_EQ(delta[1], 0.0);
}

TEST(InputSystemTest, FocusModeTogglesState) {
    InputSystem input;

    EXPECT_FALSE(input.getFocusMode());
    input.setFocusMode(true);
    EXPECT_TRUE(input.getFocusMode());
    input.setFocusMode(false);
    EXPECT_FALSE(input.getFocusMode());
}

} // namespace test
} // namespace vesper
