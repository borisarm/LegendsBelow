#include "pch.h"

import LB.AudioSystem;

using namespace LB::Systems::Audio;

// ============================================================================
// AudioSystem Initialization Tests
// ============================================================================

TEST(AudioSystemTests, Initialize_Success) {
    AudioSystem system;

    bool result = system.Initialize();

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsInitialized());
}

TEST(AudioSystemTests, Initialize_SetsDefaultVolumes) {
    AudioSystem system;
    system.Initialize();

    EXPECT_EQ(system.GetMasterVolume(), 1.0f);
    EXPECT_EQ(system.GetChannelVolume(AudioChannel::Music), 0.7f);
    EXPECT_EQ(system.GetChannelVolume(AudioChannel::SoundEffects), 1.0f);
}

TEST(AudioSystemTests, Shutdown_StopsAllAudio) {
    AudioSystem system;
    system.Initialize();
    system.PlaySound("test_sound");   // something to stop

    system.Shutdown();

    EXPECT_FALSE(system.IsInitialized());
    EXPECT_EQ(system.GetActiveSourceCount(), 0);
}

// ============================================================================
// Audio Playback Tests
// ============================================================================

TEST(AudioSystemTests, PlaySound_ValidClip_ReturnsValidId) {
    AudioSystem system;
    system.Initialize();

    auto sourceId = system.PlaySound("explosion");

    EXPECT_GT(sourceId, 0);
    EXPECT_TRUE(system.IsPlaying(sourceId));
}

TEST(AudioSystemTests, PlaySound_BeforeInitialize_ReturnsInvalidId) {
    AudioSystem system;

    auto sourceId = system.PlaySound("test_sound");

    EXPECT_EQ(sourceId, -1);
}

TEST(AudioSystemTests, PlaySound_WithCustomVolume_Success) {
    AudioSystem system;
    system.Initialize();

    auto sourceId = system.PlaySound("music", AudioChannel::Music, 0.5f);

    EXPECT_GT(sourceId, 0);
    EXPECT_TRUE(system.IsPlaying(sourceId));
}

TEST(AudioSystemTests, PlaySound_Looping_Success) {
    AudioSystem system;
    system.Initialize();

    auto sourceId = system.PlaySound("ambient", AudioChannel::Ambient, 1.0f, true);

    EXPECT_GT(sourceId, 0);
    EXPECT_TRUE(system.IsPlaying(sourceId));
}

TEST(AudioSystemTests, PlaySound_MultipleSounds_AllPlaying) {
    AudioSystem system;
    system.Initialize();

    auto id1 = system.PlaySound("sound1");
    auto id2 = system.PlaySound("sound2");
    auto id3 = system.PlaySound("sound3");

    EXPECT_EQ(system.GetActiveSourceCount(), 3);
    EXPECT_TRUE(system.IsPlaying(id1));
    EXPECT_TRUE(system.IsPlaying(id2));
    EXPECT_TRUE(system.IsPlaying(id3));
}

// ============================================================================
// Audio Control Tests
// ============================================================================

TEST(AudioSystemTests, Stop_PlayingSource_StopsAudio) {
    AudioSystem system;
    system.Initialize();
    auto sourceId = system.PlaySound("test_sound");

    bool result = system.Stop(sourceId);

    EXPECT_TRUE(result);
    EXPECT_FALSE(system.IsPlaying(sourceId));
}

TEST(AudioSystemTests, Stop_InvalidSource_ReturnsFalse) {
    AudioSystem system;
    system.Initialize();

    bool result = system.Stop(999);

    EXPECT_FALSE(result);
}

TEST(AudioSystemTests, Pause_PlayingSource_Pauses) {
    AudioSystem system;
    system.Initialize();
    auto sourceId = system.PlaySound("test_sound");

    bool result = system.Pause(sourceId);

    EXPECT_TRUE(result);
    EXPECT_FALSE(system.IsPlaying(sourceId));
}

TEST(AudioSystemTests, Resume_PausedSource_Resumes) {
    AudioSystem system;
    system.Initialize();
    auto sourceId = system.PlaySound("test_sound");
    system.Pause(sourceId);

    bool result = system.Resume(sourceId);

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsPlaying(sourceId));
}

