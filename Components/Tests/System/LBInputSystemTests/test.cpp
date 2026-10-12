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

// ============================================================================
// Gamepad Type Tests
// ============================================================================

TEST(GamepadTypesTests, ButtonMask_UsesEnumValueAsBitIndex) {
    EXPECT_EQ(ButtonMask(GamepadButton::A), 0x1u);
    EXPECT_EQ(ButtonMask(GamepadButton::Start), 1u << 7);
    EXPECT_EQ(ButtonMask(GamepadButton::DPadRight), 1u << 13);
}

TEST(GamepadTypesTests, ApplyRadialDeadZone_InsideZone_Zeroed) {
    float x = 0.1f;
    float y = -0.15f;

    ApplyRadialDeadZone(x, y, LeftThumbDeadZone);

    EXPECT_EQ(x, 0.0f);
    EXPECT_EQ(y, 0.0f);
}

TEST(GamepadTypesTests, ApplyRadialDeadZone_FullDeflection_StaysFull) {
    float x = 1.0f;
    float y = 0.0f;

    ApplyRadialDeadZone(x, y, LeftThumbDeadZone);

    EXPECT_NEAR(x, 1.0f, 1e-6f);
    EXPECT_NEAR(y, 0.0f, 1e-6f);
}

TEST(GamepadTypesTests, ApplyRadialDeadZone_JustOutsideZone_SmallOutput) {
    float x = LeftThumbDeadZone + 0.01f;
    float y = 0.0f;

    ApplyRadialDeadZone(x, y, LeftThumbDeadZone);

    EXPECT_GT(x, 0.0f);
    EXPECT_LT(x, 0.05f);
}

TEST(GamepadTypesTests, ApplyRadialDeadZone_Diagonal_PreservesDirectionAndClamps) {
    // Square-gate sticks can report (1, 1): magnitude above 1 is clamped
    float x = 1.0f;
    float y = 1.0f;

    ApplyRadialDeadZone(x, y, LeftThumbDeadZone);

    EXPECT_NEAR(x, y, 1e-6f);
    EXPECT_NEAR(std::sqrt(x * x + y * y), 1.0f, 1e-5f);
}

// ============================================================================
// Gamepad Input Tests
// ============================================================================

namespace {
    GamepadState Pad(std::uint32_t buttons) {
        GamepadState state{};
        state.connected = true;
        state.buttons = buttons;
        return state;
    }
}

TEST(InputSystemTests, SetGamepadState_StoresState) {
    InputSystem system;
    system.Initialize();
    GamepadState state = Pad(ButtonMask(GamepadButton::A));
    state.leftThumbX = 0.5f;

    system.SetGamepadState(0, state);

    EXPECT_TRUE(system.GetGamepadState(0).connected);
    EXPECT_FLOAT_EQ(system.GetGamepadState(0).leftThumbX, 0.5f);
    EXPECT_TRUE(system.IsGamepadButtonDown(GamepadButton::A));
    EXPECT_FALSE(system.IsGamepadButtonDown(GamepadButton::B));
}

TEST(InputSystemTests, IsGamepadButtonPressed_NewPress_Reported) {
    InputSystem system;
    system.Initialize();

    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::Start)));

    EXPECT_TRUE(system.IsGamepadButtonPressed(GamepadButton::Start));
}

TEST(InputSystemTests, IsGamepadButtonPressed_HeldIntoNextFrame_NotReportedAgain) {
    InputSystem system;
    system.Initialize();
    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::A)));
    system.Update();

    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::A)));

    EXPECT_FALSE(system.IsGamepadButtonPressed(GamepadButton::A));
    EXPECT_TRUE(system.IsGamepadButtonDown(GamepadButton::A));
}

TEST(InputSystemTests, IsGamepadButtonPressed_TapWithinOneFrame_StillReported) {
    InputSystem system;
    system.Initialize();

    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::B)));
    system.SetGamepadState(0, Pad(0));

    EXPECT_TRUE(system.IsGamepadButtonPressed(GamepadButton::B));
    EXPECT_FALSE(system.IsGamepadButtonDown(GamepadButton::B));
}

