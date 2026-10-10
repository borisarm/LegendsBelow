#include "pch.h"

import LB.Engine;
import LB.Core;
import LB.Combat;
import LB.Content;
import LB.CombatService;
import std;

using namespace LB::Engine;
using namespace std::chrono_literals;
using LB::Systems::Rendering::IsometricCamera;
using LB::Systems::Rendering::ZoomLevel;

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
    // Assets/Data, found by walking up from the working directory (tests run from x64/<Config>)
    std::string ShippedContentDirectory() {
        for (auto dir = std::filesystem::current_path(); ; dir = dir.parent_path()) {
            const auto candidate = dir / "Assets" / "Data";
            if (std::filesystem::exists(candidate / "classes.json")) {
                const auto text = candidate.u8string();
                return std::string(text.begin(), text.end());
            }
            if (dir == dir.root_path()) return {};
        }
    }

    // The player is created from class data, so movement tests load the shipped content. They
    // test the player and the arena, so no enemies come.
    void InitializeWithContent(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false }))
            << engine.GetInitializationError();
    }

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

    GamepadState RightStick(float x, float y) {
        GamepadState state{};
        state.connected = true;
        state.rightThumbX = x;
        state.rightThumbY = y;
        return state;
    }

    // Distance from the spawn point (the origin)
    float Travelled(const GameEngine& engine) {
        const auto p = engine.GetPlayerPosition();
        return std::sqrt(p.x * p.x + p.y * p.y);
    }

    // Ground direction of a screen direction through the engine's camera (unit for unit input)
    LB::Systems::Rendering::GroundPoint Ground(const GameEngine& engine, float right, float up) {
        return engine.GetCamera().ScreenToGround(right, up);
    }

    // Where a ground point is drawn, through the camera the engine last rendered with
    LB::Systems::Rendering::ScreenPoint OnScreen(const GameEngine& engine, float x, float y) {
        return engine.GetRendering().WorldToScreen({ x, 0.0f, y });
    }

    // Placeholder arena: floor tiles plus the ring of wall blocks around them
    std::size_t ArenaMeshCount() {
        return static_cast<std::size_t>((GameEngine::ArenaHalfWidth * 2 + 2) * (GameEngine::ArenaHalfHeight * 2 + 2));
    }

    constexpr std::size_t PlayerMeshCount = 3;  // body, head, visor
}

TEST(GameEngineMovementTests, NoContent_NoPlayer) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600));

    HoldKeysFor(engine, { Keys::FromChar('D') }, 10);

    EXPECT_FALSE(engine.GetPlayerId().has_value());
    EXPECT_EQ(engine.GetRendering().GetQueuedMeshes().size(), ArenaMeshCount());  // arena only
    EXPECT_TRUE(engine.SaveGame(1));
    EXPECT_TRUE(engine.LoadGame(1));
}

TEST(GameEngineMovementTests, Initialize_SpawnsPlayerAtOrigin) {
    GameEngine engine;
    InitializeWithContent(engine);

    ASSERT_TRUE(engine.GetPlayerId().has_value());
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(character.GetClass(), LB::Character::CharacterClass::Warrior);
    EXPECT_EQ(character.GetPosition(), (LB::Position{0, 0}));
}

TEST(GameEngineMovementTests, HoldD_MovesScreenRightAtCharacterSpeed) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    // Isometric: screen-right is a ground diagonal at the default camera yaw
    const float distance = PlayerSpeed(engine) * 30 * static_cast<float>(StepSeconds(engine));
    const auto right = Ground(engine, 1.0f, 0.0f);
    EXPECT_NEAR(engine.GetPlayerPosition().x, right.x * distance, 1e-4);
    EXPECT_NEAR(engine.GetPlayerPosition().y, right.y * distance, 1e-4);
    EXPECT_GT(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_LT(engine.GetPlayerPosition().y, 0.0f);
}

TEST(GameEngineMovementTests, HoldD_PlayerGoesRightOnScreen) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 20);  // stays inside the camera dead zone

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_GT(end.x, start.x + 10.0f);
    EXPECT_NEAR(end.y, start.y, 1.0f);
}

TEST(GameEngineMovementTests, HoldW_MovesUpTheScreen) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('W') }, 20);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_LT(end.y, start.y - 10.0f);
    EXPECT_NEAR(end.x, start.x, 1.0f);
}

TEST(GameEngineMovementTests, ArrowKeys_AlsoMove) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::Left, Keys::Down }, 20);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_LT(end.x, start.x);
    EXPECT_GT(end.y, start.y);
}

TEST(GameEngineMovementTests, Diagonal_NotFasterThanStraight) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D'), Keys::FromChar('S') }, 30);

    const float distance = PlayerSpeed(engine) * 30 * static_cast<float>(StepSeconds(engine));
    EXPECT_NEAR(Travelled(engine), distance, 1e-4);
    const auto direction = Ground(engine, std::sqrt(0.5f), -std::sqrt(0.5f));
    EXPECT_NEAR(engine.GetPlayerPosition().x, direction.x * distance, 1e-4);
    EXPECT_NEAR(engine.GetPlayerPosition().y, direction.y * distance, 1e-4);
}

TEST(GameEngineMovementTests, OppositeKeys_Cancel) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('A'), Keys::FromChar('D') }, 20);

    EXPECT_FLOAT_EQ(Travelled(engine), 0.0f);
}

TEST(GameEngineMovementTests, LeftStick_AnalogSpeed) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, Stick(0.5f, 0.0f));
    RunSteps(engine, 30);

    EXPECT_NEAR(Travelled(engine), 0.5 * PlayerSpeed(engine) * 30 * StepSeconds(engine), 1e-4);
}

TEST(GameEngineMovementTests, LeftStickUp_MovesUpTheScreen) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, Stick(0.0f, 1.0f));
    RunSteps(engine, 10);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_LT(end.y, start.y);
}

TEST(GameEngineMovementTests, Paused_DoesNotMove) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    EXPECT_FLOAT_EQ(Travelled(engine), 0.0f);
}

TEST(GameEngineMovementTests, ArenaEdge_StopsPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 60 * 10);  // far longer than needed to cross

    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, GameEngine::ArenaHalfWidth - GameEngine::PlayerSizeTiles * 0.5f);
}

TEST(GameEngineMovementTests, DomainTilePosition_FollowsPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 45);  // ~2.7 tiles at Warrior speed

    const auto expectedTileX = static_cast<std::int32_t>(std::floor(engine.GetPlayerPosition().x));
    const auto expectedTileY = static_cast<std::int32_t>(std::floor(engine.GetPlayerPosition().y));
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_GE(expectedTileX, 1);
    EXPECT_LE(expectedTileY, -2);
    EXPECT_EQ(character.GetPosition(), (LB::Position{expectedTileX, expectedTileY}));
}

TEST(GameEngineMovementTests, SaveLoad_RestoresPlayerPosition) {
    GameEngine engine;
    InitializeWithContent(engine);
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
    InitializeWithContent(engine);

    engine.Tick(OneSixtieth);

    const auto& meshes = engine.GetRendering().GetQueuedMeshes();
    ASSERT_EQ(meshes.size(), ArenaMeshCount() + PlayerMeshCount);

    // The player's body stands on the floor at the player's position...
    const auto& body = meshes[ArenaMeshCount()];
    EXPECT_EQ(body.shape, LB::Systems::Rendering::MeshShape::Cylinder);
    EXPECT_FLOAT_EQ(body.position.x, 0.0f);
    EXPECT_FLOAT_EQ(body.position.z, 0.0f);
    EXPECT_FLOAT_EQ(body.position.y - body.size.y * 0.5f, 0.0f);

    // ...in the middle of the 800x600 window, where the camera looks
    const auto feet = OnScreen(engine, 0.0f, 0.0f);
    ASSERT_TRUE(feet.inFront);
    EXPECT_NEAR(feet.x, 400.0f, 0.01f);
    EXPECT_NEAR(feet.y, 300.0f, 0.01f);
}

