// LB.AudioSystem.ixx - Audio playback and management system
export module LB.AudioSystem;

import LB.Core;
import std;

using namespace std;
/// <summary>
/// Audio system for LegendsBelow
/// Handles sound effects, music playback, and audio mixing
/// Supports spatial audio and volume control
/// </summary>
export namespace LB::Systems::Audio {
    using LB::String;

    /// <summary>
    /// Audio channel type
    /// </summary>
    enum class AudioChannel {
        Master = 0,
        Music,
        SoundEffects,
        Ambient,
        Voice
    };

    /// <summary>
    /// Audio playback state
    /// </summary>
    enum class PlaybackState {
        Stopped = 0,
        Playing,
        Paused
    };

    /// <summary>
    /// Audio source identifier
    /// </summary>
    using AudioSourceId = int32_t;

    /// <summary>
    /// Audio clip information
    /// </summary>
    struct AudioClipInfo {
        String clipId;
        AudioChannel channel{AudioChannel::SoundEffects};
        float volume{1.0f};
        bool looping{false};
        PlaybackState state{PlaybackState::Stopped};
    };

    /// <summary>
    /// Audio system for managing sound playback
    /// </summary>
    class AudioSystem {
    public:
        AudioSystem();
        ~AudioSystem();

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        /// <summary>
        /// Initialize the audio system
        /// </summary>
        bool Initialize() {
            initialized_ = true;

            // Set default volumes
            channelVolumes_[AudioChannel::Master] = 1.0f;
            channelVolumes_[AudioChannel::Music] = 0.7f;
            channelVolumes_[AudioChannel::SoundEffects] = 1.0f;
            channelVolumes_[AudioChannel::Ambient] = 0.5f;
            channelVolumes_[AudioChannel::Voice] = 1.0f;

            return true;
        }

        /// <summary>
        /// Shutdown the audio system
        /// </summary>
        void Shutdown() {
            StopAll();
            DisableDeviceOutput();
            suspended_ = false;
            initialized_ = false;
        }

        /// <summary>
        /// Open the platform audio output device (XAudio2 on Windows/Xbox). Returns false when
        /// no device is available; the system then keeps working without sound output.
        /// </summary>
        bool EnableDeviceOutput();

        /// <summary>
        /// Close the platform audio output device
        /// </summary>
        void DisableDeviceOutput();

        /// <summary>
        /// True when an audio output device is open
        /// </summary>
        [[nodiscard]] bool HasDeviceOutput() const noexcept {
            return device_ != nullptr;
        }

        /// <summary>
        /// Stop all audio processing (application suspended or minimized)
        /// </summary>
        void Suspend();

        /// <summary>
        /// Restart audio processing after Suspend
        /// </summary>
        void Resume();

        [[nodiscard]] bool IsSuspended() const noexcept {
            return suspended_;
        }

        /// <summary>
        /// Play a sound effect
        /// </summary>
        AudioSourceId PlaySound(const String& clipId, AudioChannel channel = AudioChannel::SoundEffects, float volume = 1.0f, bool looping = false) {
            if (!initialized_) return -1;

            AudioSourceId sourceId = nextSourceId_++;

            AudioClipInfo info{};
            info.clipId = clipId;
            info.channel = channel;
            info.volume = volume;
            info.looping = looping;
            info.state = PlaybackState::Playing;

            activeSources_[sourceId] = info;

            // TODO: Implement actual audio playback
            return sourceId;
        }

        /// <summary>
        /// Stop a playing audio source
        /// </summary>
        bool Stop(AudioSourceId sourceId) {
            auto it = activeSources_.find(sourceId);
            if (it == activeSources_.end()) return false;

            it->second.state = PlaybackState::Stopped;
            activeSources_.erase(it);
            return true;
        }

        /// <summary>
        /// Pause a playing audio source
        /// </summary>
        bool Pause(AudioSourceId sourceId) {
            auto it = activeSources_.find(sourceId);
            if (it == activeSources_.end()) return false;

            it->second.state = PlaybackState::Paused;
            return true;
        }

        /// <summary>
        /// Resume a paused audio source
        /// </summary>
        bool Resume(AudioSourceId sourceId) {
            auto it = activeSources_.find(sourceId);
            if (it == activeSources_.end()) return false;

            if (it->second.state == PlaybackState::Paused) {
                it->second.state = PlaybackState::Playing;
                return true;
            }
            return false;
        }

        /// <summary>
        /// Stop all audio sources
        /// </summary>
        void StopAll() {
            activeSources_.clear();
        }

        /// <summary>
        /// Set channel volume (0.0 to 1.0)
        /// </summary>
        void SetChannelVolume(AudioChannel channel, float volume) {
            channelVolumes_[channel] = std::clamp(volume, 0.0f, 1.0f);
            if (channel == AudioChannel::Master) {
                ApplyDeviceMasterVolume();
            }
        }

        /// <summary>
        /// Get channel volume
        /// </summary>
        float GetChannelVolume(AudioChannel channel) const {
            auto it = channelVolumes_.find(channel);
            if (it == channelVolumes_.end()) return 1.0f;
            return it->second;
        }

        /// <summary>
        /// Set master volume (0.0 to 1.0)
        /// </summary>
        void SetMasterVolume(float volume) {
            SetChannelVolume(AudioChannel::Master, volume);
        }

        /// <summary>
        /// Get master volume
        /// </summary>
        float GetMasterVolume() const {
            return GetChannelVolume(AudioChannel::Master);
        }

        /// <summary>
        /// Pause every playing source on a channel (e.g. sound effects during the tactical pause)
        /// </summary>
        void PauseChannel(AudioChannel channel) {
            for (auto& [id, source] : activeSources_) {
                if (source.channel == channel && source.state == PlaybackState::Playing) {
                    source.state = PlaybackState::Paused;
                }
            }
        }

        /// <summary>
        /// Resume every paused source on a channel
        /// </summary>
        void ResumeChannel(AudioChannel channel) {
            for (auto& [id, source] : activeSources_) {
                if (source.channel == channel && source.state == PlaybackState::Paused) {
                    source.state = PlaybackState::Playing;
                }
            }
        }

        /// <summary>
        /// Volume a source is heard at: master x channel x source volume (0 if not active)
        /// </summary>
        float GetEffectiveVolume(AudioSourceId sourceId) const {
            auto it = activeSources_.find(sourceId);
            if (it == activeSources_.end()) return 0.0f;

            const float channelVolume = it->second.channel == AudioChannel::Master ? 1.0f : GetChannelVolume(it->second.channel);
            return GetMasterVolume() * channelVolume * it->second.volume;
        }

        /// <summary>
        /// Check if a source is playing
        /// </summary>
        bool IsPlaying(AudioSourceId sourceId) const {
            auto it = activeSources_.find(sourceId);
            if (it == activeSources_.end()) return false;
            return it->second.state == PlaybackState::Playing;
        }

        /// <summary>
        /// Get number of active audio sources
        /// </summary>
        size_t GetActiveSourceCount() const {
            return activeSources_.size();
        }

        /// <summary>
        /// Check if system is initialized
        /// </summary>
        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized_;
        }

    private:
        void ApplyDeviceMasterVolume();

        // Platform output device (XAudio2), defined in AudioSystem.cpp; null when disabled
        struct DeviceBackend;
        std::unique_ptr<DeviceBackend> device_;

        bool suspended_{false};
        bool initialized_{false};
        AudioSourceId nextSourceId_{1};
        std::unordered_map<AudioSourceId, AudioClipInfo> activeSources_;
        std::unordered_map<AudioChannel, float> channelVolumes_;
    };

} // namespace LB::Systems::Audio