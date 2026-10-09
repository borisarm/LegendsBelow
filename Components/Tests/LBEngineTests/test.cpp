#include "pch.h"

import LB.Engine;
import std;

using namespace LB::Engine;
using namespace std::chrono_literals;

namespace {
    // One fixed step at the default 60 Hz rate, as a real-time duration.
    constexpr auto OneSixtieth = std::chrono::duration_cast<StepTimer::Clock::duration>(
        std::chrono::duration<double>(1.0 / 60.0));
}

// ============================================================================
// StepTimer Conversion Tests
// ============================================================================

TEST(StepTimerTests, TicksToSeconds_OneSecond_Converts) {
    EXPECT_DOUBLE_EQ(StepTimer::TicksToSeconds(StepTimer::TicksPerSecond), 1.0);
}

TEST(StepTimerTests, SecondsToTicks_HalfSecond_Converts) {
    EXPECT_EQ(StepTimer::SecondsToTicks(0.5), StepTimer::TicksPerSecond / 2);
}

TEST(StepTimerTests, InitialState_AllZero) {
    StepTimer timer;

    EXPECT_EQ(timer.GetFrameCount(), 0u);
    EXPECT_EQ(timer.GetElapsedTicks(), 0u);
    EXPECT_EQ(timer.GetTotalTicks(), 0u);
    EXPECT_EQ(timer.GetFramesPerSecond(), 0u);
    EXPECT_FALSE(timer.IsFixedTimeStep());
}

// ============================================================================
// StepTimer Variable Timestep Tests
// ============================================================================

TEST(StepTimerTests, VariableStep_CallsUpdateOncePerAdvance) {
    StepTimer timer;
    int updates = 0;

    timer.Advance(5ms, [&] { ++updates; });
    timer.Advance(20ms, [&] { ++updates; });

    EXPECT_EQ(updates, 2);
    EXPECT_EQ(timer.GetFrameCount(), 2u);
}

TEST(StepTimerTests, VariableStep_ElapsedMatchesRealDelta) {
    StepTimer timer;

    timer.Advance(20ms, [] {});

    EXPECT_NEAR(timer.GetElapsedSeconds(), 0.020, 1e-9);
    EXPECT_NEAR(timer.GetTotalSeconds(), 0.020, 1e-9);
}

TEST(StepTimerTests, VariableStep_LargeDelta_ClampedToTenthOfSecond) {
    StepTimer timer;

    timer.Advance(5s, [] {});

    EXPECT_EQ(timer.GetElapsedTicks(), StepTimer::TicksPerSecond / 10);
}

TEST(StepTimerTests, VariableStep_NegativeDelta_TreatedAsZero) {
    StepTimer timer;

    timer.Advance(-10ms, [] {});

    EXPECT_EQ(timer.GetElapsedTicks(), 0u);
    EXPECT_EQ(timer.GetFrameCount(), 1u);
}

// ============================================================================
// StepTimer Fixed Timestep Tests
// ============================================================================

TEST(StepTimerTests, FixedStep_LessThanTarget_NoUpdate) {
    StepTimer timer;
    timer.SetFixedTimeStep(true);
    int updates = 0;

    timer.Advance(5ms, [&] { ++updates; });

    EXPECT_EQ(updates, 0);
    EXPECT_EQ(timer.GetFrameCount(), 0u);
}

TEST(StepTimerTests, FixedStep_AccumulatesUntilTarget) {
    StepTimer timer;
    timer.SetFixedTimeStep(true);
    int updates = 0;

    timer.Advance(10ms, [&] { ++updates; });
    timer.Advance(10ms, [&] { ++updates; });  // 20ms total >= 16.67ms target

    EXPECT_EQ(updates, 1);
}

TEST(StepTimerTests, FixedStep_MultipleTargetsInOneAdvance_CatchesUp) {
    StepTimer timer;
    timer.SetFixedTimeStep(true);
    timer.SetTargetElapsedSeconds(0.010);
    int updates = 0;

    timer.Advance(35ms, [&] { ++updates; });

    EXPECT_EQ(updates, 3);
    EXPECT_EQ(timer.GetElapsedTicks(), StepTimer::SecondsToTicks(0.010));
    EXPECT_EQ(timer.GetTotalTicks(), 3 * StepTimer::SecondsToTicks(0.010));
}

