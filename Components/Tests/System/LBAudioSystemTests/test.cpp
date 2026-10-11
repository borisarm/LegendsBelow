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
