// VorbisDecoder.cpp - LB.AudioSystem implementation unit: Ogg Vorbis decoding with stb_vorbis
// (the stb vcpkg port). stb_vorbis stays in this file's global module fragment.
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

} // namespace LB::Systems::Audio::Detail
