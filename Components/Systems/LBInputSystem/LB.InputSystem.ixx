// LB.InputSystem.ixx - Input handling and device management system
export module LB.InputSystem;

import LB.Core;
import std;

using namespace std;
/// <summary>
/// Input system for LegendsBelow
/// Handles keyboard, mouse, and gamepad input
/// Provides action mapping and input buffering
/// </summary>
export namespace LB::Systems::Input {
    using LB::String;

    /// <summary>
    /// Input device type
    /// </summary>
    enum class InputDevice {
        Keyboard = 0,
        Mouse,
        Gamepad
    };

    /// <summary>
    /// Key state
    /// </summary>
    enum class KeyState {
        Released = 0,
        Pressed,
        Held
    };

    /// <summary>
    /// Mouse button enumeration
    /// </summary>
    enum class MouseButton {
        Left = 0,
        Right,
        Middle,
        X1,
        X2
    };

    /// <summary>
    /// Gamepad button enumeration
    /// </summary>
    enum class GamepadButton {
        A = 0,
        B,
        X,
        Y,
        LeftShoulder,
        RightShoulder,
        Back,
        Start,
        LeftThumb,
        RightThumb,
        DPadUp,
        DPadDown,
        DPadLeft,
        DPadRight
    };

    /// <summary>
    /// Mouse state information
    /// </summary>
    struct MouseState {
        int32_t x{0};
        int32_t y{0};
        int32_t deltaX{0};
        int32_t deltaY{0};
        int32_t wheelDelta{0};
        bool leftButton{false};
        bool rightButton{false};
        bool middleButton{false};
    };

    /// <summary>
    /// Gamepad state information
    /// </summary>
    struct GamepadState {
        float leftThumbX{0.0f};
        float leftThumbY{0.0f};
        float rightThumbX{0.0f};
        float rightThumbY{0.0f};
        float leftTrigger{0.0f};
        float rightTrigger{0.0f};
        uint32_t buttons{0};
        bool connected{false};
    };

    /// <summary>
    /// Input action for mapping
    /// </summary>
    struct InputAction {
        String name;
        int32_t keyCode{-1};
        MouseButton mouseButton{MouseButton::Left};
        GamepadButton gamepadButton{GamepadButton::A};
        InputDevice deviceType{InputDevice::Keyboard};
    };

    /// <summary>
    /// Input system for managing user input
    /// </summary>
    class InputSystem {
    public:
        InputSystem() = default;
        ~InputSystem() = default;

        /// <summary>
        /// Initialize the input system
        /// </summary>
        bool Initialize() {
            initialized_ = true;
            return true;
        }

        /// <summary>
        /// Shutdown the input system
        /// </summary>
        void Shutdown() {
            keyStates_.clear();
            actions_.clear();
            initialized_ = false;
        }

        /// <summary>
        /// Update input state (call once per frame)
        /// </summary>
        void Update() {
            if (!initialized_) return;

            // Update previous frame state
            previousKeyStates_ = keyStates_;
            previousMouseState_ = currentMouseState_;

            // TODO: Poll input devices and update current state
        }

        /// <summary>
        /// Check if a key is currently pressed
        /// </summary>
        bool IsKeyDown(int32_t keyCode) const {
            auto it = keyStates_.find(keyCode);
            if (it == keyStates_.end()) return false;
            return it->second == KeyState::Pressed || it->second == KeyState::Held;
        }

        /// <summary>
        /// Check if a key was just pressed this frame
        /// </summary>
        bool IsKeyPressed(int32_t keyCode) const {
            auto it = keyStates_.find(keyCode);
            if (it == keyStates_.end()) return false;

            auto prevIt = previousKeyStates_.find(keyCode);
            bool wasPreviouslyUp = (prevIt == previousKeyStates_.end() || prevIt->second == KeyState::Released);

            return (it->second == KeyState::Pressed || it->second == KeyState::Held) && wasPreviouslyUp;
        }

        /// <summary>
        /// Check if a key was just released this frame
        /// </summary>
        bool IsKeyReleased(int32_t keyCode) const {
            auto it = keyStates_.find(keyCode);
            bool isCurrentlyUp = (it == keyStates_.end() || it->second == KeyState::Released);

            auto prevIt = previousKeyStates_.find(keyCode);
            if (prevIt == previousKeyStates_.end()) return false;

            bool wasPreviouslyDown = (prevIt->second == KeyState::Pressed || prevIt->second == KeyState::Held);

            return isCurrentlyUp && wasPreviouslyDown;
        }

        /// <summary>
        /// Get current mouse state
        /// </summary>
        const MouseState& GetMouseState() const {
            return currentMouseState_;
        }

        /// <summary>
        /// Get current gamepad state
        /// </summary>
        const GamepadState& GetGamepadState(int32_t gamepadIndex = 0) const {
            auto it = gamepadStates_.find(gamepadIndex);
            if (it == gamepadStates_.end()) {
                static GamepadState emptyState{};
                return emptyState;
            }
            return it->second;
        }

        /// <summary>
        /// Check if mouse button is down
        /// </summary>
        bool IsMouseButtonDown(MouseButton button) const {
            switch (button) {
                case MouseButton::Left: return currentMouseState_.leftButton;
                case MouseButton::Right: return currentMouseState_.rightButton;
                case MouseButton::Middle: return currentMouseState_.middleButton;
                default: return false;
            }
        }

        /// <summary>
        /// Register an input action mapping
        /// </summary>
        void RegisterAction(const InputAction& action) {
            actions_[action.name] = action;
        }

        /// <summary>
        /// Check if a registered action is triggered
        /// </summary>
        bool IsActionTriggered(const String& actionName) const {
            auto it = actions_.find(actionName);
            if (it == actions_.end()) return false;

            const auto& action = it->second;

            switch (action.deviceType) {
                case InputDevice::Keyboard:
                    return IsKeyPressed(action.keyCode);
                case InputDevice::Mouse:
                    return IsMouseButtonDown(action.mouseButton);
                case InputDevice::Gamepad:
                    // TODO: Implement gamepad button check
                    return false;
                default:
                    return false;
            }
        }

        /// <summary>
        /// Set key state (for testing or external input injection)
        /// </summary>
        void SetKeyState(int32_t keyCode, KeyState state) {
            keyStates_[keyCode] = state;
        }

        /// <summary>
        /// Check if system is initialized
        /// </summary>
        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized_;
        }

    private:
        bool initialized_{false};
        std::unordered_map<int32_t, KeyState> keyStates_;
        std::unordered_map<int32_t, KeyState> previousKeyStates_;
        MouseState currentMouseState_{};
        MouseState previousMouseState_{};
        std::unordered_map<int32_t, GamepadState> gamepadStates_;
        std::unordered_map<String, InputAction> actions_;
    };

} // namespace LB::Systems::Input