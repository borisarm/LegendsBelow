// XAudio2Backend.cpp - Audio output device through XAudio2
//
// Internal partition of LB.AudioSystem: XAudio2 and Win32 types never leave this file.
// XAudio2 ships with Windows (xaudio2.lib in the Windows SDK), so no extra runtime is needed.
// Each channel (music, effects, ambience, voice) is a stereo submix voice feeding the mastering
// voice; each playing sound is a source voice sending to its channel's submix, with its volume
// and a stereo pan. Callers address voices by plain handles.
module;

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wrl/client.h>
#include <objbase.h>

#include <xaudio2.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "xaudio2.lib")

module LB.AudioSystem:XAudio2Backend;

namespace LB::Systems::Audio::Detail
{
    using Microsoft::WRL::ComPtr;

    class XAudio2Backend
    {
    public:
        // Submix channels: music, sound effects, ambient, voice
        static constexpr int ChannelCount = 4;

        XAudio2Backend() = default;

        ~XAudio2Backend()
        {
            // Voices must be destroyed before the engine is released: sources, then submixes,
            // then the mastering voice
            for (auto& [handle, voice] : m_sources)
                voice->DestroyVoice();
            m_sources.clear();
            for (auto*& submix : m_submixes)
            {
                if (submix)
                    submix->DestroyVoice();
                submix = nullptr;
            }
            if (m_masteringVoice)
            {
                m_masteringVoice->DestroyVoice();
                m_masteringVoice = nullptr;
            }
            m_xaudio2.Reset();
            if (m_comInitialized)
                CoUninitialize();
        }

        XAudio2Backend(const XAudio2Backend&) = delete;
        XAudio2Backend& operator=(const XAudio2Backend&) = delete;

        // Create the engine, the mastering voice on the default output device, and a stereo
        // submix per channel. Returns false when there is no usable audio device.
        bool Initialize() noexcept
        {
            // XAudio2 finds the output device through Windows' device API, which needs COM on
            // this thread. A host that already initialized it (in either apartment) is fine; what
            // we initialize here is undone in the destructor.
            const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            m_comInitialized = SUCCEEDED(com);
            if (FAILED(com) && com != RPC_E_CHANGED_MODE)
                return false;

            if (FAILED(XAudio2Create(m_xaudio2.ReleaseAndGetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR)))
                return false;

            if (FAILED(m_xaudio2->CreateMasteringVoice(&m_masteringVoice)))
            {
                m_xaudio2.Reset();
                return false;
            }

            XAUDIO2_VOICE_DETAILS details{};
            m_masteringVoice->GetVoiceDetails(&details);
            for (auto*& submix : m_submixes)
            {
                if (FAILED(m_xaudio2->CreateSubmixVoice(&submix, 2, details.InputSampleRate)))
                    return false;   // the destructor releases what was created
            }
            return true;
        }

        void SetMasterVolume(float volume) noexcept
        {
            if (m_masteringVoice)
                m_masteringVoice->SetVolume(volume);
        }

        float GetMasterVolume() const noexcept
        {
            float volume = 0.0f;
            if (m_masteringVoice)
                m_masteringVoice->GetVolume(&volume);
            return volume;
        }

        void SetChannelVolume(int channel, float volume) noexcept
        {
            if (channel >= 0 && channel < ChannelCount && m_submixes[channel])
                m_submixes[channel]->SetVolume(volume);
        }

        // Start a sound: 16-bit PCM frames (the memory must stay valid until Stop), on a channel,
        // at a volume and pan. Returns its handle, or 0 when the voice could not be created.
        std::uint32_t Play(const std::int16_t* samples, std::size_t sampleCount, std::uint32_t channels, std::uint32_t sampleRate,
                           int channel, float volume, float pan, bool looping) noexcept
        {
            if (!m_xaudio2 || channel < 0 || channel >= ChannelCount || !m_submixes[channel] || sampleCount == 0 || channels == 0 || channels > 2)
                return 0;

            WAVEFORMATEX format{};
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = static_cast<WORD>(channels);
            format.nSamplesPerSec = sampleRate;
            format.wBitsPerSample = 16;
            format.nBlockAlign = static_cast<WORD>(channels * 2);
            format.nAvgBytesPerSec = sampleRate * format.nBlockAlign;

            XAUDIO2_SEND_DESCRIPTOR send{ 0, m_submixes[channel] };
            const XAUDIO2_VOICE_SENDS sends{ 1, &send };
            IXAudio2SourceVoice* voice = nullptr;
            if (FAILED(m_xaudio2->CreateSourceVoice(&voice, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sends)))
                return 0;

            XAUDIO2_BUFFER buffer{};
            buffer.Flags = XAUDIO2_END_OF_STREAM;
            buffer.AudioBytes = static_cast<UINT32>(sampleCount * sizeof(std::int16_t));
            buffer.pAudioData = reinterpret_cast<const BYTE*>(samples);
            buffer.LoopCount = looping ? XAUDIO2_LOOP_INFINITE : 0;
            if (FAILED(voice->SubmitSourceBuffer(&buffer)))
            {
                voice->DestroyVoice();
                return 0;
            }

            voice->SetVolume(volume);
            ApplyPan(voice, channels, channel, pan);
            voice->Start(0);

            const std::uint32_t handle = m_nextHandle++;
            m_sources.emplace(handle, voice);
            return handle;
        }

        // Stop a sound and release its voice (afterwards its samples may be freed)
        void Stop(std::uint32_t handle) noexcept
        {
            const auto it = m_sources.find(handle);
            if (it == m_sources.end())
                return;
            it->second->DestroyVoice();   // stops it and waits until it no longer reads the buffer
            m_sources.erase(it);
        }

        void Pause(std::uint32_t handle) noexcept
        {
            if (const auto it = m_sources.find(handle); it != m_sources.end())
                it->second->Stop(0);
        }

        void Resume(std::uint32_t handle) noexcept
        {
            if (const auto it = m_sources.find(handle); it != m_sources.end())
                it->second->Start(0);
        }

        // Stop all audio processing (application suspended or minimized)
        void Suspend() noexcept
        {
            if (m_xaudio2)
                m_xaudio2->StopEngine();
        }

        void Resume() noexcept
        {
            if (m_xaudio2)
                m_xaudio2->StartEngine();
        }

    private:
        // Constant-power stereo pan into the channel's stereo submix
        void ApplyPan(IXAudio2SourceVoice* voice, std::uint32_t sourceChannels, int channel, float pan) noexcept
        {
            const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * std::numbers::pi_v<float> * 0.25f;
            const float left = std::cos(angle) * std::numbers::sqrt2_v<float>;   // 1 at center
            const float right = std::sin(angle) * std::numbers::sqrt2_v<float>;
            // Rows: destination channels (L, R); columns: source channels
            const float mono[2] = { left, right };
            const float stereo[4] = { left, 0.0f, 0.0f, right };
            voice->SetOutputMatrix(m_submixes[channel], sourceChannels, 2, sourceChannels == 1 ? mono : stereo);
        }

        ComPtr<IXAudio2> m_xaudio2;
        IXAudio2MasteringVoice* m_masteringVoice = nullptr;
        IXAudio2SubmixVoice* m_submixes[ChannelCount] = {};
        std::unordered_map<std::uint32_t, IXAudio2SourceVoice*> m_sources;
        std::uint32_t m_nextHandle = 1;
        bool m_comInitialized = false;
    };
}
