// GameEngine.ixx - Game loop orchestration
export module LB.Engine:GameEngine;

import std;
import LB.Core;
import LB.Character;
import LB.CharacterService;
import LB.AudioSystem;
import LB.InputSystem;
import LB.PersistenceSystem;
import LB.RenderingSystem;
import :StepTimer;

export namespace LB::Engine {

    /// <summary>
    /// Continuous position in world tiles (tile (0, 0) covers [0, 1) x [0, 1))
    /// </summary>
    struct WorldPoint {
        float x{0.0f};
        float y{0.0f};
    };

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

        // Placeholder test arena until dungeon generation is wired in: centered on the origin
        static constexpr float ArenaHalfWidth = 10.0f;
        static constexpr float ArenaHalfHeight = 7.0f;
        static constexpr float PlayerSizeTiles = 0.75f;
        static constexpr std::int32_t TileSizePixels = 32;

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

            if (!SpawnPlayer()) {
                return false;
            }

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
            characterService_.ClearAll();
            playerId_.reset();
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
        [[nodiscard]] const Services::Character::CharacterService& GetCharacters() const noexcept { return characterService_; }
        [[nodiscard]] const Systems::Rendering::RenderingSystem& GetRendering() const noexcept { return renderingSystem_; }

        /// <summary>
        /// The player character, once the engine is initialized
        /// </summary>
        [[nodiscard]] std::optional<LB::Character::CharacterId> GetPlayerId() const noexcept { return playerId_; }

        /// <summary>
        /// Player position in world tiles
        /// </summary>
        [[nodiscard]] WorldPoint GetPlayerPosition() const noexcept { return playerPosition_; }

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
            data["player.x"] = std::format("{}", playerPosition_.x);
            data["player.y"] = std::format("{}", playerPosition_.y);

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
            double playerX = 0.0;
            double playerY = 0.0;
            if (!ParseDouble(result.data, "gameTimeSeconds", gameTime) ||
                !ParseDouble(result.data, "player.x", playerX) ||
                !ParseDouble(result.data, "player.y", playerY)) {
                return false;
            }

            gameTimeSeconds_ = gameTime;
            playerPosition_ = ClampToArena(WorldPoint{ static_cast<float>(playerX), static_cast<float>(playerY) });
            characterService_.PlaceCharacter(*playerId_, ToTile(playerPosition_));
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
                MovePlayer(static_cast<float>(elapsedSeconds));
                // TODO: Add game logic here (combat, AI).
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

            // Placeholder pause feedback until there is a pause menu: dim the whole frame
            const float dim = paused_ ? 0.4f : 1.0f;
            const auto shade = [dim](float r, float g, float b) {
                return Systems::Rendering::Color{ r * dim, g * dim, b * dim, 1.0f };
            };

            renderingSystem_.Clear(shade(0.392156899f, 0.584313750f, 0.929411829f));  // CornflowerBlue
            DrawArena(shade);
            DrawPlayer(shade);

