// AudioDecoders.cpp - LB.AudioSystem implementation unit: decoding sound files to 16-bit PCM:
// Ogg Vorbis (stb_vorbis), MP3 (dr_mp3), and WAV (dr_wav), from the stb and drlibs vcpkg ports.
// The decoders stay in this file's global module fragment.
module;

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <vector>

// stb_vorbis is an implementation file compiled into this unit, so its code generation warnings
// are reported here despite /external:W0; it is vendor code, compiled without warnings
#pragma warning(push, 0)
#define STB_VORBIS_NO_STDIO
#include <stb_vorbis.c>
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>
#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include <dr_wav.h>
#pragma warning(pop)

module LB.AudioSystem;

namespace LB::Systems::Audio::Detail {

    std::optional<AudioClip> DecodeOggVorbis(std::span<const std::byte> encoded, std::string& error) {
        if (encoded.empty() || encoded.size() > static_cast<std::size_t>(INT_MAX)) {
            error = "empty or too large";
            return std::nullopt;
        }

        int channels = 0;
        int sampleRate = 0;
        short* decoded = nullptr;
        const int frames = stb_vorbis_decode_memory(reinterpret_cast<const unsigned char*>(encoded.data()), static_cast<int>(encoded.size()),
                                                    &channels, &sampleRate, &decoded);
        // Owned from the start: freed even if copying it out throws
        const std::unique_ptr<short, decltype(&std::free)> owned(decoded, &std::free);
        if (frames <= 0 || !decoded || channels < 1 || channels > 2 || sampleRate <= 0) {
            error = "not a mono or stereo Ogg Vorbis file";
            return std::nullopt;
        }

        AudioClip clip;
        clip.channels = static_cast<std::uint32_t>(channels);
        clip.sampleRate = static_cast<std::uint32_t>(sampleRate);
        clip.samples.assign(decoded, decoded + static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels));
        return clip;
    }

    std::optional<AudioClip> DecodeMp3(std::span<const std::byte> encoded, std::string& error) {
        drmp3_config config{};
        drmp3_uint64 frames = 0;
        drmp3_int16* decoded = drmp3_open_memory_and_read_pcm_frames_s16(encoded.data(), encoded.size(), &config, &frames, nullptr);
        const std::unique_ptr<drmp3_int16, void (*)(drmp3_int16*)> owned(decoded, [](drmp3_int16* p) { drmp3_free(p, nullptr); });
        if (!decoded || frames == 0 || config.channels < 1 || config.channels > 2 || config.sampleRate == 0) {
            error = "not a mono or stereo MP3 file";
            return std::nullopt;
        }

        AudioClip clip;
        clip.channels = config.channels;
        clip.sampleRate = config.sampleRate;
        clip.samples.assign(decoded, decoded + static_cast<std::size_t>(frames) * config.channels);
        return clip;
    }

    std::optional<AudioClip> DecodeWav(std::span<const std::byte> encoded, std::string& error) {
        unsigned int channels = 0;
        unsigned int sampleRate = 0;
        drwav_uint64 frames = 0;
        drwav_int16* decoded = drwav_open_memory_and_read_pcm_frames_s16(encoded.data(), encoded.size(), &channels, &sampleRate, &frames, nullptr);
        const std::unique_ptr<drwav_int16, void (*)(drwav_int16*)> owned(decoded, [](drwav_int16* p) { drwav_free(p, nullptr); });
        if (!decoded || frames == 0 || channels < 1 || channels > 2 || sampleRate == 0) {
            error = "not a mono or stereo WAV file";
            return std::nullopt;
        }

        AudioClip clip;
        clip.channels = channels;
        clip.sampleRate = sampleRate;
        clip.samples.assign(decoded, decoded + static_cast<std::size_t>(frames) * channels);
        return clip;
    }

} // namespace LB::Systems::Audio::Detail