TEST(StepTimerTests, FixedStep_NearTarget_SnapsToExactTarget) {
    StepTimer timer;
    timer.SetFixedTimeStep(true);
    int updates = 0;

    // 16.6ms is within 1/4 ms of the 60 Hz target, so it counts as exactly one step
    timer.Advance(16600us, [&] { ++updates; });

    EXPECT_EQ(updates, 1);
    EXPECT_EQ(timer.GetTotalTicks(), timer.GetTargetElapsedTicks());
}

TEST(StepTimerTests, FramesPerSecond_UpdatedAfterOneSecond) {
    StepTimer timer;
    timer.SetFixedTimeStep(true);

    for (int i = 0; i < 60; ++i) {
        timer.Advance(OneSixtieth, [] {});
    }
    // 60 truncated steps fall just short of one second; nudge past the boundary without another update
    timer.Advance(1ms, [] {});

    EXPECT_EQ(timer.GetFramesPerSecond(), 60u);
}

TEST(StepTimerTests, ResetElapsedTime_ClearsPendingTime) {
    StepTimer timer;
    timer.SetFixedTimeStep(true);
    int updates = 0;

    timer.Advance(10ms, [&] { ++updates; });
    timer.ResetElapsedTime();
    timer.Advance(10ms, [&] { ++updates; });  // would have reached the target without the reset

    EXPECT_EQ(updates, 0);
}

// ============================================================================
// GameEngine Lifecycle Tests (headless: no native window)
// ============================================================================

TEST(GameEngineTests, Construct_NotInitialized) {
    GameEngine engine;

    EXPECT_FALSE(engine.IsInitialized());
    EXPECT_FALSE(engine.HasGpuDevice());
}

TEST(GameEngineTests, Initialize_Headless_Succeeds) {
    GameEngine engine;

    EXPECT_TRUE(engine.Initialize(nullptr, 1280, 720));
    EXPECT_TRUE(engine.IsInitialized());
    EXPECT_FALSE(engine.HasGpuDevice());
}

TEST(GameEngineTests, Shutdown_ClearsInitialized) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Shutdown();

    EXPECT_FALSE(engine.IsInitialized());
}

TEST(GameEngineTests, Shutdown_Twice_NoError) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Shutdown();
    engine.Shutdown();

    EXPECT_FALSE(engine.IsInitialized());
}

TEST(GameEngineTests, GetDefaultWindowSize_ReturnsDefaults) {
    GameEngine engine;
    std::int32_t width = 0;
    std::int32_t height = 0;

    engine.GetDefaultWindowSize(width, height);

    EXPECT_EQ(width, GameEngine::DefaultWindowWidth);
    EXPECT_EQ(height, GameEngine::DefaultWindowHeight);
}

// ============================================================================
// GameEngine Loop Tests
// ============================================================================

TEST(GameEngineTests, DefaultTimer_IsFixedSixtyHertz) {
    GameEngine engine;

    EXPECT_TRUE(engine.GetTimer().IsFixedTimeStep());
    EXPECT_EQ(engine.GetTimer().GetTargetElapsedTicks(), StepTimer::SecondsToTicks(1.0 / 60.0));
}

TEST(GameEngineTests, Tick_BeforeInitialize_DoesNotAdvance) {
    GameEngine engine;

    engine.Tick(100ms);

    EXPECT_EQ(engine.GetTimer().GetFrameCount(), 0u);
}

TEST(GameEngineTests, Tick_OneStep_AdvancesOneUpdate) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetTimer().GetFrameCount(), 1u);
}

TEST(GameEngineTests, Tick_FixedStep_RunsCatchUpUpdates) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Tick(50ms);  // three 60 Hz steps (16.67ms each)

    EXPECT_EQ(engine.GetTimer().GetFrameCount(), 3u);
}

TEST(GameEngineTests, SetFixedTimeStep_Disabled_UpdatesEveryTick) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.SetFixedTimeStep(false);

    engine.Tick(1ms);
    engine.Tick(1ms);

    EXPECT_EQ(engine.GetTimer().GetFrameCount(), 2u);
}

TEST(GameEngineTests, OnResuming_DiscardsTimeSpentSuspended) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Tick(10ms);
    engine.OnSuspending();
    engine.OnResuming();
    engine.Tick(10ms);

    EXPECT_EQ(engine.GetTimer().GetFrameCount(), 0u);
}