TEST(GameEngineMovementTests, Render_FloorTopAtGroundAndWallsAroundArena) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.Tick(OneSixtieth);

    std::size_t walls = 0;
    for (const auto& mesh : engine.GetRendering().GetQueuedMeshes()) {
        const float top = mesh.position.y + mesh.size.y * 0.5f;
        if (top > 0.5f) {
            ++walls;
            const bool outsideX = std::abs(mesh.position.x) > GameEngine::ArenaHalfWidth;
            const bool outsideY = std::abs(mesh.position.z) > GameEngine::ArenaHalfHeight;
            EXPECT_TRUE(outsideX || outsideY) << mesh.position.x << ", " << mesh.position.z;
            EXPECT_FLOAT_EQ(top, GameEngine::WallHeight);
        }
        else {
            EXPECT_FLOAT_EQ(top, 0.0f);  // floor
        }
    }
    const auto perimeter = static_cast<std::size_t>((GameEngine::ArenaHalfWidth * 2 + 2) * 2 + GameEngine::ArenaHalfHeight * 2 * 2);
    EXPECT_EQ(walls, perimeter);
}

TEST(GameEngineMovementTests, Shutdown_RemovesPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);

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
    std::ofstream(dir.path / "classes.json") << "[]";
    std::ofstream(dir.path / "enemies.json") << "[]";
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

// ============================================================================
// GameEngine Class Data Tests
// ============================================================================

TEST(GameEngineClassDataTests, ShippedContent_ConfiguresPlayerFromClassData) {
    const auto dir = FindShippedContent();
    ASSERT_TRUE(dir.has_value());
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = *dir })) << engine.GetInitializationError();

    const auto* warrior = engine.GetContent()->FindClass(LB::Character::CharacterClass::Warrior);
    ASSERT_NE(warrior, nullptr);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    const float expected = warrior->baseMovementSpeed + static_cast<float>(player.GetAttributes().GetDexterityModifier()) * 0.1f;

    EXPECT_EQ(engine.GetContent()->GetClasses().size(), 4u);
    EXPECT_FLOAT_EQ(*engine.GetCharacters().GetMovementSpeed(*engine.GetPlayerId()), expected);
}

TEST(GameEngineClassDataTests, EditedClassSpeed_ChangesPlayerMovement) {
    const auto shipped = FindShippedContent();
    ASSERT_TRUE(shipped.has_value());
    TempSaveDir dir;
    std::filesystem::create_directories(dir.path);
    const std::filesystem::path source(std::u8string(shipped->begin(), shipped->end()));
    for (const char* file : { "spells.json", "weapons.json", "armour.json", "classes.json", "enemies.json" }) {
        std::filesystem::copy_file(source / file, dir.path / file);
    }

    // Data-only change: Warrior base speed 4.0 -> 8.0
    std::string classes;
    {
        std::ifstream in(dir.path / "classes.json", std::ios::binary);
        classes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const auto at = classes.find("\"baseMovementSpeed\": 4.0");
    ASSERT_NE(at, std::string::npos);
    classes.replace(at, std::string("\"baseMovementSpeed\": 4.0").size(), "\"baseMovementSpeed\": 8.0");
    std::ofstream(dir.path / "classes.json", std::ios::binary | std::ios::trunc) << classes;

    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = dir.Utf8(), .spawnEnemies = false })) << engine.GetInitializationError();
    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    const float speed = 8.0f + static_cast<float>(player.GetAttributes().GetDexterityModifier()) * 0.1f;
    EXPECT_NEAR(Travelled(engine), speed * 30 * StepSeconds(engine), 1e-4);
}

// ============================================================================
// GameEngine Class Growth and Armour Tests
// ============================================================================

TEST(GameEngineArmorTests, Player_HasExactClassStartingAttributes) {
    GameEngine engine;
    InitializeWithContent(engine);

    const auto* warrior = engine.GetContent()->FindClass(LB::Character::CharacterClass::Warrior);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(player.GetAttributes().strength, warrior->startingAttributes.strength);       // 16, no hidden bonus
    EXPECT_EQ(player.GetAttributes().constitution, warrior->startingAttributes.constitution);
    EXPECT_EQ(*engine.GetCharacters().GetAttributeCap(*engine.GetPlayerId(), LB::AttributeType::Strength), 25);
    EXPECT_EQ(*engine.GetCharacters().GetAttributeCap(*engine.GetPlayerId(), LB::AttributeType::Intelligence), 18);
}

TEST(GameEngineArmorTests, HeavyBreastplate_SlowsWarriorByScaledPenalty) {
    GameEngine engine;
    InitializeWithContent(engine);
    const float unarmored = PlayerSpeed(engine);

    ASSERT_TRUE(engine.EquipPlayerArmor("armor_chest_rusted_breastplate_t1"));  // Heavy, 15% movement, 5% cast

    // Warriors handle heavy armour at 0.5x: 7.5% slower
    EXPECT_FLOAT_EQ(PlayerSpeed(engine), unarmored * (1.0f - 0.15f * 0.5f));
    EXPECT_FLOAT_EQ(*engine.GetCharacters().GetCastTimeMultiplier(*engine.GetPlayerId()), 1.0f + 0.05f);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);
    EXPECT_NEAR(Travelled(engine), unarmored * (1.0f - 0.075f) * 30 * StepSeconds(engine), 1e-4);
}

TEST(GameEngineArmorTests, EquipPlayerArmor_UnknownKey_Fails) {
    GameEngine engine;
    InitializeWithContent(engine);

    EXPECT_FALSE(engine.EquipPlayerArmor("armor_does_not_exist"));
}

TEST(GameEngineArmorTests, EquipPlayerArmor_WithoutPlayer_Fails) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.EquipPlayerArmor("armor_chest_rusted_breastplate_t1"));
}

// ============================================================================
// GameEngine Camera Tests
// ============================================================================

TEST(GameEngineCameraTests, Initialize_CameraFocusesPlayerAtDefaults) {
    GameEngine engine;
    InitializeWithContent(engine);

    const auto& camera = engine.GetCamera();
    EXPECT_FLOAT_EQ(camera.GetFocus().x, 0.0f);
    EXPECT_FLOAT_EQ(camera.GetFocus().y, 0.0f);
    EXPECT_FLOAT_EQ(camera.GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees);
    EXPECT_EQ(camera.GetZoom(), ZoomLevel::Medium);
}

TEST(GameEngineCameraTests, KeyE_RotatesClockwise_KeyQ_Back) {
    GameEngine engine;
    InitializeWithContent(engine);

    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees + 90.0f);

    TapKey(engine, Keys::FromChar('Q'));
    RunSteps(engine, 1);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees);
}

TEST(GameEngineCameraTests, Rotation_EasesThenSettles) {
    GameEngine engine;
    InitializeWithContent(engine);

    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    EXPECT_TRUE(engine.GetCamera().IsRotating());

    RunSteps(engine, 30);  // 0.5 s > RotationSeconds
    EXPECT_FALSE(engine.GetCamera().IsRotating());
    EXPECT_FLOAT_EQ(engine.GetCamera().GetYawDegrees(), IsometricCamera::DefaultYawDegrees + 90.0f);
}

TEST(GameEngineCameraTests, AfterRotation_DStillMovesScreenRight) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    RunSteps(engine, 30);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 20);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_GT(end.x, start.x + 10.0f);
    EXPECT_NEAR(end.y, start.y, 1.0f);
}

TEST(GameEngineCameraTests, Rotation_WorksWhilePaused) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::Escape);
    RunSteps(engine, 1);
    ASSERT_TRUE(engine.IsPaused());

    TapKey(engine, Keys::FromChar('Q'));
    RunSteps(engine, 1);

    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees - 90.0f + 360.0f);
}

TEST(GameEngineCameraTests, MouseWheel_Zooms) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnMouseWheel(120);
    RunSteps(engine, 1);
    EXPECT_EQ(engine.GetCamera().GetZoom(), ZoomLevel::Close);

    engine.OnMouseWheel(-120);
    RunSteps(engine, 1);
    engine.OnMouseWheel(-120);
    RunSteps(engine, 1);
    EXPECT_EQ(engine.GetCamera().GetZoom(), ZoomLevel::Wide);

    RunSteps(engine, 60);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetDistance(), IsometricCamera::WideDistance);
}

TEST(GameEngineCameraTests, RightStickFlick_RotatesOncePerFlick) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, RightStick(1.0f, 0.0f));
    RunSteps(engine, 10);  // held: one rotation only
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees + 90.0f);

    engine.OnGamepadState(0, RightStick(0.0f, 0.0f));
    RunSteps(engine, 1);
    engine.OnGamepadState(0, RightStick(1.0f, 0.0f));
    RunSteps(engine, 1);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees + 180.0f);
}

