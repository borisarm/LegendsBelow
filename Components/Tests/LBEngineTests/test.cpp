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
