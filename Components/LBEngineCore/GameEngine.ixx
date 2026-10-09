// GameEngine.ixx - Game loop orchestration
export module LB.Engine:GameEngine;

import std;
import LB.AudioSystem;
import LB.InputSystem;
import LB.PersistenceSystem;
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
        static constexpr std::string_view QuickSaveAction = "QuickSave";
        static constexpr std::string_view QuickLoadAction = "QuickLoad";

        // Save slot used by quick save/load (F5/F9)
        static constexpr std::int32_t QuickSaveSlot = 0;

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
        /// saveDirectory (UTF-8) holds saves and preferences; empty keeps them in memory only.
        /// </summary>
        bool Initialize(void* nativeWindow, std::int32_t width, std::int32_t height, const std::string& saveDirectory = {}) {
            if (!renderingSystem_.Initialize(nativeWindow, width, height)) {
                return false;
            }

            // An unusable save directory should not stop the game; fall back to memory only
            if (!persistenceSystem_.Initialize(saveDirectory)) {
                persistenceSystem_.Initialize({});
            }

            audioSystem_.Initialize();
            if (nativeWindow != nullptr) {
                audioSystem_.EnableDeviceOutput();
            }
            ApplyPreferences(persistenceSystem_.LoadPreferences());

            inputSystem_.Initialize();
            RegisterDefaultBindings();

            // Gamepads are read from the platform only with a real window; headless runs
            // (tests, tools) stay deterministic and receive input through the On* methods
            if (nativeWindow != nullptr) {
                inputSystem_.EnableDeviceInput();
            }
            active_ = true;

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

            SaveVolumePreferences();

            inputSystem_.Shutdown();
            audioSystem_.Shutdown();
            persistenceSystem_.Shutdown();
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
            active_ = true;
        }

        void OnDeactivated() {
            // Key/button up events are not delivered while unfocused; avoid stuck input.
            // Gamepads are not polled until the window is active again.
            active_ = false;
            inputSystem_.ReleaseAll();
        }

        void OnSuspending() {
            audioSystem_.Suspend();
        }

        void OnResuming() {
            audioSystem_.Resume();

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

        /// <summary>
        /// Push a gamepad state from the host (platforms without a device backend, replays, tests)
        /// </summary>
        void OnGamepadState(std::int32_t gamepadIndex, const Systems::Input::GamepadState& state) {
            if (!initialized_) return;
            inputSystem_.SetGamepadState(gamepadIndex, state);
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
        [[nodiscard]] const Systems::Audio::AudioSystem& GetAudio() const noexcept { return audioSystem_; }
        [[nodiscard]] Systems::Audio::AudioSystem& GetAudio() noexcept { return audioSystem_; }
        [[nodiscard]] const Systems::Persistence::PersistenceSystem& GetPersistence() const noexcept { return persistenceSystem_; }

        /// <summary>
        /// True when gamepads are read from the platform (GameInput runtime available)
        /// </summary>
        [[nodiscard]] bool HasGamepadSupport() const noexcept { return inputSystem_.HasDeviceInput(); }

        /// <summary>
        /// Tactical pause (design-combat-system.md): the loop keeps running and reading input,
        /// but the simulation and the game clock stop.
        /// </summary>
        [[nodiscard]] bool IsPaused() const noexcept { return paused_; }

        /// <summary>
        /// Simulated time, excluding time spent paused
        /// </summary>
        [[nodiscard]] double GetGameTimeSeconds() const noexcept { return gameTimeSeconds_; }

        /// <summary>
        /// Save the game state to a slot
        /// </summary>
        bool SaveGame(std::int32_t slotIndex) {
            if (!initialized_) return false;

            Systems::Persistence::SaveSlotInfo metadata{};
            metadata.saveName = slotIndex == QuickSaveSlot ? "Quick Save" : std::format("Save {}", slotIndex);
            metadata.playtimeSeconds = static_cast<std::int32_t>(gameTimeSeconds_);

            Systems::Persistence::SaveData data;
            data["gameTimeSeconds"] = std::format("{}", gameTimeSeconds_);

            return persistenceSystem_.SaveGame(slotIndex, metadata, data).success;
        }

        /// <summary>
        /// Restore the game state from a slot. Returns false (state unchanged) if the slot is
        /// missing or unreadable.
        /// </summary>
        bool LoadGame(std::int32_t slotIndex) {
            if (!initialized_) return false;

            auto result = persistenceSystem_.LoadGame(slotIndex);
            if (!result.success) return false;

            double gameTime = 0.0;
            if (!ParseDouble(result.data, "gameTimeSeconds", gameTime)) return false;

            gameTimeSeconds_ = gameTime;
            return true;
        }

    private:
        /// <summary>
        /// Advance the simulation by one step.
        /// </summary>
        void Update(double elapsedSeconds) {
            if (active_) {
                inputSystem_.PollDevices();
            }

            if (inputSystem_.IsActionTriggered(std::string(PauseAction))) {
                SetPaused(!paused_);
            }

            if (inputSystem_.IsActionTriggered(std::string(QuickSaveAction))) {
                SaveGame(QuickSaveSlot);
            }
            if (inputSystem_.IsActionTriggered(std::string(QuickLoadAction))) {
                LoadGame(QuickSaveSlot);
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

        void SetPaused(bool paused) {
            paused_ = paused;

            // Gameplay sounds stop with the simulation; music keeps playing
            using Systems::Audio::AudioChannel;
            for (auto channel : { AudioChannel::SoundEffects, AudioChannel::Ambient, AudioChannel::Voice }) {
                if (paused_) audioSystem_.PauseChannel(channel);
                else audioSystem_.ResumeChannel(channel);
            }
        }

        void ApplyPreferences(const Systems::Persistence::PlayerPreferences& prefs) {
            using Systems::Audio::AudioChannel;
            audioSystem_.SetChannelVolume(AudioChannel::Master, prefs.masterVolume);
            audioSystem_.SetChannelVolume(AudioChannel::Music, prefs.musicVolume);
            audioSystem_.SetChannelVolume(AudioChannel::SoundEffects, prefs.sfxVolume);
        }

        void SaveVolumePreferences() {
            using Systems::Audio::AudioChannel;
            auto prefs = persistenceSystem_.LoadPreferences();
            prefs.masterVolume = audioSystem_.GetChannelVolume(AudioChannel::Master);
            prefs.musicVolume = audioSystem_.GetChannelVolume(AudioChannel::Music);
            prefs.sfxVolume = audioSystem_.GetChannelVolume(AudioChannel::SoundEffects);
            persistenceSystem_.SavePreferences(prefs);
        }

        static bool ParseDouble(const Systems::Persistence::SaveData& data, const std::string& key, double& value) {
            auto it = data.find(key);
            if (it == data.end()) return false;

            const auto& text = it->second;
            const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
            return ec == std::errc{} && end == text.data() + text.size();
        }

        void RegisterKeyBinding(std::string_view action, std::int32_t keyCode) {
            Systems::Input::InputAction binding{};
            binding.name = std::string(action);
            binding.deviceType = Systems::Input::InputDevice::Keyboard;
            binding.keyCode = keyCode;
            inputSystem_.RegisterAction(binding);
        }

        void RegisterDefaultBindings() {
            Systems::Input::InputAction pause{};
            pause.name = std::string(PauseAction);
            pause.deviceType = Systems::Input::InputDevice::Keyboard;
            pause.keyCode = Systems::Input::Keys::Escape;
            inputSystem_.RegisterAction(pause);

            Systems::Input::InputAction pauseGamepad{};
            pauseGamepad.name = std::string(PauseAction);
            pauseGamepad.deviceType = Systems::Input::InputDevice::Gamepad;
            pauseGamepad.gamepadButton = Systems::Input::GamepadButton::Start;
            inputSystem_.RegisterAction(pauseGamepad);

            RegisterKeyBinding(QuickSaveAction, Systems::Input::Keys::F5);
            RegisterKeyBinding(QuickLoadAction, Systems::Input::Keys::F9);
        }

        void OnDeviceLost() {
            // TODO: Release device-dependent game resources.
        }

        void OnDeviceRestored() {
            // TODO: Recreate device-dependent game resources.
        }

        Systems::Rendering::RenderingSystem renderingSystem_;
        Systems::Input::InputSystem inputSystem_;
        Systems::Audio::AudioSystem audioSystem_;
        Systems::Persistence::PersistenceSystem persistenceSystem_;
        StepTimer timer_;
        double gameTimeSeconds_{0.0};
        bool paused_{false};
        bool active_{true};
        bool initialized_{false};
    };

} // namespace LB::Engine