TEST(GameEngineTests, WindowEvents_Headless_NoError) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.OnActivated();
    engine.OnDeactivated();
    engine.OnWindowMoved();
    engine.OnDisplayChange();
    engine.OnWindowSizeChanged(1024, 768);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.IsInitialized());
}

// ============================================================================
// GameEngine Input Tests
// ============================================================================

namespace {
    namespace Keys = LB::Systems::Input::Keys;
    using LB::Systems::Input::MouseButton;

    void TapKey(GameEngine& engine, std::int32_t key) {
        engine.OnKeyDown(key);
        engine.OnKeyUp(key);
    }
}

TEST(GameEngineInputTests, OnKeyDown_ForwardsToInputSystem) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.OnKeyDown(Keys::FromChar('W'));

    EXPECT_TRUE(engine.GetInput().IsKeyDown(Keys::FromChar('W')));
}

TEST(GameEngineInputTests, OnKeyUp_ForwardsToInputSystem) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.OnKeyDown(Keys::FromChar('W'));

    engine.OnKeyUp(Keys::FromChar('W'));

    EXPECT_FALSE(engine.GetInput().IsKeyDown(Keys::FromChar('W')));
}

TEST(GameEngineInputTests, InputBeforeInitialize_Ignored) {
    GameEngine engine;

    engine.OnKeyDown(Keys::Space);
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.GetInput().IsKeyDown(Keys::Space));
}

TEST(GameEngineInputTests, MouseEvents_ForwardedToInputSystem) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.OnMouseMove(120, 80);
    engine.OnMouseButton(MouseButton::Left, true);
    engine.OnMouseWheel(120);

    const auto& mouse = engine.GetInput().GetMouseState();
    EXPECT_EQ(mouse.x, 120);
    EXPECT_EQ(mouse.y, 80);
    EXPECT_TRUE(mouse.leftButton);
    EXPECT_EQ(mouse.wheelDelta, 120);
}

TEST(GameEngineInputTests, SimulationStep_ConsumesInputFrame) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.OnKeyDown(Keys::Space);

    engine.Tick(OneSixtieth);

    // The step read the press; the next frame no longer reports it, but the key is still held
    EXPECT_FALSE(engine.GetInput().IsKeyPressed(Keys::Space));
    EXPECT_TRUE(engine.GetInput().IsKeyDown(Keys::Space));
}

TEST(GameEngineInputTests, NoSimulationStep_InputKeptForNextStep) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.OnKeyDown(Keys::Space);

    engine.Tick(1ms);  // renders, but no 60 Hz step is due yet

    EXPECT_TRUE(engine.GetInput().IsKeyPressed(Keys::Space));
}

TEST(GameEngineInputTests, OnDeactivated_ReleasesHeldInput) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.OnKeyDown(Keys::FromChar('W'));
    engine.OnMouseButton(MouseButton::Right, true);

    engine.OnDeactivated();

    EXPECT_FALSE(engine.GetInput().IsKeyDown(Keys::FromChar('W')));
    EXPECT_FALSE(engine.GetInput().IsMouseButtonDown(MouseButton::Right));
}

// ============================================================================
// GameEngine Tactical Pause Tests
// ============================================================================

TEST(GameEnginePauseTests, InitialState_NotPaused) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.IsPaused());
    EXPECT_DOUBLE_EQ(engine.GetGameTimeSeconds(), 0.0);
}

TEST(GameEnginePauseTests, EscapeTap_TogglesPause) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.IsPaused());
}

TEST(GameEnginePauseTests, SecondEscapeTap_Unpauses) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_FALSE(engine.IsPaused());
}

TEST(GameEnginePauseTests, EscapeHeldAcrossSteps_TogglesOnce) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.OnKeyDown(Keys::Escape);

    engine.Tick(OneSixtieth);
    engine.OnKeyDown(Keys::Escape);  // auto-repeat
    engine.Tick(OneSixtieth);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.IsPaused());
}

TEST(GameEnginePauseTests, EscapeDuringCatchUpSteps_TogglesOnce) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    TapKey(engine, Keys::Escape);

    engine.Tick(50ms);  // three 60 Hz steps in one tick

    EXPECT_EQ(engine.GetTimer().GetFrameCount(), 3u);
    EXPECT_TRUE(engine.IsPaused());
}