TEST(GameEngineCameraTests, RightStickUp_ZoomsIn) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, RightStick(0.0f, 0.9f));
    RunSteps(engine, 1);

    EXPECT_EQ(engine.GetCamera().GetZoom(), ZoomLevel::Close);
}

TEST(GameEngineCameraTests, Camera_FollowsPlayerOutsideDeadZone) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 60);  // ~4 tiles

    const auto focus = engine.GetCamera().GetFocus();
    const auto player = engine.GetPlayerPosition();
    EXPECT_NEAR(std::hypot(player.x - focus.x, player.y - focus.y), IsometricCamera::DeadZoneRadius, 1e-4);
}

TEST(GameEngineCameraTests, Camera_StaysInsideDeadZone) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 10);  // well under 1.5 tiles

    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().y, 0.0f);
}

TEST(GameEngineCameraTests, LoadGame_SnapsCameraToPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);
    HoldKeysFor(engine, { Keys::FromChar('D') }, 120);
    ASSERT_TRUE(engine.SaveGame(1));
    HoldKeysFor(engine, { Keys::FromChar('A') }, 240);

    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().x, engine.GetPlayerPosition().x);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().y, engine.GetPlayerPosition().y);
}

TEST(GameEngineCameraTests, Render_UsesCameraView) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    RunSteps(engine, 30);

    const auto expected = engine.GetCamera().GetView();
    const auto& rendered = engine.GetRendering().GetCameraView();
    EXPECT_FLOAT_EQ(rendered.eye.x, expected.eye.x);
    EXPECT_FLOAT_EQ(rendered.eye.y, expected.eye.y);
    EXPECT_FLOAT_EQ(rendered.eye.z, expected.eye.z);
}

// ============================================================================
// GameEngine Model Tests
// ============================================================================

namespace {
    // Assets/Models next to Assets/Data
    std::string ShippedModelDirectory() {
        const auto content = ShippedContentDirectory();
        if (content.empty()) return {};
        const auto models = std::filesystem::path(std::u8string(content.begin(), content.end())).parent_path() / "Models";
        const auto text = models.u8string();
        return std::string(text.begin(), text.end());
    }

    // Arena and player only (no enemies)
    void InitializeWithModels(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .spawnEnemies = false }))
            << engine.GetInitializationError();
    }

    using LB::Systems::Rendering::ModelRenderData;

    // Queued draws of one model by name
    std::vector<ModelRenderData> DrawsOf(const GameEngine& engine, std::string_view name) {
        std::vector<ModelRenderData> draws;
        const auto& rendering = engine.GetRendering();
        for (const auto& draw : rendering.GetQueuedModels()) {
            if (rendering.GetModel(draw.model)->name == name) draws.push_back(draw);
        }
        return draws;
    }

    const LB::Systems::Rendering::ModelData& ModelNamed(const GameEngine& engine, std::string_view name) {
        return *engine.GetRendering().GetModel(*engine.GetRendering().FindModel(name));
    }
}

TEST(GameEngineModelTests, Render_DrawsArenaAndPlayerFromModels) {
    GameEngine engine;
    InitializeWithModels(engine);

    engine.Tick(OneSixtieth);

    const auto tiles = static_cast<std::size_t>(GameEngine::ArenaHalfWidth * 2 * GameEngine::ArenaHalfHeight * 2);
    const auto walls = static_cast<std::size_t>(2 * (GameEngine::ArenaHalfWidth + GameEngine::ArenaHalfHeight));  // perimeter / 2-tile segments
    EXPECT_EQ(DrawsOf(engine, "tileBrickB_small").size() + DrawsOf(engine, "tileBrickA_small").size(), tiles);
    EXPECT_GT(DrawsOf(engine, "tileBrickA_small").size(), 0u);
    EXPECT_EQ(DrawsOf(engine, "wall").size(), walls);
    EXPECT_EQ(DrawsOf(engine, "pillar").size(), 4u);
    EXPECT_EQ(DrawsOf(engine, "character_knight").size(), 1u);
    EXPECT_TRUE(engine.GetRendering().GetQueuedMeshes().empty());
}

TEST(GameEngineModelTests, FloorTiles_TopAtGroundLevel) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.Tick(OneSixtieth);

    const auto& tile = ModelNamed(engine, "tileBrickB_small");
    for (const auto& draw : DrawsOf(engine, "tileBrickB_small")) {
        EXPECT_NEAR(draw.position.y + tile.boundsMax.y * draw.scale, 0.0f, 1e-5);
        EXPECT_NEAR((tile.boundsMax.x - tile.boundsMin.x) * draw.scale, 1.0f, 0.05f);  // one tile wide
    }
}

TEST(GameEngineModelTests, Walls_InnerFaceOnArenaEdge) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.Tick(OneSixtieth);

    const auto& wall = ModelNamed(engine, "wall");
    const float halfDepth = wall.boundsMax.z * GameEngine::ModelScale;
    for (const auto& draw : DrawsOf(engine, "wall")) {
        EXPECT_NEAR(draw.position.y + wall.boundsMin.y * draw.scale, 0.0f, 1e-5);  // standing on the floor
        if (draw.yawRadians == 0.0f) {
            EXPECT_NEAR(std::abs(draw.position.z) - halfDepth, GameEngine::ArenaHalfHeight, 1e-4);
            EXPECT_LT(std::abs(draw.position.x), GameEngine::ArenaHalfWidth);
        }
        else {
            EXPECT_NEAR(std::abs(draw.position.x) - halfDepth, GameEngine::ArenaHalfWidth, 1e-4);
            EXPECT_LT(std::abs(draw.position.z), GameEngine::ArenaHalfHeight);
        }
    }
}

TEST(GameEngineModelTests, Player_StandsAtPositionFacingWalkDirection) {
    GameEngine engine;
    InitializeWithModels(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 20);

    const auto draws = DrawsOf(engine, "character_knight");
    ASSERT_EQ(draws.size(), 1u);
    const auto& knight = ModelNamed(engine, "character_knight");
    EXPECT_FLOAT_EQ(draws[0].position.x, engine.GetPlayerPosition().x);
    EXPECT_FLOAT_EQ(draws[0].position.z, engine.GetPlayerPosition().y);
    EXPECT_NEAR(draws[0].position.y + knight.boundsMin.y * draws[0].scale, 0.0f, 1e-5);

    // Facing (sin yaw, cos yaw) is the ground direction of screen-right
    const auto right = Ground(engine, 1.0f, 0.0f);
    EXPECT_NEAR(std::sin(draws[0].yawRadians), right.x, 1e-5);
    EXPECT_NEAR(std::cos(draws[0].yawRadians), right.y, 1e-5);
}

TEST(GameEngineModelTests, Paused_DimsModels) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.Tick(OneSixtieth);
    const float normal = engine.GetRendering().GetQueuedModels().front().tint.r;

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_LT(engine.GetRendering().GetQueuedModels().front().tint.r, normal * 0.5f);
}

TEST(GameEngineModelTests, MissingModels_StopStartupListingFiles) {
    GameEngine engine;
    const auto empty = std::filesystem::temp_directory_path() / "lb_no_models_here";
    std::filesystem::create_directories(empty);

    const bool ok = engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = empty.string() });

    EXPECT_FALSE(ok);
    const auto& error = engine.GetInitializationError();
    // Arena (4), knight and sword (2), enemy bodies and weapons (5), animation sets (3)
    EXPECT_NE(error.find("14 problems"), std::string::npos) << error;
    EXPECT_NE(error.find("wall.glb"), std::string::npos) << error;
    EXPECT_NE(error.find("character_knight.gltf"), std::string::npos) << error;
    EXPECT_NE(error.find("Skeleton_Minion.glb"), std::string::npos) << error;
    EXPECT_NE(error.find("Rig_Medium_General.glb"), std::string::npos) << error;
    std::filesystem::remove_all(empty);
}

TEST(GameEngineModelTests, NoModelDirectory_FallsBackToPrimitives) {
    GameEngine engine;
    InitializeWithContent(engine);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.GetRendering().GetQueuedModels().empty());
    EXPECT_FALSE(engine.GetRendering().GetQueuedMeshes().empty());
}

