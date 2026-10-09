// GamepadTypes.ixx - Gamepad vocabulary shared by LB.InputSystem and its device backend
export module LB.InputSystem:GamepadTypes;

import std;

export namespace LB::Systems::Input {

    /// <summary>
    /// Gamepad button enumeration. The value is the button's bit index in GamepadState::buttons.
    /// </summary>
    enum class GamepadButton {
        A = 0,
        B,
        X,
        Y,
        LeftShoulder,
        RightShoulder,
        Back,           // "View" on Xbox controllers
        Start,          // "Menu" on Xbox controllers
        LeftThumb,
        RightThumb,
        DPadUp,
        DPadDown,
        DPadLeft,
        DPadRight
    };

    /// <summary>
    /// Bit for a button in GamepadState::buttons
    /// </summary>
    constexpr std::uint32_t ButtonMask(GamepadButton button) noexcept {
        return 1u << static_cast<std::uint32_t>(button);
    }

    /// <summary>
    /// Gamepad state information. Thumbsticks are in [-1, 1] after dead-zone processing,
    /// triggers in [0, 1].
    /// </summary>
    struct GamepadState {
        float leftThumbX{0.0f};
        float leftThumbY{0.0f};
        float rightThumbX{0.0f};
        float rightThumbY{0.0f};
        float leftTrigger{0.0f};
        float rightTrigger{0.0f};
        std::uint32_t buttons{0};
        bool connected{false};
    };

    // Radial dead zones matching the classic XInput defaults (7849 and 8689 of 32767)
    inline constexpr float LeftThumbDeadZone = 0.24f;
    inline constexpr float RightThumbDeadZone = 0.265f;

    /// <summary>
    /// Apply a radial dead zone to a thumbstick: inputs inside the zone become (0, 0), and the
    /// remaining range is rescaled so output magnitude still spans [0, 1] smoothly.
    /// </summary>
    inline void ApplyRadialDeadZone(float& x, float& y, float deadZone) noexcept {
        const float magnitude = std::sqrt(x * x + y * y);
        if (magnitude <= deadZone || deadZone >= 1.0f) {
            x = 0.0f;
            y = 0.0f;
            return;
        }

        const float clamped = std::min(magnitude, 1.0f);
        const float scaled = (clamped - deadZone) / (1.0f - deadZone);
        x = x / magnitude * scaled;
        y = y / magnitude * scaled;
    }

} // namespace LB::Systems::Input
