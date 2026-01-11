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
        AudioSystem() = default;
        ~AudioSystem() = default;

        /// <summary>
        /// Initialize the audio system
        /// </summary>
        bool Initialize() {
            // TODO: Initialize audio backend (XAudio2, etc.)
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
            initialized_ = false;
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
        /// Check if an audio source is playing
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
        bool initialized_{false};
        AudioSourceId nextSourceId_{1};
        std::unordered_map<AudioSourceId, AudioClipInfo> activeSources_;
        std::unordered_map<AudioChannel, float> channelVolumes_;
    };

} // namespace LB::Systems::Audio