// ============================================================================
// GameEngine Wall Fade and Batching Tests
// ============================================================================

namespace {
    std::size_t CountFaded(const GameEngine& engine) {
        std::size_t faded = 0;
        for (float opacity : engine.GetWallOpacities()) {
            if (opacity < 1.0f) ++faded;
        }
        return faded;
    }

    // Walk toward the camera (screen-down) until the player is at the arena edge nearest to it
    void WalkToNearEdge(GameEngine& engine) {
        HoldKeysFor(engine, { Keys::FromChar('S') }, 60 * 6);
    }
}

TEST(GameEngineWallFadeTests, PlayerInTheMiddle_NoWallFades) {
    GameEngine engine;
    InitializeWithModels(engine);

    RunSteps(engine, 30);

    const auto opacities = engine.GetWallOpacities();
    EXPECT_EQ(opacities.size(), static_cast<std::size_t>(2 * (GameEngine::ArenaHalfWidth + GameEngine::ArenaHalfHeight)) + 4);
    EXPECT_EQ(CountFaded(engine), 0u);
}

TEST(GameEngineWallFadeTests, PlayerBehindNearWall_ThatWallFades) {
    GameEngine engine;
    InitializeWithModels(engine);

    WalkToNearEdge(engine);
    RunSteps(engine, 30);   // longer than WallFadeSeconds

    // At the default yaw the camera is south-east: the faded pieces are on those sides only
    std::size_t faded = 0;
    const auto& draws = engine.GetRendering().GetQueuedModels();
    for (const auto& draw : draws) {
        const auto& name = engine.GetRendering().GetModel(draw.model)->name;
        if ((name == "wall" || name == "pillar") && draw.tint.a < 1.0f) {
            ++faded;
            EXPECT_NEAR(draw.tint.a, GameEngine::FadedWallAlpha, 1e-5);
            EXPECT_TRUE(draw.position.z > GameEngine::ArenaHalfHeight || draw.position.x > GameEngine::ArenaHalfWidth)
                << draw.position.x << ", " << draw.position.z;
        }
    }
    EXPECT_GE(faded, 1u);
    EXPECT_LE(faded, 4u);   // only the pieces in the sight lines, not the whole side
}

TEST(GameEngineWallFadeTests, Fade_EasesInAndBackOut) {
    GameEngine engine;
    InitializeWithModels(engine);
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    ASSERT_GE(CountFaded(engine), 1u);

    // Walk back to the middle: the walls come back
    HoldKeysFor(engine, { Keys::FromChar('W') }, 60 * 3);
    RunSteps(engine, 30);
    EXPECT_EQ(CountFaded(engine), 0u);
}

TEST(GameEngineWallFadeTests, Fade_TakesWallFadeSeconds) {
    GameEngine engine;
    InitializeWithModels(engine);
    ASSERT_TRUE(engine.SaveGame(1));           // player in the middle
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    const auto faded = engine.GetWallOpacities();
    const auto wall = std::ranges::find_if(faded, [](float opacity) { return opacity < 1.0f; }) - faded.begin();
    ASSERT_LT(static_cast<std::size_t>(wall), faded.size());
    EXPECT_FLOAT_EQ(faded[wall], GameEngine::FadedWallAlpha);

    ASSERT_TRUE(engine.LoadGame(1));           // back in the middle at once: nothing hides the player
    RunSteps(engine, 1);
    const float step = static_cast<float>(StepSeconds(engine)) / GameEngine::WallFadeSeconds * (1.0f - GameEngine::FadedWallAlpha);
    EXPECT_NEAR(engine.GetWallOpacities()[wall], GameEngine::FadedWallAlpha + step, 1e-5);

    RunSteps(engine, 12);                      // 0.2 s in all
    EXPECT_FLOAT_EQ(engine.GetWallOpacities()[wall], 1.0f);
}

TEST(GameEngineWallFadeTests, CameraRotationWhilePaused_ChangesWhichWallsFade) {
    GameEngine engine;
    InitializeWithModels(engine);
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    const auto before = engine.GetWallOpacities();

    TapKey(engine, Keys::Escape);
    RunSteps(engine, 1);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 60);                  // half a turn: the camera is now on the opposite side

    EXPECT_TRUE(engine.IsPaused());
    EXPECT_NE(engine.GetWallOpacities(), before);
    EXPECT_EQ(CountFaded(engine), 0u);     // the player stands with their back to the far wall
}

TEST(GameEngineBatchingTests, Arena_FewDrawCalls) {
    GameEngine engine;
    InitializeWithModels(engine);

    engine.Tick(OneSixtieth);

    // One instanced draw per part of each model: floor tiles (2 models), wall, pillar, knight, sword
    std::size_t expected = 0;
    for (std::string_view name : { "tileBrickB_small", "tileBrickA_small", "wall", "pillar", "character_knight", "sword_common" }) {
        expected += ModelNamed(engine, name).parts.size();
    }
    expected += 1;                         // the HUD's overlay batch
    const auto& stats = engine.GetRendering().GetStats();
    EXPECT_EQ(stats.drawCalls, expected);
    EXPECT_LT(stats.drawCalls, 35u);       // was one draw per part per copy: several hundred
    EXPECT_GT(stats.modelsRendered, 300u);  // every tile and wall piece still drawn
}

TEST(GameEngineBatchingTests, FadedWalls_TwoExtraPassesPerPart) {
    GameEngine engine;
    InitializeWithModels(engine);
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    ASSERT_GE(CountFaded(engine), 1u);

    const auto& rendering = engine.GetRendering();
    const auto wallParts = ModelNamed(engine, "wall").parts.size();
    std::size_t expected = 0;
    for (std::string_view name : { "tileBrickB_small", "tileBrickA_small", "wall", "pillar", "character_knight", "sword_common" }) {
        expected += ModelNamed(engine, name).parts.size();
    }
    bool pillarFaded = false;
    for (const auto& draw : rendering.GetQueuedModels()) {
        if (rendering.GetModel(draw.model)->name == "pillar" && draw.tint.a < 1.0f) pillarFaded = true;
    }
    expected += 1;                         // the HUD's overlay batch
    expected += 2 * wallParts;
    if (pillarFaded) expected += 2 * ModelNamed(engine, "pillar").parts.size();
    EXPECT_EQ(rendering.GetStats().drawCalls, expected);
}

// ============================================================================
// Animator Tests
// ============================================================================

TEST(AnimatorTests, FirstClip_StartsWithoutFade) {
    Animator animator;

    animator.Play(5, true);

    EXPECT_EQ(animator.GetState().current.clip, 5u);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 1.0f);
    EXPECT_EQ(animator.GetState().previous.clip, LB::Systems::Rendering::InvalidAnimationId);
}

TEST(AnimatorTests, SwitchingClips_CrossFades) {
    Animator animator;
    animator.Play(5, true);
    animator.Update(0.5f);

    animator.Play(6, false);
    EXPECT_EQ(animator.GetState().previous.clip, 5u);
    EXPECT_FLOAT_EQ(animator.GetState().previous.timeSeconds, 0.5f);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 0.0f);
    EXPECT_FALSE(animator.GetState().current.loop);

    animator.Update(Animator::DefaultFadeSeconds * 0.5f);
    EXPECT_NEAR(animator.GetState().blend, 0.5f, 1e-5f);

    animator.Update(Animator::DefaultFadeSeconds);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 1.0f);
    EXPECT_EQ(animator.GetState().previous.clip, LB::Systems::Rendering::InvalidAnimationId);
}

TEST(AnimatorTests, SameClip_KeepsTimeUnlessRestarted) {
    Animator animator;
    animator.Play(5, true);
    animator.Update(0.4f);

    animator.Play(5, true, 2.0f);
    EXPECT_FLOAT_EQ(animator.CurrentTime(), 0.4f);
    animator.Update(0.1f);
    EXPECT_FLOAT_EQ(animator.CurrentTime(), 0.6f);   // twice as fast now

    animator.Play(5, true, 1.0f, true);
    EXPECT_FLOAT_EQ(animator.CurrentTime(), 0.0f);
}