TEST(AudioSystemTests, Resume_NonPausedSource_ReturnsFalse) {
    AudioSystem system;
    system.Initialize();
    auto sourceId = system.PlaySound("test_sound");

    bool result = system.Resume(sourceId);

    EXPECT_FALSE(result);
}

TEST(AudioSystemTests, StopAll_ClearsAllSources) {
    AudioSystem system;
    system.Initialize();
    system.PlaySound("sound1");
    system.PlaySound("sound2");
    system.PlaySound("sound3");

    system.StopAll();

    EXPECT_EQ(system.GetActiveSourceCount(), 0);
}

// ============================================================================
// Volume Control Tests
// ============================================================================

TEST(AudioSystemTests, SetMasterVolume_ValidValue_Updates) {
    AudioSystem system;
    system.Initialize();

    system.SetMasterVolume(0.5f);

    EXPECT_EQ(system.GetMasterVolume(), 0.5f);
}

TEST(AudioSystemTests, SetMasterVolume_AboveMax_Clamps) {
    AudioSystem system;
    system.Initialize();

    system.SetMasterVolume(1.5f);

    EXPECT_EQ(system.GetMasterVolume(), 1.0f);
}

TEST(AudioSystemTests, SetMasterVolume_BelowMin_Clamps) {
    AudioSystem system;
    system.Initialize();

    system.SetMasterVolume(-0.5f);

    EXPECT_EQ(system.GetMasterVolume(), 0.0f);
}

TEST(AudioSystemTests, SetChannelVolume_ValidValue_Updates) {
    AudioSystem system;
    system.Initialize();

    system.SetChannelVolume(AudioChannel::Music, 0.3f);

    EXPECT_EQ(system.GetChannelVolume(AudioChannel::Music), 0.3f);
}

TEST(AudioSystemTests, SetChannelVolume_MultipleChannels_IndependentControl) {
    AudioSystem system;
    system.Initialize();

    system.SetChannelVolume(AudioChannel::Music, 0.3f);
    system.SetChannelVolume(AudioChannel::SoundEffects, 0.8f);
    system.SetChannelVolume(AudioChannel::Ambient, 0.5f);

    EXPECT_EQ(system.GetChannelVolume(AudioChannel::Music), 0.3f);
    EXPECT_EQ(system.GetChannelVolume(AudioChannel::SoundEffects), 0.8f);
    EXPECT_EQ(system.GetChannelVolume(AudioChannel::Ambient), 0.5f);
}
// ============================================================================
// Channel Pause Tests
// ============================================================================

TEST(AudioSystemTests, PauseChannel_PausesOnlyThatChannel) {
    AudioSystem system;
    system.Initialize();
    auto sfx = system.PlaySound("hit", AudioChannel::SoundEffects);
    auto music = system.PlaySound("theme", AudioChannel::Music, 1.0f, true);

    system.PauseChannel(AudioChannel::SoundEffects);

    EXPECT_FALSE(system.IsPlaying(sfx));
    EXPECT_TRUE(system.IsPlaying(music));
    EXPECT_EQ(system.GetActiveSourceCount(), 2u);
}

TEST(AudioSystemTests, ResumeChannel_ResumesPausedSources) {
    AudioSystem system;
    system.Initialize();
    auto sfx = system.PlaySound("hit", AudioChannel::SoundEffects);
    system.PauseChannel(AudioChannel::SoundEffects);

    system.ResumeChannel(AudioChannel::SoundEffects);

    EXPECT_TRUE(system.IsPlaying(sfx));
}

TEST(AudioSystemTests, ResumeChannel_OtherChannel_NoEffect) {
    AudioSystem system;
    system.Initialize();
    auto sfx = system.PlaySound("hit", AudioChannel::SoundEffects);
    system.PauseChannel(AudioChannel::SoundEffects);

    system.ResumeChannel(AudioChannel::Ambient);

    EXPECT_FALSE(system.IsPlaying(sfx));
}

// ============================================================================
// Effective Volume Tests
// ============================================================================

TEST(AudioSystemTests, GetEffectiveVolume_MultipliesMasterChannelAndSource) {
    AudioSystem system;
    system.Initialize();
    system.SetMasterVolume(0.5f);
    system.SetChannelVolume(AudioChannel::SoundEffects, 0.8f);

    auto id = system.PlaySound("hit", AudioChannel::SoundEffects, 0.5f);

    EXPECT_FLOAT_EQ(system.GetEffectiveVolume(id), 0.5f * 0.8f * 0.5f);
}

