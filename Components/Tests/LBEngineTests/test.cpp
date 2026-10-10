#include "pch.h"

import LB.Engine;
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

    // The player is created from class data, so movement tests load the shipped content
    void InitializeWithContent(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory() }))
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
    for (const char* file : { "spells.json", "weapons.json", "armour.json", "classes.json" }) {
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
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = dir.Utf8() })) << engine.GetInitializationError();
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