TEST(InputSystemTests, IsGamepadButtonPressed_OtherGamepad_Independent) {
    InputSystem system;
    system.Initialize();

    system.SetGamepadState(1, Pad(ButtonMask(GamepadButton::X)));

    EXPECT_TRUE(system.IsGamepadButtonPressed(GamepadButton::X, 1));
    EXPECT_FALSE(system.IsGamepadButtonPressed(GamepadButton::X, 0));
}

TEST(InputSystemTests, IsActionTriggered_GamepadAction_Triggers) {
    InputSystem system;
    system.Initialize();
    InputAction action{};
    action.name = "Confirm";
    action.deviceType = InputDevice::Gamepad;
    action.gamepadButton = GamepadButton::A;
    system.RegisterAction(action);

    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::A)));

    EXPECT_TRUE(system.IsActionTriggered("Confirm"));
}

TEST(InputSystemTests, IsActionTriggered_MultipleBindings_AnyTriggers) {
    InputSystem system;
    system.Initialize();
    InputAction key{};
    key.name = "Pause";
    key.deviceType = InputDevice::Keyboard;
    key.keyCode = Keys::Escape;
    InputAction pad{};
    pad.name = "Pause";
    pad.deviceType = InputDevice::Gamepad;
    pad.gamepadButton = GamepadButton::Start;
    system.RegisterAction(key);
    system.RegisterAction(pad);

    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::Start)));
    EXPECT_TRUE(system.IsActionTriggered("Pause"));

    system.Update();
    system.SetKeyState(Keys::Escape, KeyState::Pressed);
    EXPECT_TRUE(system.IsActionTriggered("Pause"));
}

TEST(InputSystemTests, ReleaseAll_ReleasesGamepadButtons) {
    InputSystem system;
    system.Initialize();
    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::A) | ButtonMask(GamepadButton::DPadUp)));

    system.ReleaseAll();

    EXPECT_FALSE(system.IsGamepadButtonDown(GamepadButton::A));
    EXPECT_FALSE(system.IsGamepadButtonDown(GamepadButton::DPadUp));
    EXPECT_TRUE(system.GetGamepadState(0).connected);
}

TEST(InputSystemTests, Shutdown_ClearsGamepadState) {
    InputSystem system;
    system.Initialize();
    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::A)));

    system.Shutdown();

    EXPECT_FALSE(system.GetGamepadState(0).connected);
}

// ============================================================================
// Device Input Tests
// ============================================================================

TEST(InputSystemTests, HasDeviceInput_Default_False) {
    InputSystem system;
    system.Initialize();

    EXPECT_FALSE(system.HasDeviceInput());
}

TEST(InputSystemTests, PollDevices_WithoutDeviceInput_KeepsPushedState) {
    InputSystem system;
    system.Initialize();
    system.SetGamepadState(0, Pad(ButtonMask(GamepadButton::Y)));

    system.PollDevices();

    EXPECT_TRUE(system.IsGamepadButtonDown(GamepadButton::Y));
}

TEST(InputSystemTests, DisableDeviceInput_WithoutDevices_NoError) {
    InputSystem system;
    system.Initialize();

    system.DisableDeviceInput();

    EXPECT_FALSE(system.HasDeviceInput());
}

// ============================================================================
// Held Action Tests
// ============================================================================

TEST(InputSystemActionTests, IsActionDown_WhileAnyBindingHeld) {
    InputSystem system;
    system.Initialize();
    InputAction key{};
    key.name = "Attack";
    key.deviceType = InputDevice::Keyboard;
    key.keyCode = Keys::Space;
    system.RegisterAction(key);
    InputAction mouse{};
    mouse.name = "Attack";
    mouse.deviceType = InputDevice::Mouse;
    mouse.mouseButton = MouseButton::Left;
    system.RegisterAction(mouse);

    EXPECT_FALSE(system.IsActionDown("Attack"));

    system.SetKeyState(Keys::Space, KeyState::Pressed);
    system.Update();
    EXPECT_TRUE(system.IsActionDown("Attack"));      // still held in the next frame
    EXPECT_FALSE(system.IsActionTriggered("Attack"));

    system.SetKeyState(Keys::Space, KeyState::Released);
    system.SetMouseButton(MouseButton::Left, true);
    EXPECT_TRUE(system.IsActionDown("Attack"));

    system.SetMouseButton(MouseButton::Left, false);
    EXPECT_FALSE(system.IsActionDown("Attack"));
    EXPECT_FALSE(system.IsActionDown("Unknown"));
}