TEST(AudioSystemTests, GetEffectiveVolume_MasterChannelSource_CountsMasterOnce) {
    AudioSystem system;
    system.Initialize();
    system.SetMasterVolume(0.5f);

    auto id = system.PlaySound("ui", AudioChannel::Master, 1.0f);

    EXPECT_FLOAT_EQ(system.GetEffectiveVolume(id), 0.5f);
}

TEST(AudioSystemTests, GetEffectiveVolume_UnknownSource_Zero) {
    AudioSystem system;
    system.Initialize();

    EXPECT_FLOAT_EQ(system.GetEffectiveVolume(999), 0.0f);
}

// ============================================================================
// Device and Suspend Tests
// ============================================================================

TEST(AudioSystemTests, HasDeviceOutput_Default_False) {
    AudioSystem system;
    system.Initialize();

    EXPECT_FALSE(system.HasDeviceOutput());
}

TEST(AudioSystemTests, SuspendResume_TracksState) {
    AudioSystem system;
    system.Initialize();

    system.Suspend();
    EXPECT_TRUE(system.IsSuspended());

    system.Resume();
    EXPECT_FALSE(system.IsSuspended());
}

TEST(AudioSystemTests, Shutdown_ClearsSuspendAndDevice) {
    AudioSystem system;
    system.Initialize();
    system.Suspend();

    system.Shutdown();

    EXPECT_FALSE(system.IsSuspended());
    EXPECT_FALSE(system.HasDeviceOutput());
}

TEST(AudioSystemTests, SetMasterVolume_WithoutDevice_StillTracked) {
    AudioSystem system;
    system.Initialize();

    system.SetMasterVolume(0.3f);

    EXPECT_FLOAT_EQ(system.GetMasterVolume(), 0.3f);
}

// ============================================================================
// Clip Loading and Playback Tests
// ============================================================================

namespace {
    // Assets/Sounds of the repository, found by walking up from the working directory
    std::filesystem::path ShippedSound(const char* file) {
        for (auto directory = std::filesystem::current_path(); !directory.empty(); directory = directory.parent_path()) {
            const auto candidate = directory / "Assets" / "Sounds" / file;
            if (std::filesystem::exists(candidate)) return candidate;
            if (directory == directory.parent_path()) break;
        }
        return {};
    }

    // A clip of `seconds` of silence (mono, 1 kHz) without a file
    AudioClip SilentClip(float seconds) {
        AudioClip clip;
        clip.channels = 1;
        clip.sampleRate = 1000;
        clip.samples.assign(static_cast<std::size_t>(seconds * 1000.0f), 0);
        return clip;
    }
}

TEST(AudioClipTests, LoadClip_ShippedOgg_DecodesToPcm) {
    AudioSystem system;
    const auto path = ShippedSound("knifeSlice.ogg");
    ASSERT_FALSE(path.empty());

    const auto result = system.LoadClip("swing", path);

    ASSERT_TRUE(result.Succeeded()) << result.error;
    const auto* clip = system.FindClip("swing");
    ASSERT_NE(clip, nullptr);
    EXPECT_GE(clip->channels, 1u);
    EXPECT_LE(clip->channels, 2u);
    EXPECT_GT(clip->sampleRate, 8000u);
    EXPECT_GT(clip->DurationSeconds(), 0.05f);
    EXPECT_LT(clip->DurationSeconds(), 5.0f);
}

TEST(AudioClipTests, LoadClip_MissingOrInvalid_ReportsTheFile) {
    AudioSystem system;
    const auto missing = system.LoadClip("nope", "Z:/no/such/sound.ogg");
    EXPECT_FALSE(missing.Succeeded());
    EXPECT_NE(missing.error.find("sound.ogg"), std::string::npos);

    const auto notOgg = ShippedSound("LICENSE-Kenney-RPGAudio.txt");
    ASSERT_FALSE(notOgg.empty());
    EXPECT_FALSE(system.LoadClip("text", notOgg).Succeeded());
    EXPECT_EQ(system.FindClip("text"), nullptr);
}

