// GameInputBackend.cpp - Gamepad input through the GDK GameInput API
//
// Internal partition of LB.InputSystem: GameInput and Win32 types never leave this file.
// gameinput.lib is a static loader that finds the GameInput runtime at run time, so a
// machine without the runtime still starts the game; Initialize() just returns false.
module;

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wrl/client.h>

#include <GameInput.h>

#include <cstdint>

module LB.InputSystem:GameInputBackend;

import :GamepadTypes;

namespace LB::Systems::Input::Detail
{
    using Microsoft::WRL::ComPtr;
    using namespace GameInput::v3;

    class GameInputBackend
    {
    public:
        // Connect to the GameInput runtime. Returns false if it is not installed.
        bool Initialize() noexcept
        {
            return SUCCEEDED(GameInputCreate(m_gameInput.ReleaseAndGetAddressOf()));
        }

        // Read the active gamepad. The first gamepad that produces input becomes the active
        // one; if its reading fails (disconnected) the next poll picks up any other gamepad.
        GamepadState ReadGamepad()
        {
            GamepadState state{};
            if (!m_gameInput)
                return state;

            ComPtr<IGameInputReading> reading;
            if (FAILED(m_gameInput->GetCurrentReading(GameInputKindGamepad, m_device.Get(), reading.GetAddressOf())))
            {
                m_device.Reset();
                return state;
            }

            if (!m_device)
                reading->GetDevice(m_device.GetAddressOf());

            GameInputGamepadState raw{};
            if (!reading->GetGamepadState(&raw))
                return state;

            state.connected = true;
            state.buttons = ToButtons(raw.buttons);
            state.leftThumbX = raw.leftThumbstickX;
            state.leftThumbY = raw.leftThumbstickY;
            state.rightThumbX = raw.rightThumbstickX;
            state.rightThumbY = raw.rightThumbstickY;
            state.leftTrigger = raw.leftTrigger;
            state.rightTrigger = raw.rightTrigger;

            ApplyRadialDeadZone(state.leftThumbX, state.leftThumbY, LeftThumbDeadZone);
            ApplyRadialDeadZone(state.rightThumbX, state.rightThumbY, RightThumbDeadZone);

            return state;
        }

    private:
        static std::uint32_t ToButtons(GameInputGamepadButtons raw) noexcept
        {
            struct Mapping { GameInputGamepadButtons from; GamepadButton to; };
            static constexpr Mapping s_mappings[] =
            {
                { GameInputGamepadA,               GamepadButton::A },
                { GameInputGamepadB,               GamepadButton::B },
                { GameInputGamepadX,               GamepadButton::X },
                { GameInputGamepadY,               GamepadButton::Y },
                { GameInputGamepadLeftShoulder,    GamepadButton::LeftShoulder },
                { GameInputGamepadRightShoulder,   GamepadButton::RightShoulder },
                { GameInputGamepadView,            GamepadButton::Back },
                { GameInputGamepadMenu,            GamepadButton::Start },
                { GameInputGamepadLeftThumbstick,  GamepadButton::LeftThumb },
                { GameInputGamepadRightThumbstick, GamepadButton::RightThumb },
                { GameInputGamepadDPadUp,          GamepadButton::DPadUp },
                { GameInputGamepadDPadDown,        GamepadButton::DPadDown },
                { GameInputGamepadDPadLeft,        GamepadButton::DPadLeft },
                { GameInputGamepadDPadRight,       GamepadButton::DPadRight },
            };

            std::uint32_t buttons = 0;
            for (const auto& mapping : s_mappings)
            {
                if ((raw & mapping.from) != 0)
                    buttons |= ButtonMask(mapping.to);
            }
            return buttons;
        }

        ComPtr<IGameInput> m_gameInput;
        ComPtr<IGameInputDevice> m_device;
    };
}
