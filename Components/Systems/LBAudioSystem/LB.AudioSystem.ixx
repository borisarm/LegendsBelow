// LB.AudioSystem.ixx - Audio playback and management system
export module LB.AudioSystem;

import LB.Core;
import std;

/// <summary>
/// Audio system for LegendsBelow
/// Loads sound clips (Ogg Vorbis), plays them on mixing channels with volume and stereo pan,
/// and outputs through XAudio2 when a device is open. Without a device every source is still
/// tracked and ends when its clip would (headless tools and tests behave the same).
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
    using AudioSourceId = std::int32_t;

    /// <summary>
    /// A decoded sound: interleaved 16-bit PCM
    /// </summary>
    struct AudioClip {
        std::uint32_t channels{1};
        std::uint32_t sampleRate{44100};
        std::vector<std::int16_t> samples;   // frames x channels

        [[nodiscard]] float DurationSeconds() const noexcept {
            const auto frames = channels > 0 ? samples.size() / channels : 0;
            return sampleRate > 0 ? static_cast<float>(frames) / static_cast<float>(sampleRate) : 0.0f;
        }
    };

    /// <summary>
    /// Result of loading a clip: empty error on success
    /// </summary>
    struct ClipLoadResult {
        std::string error;
        [[nodiscard]] bool Succeeded() const noexcept { return error.empty(); }
    };

    /// <summary>
    /// A playing (or paused) sound
    /// </summary>
    struct AudioClipInfo {
        String clipId;
        AudioChannel channel{AudioChannel::SoundEffects};
        float volume{1.0f};
        float pan{0.0f};                     // -1 left .. 1 right
        bool looping{false};
        PlaybackState state{PlaybackState::Stopped};
        float elapsedSeconds{0.0f};
        float durationSeconds{0.0f};         // 0: no clip loaded under that name (silent)
        bool onDevice{false};                // a device voice is playing it (heard)
    };

    namespace Detail {
        // Decode a sound file in memory to 16-bit PCM (AudioDecoders.cpp)
        std::optional<AudioClip> DecodeOggVorbis(std::span<const std::byte> encoded, std::string& error);
        std::optional<AudioClip> DecodeMp3(std::span<const std::byte> encoded, std::string& error);
        std::optional<AudioClip> DecodeWav(std::span<const std::byte> encoded, std::string& error);
    }

    /// <summary>
    /// Audio system for managing sound playback
    /// </summary>
    class AudioSystem {
    public:
        // Sounds playing at once; past it the oldest non-looping sound makes room
        static constexpr std::size_t MaxSources = 48;

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
            ApplyDeviceVolumes();

            return true;
        }

        /// <summary>
        /// Shutdown the audio system (loaded clips are kept for the next Initialize)
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
        /// Decode a sound file (.ogg, .mp3, or .wav; mono or stereo) and keep it under a name for
        /// PlaySound. Loading a name again replaces the clip.
        /// </summary>
        ClipLoadResult LoadClip(const String& clipId, const std::filesystem::path& path);

        /// <summary>
        /// Keep an already decoded clip under a name (generated sounds, tests)
        /// </summary>
        void AddClip(const String& clipId, AudioClip clip) {
            clips_[clipId] = std::make_shared<const AudioClip>(std::move(clip));
        }

        [[nodiscard]] const AudioClip* FindClip(std::string_view clipId) const {
            const auto it = clips_.find(clipId);
            return it != clips_.end() ? it->second.get() : nullptr;
        }

        /// <summary>
        /// Play a clip on a channel at a volume and stereo pan (-1 left, 1 right). A name with no
        /// clip loaded plays silently and ends at the next Update.
        /// </summary>
        AudioSourceId PlaySound(const String& clipId, AudioChannel channel = AudioChannel::SoundEffects, float volume = 1.0f, bool looping = false, float pan = 0.0f);

        /// <summary>
        /// Stop a playing audio source
        /// </summary>
        bool Stop(AudioSourceId sourceId);

        /// <summary>
        /// Pause a playing audio source
        /// </summary>
        bool Pause(AudioSourceId sourceId);

        /// <summary>
        /// Change a playing source's volume (fades, crossfades)
        /// </summary>
        bool SetVolume(AudioSourceId sourceId, float volume);

        /// <summary>
        /// Resume a paused audio source
        /// </summary>
        bool Resume(AudioSourceId sourceId);

        /// <summary>
        /// Stop all audio sources
        /// </summary>
        void StopAll();

        /// <summary>
        /// Advance playback: sounds that reached their clip's end are stopped and removed
        /// </summary>
        void Update(float elapsedSeconds);

        /// <summary>
        /// Set channel volume (0.0 to 1.0)
        /// </summary>
        void SetChannelVolume(AudioChannel channel, float volume) {
            channelVolumes_[channel] = std::clamp(volume, 0.0f, 1.0f);
            ApplyDeviceVolumes();
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
        void PauseChannel(AudioChannel channel);

        /// <summary>
        /// Resume every paused source on a channel
        /// </summary>
        void ResumeChannel(AudioChannel channel);

        /// <summary>
        /// Volume a source is heard at: master x channel x source volume (0 if not active)
        /// </summary>
        float GetEffectiveVolume(AudioSourceId sourceId) const {
            auto it = activeSources_.find(sourceId);
            if (it == activeSources_.end()) return 0.0f;

            const float channelVolume = it->second.info.channel == AudioChannel::Master ? 1.0f : GetChannelVolume(it->second.info.channel);
            return GetMasterVolume() * channelVolume * it->second.info.volume;
        }

        /// <summary>
        /// Check if a source is playing
        /// </summary>
        bool IsPlaying(AudioSourceId sourceId) const {
            auto it = activeSources_.find(sourceId);
            if (it == activeSources_.end()) return false;
            return it->second.info.state == PlaybackState::Playing;
        }

        /// <summary>
        /// A source's state and settings, if it is active
        /// </summary>
        [[nodiscard]] const AudioClipInfo* GetSource(AudioSourceId sourceId) const {
            auto it = activeSources_.find(sourceId);
            return it != activeSources_.end() ? &it->second.info : nullptr;
        }

        /// <summary>
        /// The clips started since the last call (oldest first): what the game asked to hear
        /// </summary>
        std::vector<String> TakePlayedClips() {
            return std::exchange(playedClips_, {});
        }

        /// <summary>
        /// Get number of active audio sources
        /// </summary>
        std::size_t GetActiveSourceCount() const {
            return activeSources_.size();
        }

        /// <summary>
        /// Check if system is initialized
        /// </summary>
        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized_;
        }

    private:
        struct StringHash {
            using is_transparent = void;
            [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept { return std::hash<std::string_view>{}(text); }
        };

        // An active source: its settings, the clip it plays (kept alive while the device reads
        // it), and the device voice (0 without a device)
        struct Source {
            AudioClipInfo info;
            std::shared_ptr<const AudioClip> clip;
            std::uint32_t voice{0};
            std::uint64_t order{0};          // start order, for making room
        };

        void ApplyDeviceVolumes();
        void ReleaseVoice(Source& source);

        // Platform output device (XAudio2), defined in AudioSystem.cpp; null when disabled
        struct DeviceBackend;
        std::unique_ptr<DeviceBackend> device_;

        bool suspended_{false};
        bool initialized_{false};
        AudioSourceId nextSourceId_{1};
        std::uint64_t nextOrder_{1};
        std::unordered_map<AudioSourceId, Source> activeSources_;
        std::unordered_map<AudioChannel, float> channelVolumes_;
        std::unordered_map<std::string, std::shared_ptr<const AudioClip>, StringHash, std::equal_to<>> clips_;
        std::vector<String> playedClips_;
    };

} // namespace LB::Systems::Audio