TEST(AudioPlaybackTests, Sound_EndsWhenItsClipDoes) {
    AudioSystem system;
    system.Initialize();
    system.AddClip("beep", SilentClip(0.5f));
    const auto id = system.PlaySound("beep");

    system.Update(0.3f);
    EXPECT_TRUE(system.IsPlaying(id));
    system.Update(0.3f);
    EXPECT_FALSE(system.IsPlaying(id));
    EXPECT_EQ(system.GetActiveSourceCount(), 0u);
}

TEST(AudioPlaybackTests, Looping_And_Paused_KeepGoing) {
    AudioSystem system;
    system.Initialize();
    system.AddClip("beep", SilentClip(0.5f));
    const auto loop = system.PlaySound("beep", AudioChannel::Ambient, 1.0f, true);
    const auto paused = system.PlaySound("beep");
    system.Pause(paused);

    system.Update(5.0f);

    EXPECT_TRUE(system.IsPlaying(loop));
    ASSERT_NE(system.GetSource(paused), nullptr);
    EXPECT_FLOAT_EQ(system.GetSource(paused)->elapsedSeconds, 0.0f);   // paused time does not count
}

TEST(AudioPlaybackTests, UnknownClip_SilentAndGoneAtTheNextUpdate) {
    AudioSystem system;
    system.Initialize();
    const auto id = system.PlaySound("never_loaded");
    EXPECT_TRUE(system.IsPlaying(id));

    system.Update(0.01f);

    EXPECT_FALSE(system.IsPlaying(id));
}

TEST(AudioPlaybackTests, PanAndVolume_Clamped) {
    AudioSystem system;
    system.Initialize();
    system.AddClip("beep", SilentClip(1.0f));
    const auto id = system.PlaySound("beep", AudioChannel::SoundEffects, 3.0f, false, -7.0f);

    ASSERT_NE(system.GetSource(id), nullptr);
    EXPECT_FLOAT_EQ(system.GetSource(id)->volume, 1.0f);
    EXPECT_FLOAT_EQ(system.GetSource(id)->pan, -1.0f);
}

TEST(AudioPlaybackTests, TooManySounds_OldestMakesRoom_LoopsStay) {
    AudioSystem system;
    system.Initialize();
    system.AddClip("beep", SilentClip(10.0f));
    const auto loop = system.PlaySound("beep", AudioChannel::Ambient, 1.0f, true);
    const auto first = system.PlaySound("beep");
    for (std::size_t i = 0; i < AudioSystem::MaxSources; ++i) system.PlaySound("beep");

    EXPECT_EQ(system.GetActiveSourceCount(), AudioSystem::MaxSources);
    EXPECT_TRUE(system.IsPlaying(loop));
    EXPECT_FALSE(system.IsPlaying(first));
}

TEST(AudioPlaybackTests, PlayedClips_RecordedInOrder) {
    AudioSystem system;
    system.Initialize();
    system.PlaySound("a");
    system.PlaySound("b");

    const auto played = system.TakePlayedClips();

    ASSERT_EQ(played.size(), 2u);
    EXPECT_EQ(played[0], "a");
    EXPECT_EQ(played[1], "b");
    EXPECT_TRUE(system.TakePlayedClips().empty());
}

TEST(AudioPlaybackTests, Suspended_TimeStands) {
    AudioSystem system;
    system.Initialize();
    system.AddClip("beep", SilentClip(0.5f));
    const auto id = system.PlaySound("beep");
    system.Suspend();

    system.Update(5.0f);

    EXPECT_TRUE(system.IsPlaying(id));
}

TEST(AudioDeviceTests, RealDevice_ShippedClip_PlaysOnAVoiceAndEnds) {
    AudioSystem system;
    system.Initialize();
    if (!system.EnableDeviceOutput()) {
        // GoogleTest 1.8.1 has no GTEST_SKIP: without a device (CI runners) there is nothing to check
        std::printf("  (no audio output device: nothing to check)\n");
        return;
    }
    ASSERT_TRUE(system.LoadClip("coins", ShippedSound("handleCoins.ogg")).Succeeded());
    system.SetMasterVolume(0.0f);   // inaudible while testing

    const auto id = system.PlaySound("coins", AudioChannel::SoundEffects, 1.0f, false, 0.5f);

    ASSERT_NE(system.GetSource(id), nullptr);
    EXPECT_TRUE(system.GetSource(id)->onDevice);
    system.Update(system.FindClip("coins")->DurationSeconds() + 0.1f);
    EXPECT_EQ(system.GetSource(id), nullptr);
    system.Shutdown();
}