TEST(GameEnginePauseTests, GameTime_AdvancesWhileRunning) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Tick(OneSixtieth);
    engine.Tick(OneSixtieth);

    EXPECT_NEAR(engine.GetGameTimeSeconds(), 2.0 / 60.0, 1e-6);
}

TEST(GameEnginePauseTests, GameTime_StopsWhilePaused) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.Tick(OneSixtieth);
    const double before = engine.GetGameTimeSeconds();

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.IsPaused());
    EXPECT_DOUBLE_EQ(engine.GetGameTimeSeconds(), before);
    EXPECT_EQ(engine.GetTimer().GetFrameCount(), 3u);  // the loop keeps stepping
}

TEST(GameEnginePauseTests, Shutdown_ClearsPause) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    engine.Shutdown();

    EXPECT_FALSE(engine.IsPaused());
}

// ============================================================================
// GameEngine Gamepad Tests
// ============================================================================

namespace {
    using LB::Systems::Input::ButtonMask;
    using LB::Systems::Input::GamepadButton;
    using LB::Systems::Input::GamepadState;

    GamepadState PadWith(std::uint32_t buttons) {
        GamepadState state{};
        state.connected = true;
        state.buttons = buttons;
        return state;
    }

    void TapGamepadButton(GameEngine& engine, GamepadButton button) {
        engine.OnGamepadState(0, PadWith(ButtonMask(button)));
        engine.OnGamepadState(0, PadWith(0));
    }
}

TEST(GameEngineGamepadTests, Headless_NoPlatformGamepadSupport) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.HasGamepadSupport());
}

TEST(GameEngineGamepadTests, OnGamepadState_ForwardsToInputSystem) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.OnGamepadState(0, PadWith(ButtonMask(GamepadButton::A)));

    EXPECT_TRUE(engine.GetInput().IsGamepadButtonDown(GamepadButton::A));
}

TEST(GameEngineGamepadTests, OnGamepadState_BeforeInitialize_Ignored) {
    GameEngine engine;

    engine.OnGamepadState(0, PadWith(ButtonMask(GamepadButton::A)));
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.GetInput().GetGamepadState(0).connected);
}

TEST(GameEngineGamepadTests, StartTap_TogglesPause) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    TapGamepadButton(engine, GamepadButton::Start);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.IsPaused());
}

TEST(GameEngineGamepadTests, StartThenEscape_PauseThenResume) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    TapGamepadButton(engine, GamepadButton::Start);
    engine.Tick(OneSixtieth);

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_FALSE(engine.IsPaused());
}

TEST(GameEngineGamepadTests, OtherButton_DoesNotPause) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    TapGamepadButton(engine, GamepadButton::A);
    engine.Tick(OneSixtieth);

    EXPECT_FALSE(engine.IsPaused());
}

TEST(GameEngineGamepadTests, OnDeactivated_ReleasesGamepadButtons) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.OnGamepadState(0, PadWith(ButtonMask(GamepadButton::RightShoulder)));

    engine.OnDeactivated();

    EXPECT_FALSE(engine.GetInput().IsGamepadButtonDown(GamepadButton::RightShoulder));
}

// ============================================================================
// GameEngine Audio Tests
// ============================================================================

namespace {
    using LB::Systems::Audio::AudioChannel;
}

TEST(GameEngineAudioTests, Headless_NoAudioDevice) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    EXPECT_TRUE(engine.GetAudio().IsInitialized());
    EXPECT_FALSE(engine.GetAudio().HasDeviceOutput());
}

TEST(GameEngineAudioTests, Pause_PausesGameplaySoundsButNotMusic) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    auto sfx = engine.GetAudio().PlaySound("swing", AudioChannel::SoundEffects);
    auto ambient = engine.GetAudio().PlaySound("drip", AudioChannel::Ambient, 1.0f, true);
    auto music = engine.GetAudio().PlaySound("crypt_theme", AudioChannel::Music, 1.0f, true);

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_FALSE(engine.GetAudio().IsPlaying(sfx));
    EXPECT_FALSE(engine.GetAudio().IsPlaying(ambient));
    EXPECT_TRUE(engine.GetAudio().IsPlaying(music));
}

TEST(GameEngineAudioTests, Unpause_ResumesGameplaySounds) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    auto sfx = engine.GetAudio().PlaySound("swing", AudioChannel::SoundEffects);
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.GetAudio().IsPlaying(sfx));
}

