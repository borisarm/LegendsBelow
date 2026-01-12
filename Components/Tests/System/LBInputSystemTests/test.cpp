#include "pch.h"

import LB.InputSystem;

using namespace LB::Systems::Input;

// ============================================================================
// InputSystem Initialization Tests
// ============================================================================

TEST(InputSystemTests, Initialize_Success) {
    InputSystem system;

    bool result = system.Initialize();

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsInitialized());
}

TEST(InputSystemTests, Shutdown_ClearsState) {
    InputSystem system;
    system.Initialize();

    system.Shutdown();

    EXPECT_FALSE(system.IsInitialized());
}

// ============================================================================
// Keyboard Input Tests
// ============================================================================

TEST(InputSystemTests, IsKeyDown_NotPressed_ReturnsFalse) {
    InputSystem system;
    system.Initialize();

    bool result = system.IsKeyDown(65); // 'A' key

    EXPECT_FALSE(result);
}

TEST(InputSystemTests, IsKeyDown_AfterSetPressed_ReturnsTrue) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(65, KeyState::Pressed);

    bool result = system.IsKeyDown(65);

    EXPECT_TRUE(result);
}

TEST(InputSystemTests, IsKeyDown_AfterSetHeld_ReturnsTrue) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(65, KeyState::Held);

    bool result = system.IsKeyDown(65);

    EXPECT_TRUE(result);
}

TEST(InputSystemTests, IsKeyPressed_FirstFrame_ReturnsTrue) {
    InputSystem system;
    system.Initialize();
    system.Update(); // Clear previous state

    system.SetKeyState(65, KeyState::Pressed);

    EXPECT_TRUE(system.IsKeyPressed(65));
}

TEST(InputSystemTests, IsKeyPressed_SecondFrame_ReturnsFalse) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(65, KeyState::Pressed);
    system.Update(); // First frame

    system.SetKeyState(65, KeyState::Held);
    system.Update(); // Second frame

    EXPECT_FALSE(system.IsKeyPressed(65));
}

TEST(InputSystemTests, IsKeyReleased_AfterRelease_ReturnsTrue) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(65, KeyState::Pressed);
    system.Update();

    system.SetKeyState(65, KeyState::Released);
    // Check BEFORE Update() - IsKeyReleased checks current vs previous frame
    EXPECT_TRUE(system.IsKeyReleased(65));
}

TEST(InputSystemTests, IsKeyReleased_StillPressed_ReturnsFalse) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(65, KeyState::Pressed);
    system.Update();

    EXPECT_FALSE(system.IsKeyReleased(65));
}

// ============================================================================
// Mouse Input Tests
// ============================================================================

TEST(InputSystemTests, GetMouseState_InitialState_ReturnsDefaults) {
    InputSystem system;
    system.Initialize();

    const auto& state = system.GetMouseState();

    EXPECT_EQ(state.x, 0);
    EXPECT_EQ(state.y, 0);
    EXPECT_FALSE(state.leftButton);
    EXPECT_FALSE(state.rightButton);
    EXPECT_FALSE(state.middleButton);
}

TEST(InputSystemTests, IsMouseButtonDown_LeftButton_ReturnsCorrectState) {
    InputSystem system;
    system.Initialize();

    bool result = system.IsMouseButtonDown(MouseButton::Left);

    EXPECT_FALSE(result); // Initially not pressed
}

TEST(InputSystemTests, IsMouseButtonDown_RightButton_ReturnsCorrectState) {
    InputSystem system;
    system.Initialize();

    bool result = system.IsMouseButtonDown(MouseButton::Right);

    EXPECT_FALSE(result);
}

TEST(InputSystemTests, IsMouseButtonDown_MiddleButton_ReturnsCorrectState) {
    InputSystem system;
    system.Initialize();

    bool result = system.IsMouseButtonDown(MouseButton::Middle);

    EXPECT_FALSE(result);
}

// ============================================================================
// Gamepad Input Tests
// ============================================================================

TEST(InputSystemTests, GetGamepadState_NoGamepad_ReturnsDisconnected) {
    InputSystem system;
    system.Initialize();

    const auto& state = system.GetGamepadState(0);

    EXPECT_FALSE(state.connected);
}

TEST(InputSystemTests, GetGamepadState_InitialState_ReturnsDefaults) {
    InputSystem system;
    system.Initialize();

    const auto& state = system.GetGamepadState(0);

    EXPECT_EQ(state.leftThumbX, 0.0f);
    EXPECT_EQ(state.leftThumbY, 0.0f);
    EXPECT_EQ(state.rightThumbX, 0.0f);
    EXPECT_EQ(state.rightThumbY, 0.0f);
    EXPECT_EQ(state.leftTrigger, 0.0f);
    EXPECT_EQ(state.rightTrigger, 0.0f);
}

// ============================================================================
// Input Action Tests
// ============================================================================

TEST(InputSystemTests, RegisterAction_ValidAction_Success) {
    InputSystem system;
    system.Initialize();

    InputAction action{};
    action.name = "Jump";
    action.keyCode = 32; // Space
    action.deviceType = InputDevice::Keyboard;

    system.RegisterAction(action);

    // No exception thrown, action registered
    SUCCEED();
}

TEST(InputSystemTests, IsActionTriggered_KeyboardAction_ReturnsCorrectState) {
    InputSystem system;
    system.Initialize();

    InputAction action{};
    action.name = "Jump";
    action.keyCode = 32; // Space
    action.deviceType = InputDevice::Keyboard;
    system.RegisterAction(action);

    system.Update();
    system.SetKeyState(32, KeyState::Pressed);

    EXPECT_TRUE(system.IsActionTriggered("Jump"));
}

TEST(InputSystemTests, IsActionTriggered_UnregisteredAction_ReturnsFalse) {
    InputSystem system;
    system.Initialize();

    bool result = system.IsActionTriggered("NonExistent");

    EXPECT_FALSE(result);
}

TEST(InputSystemTests, IsActionTriggered_MouseAction_ReturnsCorrectState) {
    InputSystem system;
    system.Initialize();

    InputAction action{};
    action.name = "Attack";
    action.mouseButton = MouseButton::Left;
    action.deviceType = InputDevice::Mouse;
    system.RegisterAction(action);

    bool result = system.IsActionTriggered("Attack");

    EXPECT_FALSE(result); // Mouse not clicked
}

// ============================================================================
// Update Tests
// ============================================================================

TEST(InputSystemTests, Update_CopiesPreviousState) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(65, KeyState::Pressed);

    system.Update();

    // After update, previous state should be set
    // Next press detection should work correctly
    EXPECT_FALSE(system.IsKeyPressed(65));
}

TEST(InputSystemTests, Update_BeforeInitialize_NoError) {
    InputSystem system;

    system.Update(); // Should not crash

    SUCCEED();
}