TEST(AnimatorTests, NoClips_StaysInRestPose) {
    Animator animator;

    animator.Play(LB::Systems::Rendering::InvalidAnimationId, true);
    animator.Play(LB::Systems::Rendering::InvalidAnimationId, false, 1.0f, true);
    animator.Update(1.0f);

    EXPECT_EQ(animator.CurrentClip(), LB::Systems::Rendering::InvalidAnimationId);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 1.0f);
}

// ============================================================================
// Encounter Tests
// ============================================================================

namespace {
    constexpr float Step = 1.0f / 60.0f;
    constexpr std::uint32_t CombatSeed = 7;

    // Shipped content (and its enemy definitions), kept alive by the engine
    struct EncounterContent {
        GameEngine engine;
        const LB::Content::EnemyDefinition* minion{nullptr};
        const LB::Content::EnemyDefinition* warrior{nullptr};

        EncounterContent() {
            engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false });
            minion = engine.GetContent()->FindEnemy(GameEngine::LightEnemy);
            warrior = engine.GetContent()->FindEnemy(GameEngine::HeavyEnemy);
        }
    };

    Encounter MakeEncounter(const EncounterContent& content) {
        Encounter encounter(CombatSeed);
        encounter.Configure(content.minion, content.warrior, GameEngine::ArenaHalfWidth, GameEngine::ArenaHalfHeight);
        return encounter;
    }

    PlayerTarget PlayerAt(float x, float y, bool alive = true) {
        PlayerTarget player{};
        player.position = { x, y };
        player.alive = alive;
        player.defense.armorClass = 10;
        return player;
    }

    std::vector<CombatEvent> RunEncounter(Encounter& encounter, const PlayerTarget& player, float seconds) {
        std::vector<CombatEvent> events;
        for (int i = 0; i < static_cast<int>(std::lround(seconds / Step)); ++i) {
            for (const auto& event : encounter.Update(Step, player)) events.push_back(event);
        }
        return events;
    }

    // Hits for exactly `damage` (natural 1s still miss)
    LB::Services::Combat::WeaponAttackProfile FixedHit(std::int32_t damage) {
        LB::Services::Combat::WeaponAttackProfile attack{};
        attack.attackBonus = 100;
        attack.damageDice = LB::Dice{ 1, 2, 0 };
        attack.damageBonus = damage - 1;
        attack.damageType = LB::Combat::DamageType::Fire;   // no physical armour, no skeleton resistance
        attack.critRangeMin = 20;
        attack.critMultiplier = 1.0f;
        return attack;
    }

    // Strike until a hit lands (skipping natural 1s); the hit's event
    CombatEvent StrikeUntilHit(Encounter& encounter, std::uint32_t id, std::int32_t damage) {
        for (int i = 0; i < 20; ++i) {
            const auto event = encounter.StrikeEnemy(id, FixedHit(damage), encounter.FindEnemy(id)->position, 1.0f);
            if (event && event->result.hit) return *event;
        }
        ADD_FAILURE() << "no hit in 20 strikes";
        return {};
    }
}

TEST(EncounterTests, WaveComposition_GrowsAndCaps) {
    EXPECT_EQ(Encounter::WaveComposition(1), std::make_pair(2, 0));
    EXPECT_EQ(Encounter::WaveComposition(2), std::make_pair(2, 1));
    EXPECT_EQ(Encounter::WaveComposition(3), std::make_pair(3, 1));
    EXPECT_EQ(Encounter::WaveComposition(4), std::make_pair(3, 2));
    EXPECT_EQ(Encounter::WaveComposition(40), std::make_pair(4, 2));
}

TEST(EncounterTests, StartWave_RisesAwayFromThePlayer) {
    EncounterContent content;
    ASSERT_NE(content.minion, nullptr);
    ASSERT_NE(content.warrior, nullptr);
    auto encounter = MakeEncounter(content);

    encounter.StartWave(2, { 1.0f, 1.0f });

    EXPECT_EQ(encounter.GetWave(), 2);
    ASSERT_EQ(encounter.GetEnemies().size(), 3u);
    EXPECT_EQ(std::ranges::count(encounter.GetEnemies(), content.warrior, &Enemy::definition), 1);
    for (const auto& enemy : encounter.GetEnemies()) {
        EXPECT_EQ(enemy.state, EnemyState::Spawning);
        EXPECT_GE(Encounter::Distance(enemy.position, { 1.0f, 1.0f }), Encounter::MinimumSpawnDistance);
        EXPECT_LE(std::abs(enemy.position.x), GameEngine::ArenaHalfWidth);
        EXPECT_LE(std::abs(enemy.position.y), GameEngine::ArenaHalfHeight);
        EXPECT_EQ(enemy.hitPoints, enemy.definition->hitPoints);
    }
}

TEST(EncounterTests, Spawning_UntargetableUntilRisen) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f);

    EXPECT_FALSE(encounter.FindTarget({ 0.0f, 0.0f }, 2.0f).has_value());
    EXPECT_FALSE(encounter.StrikeEnemy(id, FixedHit(3), { 0.0f, 0.0f }, 2.0f).has_value());

    RunEncounter(encounter, PlayerAt(0.0f, 0.0f), Encounter::SpawnSeconds + Step);
    EXPECT_EQ(encounter.FindTarget({ 0.0f, 0.0f }, 2.0f), std::optional<std::uint32_t>(id));
}

TEST(EncounterTests, Idle_UntilThePlayerComesWithinAggroRange) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 9.0f, 0.0f }, 0.0f, false);

    RunEncounter(encounter, PlayerAt(-9.0f, 0.0f), 2.0f);   // 18 tiles away
    EXPECT_EQ(encounter.GetEnemies()[0].state, EnemyState::Idle);
    EXPECT_FLOAT_EQ(encounter.GetEnemies()[0].position.x, 9.0f);

    RunEncounter(encounter, PlayerAt(4.0f, 0.0f), 0.5f);
    EXPECT_EQ(encounter.GetEnemies()[0].state, EnemyState::Chasing);
    EXPECT_LT(encounter.GetEnemies()[0].position.x, 9.0f);
}

TEST(EncounterTests, ClosesInAndAttacksThePlayer) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 4.0f, 0.0f }, 0.0f, false);

    const auto events = RunEncounter(encounter, PlayerAt(0.0f, 0.0f), 4.0f);

    EXPECT_LE(encounter.FindEnemy(id)->position.x, content.minion->attackRange + 1e-3f);
    EXPECT_GE(encounter.FindEnemy(id)->position.x, Encounter::CharacterRadius * 2.0f - 1e-3f);
    ASSERT_FALSE(events.empty());
    for (const auto& event : events) {
        EXPECT_EQ(event.kind, CombatEventKind::EnemyAttack);
        EXPECT_EQ(event.enemyId, id);
    }
}

TEST(EncounterTests, Attack_LandsPartWayThroughItsTime) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 1.0f, 0.0f }, -std::numbers::pi_v<float> * 0.5f, false);   // facing the player
    const auto player = PlayerAt(0.0f, 0.0f);

    int stepsToAttack = 0;
    while (encounter.GetEnemies()[0].state != EnemyState::Attacking && stepsToAttack < 600) {
        encounter.Update(Step, player);
        ++stepsToAttack;
    }
    ASSERT_EQ(encounter.GetEnemies()[0].state, EnemyState::Attacking);

    int steps = 0;
    while (encounter.Update(Step, player).empty() && steps < 600) ++steps;
    const float impact = content.minion->attackTimeSeconds * content.minion->attackImpactFraction;
    EXPECT_NEAR((steps + 1) * Step, impact, Step * 1.5f);
}

TEST(EncounterTests, PlayerSteppingAway_AttackWhiffs) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 1.0f, 0.0f }, -std::numbers::pi_v<float> * 0.5f, false);
    for (int i = 0; i < 600 && encounter.GetEnemies()[0].state != EnemyState::Attacking; ++i) {
        encounter.Update(Step, PlayerAt(0.0f, 0.0f));
    }
    ASSERT_EQ(encounter.GetEnemies()[0].state, EnemyState::Attacking);

    const auto events = RunEncounter(encounter, PlayerAt(-4.0f, 0.0f), content.minion->attackTimeSeconds);

    EXPECT_TRUE(events.empty());
}

