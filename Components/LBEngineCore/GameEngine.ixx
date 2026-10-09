// GameEngine.ixx - Game loop orchestration
export module LB.Engine:GameEngine;

import std;
import LB.InputSystem;
import LB.RenderingSystem;
import :StepTimer;

export namespace LB::Engine {

    /// <summary>
    /// Main game engine: owns the frame timer and the systems, and runs Tick → Update → Render.
    /// Platform-agnostic: the host passes an opaque native window handle (HWND on Windows)
    /// and forwards window events. A null window runs the engine headless.
    /// </summary>
    class GameEngine {
    public:
        static constexpr std::int32_t DefaultWindowWidth = 800;
        static constexpr std::int32_t DefaultWindowHeight = 600;

        // Simulation runs at a fixed 60 Hz (see design-combat-system.md, combat model)
        static constexpr double DefaultTargetElapsedSeconds = 1.0 / 60.0;

        // Input action names registered by the engine (bindings from design-ui-ux-and-audio.md)
        static constexpr std::string_view PauseAction = "Pause";

        GameEngine() {
            timer_.SetFixedTimeStep(true);
            timer_.SetTargetElapsedSeconds(DefaultTargetElapsedSeconds);
        }

        ~GameEngine() {
            Shutdown();
        }

        GameEngine(const GameEngine&) = delete;
        GameEngine& operator=(const GameEngine&) = delete;

        /// <summary>
        /// Initialize the engine and its systems. Returns false if the graphics device could not be created.
        /// </summary>
        bool Initialize(void* nativeWindow, std::int32_t width, std::int32_t height) {
            if (!renderingSystem_.Initialize(nativeWindow, width, height)) {
                return false;
            }

            inputSystem_.Initialize();
            RegisterDefaultBindings();

            renderingSystem_.SetDeviceCallbacks(
                [this] { OnDeviceLost(); },
                [this] { OnDeviceRestored(); });

            timer_.ResetElapsedTime();
            initialized_ = true;

            return true;
        }

        /// <summary>
        /// Shut down all systems. Safe to call more than once.
        /// </summary>
        void Shutdown() {
            if (!initialized_) return;

            inputSystem_.Shutdown();
            renderingSystem_.Shutdown();
            paused_ = false;
            initialized_ = false;
        }

        /// <summary>
        /// Run one iteration of the game loop using real elapsed time. Called by the host's message loop.
        /// </summary>
        void Tick() {
            if (!initialized_) return;

            timer_.Tick([this] { Update(timer_.GetElapsedSeconds()); });
            Render();
        }

        /// <summary>
        /// Run one iteration of the game loop advancing by an explicit amount of real time.
        /// </summary>
        void Tick(StepTimer::Clock::duration elapsed) {
            if (!initialized_) return;

            timer_.Advance(elapsed, [this] { Update(timer_.GetElapsedSeconds()); });
            Render();
        }

        // Window events (forwarded by the host)
        void OnActivated() {
            // TODO: Game is becoming active window.
        }

        void OnDeactivated() {
            // Key/button up events are not delivered while unfocused; avoid stuck input
            inputSystem_.ReleaseAll();
        }

        void OnSuspending() {
            // TODO: Game is being power-suspended (or minimized).
        }

        void OnResuming() {
            // Avoid a burst of catch-up updates for the time spent suspended
            timer_.ResetElapsedTime();
        }

        void OnWindowMoved() {
            renderingSystem_.OnWindowMoved();
        }

        void OnDisplayChange() {
            renderingSystem_.OnDisplayChange();
        }

        void OnWindowSizeChanged(std::int32_t width, std::int32_t height) {
            renderingSystem_.OnWindowSizeChanged(width, height);
        }

        // Input events (forwarded by the host). Key codes are LB::Systems::Input::Keys values.
        void OnKeyDown(std::int32_t keyCode) {
            if (!initialized_) return;
            inputSystem_.SetKeyState(keyCode, Systems::Input::KeyState::Pressed);
        }