TEST(InputSystemActionTests, IsActionDown_GamepadButton) {
    InputSystem system;
    system.Initialize();
    InputAction pad{};
    pad.name = "Attack";
    pad.deviceType = InputDevice::Gamepad;
    pad.gamepadButton = GamepadButton::A;
    system.RegisterAction(pad);

    GamepadState state{};
    state.connected = true;
    state.buttons = ButtonMask(GamepadButton::A);
    system.SetGamepadState(0, state);

    EXPECT_TRUE(system.IsActionDown("Attack"));
}

// ============================================================================
// Bindings and names
// ============================================================================

TEST(InputSystemTests, GetBindings_InRegistrationOrder) {
    InputSystem system;
    system.Initialize();
    InputAction key{};
    key.name = "Jump";
    key.keyCode = Keys::Space;
    InputAction pad{};
    pad.name = "Jump";
    pad.deviceType = InputDevice::Gamepad;
    pad.gamepadButton = GamepadButton::A;
    system.RegisterAction(key);
    system.RegisterAction(pad);

    const auto bindings = system.GetBindings("Jump");
    ASSERT_EQ(bindings.size(), 2u);
    EXPECT_EQ(bindings[0].keyCode, Keys::Space);
    EXPECT_EQ(bindings[1].gamepadButton, GamepadButton::A);
    EXPECT_TRUE(system.GetBindings("Nothing").empty());
}

TEST(InputSystemTests, Names_ForPlayers) {
    EXPECT_EQ(Keys::Name(Keys::FromChar('w')), "W");
    EXPECT_EQ(Keys::Name('7'), "7");
    EXPECT_EQ(Keys::Name(Keys::F5), "F5");
    EXPECT_EQ(Keys::Name(Keys::Escape), "Esc");
    EXPECT_EQ(Keys::Name(Keys::Space), "Space");
    EXPECT_EQ(GamepadButtonName(GamepadButton::LeftShoulder), "LB");
    EXPECT_EQ(GamepadButtonName(GamepadButton::Back), "View");
    EXPECT_EQ(GamepadButtonName(GamepadButton::DPadUp), "D-pad up");
}

TEST(InputSystemTests, SetBindings_ReplacesAnAction_AndClearActionsForgetsAll) {
    InputSystem system;
    system.Initialize();
    InputAction key{};
    key.name = "Jump";
    key.keyCode = Keys::Space;
    system.RegisterAction(key);
    InputAction other{};
    other.keyCode = Keys::FromChar('J');

    system.SetBindings("Jump", { other });

    ASSERT_EQ(system.GetBindings("Jump").size(), 1u);
    EXPECT_EQ(system.GetBindings("Jump")[0].keyCode, Keys::FromChar('J'));
    EXPECT_EQ(system.GetBindings("Jump")[0].name, "Jump");   // named for the action
    system.ClearActions();
    EXPECT_TRUE(system.GetBindings("Jump").empty());
}

TEST(InputSystemTests, FirstPressed_KeyAndButton_ThisFrameOnly) {
    InputSystem system;
    system.Initialize();
    EXPECT_FALSE(system.FirstKeyPressed().has_value());
    system.SetKeyState(Keys::FromChar('K'), KeyState::Pressed);
    GamepadState pad{};
    pad.connected = true;
    pad.buttons = ButtonMask(GamepadButton::Y) | ButtonMask(GamepadButton::DPadLeft);
    system.SetGamepadState(0, pad);

    EXPECT_EQ(system.FirstKeyPressed(), Keys::FromChar('K'));
    EXPECT_EQ(system.FirstGamepadButtonPressed(), GamepadButton::Y);
    system.Update();
    EXPECT_FALSE(system.FirstKeyPressed().has_value());
    EXPECT_FALSE(system.FirstGamepadButtonPressed().has_value());
}
