// InputSystem.cpp - LB.InputSystem implementation unit (platform device backend wiring)
module LB.InputSystem;

import :GameInputBackend;
import std;

namespace LB::Systems::Input {

    struct InputSystem::DeviceBackend {
        Detail::GameInputBackend gameInput;
    };

    InputSystem::InputSystem() = default;

    InputSystem::~InputSystem() = default;

    bool InputSystem::EnableDeviceInput() {
        if (devices_) return true;

        auto backend = std::make_unique<DeviceBackend>();
        if (!backend->gameInput.Initialize()) {
            return false;
        }

        devices_ = std::move(backend);
        return true;
    }

    void InputSystem::DisableDeviceInput() {
        devices_.reset();
    }

    void InputSystem::PollDevices() {
        if (!initialized_ || !devices_) return;

        // Single-player: the most recently active gamepad drives player 0
        SetGamepadState(0, devices_->gameInput.ReadGamepad());
    }

} // namespace LB::Systems::Input