TEST(EncounterTests, PlayerDown_EnemiesStandDown) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f, false);

    const auto events = RunEncounter(encounter, PlayerAt(0.0f, 0.0f, false), 3.0f);

    EXPECT_TRUE(events.empty());
    EXPECT_EQ(encounter.GetEnemies()[0].state, EnemyState::Idle);
}

TEST(EncounterTests, StrikeEnemy_DamagesStaggersAndKills) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f, false);
    const auto hp = content.minion->hitPoints;

    const auto first = StrikeUntilHit(encounter, id, 3);
    EXPECT_EQ(first.kind, CombatEventKind::PlayerAttack);
    EXPECT_EQ(first.result.finalDamage, 3);
    EXPECT_FALSE(first.killed);
    EXPECT_EQ(encounter.FindEnemy(id)->hitPoints, hp - 3);
    EXPECT_EQ(encounter.FindEnemy(id)->state, EnemyState::Staggered);
    EXPECT_GT(encounter.FindEnemy(id)->hitFlashSeconds, 0.0f);

    const auto last = StrikeUntilHit(encounter, id, hp);
    EXPECT_TRUE(last.killed);
    EXPECT_EQ(encounter.FindEnemy(id)->hitPoints, 0);
    EXPECT_TRUE(encounter.FindEnemy(id)->IsDead());
    EXPECT_EQ(encounter.LivingEnemyCount(), 0u);
    EXPECT_FALSE(encounter.StrikeEnemy(id, FixedHit(1), { 1.0f, 0.0f }, 1.0f).has_value());
}

TEST(EncounterTests, StrikeEnemy_DefensesApply) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.warrior, { 1.0f, 0.0f }, 0.0f, false);
    auto piercing = FixedHit(9);
    piercing.damageType = LB::Combat::DamageType::Piercing;

    std::optional<CombatEvent> hit;
    for (int i = 0; i < 20 && !(hit && hit->result.hit); ++i) hit = encounter.StrikeEnemy(id, piercing, { 0.0f, 0.0f }, 1.5f);

    ASSERT_TRUE(hit && hit->result.hit);
    // Skeleton warrior: physical armour first, then half of piercing resisted
    const auto expected = static_cast<std::int32_t>(std::lround((9 - content.warrior->physicalArmor) * 0.5f));
    EXPECT_EQ(hit->result.finalDamage, expected);
}

TEST(EncounterTests, StrikeEnemy_OutOfReach_Whiffs) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 5.0f, 0.0f }, 0.0f, false);

    EXPECT_FALSE(encounter.StrikeEnemy(id, FixedHit(3), { 0.0f, 0.0f }, 1.4f).has_value());
    EXPECT_FALSE(encounter.FindTarget({ 0.0f, 0.0f }, 1.4f).has_value());
}

TEST(EncounterTests, HitWhileAttacking_NoStagger) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.warrior, { 1.0f, 0.0f }, -std::numbers::pi_v<float> * 0.5f, false);
    for (int i = 0; i < 600 && encounter.FindEnemy(id)->state != EnemyState::Attacking; ++i) {
        encounter.Update(Step, PlayerAt(0.0f, 0.0f));
    }
    ASSERT_EQ(encounter.FindEnemy(id)->state, EnemyState::Attacking);

    StrikeUntilHit(encounter, id, 1);

    EXPECT_EQ(encounter.FindEnemy(id)->state, EnemyState::Attacking);
}

TEST(EncounterTests, Corpse_FadesThenDisappears) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f, false);
    StrikeUntilHit(encounter, id, 100);
    const auto player = PlayerAt(-5.0f, 0.0f);

    RunEncounter(encounter, player, Encounter::CorpseSeconds - Encounter::CorpseFadeSeconds * 0.5f);
    ASSERT_NE(encounter.FindEnemy(id), nullptr);
    EXPECT_NEAR(Encounter::Opacity(*encounter.FindEnemy(id)), 0.5f, 0.05f);

    RunEncounter(encounter, player, Encounter::CorpseFadeSeconds);
    EXPECT_EQ(encounter.FindEnemy(id), nullptr);
}

TEST(EncounterTests, WaveCleared_NextWaveAfterDelay) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.StartWave(1, { 0.0f, 0.0f });
    const auto player = PlayerAt(0.0f, 0.0f);
    RunEncounter(encounter, player, Encounter::SpawnSeconds + Step);

    for (const auto& enemy : std::vector<Enemy>(encounter.GetEnemies())) StrikeUntilHit(encounter, enemy.id, 100);
    ASSERT_EQ(encounter.LivingEnemyCount(), 0u);

    RunEncounter(encounter, player, Encounter::WaveDelaySeconds - 0.1f);
    EXPECT_EQ(encounter.GetWave(), 1);

    RunEncounter(encounter, player, 0.2f);
    EXPECT_EQ(encounter.GetWave(), 2);
    const auto [light, heavy] = Encounter::WaveComposition(2);
    EXPECT_EQ(encounter.LivingEnemyCount(), static_cast<std::size_t>(light + heavy));
}

TEST(EncounterTests, Bodies_DoNotOverlap) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    for (int i = 0; i < 3; ++i) encounter.Spawn(*content.minion, { 5.0f, 0.0f }, 0.0f, false);

    RunEncounter(encounter, PlayerAt(0.0f, 0.0f), 2.0f);

    const auto& enemies = encounter.GetEnemies();
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        EXPECT_GE(Encounter::Distance(enemies[i].position, { 0.0f, 0.0f }), Encounter::CharacterRadius * 2.0f - 1e-3f);
        for (std::size_t j = i + 1; j < enemies.size(); ++j) {
            EXPECT_GE(Encounter::Distance(enemies[i].position, enemies[j].position), Encounter::CharacterRadius * 2.0f - 0.05f);
        }
    }
}

TEST(EncounterTests, SameSeed_SameFight) {
    EncounterContent content;
    auto a = MakeEncounter(content);
    auto b = MakeEncounter(content);
    a.StartWave(3, { 0.0f, 0.0f });
    b.StartWave(3, { 0.0f, 0.0f });

    const auto ea = RunEncounter(a, PlayerAt(0.0f, 0.0f), 6.0f);
    const auto eb = RunEncounter(b, PlayerAt(0.0f, 0.0f), 6.0f);

    ASSERT_EQ(ea.size(), eb.size());
    for (std::size_t i = 0; i < ea.size(); ++i) {
        EXPECT_EQ(ea[i].enemyId, eb[i].enemyId);
        EXPECT_EQ(ea[i].result.finalDamage, eb[i].result.finalDamage);
    }
}

// ============================================================================
// GameEngine Combat Tests
// ============================================================================

namespace {
    void InitializeForCombat(GameEngine& engine, bool waves = false) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .spawnEnemies = waves, .randomSeed = CombatSeed }))
            << engine.GetInitializationError();
    }

    std::int32_t PlayerHitPoints(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetCurrentHitPoints();
    }

    std::int32_t PlayerMaxHitPoints(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetMaxHitPoints();
    }

    // Step until a condition holds (at most `seconds`); the events seen on the way
    template<typename Condition>
    std::vector<CombatEvent> StepUntil(GameEngine& engine, Condition done, float seconds) {
        std::vector<CombatEvent> events;
        for (int i = 0; i < static_cast<int>(seconds * 60.0f) && !done(); ++i) {
            engine.Tick(OneSixtieth);
            events.insert(events.end(), engine.GetCombatEvents().begin(), engine.GetCombatEvents().end());
        }
        return events;
    }

    std::size_t CountKind(const std::vector<CombatEvent>& events, CombatEventKind kind) {
        return static_cast<std::size_t>(std::ranges::count(events, kind, &CombatEvent::kind));
    }
}

TEST(GameEngineCombatTests, Initialize_FirstWaveRises) {
    GameEngine engine;
    InitializeForCombat(engine, true);

    EXPECT_EQ(engine.GetEncounter().GetWave(), 1);
    ASSERT_EQ(engine.GetEncounter().GetEnemies().size(), 2u);
    for (const auto& enemy : engine.GetEncounter().GetEnemies()) {
        EXPECT_EQ(enemy.state, EnemyState::Spawning);
        EXPECT_GE(Encounter::Distance(enemy.position, engine.GetPlayerPosition()), Encounter::MinimumSpawnDistance);
    }
}

