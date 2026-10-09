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
// ============================================================================
// Key Transition Tests
// ============================================================================

TEST(InputSystemTests, IsKeyPressed_TapWithinOneFrame_StillReported) {
    InputSystem system;
    system.Initialize();

    // Down and up before the frame is read (e.g. both messages drained before a simulation step)
    system.SetKeyState(Keys::Escape, KeyState::Pressed);
    system.SetKeyState(Keys::Escape, KeyState::Released);

    EXPECT_TRUE(system.IsKeyPressed(Keys::Escape));
    EXPECT_TRUE(system.IsKeyReleased(Keys::Escape));
    EXPECT_FALSE(system.IsKeyDown(Keys::Escape));
}

TEST(InputSystemTests, IsKeyPressed_RepeatedDownWhileHeld_NotReportedAgain) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(Keys::Space, KeyState::Pressed);
    system.Update();

    // Keyboard auto-repeat sends further key-down events while held
    system.SetKeyState(Keys::Space, KeyState::Pressed);

    EXPECT_FALSE(system.IsKeyPressed(Keys::Space));
    EXPECT_TRUE(system.IsKeyDown(Keys::Space));
}

TEST(InputSystemTests, IsKeyReleased_ClearedByUpdate) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(Keys::Space, KeyState::Pressed);
    system.Update();
    system.SetKeyState(Keys::Space, KeyState::Released);

    system.Update();

    EXPECT_FALSE(system.IsKeyReleased(Keys::Space));
}

TEST(InputSystemTests, IsActionTriggered_TapWithinOneFrame_Triggers) {
    InputSystem system;
    system.Initialize();
    InputAction action{};
    action.name = "Pause";
    action.keyCode = Keys::Escape;
    action.deviceType = InputDevice::Keyboard;
    system.RegisterAction(action);

    system.SetKeyState(Keys::Escape, KeyState::Pressed);
    system.SetKeyState(Keys::Escape, KeyState::Released);

    EXPECT_TRUE(system.IsActionTriggered("Pause"));
}

// ============================================================================
// Key Code Tests
// ============================================================================

TEST(InputSystemTests, KeysFromChar_LettersAndDigits) {
    EXPECT_EQ(Keys::FromChar('A'), 65);
    EXPECT_EQ(Keys::FromChar('w'), Keys::FromChar('W'));
    EXPECT_EQ(Keys::FromChar('0'), 48);
    EXPECT_EQ(Keys::FromChar('9'), 57);
}

TEST(InputSystemTests, KeysFromChar_Unsupported_ReturnsMinusOne) {
    EXPECT_EQ(Keys::FromChar('!'), -1);
    EXPECT_EQ(Keys::FromChar(' '), -1);
}

// ============================================================================
// Mouse Input Tests
// ============================================================================

TEST(InputSystemTests, SetMousePosition_UpdatesPositionAndDelta) {
    InputSystem system;
    system.Initialize();
    system.SetMousePosition(100, 50);
    system.Update();

    system.SetMousePosition(110, 45);

    const auto& mouse = system.GetMouseState();
    EXPECT_EQ(mouse.x, 110);
    EXPECT_EQ(mouse.y, 45);
    EXPECT_EQ(mouse.deltaX, 10);
    EXPECT_EQ(mouse.deltaY, -5);
}

TEST(InputSystemTests, SetMousePosition_MultipleMovesInFrame_DeltaIsTotal) {
    InputSystem system;
    system.Initialize();
    system.SetMousePosition(0, 0);
    system.Update();

    system.SetMousePosition(5, 5);
    system.SetMousePosition(20, 8);

    EXPECT_EQ(system.GetMouseState().deltaX, 20);
    EXPECT_EQ(system.GetMouseState().deltaY, 8);
}

TEST(InputSystemTests, Update_ResetsMouseDeltaAndWheel) {
    InputSystem system;
    system.Initialize();
    system.SetMousePosition(30, 40);
    system.AddMouseWheel(120);

    system.Update();

    const auto& mouse = system.GetMouseState();
    EXPECT_EQ(mouse.x, 30);
    EXPECT_EQ(mouse.y, 40);
    EXPECT_EQ(mouse.deltaX, 0);
    EXPECT_EQ(mouse.deltaY, 0);
    EXPECT_EQ(mouse.wheelDelta, 0);
}

TEST(InputSystemTests, AddMouseWheel_Accumulates) {
    InputSystem system;
    system.Initialize();

    system.AddMouseWheel(120);
    system.AddMouseWheel(-240);

    EXPECT_EQ(system.GetMouseState().wheelDelta, -120);
}

TEST(InputSystemTests, SetMouseButton_DownAndUp) {
    InputSystem system;
    system.Initialize();

    system.SetMouseButton(MouseButton::Right, true);
    EXPECT_TRUE(system.IsMouseButtonDown(MouseButton::Right));

    system.SetMouseButton(MouseButton::Right, false);
    EXPECT_FALSE(system.IsMouseButtonDown(MouseButton::Right));
}

TEST(InputSystemTests, IsMouseButtonPressed_ClickWithinOneFrame_Reported) {
    InputSystem system;
    system.Initialize();

    system.SetMouseButton(MouseButton::Left, true);
    system.SetMouseButton(MouseButton::Left, false);

    EXPECT_TRUE(system.IsMouseButtonPressed(MouseButton::Left));
}

TEST(InputSystemTests, IsMouseButtonPressed_HeldIntoNextFrame_NotReportedAgain) {
    InputSystem system;
    system.Initialize();
    system.SetMouseButton(MouseButton::Left, true);

    system.Update();

    EXPECT_FALSE(system.IsMouseButtonPressed(MouseButton::Left));
    EXPECT_TRUE(system.IsMouseButtonDown(MouseButton::Left));
}

TEST(InputSystemTests, IsActionTriggered_MouseClick_Triggers) {
    InputSystem system;
    system.Initialize();
    InputAction action{};
    action.name = "Attack";
    action.mouseButton = MouseButton::Left;
    action.deviceType = InputDevice::Mouse;
    system.RegisterAction(action);

    system.SetMouseButton(MouseButton::Left, true);

    EXPECT_TRUE(system.IsActionTriggered("Attack"));
}

// ============================================================================
// Focus Loss Tests
// ============================================================================

TEST(InputSystemTests, ReleaseAll_ReleasesKeysAndButtons) {
    InputSystem system;
    system.Initialize();
    system.SetKeyState(Keys::FromChar('W'), KeyState::Held);
    system.SetMouseButton(MouseButton::Left, true);
    system.SetMouseButton(MouseButton::Middle, true);

    system.ReleaseAll();

    EXPECT_FALSE(system.IsKeyDown(Keys::FromChar('W')));
    EXPECT_TRUE(system.IsKeyReleased(Keys::FromChar('W')));
    EXPECT_FALSE(system.IsMouseButtonDown(MouseButton::Left));
    EXPECT_FALSE(system.IsMouseButtonDown(MouseButton::Middle));
}
