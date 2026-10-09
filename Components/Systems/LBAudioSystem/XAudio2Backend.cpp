// XAudio2Backend.cpp - Audio output device through XAudio2
//
// Internal partition of LB.AudioSystem: XAudio2 and Win32 types never leave this file.
// XAudio2 ships with Windows (xaudio2.lib in the Windows SDK), so no extra runtime is needed.
module;

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wrl/client.h>

#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")

module LB.AudioSystem:XAudio2Backend;

namespace LB::Systems::Audio::Detail
{
    using Microsoft::WRL::ComPtr;

    class XAudio2Backend
    {
    public:
        XAudio2Backend() = default;

        ~XAudio2Backend()
        {
            // Voices must be destroyed before the engine is released
            if (m_masteringVoice)
            {
                m_masteringVoice->DestroyVoice();
                m_masteringVoice = nullptr;
            }
        }

        XAudio2Backend(const XAudio2Backend&) = delete;
        XAudio2Backend& operator=(const XAudio2Backend&) = delete;

        // Create the engine and the mastering voice on the default output device.
        // Returns false when there is no usable audio device.
        bool Initialize() noexcept
        {
            if (FAILED(XAudio2Create(m_xaudio2.ReleaseAndGetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR)))
                return false;

            if (FAILED(m_xaudio2->CreateMasteringVoice(&m_masteringVoice)))
            {
                m_xaudio2.Reset();
                return false;
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
        ComPtr<IXAudio2> m_xaudio2;
        IXAudio2MasteringVoice* m_masteringVoice = nullptr;
    };
}