TEST(GameEngineCombatTests, SpawnEnemiesOff_EmptyArena) {
    GameEngine engine;
    InitializeForCombat(engine);
    RunSteps(engine, 60);

    EXPECT_EQ(engine.GetEncounter().GetWave(), 0);
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());
    EXPECT_FALSE(engine.SpawnEnemy("enemy_unknown", { 1.0f, 0.0f }).has_value());
}

TEST(GameEngineCombatTests, NoContent_NoCombat) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600));
    TapKey(engine, Keys::Space);
    RunSteps(engine, 30);

    EXPECT_FALSE(engine.IsPlayerAlive());
    EXPECT_FALSE(engine.IsPlayerAttacking());
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());
}

TEST(GameEngineCombatTests, PlayerAttack_FromStartingWeaponAndClass) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto* sword = engine.GetContent()->FindWeapon(GameEngine::StartingWeapon);
    ASSERT_NE(sword, nullptr);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    const auto strength = player.GetAttributes().GetStrengthModifier();

    const auto attack = engine.GetPlayerAttackProfile();

    EXPECT_EQ(attack.attackBonus, *engine.GetCharacters().GetAttackBonus(*engine.GetPlayerId(), sword->category));
    EXPECT_EQ(attack.damageDice.sides, sword->damageDice.sides);
    EXPECT_EQ(attack.damageBonus, sword->damageBonus + strength);
    EXPECT_EQ(attack.damageType, LB::Combat::DamageType::Slashing);
    EXPECT_EQ(attack.critRangeMin, sword->critRangeMin);
    EXPECT_FLOAT_EQ(attack.critMultiplier, sword->critMultiplier);
}

TEST(GameEngineCombatTests, PlayerDefense_TenPlusDexterityAndArmour) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();

    EXPECT_EQ(engine.GetPlayerDefense().armorClass, 10 + player.GetAttributes().GetDexterityModifier());
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, 0);

    ASSERT_TRUE(engine.EquipPlayerArmor("armor_chest_rusted_breastplate_t1"));
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, engine.GetContent()->FindArmor("armor_chest_rusted_breastplate_t1")->physicalAR);
}

TEST(GameEngineCombatTests, Attack_TurnsTowardAndStrikesTheEnemyInReach) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });
    ASSERT_TRUE(id.has_value());

    TapKey(engine, Keys::Space);
    engine.Tick(OneSixtieth);
    EXPECT_TRUE(engine.IsPlayerAttacking());

    const auto events = StepUntil(engine, [&] { return !engine.IsPlayerAttacking(); }, 2.0f);
    ASSERT_EQ(CountKind(events, CombatEventKind::PlayerAttack), 1u);
    const auto strike = *std::ranges::find(events, CombatEventKind::PlayerAttack, &CombatEvent::kind);
    EXPECT_EQ(strike.enemyId, *id);

    // Turned east (+X) toward the enemy: a quarter turn from facing +Z
    EXPECT_NEAR(engine.GetPlayerFacingYaw(), std::numbers::pi_v<float> * 0.5f, 1e-4f);
    if (strike.result.hit) {
        EXPECT_LT(engine.GetEncounter().FindEnemy(*id)->hitPoints, engine.GetContent()->FindEnemy(GameEngine::LightEnemy)->hitPoints);
    }
}

TEST(GameEngineCombatTests, Swing_RootsThePlayerUntilTheStrike) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto* sword = engine.GetContent()->FindWeapon(GameEngine::StartingWeapon);

    engine.OnKeyDown(Keys::FromChar('D'));
    TapKey(engine, Keys::Space);
    const int windUpSteps = static_cast<int>(sword->attackTimeSeconds * Encounter::AttackImpactFraction * 60.0f) - 1;
    RunSteps(engine, windUpSteps);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);

    RunSteps(engine, 10);   // past the strike: walking again
    engine.OnKeyUp(Keys::FromChar('D'));
    EXPECT_NE(engine.GetPlayerPosition().x, 0.0f);
}

TEST(GameEngineCombatTests, NoEnemyInReach_SwingWhiffs) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 6.0f, 0.0f });

    TapKey(engine, Keys::Space);
    const auto events = StepUntil(engine, [&] { return false; }, 0.8f);

    EXPECT_EQ(CountKind(events, CombatEventKind::PlayerAttack), 0u);
}

TEST(GameEngineCombatTests, HoldingAttack_KillsAMinion) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    engine.OnKeyDown(Keys::Space);
    const auto events = StepUntil(engine, [&] { return engine.GetEncounter().FindEnemy(id)->IsDead(); }, 15.0f);
    engine.OnKeyUp(Keys::Space);

    EXPECT_TRUE(engine.GetEncounter().FindEnemy(id)->IsDead());
    EXPECT_GE(CountKind(events, CombatEventKind::PlayerAttack), 2u);
    EXPECT_TRUE(std::ranges::any_of(events, &CombatEvent::killed));
}

TEST(GameEngineCombatTests, EnemyHits_ReducePlayerHealth) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f });
    const auto before = PlayerHitPoints(engine);

    const auto events = StepUntil(engine, [&] { return PlayerHitPoints(engine) < before; }, 20.0f);

    std::int32_t dealt = 0;
    for (const auto& event : events) {
        if (event.kind == CombatEventKind::EnemyAttack && event.result.hit) dealt += event.result.finalDamage;
    }
    EXPECT_GT(dealt, 0);
    EXPECT_EQ(PlayerHitPoints(engine), before - dealt);
}

TEST(GameEngineCombatTests, PlayerDown_GetsBackUpWithFullHealth) {
    GameEngine engine;
    InitializeForCombat(engine);
    for (float y : { -1.0f, 1.0f }) {
        for (float x : { -1.0f, 1.0f }) engine.SpawnEnemy(GameEngine::HeavyEnemy, { x, y });
    }

    StepUntil(engine, [&] { return !engine.IsPlayerAlive(); }, 60.0f);
    ASSERT_FALSE(engine.IsPlayerAlive());

    // Down: no moving, no swinging
    engine.OnKeyDown(Keys::FromChar('D'));
    TapKey(engine, Keys::Space);
    RunSteps(engine, 30);
    engine.OnKeyUp(Keys::FromChar('D'));
    EXPECT_FALSE(engine.IsPlayerAttacking());

    RunSteps(engine, static_cast<int>(GameEngine::PlayerRespawnSeconds * 60.0f));
    EXPECT_TRUE(engine.IsPlayerAlive());
    EXPECT_EQ(PlayerHitPoints(engine), PlayerMaxHitPoints(engine));
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());   // waves off: the arena is cleared
}

TEST(GameEngineCombatTests, PlayerDown_WavesStartOver) {
    GameEngine engine;
    InitializeForCombat(engine, true);
    for (float y : { -1.0f, 1.0f }) {
        for (float x : { -1.0f, 1.0f }) engine.SpawnEnemy(GameEngine::HeavyEnemy, { x, y });
    }
    StepUntil(engine, [&] { return !engine.IsPlayerAlive(); }, 60.0f);
    ASSERT_FALSE(engine.IsPlayerAlive());

    RunSteps(engine, static_cast<int>(GameEngine::PlayerRespawnSeconds * 60.0f) + 1);

    EXPECT_EQ(engine.GetEncounter().GetWave(), 1);
    EXPECT_EQ(engine.GetEncounter().LivingEnemyCount(), 2u);
}

TEST(GameEngineCombatTests, Pause_FreezesEnemies) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 5.0f, 0.0f });
    RunSteps(engine, 10);
    TapKey(engine, Keys::Escape);
    RunSteps(engine, 1);
    const auto frozen = engine.GetEncounter().GetEnemies()[0].position;

    RunSteps(engine, 60);

    EXPECT_FLOAT_EQ(engine.GetEncounter().GetEnemies()[0].position.x, frozen.x);
    EXPECT_FLOAT_EQ(engine.GetEncounter().GetEnemies()[0].position.y, frozen.y);
}