TEST(GameEngineAudioTests, SuspendResume_ForwardedToAudio) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.OnSuspending();
    EXPECT_TRUE(engine.GetAudio().IsSuspended());

    engine.OnResuming();
    EXPECT_FALSE(engine.GetAudio().IsSuspended());
}

// ============================================================================
// GameEngine Save/Load Tests
// ============================================================================

namespace {
    // Temporary save directory for one test, removed on destruction
    struct TempSaveDir {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "LegendsBelowTests" /
            ("engine_" + std::to_string(std::random_device{}()));

        ~TempSaveDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        std::string Utf8() const {
            const auto text = path.u8string();
            return std::string(text.begin(), text.end());
        }
    };

    void RunSteps(GameEngine& engine, int steps) {
        for (int i = 0; i < steps; ++i) {
            engine.Tick(OneSixtieth);
        }
    }
}

TEST(GameEngineSaveTests, Headless_NoDirectory_MemoryOnly) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.GetPersistence().IsPersistentStorage());
}

TEST(GameEngineSaveTests, SaveThenLoad_RestoresGameTime) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    RunSteps(engine, 30);
    const double savedTime = engine.GetGameTimeSeconds();
    ASSERT_TRUE(engine.SaveGame(1));

    RunSteps(engine, 30);
    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_DOUBLE_EQ(engine.GetGameTimeSeconds(), savedTime);
}

TEST(GameEngineSaveTests, LoadGame_MissingSlot_FailsAndKeepsState) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    RunSteps(engine, 10);
    const double before = engine.GetGameTimeSeconds();

    EXPECT_FALSE(engine.LoadGame(5));
    EXPECT_DOUBLE_EQ(engine.GetGameTimeSeconds(), before);
}

TEST(GameEngineSaveTests, SaveGame_BeforeInitialize_Fails) {
    GameEngine engine;

    EXPECT_FALSE(engine.SaveGame(0));
}

TEST(GameEngineSaveTests, QuickSaveAndQuickLoad_Keys) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    RunSteps(engine, 20);
    TapKey(engine, Keys::F5);
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetPersistence().SaveSlotExists(GameEngine::QuickSaveSlot));
    EXPECT_EQ(engine.GetPersistence().GetSaveSlotInfo(GameEngine::QuickSaveSlot)->saveName, "Quick Save");

    RunSteps(engine, 120);
    TapKey(engine, Keys::F9);
    engine.Tick(OneSixtieth);

    // Save and load both run at the start of a step, before that step's time is simulated:
    // the save captured 20 steps, and the step that loaded it adds one more
    const double stepSeconds = StepTimer::TicksToSeconds(engine.GetTimer().GetTargetElapsedTicks());
    EXPECT_NEAR(engine.GetGameTimeSeconds(), 21.0 * stepSeconds, 1e-9);
}

TEST(GameEngineSaveTests, SaveDirectory_SavesSurviveRestart) {
    TempSaveDir dir;
    double savedTime = 0.0;
    {
        GameEngine engine;
        engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });
        ASSERT_TRUE(engine.GetPersistence().IsPersistentStorage());
        RunSteps(engine, 45);
        savedTime = engine.GetGameTimeSeconds();
        ASSERT_TRUE(engine.SaveGame(2));
    }

    GameEngine restarted;
    restarted.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });

    ASSERT_TRUE(restarted.LoadGame(2));
    EXPECT_DOUBLE_EQ(restarted.GetGameTimeSeconds(), savedTime);
}

TEST(GameEngineSaveTests, VolumePreferences_SurviveRestart) {
    TempSaveDir dir;
    {
        GameEngine engine;
        engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });
        engine.GetAudio().SetMasterVolume(0.35f);
        engine.GetAudio().SetChannelVolume(AudioChannel::Music, 0.2f);
        engine.Shutdown();
    }

    GameEngine restarted;
    restarted.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });

    EXPECT_FLOAT_EQ(restarted.GetAudio().GetMasterVolume(), 0.35f);
    EXPECT_FLOAT_EQ(restarted.GetAudio().GetChannelVolume(AudioChannel::Music), 0.2f);
}