            renderingSystem_.EndFrame();
        }

        bool SpawnPlayer() {
            // Warrior starting stats from game-design.md (Character Classes)
            Services::Character::CharacterCreationRequest request(
                "Hero", LB::Character::CharacterClass::Warrior, LB::Attributes{ 16, 12, 14, 8, 10, 10 });

            playerId_ = characterService_.CreateCharacter(request);
            if (!playerId_) return false;

            playerPosition_ = WorldPoint{};
            characterService_.PlaceCharacter(*playerId_, ToTile(playerPosition_));
            return true;
        }

        /// <summary>
        /// Real-time movement for one simulation step: WASD/arrow keys (screen directions,
        /// diagonals normalized) or the left stick (analog). Speed comes from CharacterService.
        /// </summary>
        void MovePlayer(float elapsedSeconds) {
            if (!playerId_) return;

            namespace Keys = Systems::Input::Keys;
            const auto held = [this](std::int32_t a, std::int32_t b) {
                return inputSystem_.IsKeyDown(a) || inputSystem_.IsKeyDown(b);
            };

            float dx = 0.0f;
            float dy = 0.0f;
            if (held(Keys::FromChar('D'), Keys::Right)) dx += 1.0f;
            if (held(Keys::FromChar('A'), Keys::Left)) dx -= 1.0f;
            if (held(Keys::FromChar('S'), Keys::Down)) dy += 1.0f;
            if (held(Keys::FromChar('W'), Keys::Up)) dy -= 1.0f;

            if (dx != 0.0f || dy != 0.0f) {
                const float length = std::sqrt(dx * dx + dy * dy);
                dx /= length;
                dy /= length;
            }
            else {
                // Stick up is +Y; screen up is -Y. Dead zones are applied by the input backend.
                const auto& pad = inputSystem_.GetGamepadState(0);
                dx = pad.leftThumbX;
                dy = -pad.leftThumbY;
                const float magnitude = std::sqrt(dx * dx + dy * dy);
                if (magnitude > 1.0f) {
                    dx /= magnitude;
                    dy /= magnitude;
                }
            }

            if (dx == 0.0f && dy == 0.0f) return;

            const float speed = characterService_.GetMovementSpeed(*playerId_).value_or(0.0f);
            playerPosition_ = ClampToArena(WorldPoint{
                playerPosition_.x + dx * speed * elapsedSeconds,
                playerPosition_.y + dy * speed * elapsedSeconds });

            // Keep the domain's tile position in sync (at most one tile per step at these speeds)
            const auto tile = ToTile(playerPosition_);
            if (!characterService_.MoveCharacter(*playerId_, tile)) {
                characterService_.PlaceCharacter(*playerId_, tile);
            }
        }

        static WorldPoint ClampToArena(WorldPoint point) noexcept {
            constexpr float halfPlayer = PlayerSizeTiles * 0.5f;
            point.x = std::clamp(point.x, -ArenaHalfWidth + halfPlayer, ArenaHalfWidth - halfPlayer);
            point.y = std::clamp(point.y, -ArenaHalfHeight + halfPlayer, ArenaHalfHeight - halfPlayer);
            return point;
        }

        static LB::Position ToTile(WorldPoint point) noexcept {
            return LB::Position{ static_cast<std::int32_t>(std::floor(point.x)), static_cast<std::int32_t>(std::floor(point.y)) };
        }

        // World tiles -> screen pixels; the arena is centered in the window (placeholder top-down
        // view until the isometric renderer exists)
        Systems::Rendering::Rectangle ToScreen(float x, float y, float width, float height) const {
            std::int32_t screenWidth = 0;
            std::int32_t screenHeight = 0;
            renderingSystem_.GetScreenSize(screenWidth, screenHeight);

            const float originX = static_cast<float>(screenWidth) * 0.5f;
            const float originY = static_cast<float>(screenHeight) * 0.5f;
            const auto px = [](float value) { return static_cast<std::int32_t>(std::lround(value)); };

            const std::int32_t left = px(originX + x * TileSizePixels);
            const std::int32_t top = px(originY + y * TileSizePixels);
            const std::int32_t right = px(originX + (x + width) * TileSizePixels);
            const std::int32_t bottom = px(originY + (y + height) * TileSizePixels);
            return Systems::Rendering::Rectangle{ left, top, right - left, bottom - top };
        }

        template<typename Shade>
        void DrawArena(const Shade& shade) {
            const auto tilesX = static_cast<std::int32_t>(ArenaHalfWidth);
            const auto tilesY = static_cast<std::int32_t>(ArenaHalfHeight);
            for (std::int32_t ty = -tilesY; ty < tilesY; ++ty) {
                for (std::int32_t tx = -tilesX; tx < tilesX; ++tx) {
                    const bool light = ((tx + ty) & 1) == 0;
                    const auto color = light ? shade(0.30f, 0.29f, 0.27f) : shade(0.24f, 0.23f, 0.22f);
                    renderingSystem_.DrawRectangle(ToScreen(static_cast<float>(tx), static_cast<float>(ty), 1.0f, 1.0f), color, 0);
                }
            }
        }

        template<typename Shade>
        void DrawPlayer(const Shade& shade) {
            if (!playerId_) return;

            constexpr float half = PlayerSizeTiles * 0.5f;
            renderingSystem_.DrawRectangle(
                ToScreen(playerPosition_.x - half, playerPosition_.y - half, PlayerSizeTiles, PlayerSizeTiles),
                shade(0.95f, 0.75f, 0.20f), 10);
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
        Services::Character::CharacterService characterService_;
        std::optional<LB::Character::CharacterId> playerId_;
        WorldPoint playerPosition_{};
        StepTimer timer_;
        double gameTimeSeconds_{0.0};
        bool paused_{false};
        bool active_{true};
        bool initialized_{false};
    };

} // namespace LB::Engine
