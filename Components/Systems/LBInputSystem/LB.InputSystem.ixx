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
    /// Key codes. Values match Windows virtual-key codes so the Windows host can forward
    /// WM_KEYDOWN/WM_KEYUP wParam directly; other platforms map their keys to these.
    /// Letters and digits use their uppercase ASCII value (see FromChar).
    /// </summary>
    namespace Keys {
        inline constexpr int32_t Backspace = 0x08;
        inline constexpr int32_t Tab = 0x09;
        inline constexpr int32_t Enter = 0x0D;
        inline constexpr int32_t Shift = 0x10;
        inline constexpr int32_t Control = 0x11;
        inline constexpr int32_t Alt = 0x12;
        inline constexpr int32_t Escape = 0x1B;
        inline constexpr int32_t Space = 0x20;
        inline constexpr int32_t Left = 0x25;
        inline constexpr int32_t Up = 0x26;
        inline constexpr int32_t Right = 0x27;
        inline constexpr int32_t Down = 0x28;

        /// <summary>
        /// Key code for a letter ('a'-'z', 'A'-'Z') or digit ('0'-'9'); -1 for anything else
        /// </summary>
        constexpr int32_t FromChar(char c) noexcept {
            if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
            return -1;
        }
    }

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
            pressedSinceUpdate_.clear();
            releasedSinceUpdate_.clear();
            mouseButtonsPressedSinceUpdate_.clear();
            actions_.clear();
            initialized_ = false;
        }

        /// <summary>
        /// Advance to the next input frame (call once per simulation step, after the step has
        /// read input). The current state becomes the previous state and per-frame mouse
        /// deltas reset. Device state is pushed in by the host through the Set* methods.
        /// </summary>
        void Update() {
            if (!initialized_) return;

            previousMouseState_ = currentMouseState_;

            currentMouseState_.deltaX = 0;
            currentMouseState_.deltaY = 0;
            currentMouseState_.wheelDelta = 0;

            pressedSinceUpdate_.clear();
            releasedSinceUpdate_.clear();
            mouseButtonsPressedSinceUpdate_.clear();
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
        /// Check if a key went down during this input frame. Transitions are recorded as they
        /// happen, so a press is reported even if the key was released again before the frame
        /// was read (a quick tap between two simulation steps).
        /// </summary>
        bool IsKeyPressed(int32_t keyCode) const {
            return pressedSinceUpdate_.contains(keyCode);
        }

        /// <summary>
        /// Check if a key went up during this input frame
        /// </summary>
        bool IsKeyReleased(int32_t keyCode) const {
            return releasedSinceUpdate_.contains(keyCode);
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
        /// Check if a mouse button went down during this input frame
        /// </summary>
        bool IsMouseButtonPressed(MouseButton button) const {
            return mouseButtonsPressedSinceUpdate_.contains(button);
        }

        /// <summary>
        /// Register an input action mapping
        /// </summary>
        void RegisterAction(const InputAction& action) {
            actions_[action.name] = action;
        }

        /// <summary>
        /// Check if a registered action was triggered (its key or button went down) this input frame
        /// </summary>
        bool IsActionTriggered(const String& actionName) const {
            auto it = actions_.find(actionName);
            if (it == actions_.end()) return false;

            const auto& action = it->second;

            switch (action.deviceType) {
                case InputDevice::Keyboard:
                    return IsKeyPressed(action.keyCode);
                case InputDevice::Mouse:
                    return IsMouseButtonPressed(action.mouseButton);
                case InputDevice::Gamepad:
                    // TODO: Implement gamepad button check
                    return false;
                default:
                    return false;
            }
        }

        /// <summary>
        /// Set key state (host input events, tests, or external input injection)
        /// </summary>
        void SetKeyState(int32_t keyCode, KeyState state) {
            auto& current = keyStates_[keyCode];
            const bool wasDown = current != KeyState::Released;
            const bool isDown = state != KeyState::Released;

            if (!wasDown && isDown) pressedSinceUpdate_.insert(keyCode);
            if (wasDown && !isDown) releasedSinceUpdate_.insert(keyCode);

            current = state;
        }

        /// <summary>
        /// Set the cursor position in client coordinates; the delta accumulates
        /// relative to the previous input frame
        /// </summary>
        void SetMousePosition(int32_t x, int32_t y) {
            currentMouseState_.x = x;
            currentMouseState_.y = y;
            currentMouseState_.deltaX = x - previousMouseState_.x;
            currentMouseState_.deltaY = y - previousMouseState_.y;
        }

        /// <summary>
        /// Set a mouse button's state (X1/X2 are not tracked)
        /// </summary>
        void SetMouseButton(MouseButton button, bool down) {
            bool* state = nullptr;
            switch (button) {
                case MouseButton::Left: state = &currentMouseState_.leftButton; break;
                case MouseButton::Right: state = &currentMouseState_.rightButton; break;
                case MouseButton::Middle: state = &currentMouseState_.middleButton; break;
                default: return;
            }

            if (down && !*state) mouseButtonsPressedSinceUpdate_.insert(button);
            *state = down;
        }

        /// <summary>
        /// Accumulate wheel movement for this input frame (positive = away from the user)
        /// </summary>
        void AddMouseWheel(int32_t delta) {
            currentMouseState_.wheelDelta += delta;
        }

        /// <summary>
        /// Release every key and mouse button, e.g. when the window loses focus and
        /// would otherwise never receive the matching up events
        /// </summary>
        void ReleaseAll() {
            for (auto& [keyCode, state] : keyStates_) {
                if (state != KeyState::Released) releasedSinceUpdate_.insert(keyCode);
                state = KeyState::Released;
            }
            currentMouseState_.leftButton = false;
            currentMouseState_.rightButton = false;
            currentMouseState_.middleButton = false;
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
        // Transitions since the last Update(), so taps shorter than a frame are not lost
        std::unordered_set<int32_t> pressedSinceUpdate_;
        std::unordered_set<int32_t> releasedSinceUpdate_;
        std::unordered_set<MouseButton> mouseButtonsPressedSinceUpdate_;
        MouseState currentMouseState_{};
        MouseState previousMouseState_{};
        std::unordered_map<int32_t, GamepadState> gamepadStates_;
        std::unordered_map<String, InputAction> actions_;
    };

} // namespace LB::Systems::Input