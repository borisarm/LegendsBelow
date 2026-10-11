// AudioSystem.cpp - LB.AudioSystem implementation unit: clip loading, playback bookkeeping, and
// the output device wiring
module LB.AudioSystem;

import :XAudio2Backend;
import std;

namespace LB::Systems::Audio {

    namespace {
        // The device's submix for a channel (Master-channel sounds mix with the effects)
        int SubmixOf(AudioChannel channel) noexcept {
            switch (channel) {
            case AudioChannel::Music: return 0;
            case AudioChannel::Ambient: return 2;
            case AudioChannel::Voice: return 3;
            default: return 1;
            }
        }
    }

    struct AudioSystem::DeviceBackend {
        Detail::XAudio2Backend xaudio2;
    };

    AudioSystem::AudioSystem() = default;

    AudioSystem::~AudioSystem() {
        StopAll();   // voices go before the clips they read
    }

    bool AudioSystem::EnableDeviceOutput() {
        if (device_) return true;

        auto backend = std::make_unique<DeviceBackend>();
        if (!backend->xaudio2.Initialize()) {
            return false;
        }

        device_ = std::move(backend);
        ApplyDeviceVolumes();
        if (suspended_) {
            device_->xaudio2.Suspend();
        }
        return true;
    }

    void AudioSystem::DisableDeviceOutput() {
        for (auto& [id, source] : activeSources_) ReleaseVoice(source);
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

    void AudioSystem::ApplyDeviceVolumes() {
        if (!device_) return;
        device_->xaudio2.SetMasterVolume(GetMasterVolume());
        for (const auto channel : { AudioChannel::Music, AudioChannel::SoundEffects, AudioChannel::Ambient, AudioChannel::Voice }) {
            device_->xaudio2.SetChannelVolume(SubmixOf(channel), GetChannelVolume(channel));
        }
    }

    void AudioSystem::ReleaseVoice(Source& source) {
        if (device_ && source.voice != 0) {
            device_->xaudio2.Stop(source.voice);
        }
        source.voice = 0;
        source.info.onDevice = false;
    }

    ClipLoadResult AudioSystem::LoadClip(const String& clipId, const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return { std::format("{}: cannot be opened", path.filename().string()) };
        }
        std::vector<std::byte> encoded;
        in.seekg(0, std::ios::end);
        encoded.resize(static_cast<std::size_t>(in.tellg()));
        in.seekg(0, std::ios::beg);
        in.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));

        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::string error;
        std::optional<AudioClip> clip;
        if (extension == ".ogg") clip = Detail::DecodeOggVorbis(encoded, error);
        else if (extension == ".mp3") clip = Detail::DecodeMp3(encoded, error);
        else if (extension == ".wav") clip = Detail::DecodeWav(encoded, error);
        else error = "not a .ogg, .mp3, or .wav file";
        if (!clip) {
            return { std::format("{}: {}", path.filename().string(), error) };
        }
        clips_[clipId] = std::make_shared<const AudioClip>(std::move(*clip));
        return {};
    }

    AudioSourceId AudioSystem::PlaySound(const String& clipId, AudioChannel channel, float volume, bool looping, float pan) {
        if (!initialized_) return -1;

        // Make room: the oldest sound that will end on its own goes
        if (activeSources_.size() >= MaxSources) {
            auto oldest = activeSources_.end();
            for (auto it = activeSources_.begin(); it != activeSources_.end(); ++it) {
                if (!it->second.info.looping && (oldest == activeSources_.end() || it->second.order < oldest->second.order)) oldest = it;
            }
            if (oldest == activeSources_.end()) return -1;   // only loops: keep them
            ReleaseVoice(oldest->second);
            activeSources_.erase(oldest);
        }

        Source source;
        source.info.clipId = clipId;
        source.info.channel = channel;
        source.info.volume = std::clamp(volume, 0.0f, 1.0f);
        source.info.pan = std::clamp(pan, -1.0f, 1.0f);
        source.info.looping = looping;
        source.info.state = PlaybackState::Playing;
        source.order = nextOrder_++;
        if (const auto it = clips_.find(clipId); it != clips_.end()) {
            source.clip = it->second;
            source.info.durationSeconds = source.clip->DurationSeconds();
            if (device_) {
                source.voice = device_->xaudio2.Play(source.clip->samples.data(), source.clip->samples.size(), source.clip->channels,
                                                     source.clip->sampleRate, SubmixOf(channel), source.info.volume, source.info.pan, looping);
                source.info.onDevice = source.voice != 0;
            }
        }

        const AudioSourceId sourceId = nextSourceId_++;
        activeSources_.emplace(sourceId, std::move(source));
        playedClips_.push_back(clipId);
        return sourceId;
    }

    bool AudioSystem::Stop(AudioSourceId sourceId) {
        auto it = activeSources_.find(sourceId);
        if (it == activeSources_.end()) return false;

        ReleaseVoice(it->second);
        activeSources_.erase(it);
        return true;
    }

    bool AudioSystem::Pause(AudioSourceId sourceId) {
        auto it = activeSources_.find(sourceId);
        if (it == activeSources_.end()) return false;

        it->second.info.state = PlaybackState::Paused;
        if (device_ && it->second.voice != 0) device_->xaudio2.Pause(it->second.voice);
        return true;
    }

    bool AudioSystem::SetVolume(AudioSourceId sourceId, float volume) {
        auto it = activeSources_.find(sourceId);
        if (it == activeSources_.end()) return false;

        it->second.info.volume = std::clamp(volume, 0.0f, 1.0f);
        if (device_ && it->second.voice != 0) device_->xaudio2.SetVolume(it->second.voice, it->second.info.volume);
        return true;
    }

    bool AudioSystem::Resume(AudioSourceId sourceId) {
        auto it = activeSources_.find(sourceId);
        if (it == activeSources_.end()) return false;

        if (it->second.info.state == PlaybackState::Paused) {
            it->second.info.state = PlaybackState::Playing;
            if (device_ && it->second.voice != 0) device_->xaudio2.Resume(it->second.voice);
            return true;
        }
        return false;
    }

    void AudioSystem::StopAll() {
        for (auto& [id, source] : activeSources_) ReleaseVoice(source);
        activeSources_.clear();
    }

    void AudioSystem::PauseChannel(AudioChannel channel) {
        for (auto& [id, source] : activeSources_) {
            if (source.info.channel == channel && source.info.state == PlaybackState::Playing) {
                source.info.state = PlaybackState::Paused;
                if (device_ && source.voice != 0) device_->xaudio2.Pause(source.voice);
            }
        }
    }

    void AudioSystem::ResumeChannel(AudioChannel channel) {
        for (auto& [id, source] : activeSources_) {
            if (source.info.channel == channel && source.info.state == PlaybackState::Paused) {
                source.info.state = PlaybackState::Playing;
                if (device_ && source.voice != 0) device_->xaudio2.Resume(source.voice);
            }
        }
    }

    void AudioSystem::Update(float elapsedSeconds) {
        if (suspended_) return;
        for (auto it = activeSources_.begin(); it != activeSources_.end();) {
            auto& source = it->second;
            if (source.info.state == PlaybackState::Playing) source.info.elapsedSeconds += elapsedSeconds;
            const bool finished = !source.info.looping && source.info.state == PlaybackState::Playing
                && source.info.elapsedSeconds >= source.info.durationSeconds;
            if (finished) {
                ReleaseVoice(source);
                it = activeSources_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

} // namespace LB::Systems::Audio