TEST(GameEngineSaveTests, UnusableSaveDirectory_FallsBackToMemory) {
    TempSaveDir dir;
    std::filesystem::create_directories(dir.path);
    std::ofstream(dir.path / "file") << "x";
    const auto filePath = (dir.path / "file").u8string();

    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = std::string(filePath.begin(), filePath.end()) }));

    EXPECT_FALSE(engine.GetPersistence().IsPersistentStorage());
    EXPECT_TRUE(engine.SaveGame(0));
}

// ============================================================================
// GameEngine Player Movement Tests
// ============================================================================

namespace {
    float PlayerSpeed(const GameEngine& engine) {
        return *engine.GetCharacters().GetMovementSpeed(*engine.GetPlayerId());
    }

    double StepSeconds(const GameEngine& engine) {
        return StepTimer::TicksToSeconds(engine.GetTimer().GetTargetElapsedTicks());
    }

    void HoldKeysFor(GameEngine& engine, std::initializer_list<std::int32_t> keys, int steps) {
        for (auto key : keys) engine.OnKeyDown(key);
        RunSteps(engine, steps);
        for (auto key : keys) engine.OnKeyUp(key);
    }

    GamepadState Stick(float x, float y) {
        GamepadState state{};
        state.connected = true;
        state.leftThumbX = x;
        state.leftThumbY = y;
        return state;
    }
}

TEST(GameEngineMovementTests, Initialize_SpawnsPlayerAtOrigin) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    ASSERT_TRUE(engine.GetPlayerId().has_value());
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(character.GetClass(), LB::Character::CharacterClass::Warrior);
    EXPECT_EQ(character.GetPosition(), (LB::Position{0, 0}));
}

TEST(GameEngineMovementTests, HoldD_MovesRightAtCharacterSpeed) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    EXPECT_NEAR(engine.GetPlayerPosition().x, PlayerSpeed(engine) * 30 * StepSeconds(engine), 1e-4);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);
}

TEST(GameEngineMovementTests, HoldW_MovesUpTheScreen) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    HoldKeysFor(engine, { Keys::FromChar('W') }, 20);

    EXPECT_LT(engine.GetPlayerPosition().y, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
}

TEST(GameEngineMovementTests, ArrowKeys_AlsoMove) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    HoldKeysFor(engine, { Keys::Left, Keys::Down }, 20);

    EXPECT_LT(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_GT(engine.GetPlayerPosition().y, 0.0f);
}

TEST(GameEngineMovementTests, Diagonal_NotFasterThanStraight) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    HoldKeysFor(engine, { Keys::FromChar('D'), Keys::FromChar('S') }, 30);

    const auto p = engine.GetPlayerPosition();
    EXPECT_NEAR(std::sqrt(p.x * p.x + p.y * p.y), PlayerSpeed(engine) * 30 * StepSeconds(engine), 1e-4);
    EXPECT_NEAR(p.x, p.y, 1e-6);
}

TEST(GameEngineMovementTests, OppositeKeys_Cancel) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    HoldKeysFor(engine, { Keys::FromChar('A'), Keys::FromChar('D') }, 20);

    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
}

TEST(GameEngineMovementTests, LeftStick_AnalogSpeed) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.OnGamepadState(0, Stick(0.5f, 0.0f));
    RunSteps(engine, 30);

    EXPECT_NEAR(engine.GetPlayerPosition().x, 0.5 * PlayerSpeed(engine) * 30 * StepSeconds(engine), 1e-4);
}

TEST(GameEngineMovementTests, LeftStickUp_MovesUpTheScreen) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.OnGamepadState(0, Stick(0.0f, 1.0f));
    RunSteps(engine, 10);

    EXPECT_LT(engine.GetPlayerPosition().y, 0.0f);
}

TEST(GameEngineMovementTests, Paused_DoesNotMove) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
}

TEST(GameEngineMovementTests, ArenaEdge_StopsPlayer) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 60 * 10);  // far longer than needed to cross

    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, GameEngine::ArenaHalfWidth - GameEngine::PlayerSizeTiles * 0.5f);
}

TEST(GameEngineMovementTests, DomainTilePosition_FollowsPlayer) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 45);  // ~2.7 tiles at Warrior speed

    const auto expectedTileX = static_cast<std::int32_t>(std::floor(engine.GetPlayerPosition().x));
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_GE(expectedTileX, 2);
    EXPECT_EQ(character.GetPosition(), (LB::Position{expectedTileX, 0}));
}