        void OnKeyUp(std::int32_t keyCode) {
            if (!initialized_) return;
            inputSystem_.SetKeyState(keyCode, Systems::Input::KeyState::Released);
        }

        void OnMouseMove(std::int32_t x, std::int32_t y) {
            if (!initialized_) return;
            inputSystem_.SetMousePosition(x, y);
        }

        void OnMouseButton(Systems::Input::MouseButton button, bool down) {
            if (!initialized_) return;
            inputSystem_.SetMouseButton(button, down);
        }

        void OnMouseWheel(std::int32_t delta) {
            if (!initialized_) return;
            inputSystem_.AddMouseWheel(delta);
        }

        // Configuration
        void GetDefaultWindowSize(std::int32_t& width, std::int32_t& height) const noexcept {
            width = DefaultWindowWidth;
            height = DefaultWindowHeight;
        }

        void SetFixedTimeStep(bool enabled, double targetElapsedSeconds = DefaultTargetElapsedSeconds) {
            timer_.SetFixedTimeStep(enabled);
            timer_.SetTargetElapsedSeconds(targetElapsedSeconds);
        }

        // Properties
        [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }
        [[nodiscard]] bool HasGpuDevice() const noexcept { return renderingSystem_.HasGpuDevice(); }
        [[nodiscard]] const StepTimer& GetTimer() const noexcept { return timer_; }
        [[nodiscard]] const Systems::Input::InputSystem& GetInput() const noexcept { return inputSystem_; }

        /// <summary>
        /// Tactical pause (design-combat-system.md): the loop keeps running and reading input,
        /// but the simulation and the game clock stop.
        /// </summary>
        [[nodiscard]] bool IsPaused() const noexcept { return paused_; }

        /// <summary>
        /// Simulated time, excluding time spent paused
        /// </summary>
        [[nodiscard]] double GetGameTimeSeconds() const noexcept { return gameTimeSeconds_; }

    private:
        /// <summary>
        /// Advance the simulation by one step.
        /// </summary>
        void Update(double elapsedSeconds) {
            if (inputSystem_.IsActionTriggered(std::string(PauseAction))) {
                paused_ = !paused_;
            }

            if (!paused_) {
                gameTimeSeconds_ += elapsedSeconds;
                // TODO: Add game logic here (services, AI).
            }

            // This step has consumed the input frame; presses are seen by exactly one step
            inputSystem_.Update();
        }

        /// <summary>
        /// Draw the current frame.
        /// </summary>
        void Render() {
            // Don't try to render anything before the first Update.
            if (timer_.GetFrameCount() == 0) {
                return;
            }

            renderingSystem_.BeginFrame();

            // Placeholder pause feedback until there is a pause menu: dim the background
            auto clearColor = Systems::Rendering::Color::CornflowerBlue();
            if (paused_) {
                clearColor = Systems::Rendering::Color{ clearColor.r * 0.4f, clearColor.g * 0.4f, clearColor.b * 0.4f, clearColor.a };
            }
            renderingSystem_.Clear(clearColor);

            // TODO: Queue world, characters, and UI draws here.

            renderingSystem_.EndFrame();
        }

        void RegisterDefaultBindings() {
            Systems::Input::InputAction pause{};
            pause.name = std::string(PauseAction);
            pause.deviceType = Systems::Input::InputDevice::Keyboard;
            pause.keyCode = Systems::Input::Keys::Escape;
            inputSystem_.RegisterAction(pause);
        }

        void OnDeviceLost() {
            // TODO: Release device-dependent game resources.
        }

        void OnDeviceRestored() {
            // TODO: Recreate device-dependent game resources.
        }

        Systems::Rendering::RenderingSystem renderingSystem_;
        Systems::Input::InputSystem inputSystem_;
        StepTimer timer_;
        double gameTimeSeconds_{0.0};
        bool paused_{false};
        bool initialized_{false};
    };

} // namespace LB::Engine