TEST(GameEngineCombatTests, Hud_HealthBarsFollowHitPoints) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    const auto fills = [&] {
        std::vector<LB::Systems::Rendering::RectangleRenderData> bars;
        for (const auto& rect : engine.GetRendering().GetQueuedRectangles()) {
            if (rect.layerDepth == 12) bars.push_back(rect);
        }
        return bars;
    };

    engine.Tick(OneSixtieth);
    auto bars = fills();
    ASSERT_EQ(bars.size(), 1u);   // the player's; unhurt enemies show no bar
    EXPECT_EQ(bars[0].rect.width, 240);

    engine.OnKeyDown(Keys::Space);
    StepUntil(engine, [&] {
        const auto* enemy = engine.GetEncounter().FindEnemy(id);
        return enemy->hitPoints < enemy->definition->hitPoints;
    }, 10.0f);
    engine.OnKeyUp(Keys::Space);
    engine.Tick(OneSixtieth);

    bars = fills();
    const auto* enemy = engine.GetEncounter().FindEnemy(id);
    if (!enemy->IsDead()) {
        ASSERT_EQ(bars.size(), 2u);
        EXPECT_LT(bars[1].rect.width, 44);
    }
}

TEST(GameEngineCombatTests, PrimitiveEnemies_WithoutModels) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.Tick(OneSixtieth);
    const auto before = engine.GetRendering().GetQueuedMeshes().size();

    engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetRendering().GetQueuedMeshes().size(), before + 2);   // body and skull
}

TEST(GameEngineModelTests, Enemies_AnimatedAndHoldingTheirWeapons) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .spawnEnemies = false }))
        << engine.GetInitializationError();
    engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { -3.0f, 0.0f });

    RunSteps(engine, 30);

    const auto minions = DrawsOf(engine, "Skeleton_Minion");
    ASSERT_EQ(minions.size(), 1u);
    const auto walk = engine.GetRendering().FindAnimation("Walking_A");
    ASSERT_TRUE(walk.has_value());
    EXPECT_EQ(minions[0].animation.current.clip, *walk);   // closing in on the player
    EXPECT_FLOAT_EQ(minions[0].scale, engine.GetContent()->FindEnemy(GameEngine::LightEnemy)->modelScale);

    for (std::string_view weapon : { "Skeleton_Blade", "Skeleton_Axe", "Skeleton_Shield_Large_A" }) {
        const auto held = DrawsOf(engine, weapon);
        ASSERT_EQ(held.size(), 1u) << weapon;
        EXPECT_TRUE(held[0].transform.has_value()) << weapon;
    }
    EXPECT_EQ(DrawsOf(engine, "Skeleton_Warrior").size(), 1u);
}

TEST(GameEngineModelTests, Player_SwingsTheSword) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.Tick(OneSixtieth);
    const auto restingSword = DrawsOf(engine, "sword_common");
    ASSERT_EQ(restingSword.size(), 1u);
    ASSERT_TRUE(restingSword[0].transform.has_value());
    EXPECT_TRUE(DrawsOf(engine, "character_knight")[0].partPoses.size() == 1u);

    TapKey(engine, Keys::Space);
    RunSteps(engine, 15);

    const auto swinging = DrawsOf(engine, "sword_common");
    ASSERT_EQ(swinging.size(), 1u);
    const auto rest = restingSword[0].transform->Translation();
    const auto now = swinging[0].transform->Translation();
    EXPECT_GT(std::abs(rest.x - now.x) + std::abs(rest.y - now.y) + std::abs(rest.z - now.z), 0.05f);
}

TEST(GameEngineModelTests, Enemies_AttackWithTheirMeleeClips) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .spawnEnemies = false }))
        << engine.GetInitializationError();
    const auto& rendering = engine.GetRendering();
    for (const auto& type : engine.GetContent()->GetEnemies()) {
        ASSERT_TRUE(rendering.FindAnimation(type.attackAnimation).has_value()) << type.attackAnimation;
    }
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    for (int i = 0; i < 300 && engine.GetEncounter().FindEnemy(id)->state != EnemyState::Attacking; ++i) engine.Tick(OneSixtieth);
    ASSERT_EQ(engine.GetEncounter().FindEnemy(id)->state, EnemyState::Attacking);
    engine.Tick(OneSixtieth);

    const auto minions = DrawsOf(engine, "Skeleton_Minion");
    ASSERT_EQ(minions.size(), 1u);
    const auto* minion = engine.GetContent()->FindEnemy(GameEngine::LightEnemy);
    EXPECT_EQ(minions[0].animation.current.clip, *rendering.FindAnimation(minion->attackAnimation));
}

// ============================================================================
// GameEngine Combat Text Tests
// ============================================================================

TEST(GameEngineCombatTextTests, PlayerStrike_FloatsTheDamageOverTheEnemy) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    TapKey(engine, Keys::Space);
    const auto events = StepUntil(engine, [&] { return !engine.GetFloatingTexts().empty(); }, 2.0f);

    ASSERT_EQ(engine.GetFloatingTexts().size(), 1u);
    const auto& text = engine.GetFloatingTexts()[0];
    const auto strike = std::ranges::find(events, CombatEventKind::PlayerAttack, &CombatEvent::kind);
    ASSERT_NE(strike, events.end());
    if (!strike->result.hit) {
        EXPECT_EQ(text.text, "MISS");
    }
    else {
        EXPECT_EQ(text.text, std::format("{}{}", strike->result.finalDamage, strike->result.critical ? "!" : ""));
    }
    EXPECT_NEAR(text.position.z, engine.GetEncounter().FindEnemy(id)->position.y, 1e-4f);
    EXPECT_GT(text.position.y, 1.0f);   // over the head
}

TEST(GameEngineCombatTextTests, EnemyHits_FloatRedOverThePlayer) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f });
    const auto before = PlayerHitPoints(engine);

    StepUntil(engine, [&] { return PlayerHitPoints(engine) < before; }, 20.0f);

    const auto red = std::ranges::find_if(engine.GetFloatingTexts(), [](const FloatingText& t) { return t.text != "MISS" && t.color.g < 0.5f; });
    ASSERT_NE(red, engine.GetFloatingTexts().end());
    EXPECT_NEAR(red->position.z, engine.GetPlayerPosition().y, 1e-4f);
}

TEST(GameEngineCombatTextTests, Texts_FadeAwayAndFreezeWhilePaused) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });
    TapKey(engine, Keys::Space);
    StepUntil(engine, [&] { return !engine.GetFloatingTexts().empty(); }, 2.0f);
    ASSERT_FALSE(engine.GetFloatingTexts().empty());

    TapKey(engine, Keys::Escape);
    RunSteps(engine, 120);
    EXPECT_FALSE(engine.GetFloatingTexts().empty());   // paused: still there

    TapKey(engine, Keys::Escape);
    RunSteps(engine, static_cast<int>(GameEngine::FloatingTextSeconds * 60.0f) + 2);
    for (const auto& text : engine.GetFloatingTexts()) EXPECT_LT(text.ageSeconds, GameEngine::FloatingTextSeconds);
}

TEST(GameEngineCombatTextTests, Hud_LabelsHitPointsAndWave) {
    GameEngine engine;
    InitializeForCombat(engine, true);

    engine.Tick(OneSixtieth);

    const auto& texts = engine.GetRendering().GetQueuedText();
    const auto has = [&](std::string_view label) {
        return std::ranges::any_of(texts, [&](const auto& t) { return t.text == label; });
    };
    const auto max = PlayerMaxHitPoints(engine);
    EXPECT_TRUE(has(std::format("HP {}/{}", max, max)));
    EXPECT_TRUE(has("WAVE 1"));
}

TEST(GameEngineCombatTextTests, FontDirectory_LoadsTheUiFont) {
    const auto models = ShippedModelDirectory();
    const auto fonts = std::filesystem::path(std::u8string(models.begin(), models.end())).parent_path() / "Fonts";
    const auto text = fonts.u8string();
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .fontDirectory = std::string(text.begin(), text.end()), .spawnEnemies = false }))
        << engine.GetInitializationError();

    EXPECT_TRUE(engine.GetRendering().FindFont("MedievalSharp-Regular").has_value());
}

TEST(GameEngineCombatTextTests, MissingFont_StopsStartup) {
    GameEngine engine;
    const auto empty = std::filesystem::temp_directory_path() / "lb_no_fonts_here";
    std::filesystem::create_directories(empty);

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .fontDirectory = empty.string() }));
    EXPECT_NE(engine.GetInitializationError().find("MedievalSharp-Regular.ttf: file not found"), std::string::npos) << engine.GetInitializationError();
    std::filesystem::remove_all(empty);
}