TEST(GameEngineMovementTests, SaveLoad_RestoresPlayerPosition) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    HoldKeysFor(engine, { Keys::FromChar('D'), Keys::FromChar('W') }, 20);
    const auto saved = engine.GetPlayerPosition();
    ASSERT_TRUE(engine.SaveGame(1));

    HoldKeysFor(engine, { Keys::FromChar('A') }, 40);
    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, saved.x);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, saved.y);
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(character.GetPosition().x, static_cast<std::int32_t>(std::floor(saved.x)));
}

TEST(GameEngineMovementTests, Render_DrawsArenaAndCenteredPlayer) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Tick(OneSixtieth);

    const auto& rects = engine.GetRendering().GetQueuedRectangles();
    const auto arenaTiles = static_cast<std::size_t>(GameEngine::ArenaHalfWidth * 2 * GameEngine::ArenaHalfHeight * 2);
    ASSERT_EQ(rects.size(), arenaTiles + 1);

    // Player is drawn last, centered in the 800x600 window, 0.75 tiles of 32 px = 24 px
    const auto& player = rects.back();
    EXPECT_EQ(player.rect.x, 388);
    EXPECT_EQ(player.rect.y, 288);
    EXPECT_EQ(player.rect.width, 24);
    EXPECT_EQ(player.rect.height, 24);
    EXPECT_GT(player.layerDepth, rects.front().layerDepth);
}

TEST(GameEngineMovementTests, Shutdown_RemovesPlayer) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    engine.Shutdown();

    EXPECT_FALSE(engine.GetPlayerId().has_value());
    EXPECT_EQ(engine.GetCharacters().GetCharacterCount(), 0u);
}

// ============================================================================
// GameEngine Content Tests
// ============================================================================

namespace {
    std::optional<std::string> FindShippedContent() {
        for (auto dir = std::filesystem::current_path(); ; dir = dir.parent_path()) {
            const auto candidate = dir / "Assets" / "Data";
            if (std::filesystem::exists(candidate / "spells.json")) {
                const auto text = candidate.u8string();
                return std::string(text.begin(), text.end());
            }
            if (dir == dir.root_path()) return std::nullopt;
        }
    }
}

TEST(GameEngineContentTests, NoContentDirectory_StartsWithEmptyContent) {
    GameEngine engine;

    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600));

    ASSERT_NE(engine.GetContent(), nullptr);
    EXPECT_TRUE(engine.GetContent()->IsEmpty());
    EXPECT_TRUE(engine.GetInitializationError().empty());
}

TEST(GameEngineContentTests, ShippedContent_Loads) {
    const auto dir = FindShippedContent();
    ASSERT_TRUE(dir.has_value());
    GameEngine engine;

    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = *dir })) << engine.GetInitializationError();

    EXPECT_EQ(engine.GetContent()->GetSpells().size(), 45u);
    EXPECT_EQ(engine.GetContent()->GetWeapons().size(), 12u);
    EXPECT_EQ(engine.GetContent()->GetArmour().size(), 15u);
    EXPECT_NE(engine.GetContent()->FindWeapon("weapon_sword_rusted_shortsword_t1"), nullptr);
}

TEST(GameEngineContentTests, InvalidContent_FailsWithAllErrors) {
    TempSaveDir dir;
    std::filesystem::create_directories(dir.path);
    std::ofstream(dir.path / "spells.json") << R"([{"id":"spell_a","name":"A","school":"Nope","level":1,"categories":["Damage"],"manaCost":5,"castTimeSeconds":1.0}])";
    std::ofstream(dir.path / "weapons.json") << "[{";
    std::ofstream(dir.path / "armour.json") << "[]";
    GameEngine engine;

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = dir.Utf8() }));

    const auto& error = engine.GetInitializationError();
    EXPECT_NE(error.find("invalid (2 problems)"), std::string::npos) << error;
    EXPECT_NE(error.find("'school' has unknown value 'Nope'"), std::string::npos) << error;
    EXPECT_NE(error.find("weapons.json: not valid JSON"), std::string::npos) << error;
    EXPECT_FALSE(engine.IsInitialized());
}

TEST(GameEngineContentTests, MissingContentDirectory_Fails) {
    GameEngine engine;

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = "Z:/no/such/content" }));

    EXPECT_NE(engine.GetInitializationError().find("file not found"), std::string::npos);
}
