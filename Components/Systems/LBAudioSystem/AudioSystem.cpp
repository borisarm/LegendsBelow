// AudioSystem.cpp - LB.AudioSystem implementation unit (output device wiring)
module LB.AudioSystem;

import :XAudio2Backend;
import std;

namespace LB::Systems::Audio {

    struct AudioSystem::DeviceBackend {
        Detail::XAudio2Backend xaudio2;
    };

    AudioSystem::AudioSystem() = default;

    AudioSystem::~AudioSystem() = default;

    bool AudioSystem::EnableDeviceOutput() {
        if (device_) return true;

        auto backend = std::make_unique<DeviceBackend>();
        if (!backend->xaudio2.Initialize()) {
            return false;
        }

        device_ = std::move(backend);
        ApplyDeviceMasterVolume();
        if (suspended_) {
            device_->xaudio2.Suspend();
        }
        return true;
    }

    void AudioSystem::DisableDeviceOutput() {
        device_.reset();
    }

    void AudioSystem::Suspend() {
        suspended_ = true;
        if (device_) {
            device_->xaudio2.Suspend();
        }
    }

    void AudioSystem::Resume() {
        suspended_ = false;
        if (device_) {
            device_->xaudio2.Resume();
        }
    }

    void AudioSystem::ApplyDeviceMasterVolume() {
        if (device_) {
            device_->xaudio2.SetMasterVolume(GetMasterVolume());
        }
    }

} // namespace LB::Systems::Audio
