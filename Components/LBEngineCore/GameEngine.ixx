// GameEngine.ixx - Game loop orchestration
export module LB.Engine:GameEngine;

import std;
import LB.Core;
import LB.Character;
import LB.CharacterService;
import LB.Content;
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
    /// Startup configuration supplied by the host. Paths are UTF-8; empty means "not used".
    /// </summary>
    struct EngineOptions {
        // Saves and preferences; empty keeps them in memory only
        std::string saveDirectory;
        // Game content (spells.json, weapons.json, armour.json); empty starts without content
        std::string contentDirectory;
        // 3D models (KayKit glTF files); empty draws the world with placeholder primitives
        std::string modelDirectory;
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
        static constexpr std::string_view RotateCameraLeftAction = "RotateCameraLeft";
        static constexpr std::string_view RotateCameraRightAction = "RotateCameraRight";

        // Right stick flicks past this deflection rotate (X) or zoom (Y) the camera
        static constexpr float CameraStickThreshold = 0.6f;

        // Save slot used by quick save/load (F5/F9)
        static constexpr std::int32_t QuickSaveSlot = 0;

        // Placeholder test arena until dungeon generation is wired in: centered on the origin
        static constexpr float ArenaHalfWidth = 10.0f;
        static constexpr float ArenaHalfHeight = 7.0f;
        static constexpr float PlayerSizeTiles = 0.75f;
        static constexpr float WallHeight = 1.2f;

        // KayKit models are built on a 2-unit grid (small floor tile 2x2, wall segment 4 wide):
        // one uniform scale maps them onto 1-unit tiles
        static constexpr float ModelScale = 0.5f;
        static constexpr std::string_view FloorModel = "tileBrickB_small.glb";
        static constexpr std::string_view FloorVariantModel = "tileBrickA_small.glb";
        static constexpr std::string_view WallModel = "wall.glb";
        static constexpr std::string_view CornerModel = "pillar.glb";
        static constexpr std::string_view PlayerModel = "character_knight.gltf";

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
        /// Initialize the engine and its systems. Returns false if the graphics device could not
        /// be created or the game content is invalid; GetInitializationError() explains why.
        /// </summary>
        bool Initialize(void* nativeWindow, std::int32_t width, std::int32_t height, const EngineOptions& options = {}) {
            initializationError_.clear();

            // Content first: invalid data is a fatal authoring error (fail fast, report everything)
            if (!LoadContent(options.contentDirectory)) {
                return false;
            }

            if (!renderingSystem_.Initialize(nativeWindow, width, height)) {
                initializationError_ = "Failed to initialize Direct3D 12. A GPU supporting Shader Model 6.0 is required.";
                return false;
            }

            if (!LoadModels(options.modelDirectory)) {
                renderingSystem_.Shutdown();
                return false;
            }

            // An unusable save directory should not stop the game; fall back to memory only
            if (!persistenceSystem_.Initialize(options.saveDirectory)) {
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
            models_ = {};
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
        /// Loaded game content (never null; empty when started without a content directory).
        /// Shared and immutable, so services and repositories can hold on to it.
        /// </summary>
        [[nodiscard]] std::shared_ptr<const Content::ContentDatabase> GetContent() const noexcept { return content_; }

        /// <summary>
        /// Why the last Initialize failed (empty if it succeeded)
        /// </summary>
        [[nodiscard]] const std::string& GetInitializationError() const noexcept { return initializationError_; }

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
        /// Put an armour piece from armour.json on the player (replacing the piece in its slot).
        /// Returns false without a player or for an unknown key.
        /// </summary>
        bool EquipPlayerArmor(std::string_view armorKey) {
            const auto* armor = content_->FindArmor(armorKey);
            if (!playerId_ || !armor) return false;

            Services::Character::ArmorPiece piece{};
            piece.armorId = armor->id;
            piece.slot = armor->slot;
            piece.category = armor->category;
            piece.movementPenalty = armor->movementPenalty;
            piece.castPenalty = armor->castPenalty;
            return characterService_.EquipArmor(*playerId_, piece);
        }

        /// <summary>
        /// The isometric camera following the player
        /// </summary>
        [[nodiscard]] const Systems::Rendering::IsometricCamera& GetCamera() const noexcept {
            return camera_;
        }

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
            if (playerId_) {
                data["player.x"] = std::format("{}", playerPosition_.x);
                data["player.y"] = std::format("{}", playerPosition_.y);
            }

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
            if (!ParseDouble(result.data, "gameTimeSeconds", gameTime)) {
                return false;
            }

            // The player's position is restored when both the game and the save have a player
            double playerX = 0.0;
            double playerY = 0.0;
            const bool hasPlayerPosition = ParseDouble(result.data, "player.x", playerX) && ParseDouble(result.data, "player.y", playerY);

            gameTimeSeconds_ = gameTime;
            if (playerId_ && hasPlayerPosition) {
                playerPosition_ = ClampToArena(WorldPoint{ static_cast<float>(playerX), static_cast<float>(playerY) });
                characterService_.PlaceCharacter(*playerId_, ToTile(playerPosition_));
                camera_.SnapTo({ playerPosition_.x, playerPosition_.y });  // instant refocus
            }
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

            // The camera can be turned and zoomed during the tactical pause
            UpdateCameraControls();

            if (!paused_) {
                gameTimeSeconds_ += elapsedSeconds;
                MovePlayer(static_cast<float>(elapsedSeconds));
                // TODO: Add game logic here (combat, AI).
            }

            if (playerId_) {
                camera_.Follow({ playerPosition_.x, playerPosition_.y });
            }
            camera_.Update(static_cast<float>(elapsedSeconds));

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

            // Placeholder pause feedback until there is a pause menu: dim the whole frame.
            // Colors are picked as display (sRGB) values and handed to the renderer as linear.
            const float dim = paused_ ? 0.4f : 1.0f;
            const auto shade = [dim](float r, float g, float b) {
                const auto linear = [dim](float c) { return std::pow(c * dim, 2.2f); };
                return Systems::Rendering::Color{ linear(r), linear(g), linear(b), 1.0f };
            };

            // Dim ambient background (design-technical-directx12.md 2.2), then the 3D world
            renderingSystem_.Clear(shade(0.05f, 0.05f, 0.07f));
            renderingSystem_.SetCamera(camera_.GetView());
            DrawArena(shade);
            DrawPlayer(shade);

            renderingSystem_.EndFrame();
        }

        /// <summary>
        /// Load the arena and player models. Without a model directory the world is drawn with
        /// primitives; a missing or invalid model stops startup with every problem listed.
        /// </summary>
        bool LoadModels(const std::string& modelDirectory) {
            models_ = {};
            if (modelDirectory.empty()) return true;

            const auto directory = std::filesystem::path(std::u8string(modelDirectory.begin(), modelDirectory.end()));
            std::vector<std::string> errors;
            const auto load = [&](std::string_view file) {
                const auto result = renderingSystem_.LoadModel(directory / file);
                if (!result.Succeeded()) errors.push_back(result.error);
                return result.id;
            };

            models_.floor = load(FloorModel);
            models_.floorVariant = load(FloorVariantModel);
            models_.wall = load(WallModel);
            models_.corner = load(CornerModel);
            models_.player = load(PlayerModel);

            if (!errors.empty()) {
                initializationError_ = std::format("3D models in '{}' could not be loaded ({} problems):", modelDirectory, errors.size());
                for (const auto& error : errors) {
                    initializationError_ += "\n" + error;
                }
                models_ = {};
                return false;
            }
            models_.loaded = true;
            return true;
        }

        bool LoadContent(const std::string& contentDirectory) {
            auto content = std::make_shared<Content::ContentDatabase>();
            if (!contentDirectory.empty()) {
                const auto result = content->LoadFromDirectory(contentDirectory);
                if (!result.Succeeded()) {
                    initializationError_ = std::format("Game content in '{}' is invalid ({} problems):", contentDirectory, result.errors.size());
                    for (const auto& error : result.errors) {
                        initializationError_ += "\n" + error;
                    }
                    return false;
                }
            }
            content_ = std::move(content);
            return true;
        }

        // Class rules (progression, speed, weapon adjustments) come from classes.json
        void ConfigureClassRules() {
            for (const auto& definition : content_->GetClasses()) {
                Services::Character::ClassRules rules{};
                rules.baseMovementSpeed = definition.baseMovementSpeed;
                rules.progression = definition.progression;
                rules.weaponAdjustments = definition.weaponAdjustments;
                rules.primaryAttributes = definition.primaryAttributes;
                rules.attributeGrowth = definition.attributeGrowth;
                rules.armorAdjustments = definition.armorAdjustments;
                characterService_.SetClassRules(definition.characterClass, std::move(rules));
            }
        }

        /// <summary>
        /// Create the player from class data. Without class data (no content) there is no player.
        /// </summary>
        bool SpawnPlayer() {
            ConfigureClassRules();

            constexpr auto playerClass = LB::Character::CharacterClass::Warrior;
            const auto* classData = content_->FindClass(playerClass);
            if (!classData) {
                return true;
            }

            Services::Character::CharacterCreationRequest request("Hero", playerClass, classData->startingAttributes);

            playerId_ = characterService_.CreateCharacter(request);
            if (!playerId_) return false;

            playerPosition_ = WorldPoint{};
            characterService_.PlaceCharacter(*playerId_, ToTile(playerPosition_));
            camera_.SnapTo({ playerPosition_.x, playerPosition_.y });
            return true;
        }

        /// <summary>
        /// Q/E and right stick flicks rotate the camera; the mouse wheel and right stick up/down zoom
        /// </summary>
        void UpdateCameraControls() {
            if (inputSystem_.IsActionTriggered(std::string(RotateCameraLeftAction))) camera_.RotateCounterClockwise();
            if (inputSystem_.IsActionTriggered(std::string(RotateCameraRightAction))) camera_.RotateClockwise();

            const auto wheel = inputSystem_.GetMouseState().wheelDelta;
            if (wheel > 0) camera_.ZoomIn();
            if (wheel < 0) camera_.ZoomOut();

            // Flicks act once when the stick crosses the threshold
            const auto& pad = inputSystem_.GetGamepadState(0);
            const auto flick = [](float now, float before) {
                if (now >= CameraStickThreshold && before < CameraStickThreshold) return 1;
                if (now <= -CameraStickThreshold && before > -CameraStickThreshold) return -1;
                return 0;
            };
            const int turn = flick(pad.rightThumbX, previousRightStick_.x);
            const int zoom = flick(pad.rightThumbY, previousRightStick_.y);
            if (turn > 0) camera_.RotateClockwise();
            if (turn < 0) camera_.RotateCounterClockwise();
            if (zoom > 0) camera_.ZoomIn();
            if (zoom < 0) camera_.ZoomOut();
            previousRightStick_ = WorldPoint{ pad.rightThumbX, pad.rightThumbY };
        }

        /// <summary>
        /// Real-time movement for one simulation step: WASD/arrow keys (screen directions,
        /// diagonals normalized) or the left stick (analog), turned to the ground through the
        /// camera so "up" always walks away from it. Speed comes from CharacterService.
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

            // dy is screen-down; the camera maps (right, up) onto the ground
            const auto direction = camera_.ScreenToGround(dx, -dy);
            playerFacingYaw_ = std::atan2(direction.x, direction.y);

            const float speed = characterService_.GetMovementSpeed(*playerId_).value_or(0.0f);
            playerPosition_ = ClampToArena(WorldPoint{
                playerPosition_.x + direction.x * speed * elapsedSeconds,
                playerPosition_.y + direction.y * speed * elapsedSeconds });

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

        // Placeholder 3D arena: one floor slab per tile (top at height 0) and a ring of wall blocks
        // around it. Tile (tx, ty) covers [tx, tx + 1) x [ty, ty + 1); ground y is world Z.
        template<typename Shade>
        void DrawArena(const Shade& shade) {
            if (models_.loaded) {
                DrawArenaModels(shade);
                return;
            }

            using Systems::Rendering::MeshRenderData;
            using Systems::Rendering::MeshShape;

            constexpr float FloorThickness = 0.2f;
            const auto tilesX = static_cast<std::int32_t>(ArenaHalfWidth);
            const auto tilesY = static_cast<std::int32_t>(ArenaHalfHeight);
            for (std::int32_t ty = -tilesY - 1; ty <= tilesY; ++ty) {
                for (std::int32_t tx = -tilesX - 1; tx <= tilesX; ++tx) {
                    const bool wall = tx < -tilesX || tx >= tilesX || ty < -tilesY || ty >= tilesY;
                    const float centerX = static_cast<float>(tx) + 0.5f;
                    const float centerZ = static_cast<float>(ty) + 0.5f;

                    MeshRenderData mesh{};
                    mesh.shape = MeshShape::Box;
                    if (wall) {
                        mesh.position = { centerX, WallHeight * 0.5f - FloorThickness * 0.5f, centerZ };
                        mesh.size = { 1.0f, WallHeight + FloorThickness, 1.0f };
                        mesh.color = shade(0.42f, 0.40f, 0.38f);
                    }
                    else {
                        const bool light = ((tx + ty) & 1) == 0;
                        mesh.position = { centerX, -FloorThickness * 0.5f, centerZ };
                        mesh.size = { 0.96f, FloorThickness, 0.96f };  // thin seams between flagstones
                        mesh.color = light ? shade(0.30f, 0.29f, 0.27f) : shade(0.24f, 0.23f, 0.22f);
                    }
                    renderingSystem_.DrawMesh(mesh);
                }
            }
        }

        // Arena from KayKit pieces: a floor tile per tile (top at height 0), wall segments two tiles
        // long whose inner face is on the arena edge, and pillars filling the corners
        template<typename Shade>
        void DrawArenaModels(const Shade& shade) {
            using Systems::Rendering::ModelRenderData;

            const auto tint = shade(1.0f, 1.0f, 1.0f);
            const auto place = [&](Systems::Rendering::ModelId model, float x, float y, float z, float yaw) {
                renderingSystem_.DrawModel(ModelRenderData{ model, { x, y, z }, yaw, ModelScale, tint });
            };
            const auto bottomOf = [&](Systems::Rendering::ModelId model) {
                return renderingSystem_.GetModel(model)->boundsMin.y * ModelScale;
            };
            const auto topOf = [&](Systems::Rendering::ModelId model) {
                return renderingSystem_.GetModel(model)->boundsMax.y * ModelScale;
            };

            const auto tilesX = static_cast<std::int32_t>(ArenaHalfWidth);
            const auto tilesY = static_cast<std::int32_t>(ArenaHalfHeight);
            for (std::int32_t ty = -tilesY; ty < tilesY; ++ty) {
                for (std::int32_t tx = -tilesX; tx < tilesX; ++tx) {
                    // A fixed scatter of the decorated variant breaks up the grid
                    const bool variant = ((tx * 7 + ty * 13) % 11 + 11) % 11 == 0;
                    const auto model = variant ? models_.floorVariant : models_.floor;
                    place(model, static_cast<float>(tx) + 0.5f, -topOf(model), static_cast<float>(ty) + 0.5f, 0.0f);
                }
            }

            const auto* wall = renderingSystem_.GetModel(models_.wall);
            const float segment = (wall->boundsMax.x - wall->boundsMin.x) * ModelScale;     // 2 tiles
            const float halfDepth = (wall->boundsMax.z - wall->boundsMin.z) * ModelScale * 0.5f;
            const float wallY = -bottomOf(models_.wall);
            constexpr float quarterTurn = std::numbers::pi_v<float> * 0.5f;
            for (float x = -ArenaHalfWidth + segment * 0.5f; x < ArenaHalfWidth; x += segment) {
                place(models_.wall, x, wallY, -ArenaHalfHeight - halfDepth, 0.0f);
                place(models_.wall, x, wallY, ArenaHalfHeight + halfDepth, 0.0f);
            }
            for (float z = -ArenaHalfHeight + segment * 0.5f; z < ArenaHalfHeight; z += segment) {
                place(models_.wall, -ArenaHalfWidth - halfDepth, wallY, z, quarterTurn);
                place(models_.wall, ArenaHalfWidth + halfDepth, wallY, z, quarterTurn);
            }

            const float cornerY = -bottomOf(models_.corner);
            for (float sx : { -1.0f, 1.0f }) {
                for (float sz : { -1.0f, 1.0f }) {
                    place(models_.corner, sx * (ArenaHalfWidth + halfDepth), cornerY, sz * (ArenaHalfHeight + halfDepth), 0.0f);
                }
            }
        }

        // Placeholder hero: a body, a head, and a visor showing where they face (or the knight model)
        template<typename Shade>
        void DrawPlayer(const Shade& shade) {
            if (!playerId_) return;

            if (models_.loaded) {
                // Feet on the floor, facing the walk direction (glTF characters face +Z)
                const float feet = -renderingSystem_.GetModel(models_.player)->boundsMin.y * ModelScale;
                renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{
                    models_.player, { playerPosition_.x, feet, playerPosition_.y }, playerFacingYaw_, ModelScale, shade(1.0f, 1.0f, 1.0f) });
                return;
            }

            using Systems::Rendering::MeshRenderData;
            using Systems::Rendering::MeshShape;

            constexpr float BodyHeight = 1.1f;
            constexpr float HeadSize = 0.42f;
            const float x = playerPosition_.x;
            const float z = playerPosition_.y;
            const float forwardX = std::sin(playerFacingYaw_);
            const float forwardZ = std::cos(playerFacingYaw_);

            MeshRenderData body{};
            body.shape = MeshShape::Cylinder;
            body.position = { x, BodyHeight * 0.5f, z };
            body.size = { PlayerSizeTiles * 0.8f, BodyHeight, PlayerSizeTiles * 0.8f };
            body.color = shade(0.95f, 0.75f, 0.20f);
            renderingSystem_.DrawMesh(body);

            MeshRenderData head{};
            head.shape = MeshShape::Sphere;
            head.position = { x, BodyHeight + HeadSize * 0.45f, z };
            head.size = { HeadSize, HeadSize, HeadSize };
            head.color = shade(0.85f, 0.70f, 0.55f);
            renderingSystem_.DrawMesh(head);

            MeshRenderData visor{};
            visor.shape = MeshShape::Box;
            visor.position = { x + forwardX * HeadSize * 0.45f, BodyHeight + HeadSize * 0.5f, z + forwardZ * HeadSize * 0.45f };
            visor.size = { HeadSize * 0.7f, HeadSize * 0.2f, HeadSize * 0.2f };
            visor.yawRadians = playerFacingYaw_;
            visor.color = shade(0.15f, 0.15f, 0.20f);
            renderingSystem_.DrawMesh(visor);
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
            RegisterKeyBinding(RotateCameraLeftAction, Systems::Input::Keys::FromChar('Q'));
            RegisterKeyBinding(RotateCameraRightAction, Systems::Input::Keys::FromChar('E'));
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
        std::shared_ptr<const Content::ContentDatabase> content_ = std::make_shared<Content::ContentDatabase>();
        std::string initializationError_;
        WorldPoint playerPosition_{};
        float playerFacingYaw_{0.0f};             // radians around the vertical axis; 0 faces +Z (south)
        Systems::Rendering::IsometricCamera camera_;

        // Models used to draw the world (all valid when loaded is true)
        struct WorldModels {
            bool loaded{false};
            Systems::Rendering::ModelId floor{};
            Systems::Rendering::ModelId floorVariant{};
            Systems::Rendering::ModelId wall{};
            Systems::Rendering::ModelId corner{};
            Systems::Rendering::ModelId player{};
        };
        WorldModels models_;
        WorldPoint previousRightStick_{};         // for right stick flick detection
        StepTimer timer_;
        double gameTimeSeconds_{0.0};
        bool paused_{false};
        bool active_{true};
        bool initialized_{false};
    };

} // namespace LB::Engine
