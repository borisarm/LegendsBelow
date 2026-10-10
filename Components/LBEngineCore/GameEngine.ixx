// GameEngine.ixx - Game loop orchestration
export module LB.Engine:GameEngine;

import std;
import LB.Core;
import LB.Character;
import LB.CharacterService;
import LB.Combat;
import LB.CombatService;
import LB.Content;
import LB.Dungeon;
import LB.DungeonService;
import LB.Inventory;
import LB.InventoryService;
import LB.AudioSystem;
import LB.InputSystem;
import LB.PersistenceSystem;
import LB.RenderingSystem;
import :StepTimer;
import :World;
import :Encounter;
import :Loot;
import :CharacterScreen;

export namespace LB::Engine {

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
        // TrueType fonts for text (UiFont); empty draws text in the built-in pixel font
        std::string fontDirectory;
        // Waves of enemies from enemies.json fight the player (needs content with a player)
        bool spawnEnemies{true};
        // Fixed seed for dice, spawn points, and dungeon floors (tests, replays); empty: random
        std::optional<std::uint32_t> randomSeed;
        // Generated dungeon floors (rooms, corridors, a trapdoor to the next floor); off: the
        // placeholder test arena with waves of enemies
        bool dungeon{false};
    };

    /// <summary>
    /// Combat feedback floating up from a character: damage dealt or taken, or a miss
    /// (game-design.md, UI feedback). Rises and fades over GameEngine::FloatingTextSeconds.
    /// </summary>
    struct FloatingText {
        std::string text;                       // "7", "12!" (critical), "MISS"
        Systems::Rendering::Vector3 position{}; // world point it starts from (over the character's head)
        Systems::Rendering::Color color{};      // display (sRGB) color
        float scale{3.0f};                      // pixel font scale
        float ageSeconds{0.0f};
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
        static constexpr std::string_view AttackAction = "Attack";
        static constexpr std::string_view CharacterScreenAction = "CharacterScreen";
        static constexpr std::string_view UsePotionAction = "UsePotion";
        static constexpr std::string_view MenuUpAction = "MenuUp";
        static constexpr std::string_view MenuDownAction = "MenuDown";
        static constexpr std::string_view MenuConfirmAction = "MenuConfirm";
        static constexpr std::string_view MenuBackAction = "MenuBack";
        static constexpr std::string_view MapAction = "Map";

        // Right stick flicks past this deflection rotate (X) or zoom (Y) the camera
        static constexpr float CameraStickThreshold = 0.6f;

        // Save slot used by quick save/load (F5/F9)
        static constexpr std::int32_t QuickSaveSlot = 0;

        // Placeholder test arena (without EngineOptions::dungeon): centered on the origin
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
        static constexpr std::string_view PlayerWeaponModel = "sword_common.glb";
        // Font of the HUD and the damage numbers (Assets/Fonts, SIL Open Font License)
        static constexpr std::string_view UiFont = "MedievalSharp-Regular.ttf";
        static constexpr std::string_view PlayerSwordArmPart = "character_knightArmRight";
        // Props for loot on the floor and the weapon categories the knight can hold
        static constexpr std::array<std::string_view, 10> PropModels = {
            "lootSackA.glb", "coinsSmall.glb", "coinsMedium.glb", "coinsLarge.glb", "axe_common.glb",
            "dagger_common.glb", "hammer_common.glb", "staff_common.glb", "crossbow_common.glb", "shield_common.glb" };
        // Animation sets of the skinned characters (KayKit Rig_Medium), in the model directory
        static constexpr std::array<std::string_view, 3> AnimationFiles = {
            "Animations/Rig_Medium_General.glb", "Animations/Rig_Medium_MovementBasic.glb", "Animations/Rig_Medium_CombatMelee.glb" };

        // Combat in the test arena: the player's starting weapon (weapons.json) and the enemies of
        // the waves (enemies.json)
        static constexpr std::string_view StartingWeapon = "weapon_sword_rusted_shortsword_t1";
        static constexpr std::string_view LightEnemy = "enemy_skeleton_minion";
        static constexpr std::string_view HeavyEnemy = "enemy_skeleton_warrior";
        static constexpr float PlayerReach = 1.4f;              // tiles between centers
        static constexpr float PlayerRespawnSeconds = 3.0f;     // after going down

        // Loot: the potion the quick-use button drinks, the bag size, and how close the player
        // walks to pick things up
        static constexpr std::string_view HealingPotion = "consumable_potion_healing_minor";
        static constexpr std::int32_t StartingPotions = 1;
        static constexpr std::int32_t BagCapacity = 20;
        static constexpr float PickupRadius = 0.7f;

        // Dungeon floors: props, how close the player steps to open a chest or take the trapdoor,
        // and the loot table of treasure chests
        static constexpr std::string_view ChestModel = "chest_common.glb";
        static constexpr std::string_view OpenChestModel = "chest_common_empty.glb";
        static constexpr std::string_view TrapdoorModel = "trapdoor.glb";
        static constexpr std::string_view ChestLootTable = "loot_chest";
        static constexpr float InteractRadius = 1.0f;

        // Torches on room walls: the prop, how far their light reaches (tiles), and how dark the
        // dungeon is without them (the directional lights' share and the ambient light)
        static constexpr std::string_view TorchModel = "torchWall.glb";
        static constexpr float TorchLightRadius = 6.0f;
        static constexpr float TorchLightIntensity = 1.6f;
        static constexpr float DungeonDirectionalLight = 0.3f;

        // The minimap reveals floor cells this far (cells) around the player, in sight
        static constexpr std::int32_t MapRevealCells = 4;

        // Damage numbers rise this far (tiles) over their lifetime, fading out at the end
        static constexpr float FloatingTextSeconds = 1.0f;
        static constexpr float FloatingTextRise = 0.8f;

        // Walls between the camera and the player fade (game-design.md, occlusion handling)
        static constexpr float FadedWallAlpha = 0.3f;
        static constexpr float WallFadeSeconds = 0.2f;   // full fade in or out

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

            dungeonMode_ = options.dungeon;
            baseSeed_ = options.randomSeed ? *options.randomSeed : std::random_device{}();
            world_ = WorldMap::Arena(ArenaHalfWidth, ArenaHalfHeight);
            depth_ = 0;
            rooms_.clear();

            if (!renderingSystem_.Initialize(nativeWindow, width, height)) {
                initializationError_ = "Failed to initialize Direct3D 12. A GPU supporting Shader Model 6.0 is required.";
                return false;
            }

            if (!LoadModels(options.modelDirectory) || !LoadFonts(options.fontDirectory)) {
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

            encounter_ = options.randomSeed ? Encounter(*options.randomSeed) : Encounter();
            lootRoller_ = options.randomSeed ? LootRoller(*options.randomSeed + 1) : LootRoller();
            spawnEnemies_ = options.spawnEnemies;
            GiveStartingKit();
            if (dungeonMode_ && playerId_) {
                StartFloor(1);
            }
            StartEncounter();

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
            playerWeapon_ = nullptr;
            playerCombat_ = {};
            inventoryService_ = {};
            gold_ = 0;
            groundItems_.clear();
            characterScreen_.Close();
            encounter_.Reset();
            combatEvents_.clear();
            floatingTexts_.clear();
            models_ = {};
            characterModels_.clear();
            enemyClips_ = {};
            walls_.clear();
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
        /// Where the player faces: radians around the vertical axis, 0 faces +Z (south)
        /// </summary>
        [[nodiscard]] float GetPlayerFacingYaw() const noexcept { return playerFacingYaw_; }

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
        /// The walkable world: the test arena, or the current dungeon floor
        /// </summary>
        [[nodiscard]] const WorldMap& GetWorld() const noexcept { return world_; }

        /// <summary>
        /// The dungeon floor the player is on (1 for the first; 0 in the test arena)
        /// </summary>
        [[nodiscard]] std::int32_t GetDepth() const noexcept { return depth_; }

        /// <summary>
        /// State of a room of the current floor: enemies raised (when the player first walks in),
        /// all of them dead, a chest opened
        /// </summary>
        struct RoomState {
            bool triggered{false};
            bool cleared{false};
            bool chestOpened{false};
            std::vector<std::uint32_t> enemies;
        };

        [[nodiscard]] const std::vector<RoomState>& GetRoomStates() const noexcept { return rooms_; }

        /// <summary>
        /// A torch on a room wall: where its flame burns, which way it faces, and the wall piece it
        /// hangs on (it fades with that wall; none without models)
        /// </summary>
        struct Torch {
            WorldPoint position{};       // under the flame, on the floor plane
            float yaw{0.0f};             // faces into the room
            std::optional<std::size_t> wall;
            float phase{0.0f};           // flicker offset
        };

        [[nodiscard]] const std::vector<Torch>& GetTorches() const noexcept { return torches_; }

        /// <summary>
        /// Has the player seen this floor cell? (what the minimap shows)
        /// </summary>
        [[nodiscard]] bool IsCellExplored(std::int32_t x, std::int32_t y) const noexcept {
            const auto& layout = world_.Layout();
            return layout.InBounds(x, y) && explored_.size() == layout.cells.size() && explored_[static_cast<std::size_t>(y * layout.width + x)] != 0;
        }

        /// <summary>
        /// The minimap shown large (M or the right stick click toggles it)
        /// </summary>
        [[nodiscard]] bool IsMapExpanded() const noexcept { return mapExpanded_; }

        /// <summary>
        /// Go down to the next floor (normally by stepping on the trapdoor of a cleared exit room)
        /// </summary>
        void Descend();

        /// <summary>
        /// The enemies and waves of the arena
        [[nodiscard]] const Encounter& GetEncounter() const noexcept { return encounter_; }

        /// <summary>
        /// Attacks resolved during the last simulation step (the player's and the enemies')
        /// </summary>
        [[nodiscard]] const std::vector<CombatEvent>& GetCombatEvents() const noexcept { return combatEvents_; }

        /// <summary>
        /// Damage numbers and misses currently floating over characters
        /// </summary>
        [[nodiscard]] const std::vector<FloatingText>& GetFloatingTexts() const noexcept { return floatingTexts_; }

        /// <summary>
        /// True while there is a player with hit points left
        /// </summary>
        [[nodiscard]] bool IsPlayerAlive() const {
            if (!playerId_) return false;
            const auto character = characterService_.GetCharacter(*playerId_);
            return character && character->get().IsAlive();
        }

        /// <summary>
        /// True while the player's weapon swing plays (attacks repeat while Attack is held)
        /// </summary>
        [[nodiscard]] bool IsPlayerAttacking() const noexcept { return playerCombat_.swinging; }

        /// <summary>
        /// The player's weapon attack (design-combat-system.md 4.1, 4.3): class attack bonus with the
        /// weapon's category, the weapon's dice, and its scaling attribute's modifier
        /// </summary>
        [[nodiscard]] Services::Combat::WeaponAttackProfile GetPlayerAttackProfile() const {
            Services::Combat::WeaponAttackProfile attack{};
            if (!playerId_ || !playerWeapon_) return attack;
            const auto character = characterService_.GetCharacter(*playerId_);
            if (!character) return attack;

            const auto& weapon = *playerWeapon_;
            const auto& attributes = character->get().GetAttributes();
            const std::int32_t modifier = weapon.attributeScale == "DEX" ? attributes.GetDexterityModifier()
                                        : weapon.attributeScale == "STR" ? attributes.GetStrengthModifier() : 0;
            attack.attackBonus = characterService_.GetAttackBonus(*playerId_, weapon.category).value_or(0);
            attack.damageDice = weapon.damageDice;
            attack.damageBonus = weapon.damageBonus + static_cast<std::int32_t>(std::lround(static_cast<float>(modifier) * weapon.scaleFactor));
            attack.damageType = weapon.damageTypes.empty() ? LB::Combat::DamageType::Slashing : weapon.damageTypes.front();
            attack.critRangeMin = weapon.critRangeMin;
            attack.critMultiplier = weapon.critMultiplier;
            return attack;
        }

        /// <summary>
        /// How the player defends: AC 10 + DEX modifier (unarmoured), and the physical armour
        /// rating of the equipped pieces plus shields' armour bonus (design-armour-system.md 3.1)
        /// </summary>
        [[nodiscard]] Services::Combat::DefenseProfile GetPlayerDefense() const {
            Services::Combat::DefenseProfile defense{};
            if (!playerId_) return defense;
            const auto character = characterService_.GetCharacter(*playerId_);
            if (!character) return defense;

            defense.armorClass = character->get().GetArmorClass() + character->get().GetAttributes().GetDexterityModifier();
            for (const auto& piece : characterService_.GetEquippedArmor(*playerId_)) {
                if (const auto* armor = content_->FindArmor(piece.armorId)) {
                    defense.physicalArmor += armor->physicalAR + armor->armorBonus;
                }
            }
            return defense;
        }

        /// <summary>
        /// Gold carried
        /// </summary>
        [[nodiscard]] std::int32_t GetGold() const noexcept { return gold_; }

        /// <summary>
        /// The bag (unequipped items); item ids are packed ItemRefs
        /// </summary>
        [[nodiscard]] std::vector<Inventory::InventoryItem> GetInventory() const {
            return playerId_ ? inventoryService_.GetInventory(*playerId_) : std::vector<Inventory::InventoryItem>{};
        }

        /// <summary>
        /// How many of an item (by content key) the bag holds
        /// </summary>
        [[nodiscard]] std::int32_t CountItem(std::string_view key) const;

        /// <summary>
        /// The weapon the player attacks with
        /// </summary>
        [[nodiscard]] const Content::WeaponDefinition* GetPlayerWeapon() const noexcept { return playerWeapon_; }

        /// <summary>
        /// Gold and items lying on the floor, waiting to be picked up
        /// </summary>
        [[nodiscard]] const std::vector<GroundItem>& GetGroundItems() const noexcept { return groundItems_; }

        /// <summary>
        /// The character and inventory screen (I, C, or the gamepad View button)
        /// </summary>
        [[nodiscard]] const CharacterScreen& GetCharacterScreen() const noexcept { return characterScreen_; }

        /// <summary>
        /// Put items (by content key) in the bag. Returns how many fit.
        /// </summary>
        std::int32_t GiveItem(std::string_view key, std::int32_t quantity = 1);

        /// <summary>
        /// Equip a weapon or armour piece from the bag (what was in its slot goes back to the bag).
        /// Fails for items not in the bag, not usable by the player's class, or above their level.
        /// </summary>
        bool EquipItem(std::string_view key);

        /// <summary>
        /// Take an armour piece off into the bag (fails when the bag is full)
        /// </summary>
        bool UnequipArmorSlot(LB::Combat::EquipmentSlot slot);

        /// <summary>
        /// Use a consumable from the bag (a healing potion restores its dice of hit points)
        /// </summary>
        bool UseItem(std::string_view key);

        /// <summary>
        /// Drop a loot table's roll at a point (enemies do this when they die)
        /// </summary>
        void DropLoot(std::string_view lootTable, WorldPoint at);

        /// <summary>
        /// Place an enemy from enemies.json, ready to act (no spawn animation). Returns its id, or
        /// nothing for an unknown key or before Initialize.
        /// </summary>
        std::optional<std::uint32_t> SpawnEnemy(std::string_view key, WorldPoint at) {
            const auto* type = content_->FindEnemy(key);
            if (!initialized_ || !type) return std::nullopt;
            return encounter_.Spawn(*type, at, std::atan2(playerPosition_.x - at.x, playerPosition_.y - at.y), false);
        }

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
        /// Opacity of each wall piece (1 opaque, FadedWallAlpha while hiding the player); empty
        /// when the world is drawn with primitives
        /// </summary>
        [[nodiscard]] std::vector<float> GetWallOpacities() const {
            std::vector<float> opacities;
            for (const auto& wall : walls_) opacities.push_back(wall.opacity);
            return opacities;
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
                WriteProgress(data);
                if (dungeonMode_) WriteFloor(data);
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
            if (playerId_ && dungeonMode_) {
                ReadFloor(result.data);      // the floor first: the position is on it
            }
            const WorldPoint saved{ static_cast<float>(playerX), static_cast<float>(playerY) };
            if (playerId_ && hasPlayerPosition && world_.CircleFits(saved, PlayerRadius)) {
                playerPosition_ = saved;
                characterService_.PlaceCharacter(*playerId_, ToTile(playerPosition_));
                camera_.SnapTo({ playerPosition_.x, playerPosition_.y });  // instant refocus
            }
            if (playerId_) {
                ReadProgress(result.data);   // saves from before progression keep the current one
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

            // The character screen takes the input while open (Esc closes it instead of pausing)
            const bool screenWasOpen = characterScreen_.IsOpen();
            UpdateCharacterScreen();

            if (!screenWasOpen && !characterScreen_.IsOpen() && inputSystem_.IsActionTriggered(std::string(PauseAction))) {
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
            if (inputSystem_.IsActionTriggered(std::string(MapAction))) {
                mapExpanded_ = !mapExpanded_;
            }

            combatEvents_.clear();
            if (!paused_ && !screenWasOpen && !characterScreen_.IsOpen()) {
                gameTimeSeconds_ += elapsedSeconds;
                // Attacks first: a swing that starts this step roots the player at once
                UpdatePlayerCombat(static_cast<float>(elapsedSeconds));
                MovePlayer(static_cast<float>(elapsedSeconds));
                UpdateEncounter(static_cast<float>(elapsedSeconds));
                UpdateQuickUse();
                if (dungeonMode_) RevealAround();
                UpdateGroundItems(static_cast<float>(elapsedSeconds));
                UpdateFloatingTexts(static_cast<float>(elapsedSeconds));
            }

            if (playerId_) {
                camera_.Follow({ playerPosition_.x, playerPosition_.y });
            }
            camera_.Update(static_cast<float>(elapsedSeconds));
            UpdateWallFade(static_cast<float>(elapsedSeconds));

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
            const float dim = paused_ || characterScreen_.IsOpen() ? 0.4f : 1.0f;
            const auto shade = [dim](float r, float g, float b) {
                const auto linear = [dim](float c) { return std::pow(c * dim, 2.2f); };
                return Systems::Rendering::Color{ linear(r), linear(g), linear(b), 1.0f };
            };

            // Dim ambient background (design-technical-directx12.md 2.2), then the 3D world
            renderingSystem_.Clear(shade(0.05f, 0.05f, 0.07f));
            // Dungeon floors are dark: the torches light them
            Systems::Rendering::SceneLighting lighting{};
            if (dungeonMode_) {
                lighting.directional = DungeonDirectionalLight * dim;
                lighting.ambient = { 0.02f * dim, 0.018f * dim, 0.025f * dim, 1.0f };
            }
            else if (dim < 1.0f) {
                lighting.directional = dim;
            }
            renderingSystem_.SetLighting(lighting);
            renderingSystem_.SetCamera(camera_.GetView());
            DrawArena(shade);
            DrawPlayer(shade);
            DrawEnemies(shade);
            DrawGroundItems(shade(1.0f, 1.0f, 1.0f));
            DrawFloorProps(shade(1.0f, 1.0f, 1.0f));
            DrawTorches(shade(1.0f, 1.0f, 1.0f));
            DrawHud();
            DrawMinimap();
            DrawCharacterScreen();

            renderingSystem_.EndFrame();
        }

        /// <summary>
        /// Load the arena and player models. Without a model directory the world is drawn with
        /// primitives; a missing or invalid model stops startup with every problem listed.
        /// </summary>
        bool LoadModels(const std::string& modelDirectory) {
            models_ = {};
            walls_.clear();
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
            models_.playerWeapon = load(PlayerWeaponModel);
            characterModels_.clear();

            // Loot on the floor, weapons in the knight's hand, and dungeon props (KayKit Dungeon Pack)
            for (const auto file : { ChestModel, OpenChestModel, TrapdoorModel, TorchModel }) {
                characterModels_[std::string(file)] = load(file);
            }
            for (const auto file : PropModels) {
                characterModels_[std::string(file)] = load(file);
            }
            for (const auto& consumable : content_->GetConsumables()) {
                if (!characterModels_.contains(consumable.model)) characterModels_[consumable.model] = load(consumable.model);
            }

            // Enemy bodies and the weapons they hold, by file name
            for (const auto& enemy : content_->GetEnemies()) {
                for (const auto& file : { enemy.model, enemy.mainHandModel, enemy.offHandModel }) {
                    if (!file.empty() && !characterModels_.contains(file)) characterModels_[file] = load(file);
                }
            }

            for (const auto file : AnimationFiles) {
                const auto result = renderingSystem_.LoadAnimations(directory / file);
                if (!result.Succeeded()) errors.push_back(result.error);
            }
            if (errors.empty()) {
                ResolveEnemyClips(errors);
            }

            if (!errors.empty()) {
                initializationError_ = std::format("3D models in '{}' could not be loaded ({} problems):", modelDirectory, errors.size());
                for (const auto& error : errors) {
                    initializationError_ += "\n" + error;
                }
                models_ = {};
                characterModels_.clear();
                enemyClips_ = {};
                return false;
            }
            models_.loaded = true;
            BuildWalls();
            return true;
        }

        // A wall or corner piece of the arena, placed once when the models load
        struct WallPiece {
            Systems::Rendering::ModelId model{};
            Systems::Rendering::Vector3 position{};
            float yaw{0.0f};
            Systems::Rendering::Vector3 boundsMin{};   // world space
            Systems::Rendering::Vector3 boundsMax{};
            float opacity{1.0f};
        };

        // World bounding box of a model placed with scale, a turn around Y, and a position
        // (matches the renderer's Scale * RotationY * Translation)
        WallPiece PlacePiece(Systems::Rendering::ModelId model, float x, float y, float z, float yaw) const {
            const auto* data = renderingSystem_.GetModel(model);
            WallPiece piece{ model, { x, y, z }, yaw };
            const float c = std::cos(yaw);
            const float s = std::sin(yaw);
            constexpr float big = std::numeric_limits<float>::max();
            piece.boundsMin = { big, data->boundsMin.y * ModelScale + y, big };
            piece.boundsMax = { -big, data->boundsMax.y * ModelScale + y, -big };
            for (float cx : { data->boundsMin.x, data->boundsMax.x }) {
                for (float cz : { data->boundsMin.z, data->boundsMax.z }) {
                    const float wx = (cx * c + cz * s) * ModelScale + x;
                    const float wz = (-cx * s + cz * c) * ModelScale + z;
                    piece.boundsMin.x = std::min(piece.boundsMin.x, wx);
                    piece.boundsMax.x = std::max(piece.boundsMax.x, wx);
                    piece.boundsMin.z = std::min(piece.boundsMin.z, wz);
                    piece.boundsMax.z = std::max(piece.boundsMax.z, wz);
                }
            }
            return piece;
        }

        // Walls along every edge between a floor cell and a solid one (a wall segment is one cell
        // long; its inner face sits on the edge), and pillars filling the outer corners of rooms
        void BuildWalls() {
            walls_.clear();
            const auto* wall = renderingSystem_.GetModel(models_.wall);
            const float halfDepth = (wall->boundsMax.z - wall->boundsMin.z) * ModelScale * 0.5f;
            const float wallY = -wall->boundsMin.y * ModelScale;
            const float cornerY = -renderingSystem_.GetModel(models_.corner)->boundsMin.y * ModelScale;
            constexpr float quarterTurn = std::numbers::pi_v<float> * 0.5f;
            constexpr float cell = WorldMap::CellTiles;

            const auto& layout = world_.Layout();
            for (std::int32_t y = 0; y < layout.height; ++y) {
                for (std::int32_t x = 0; x < layout.width; ++x) {
                    if (!layout.IsFloor(x, y)) continue;
                    const auto corner = world_.CellCorner(x, y);
                    const float midX = corner.x + cell * 0.5f;
                    const float midZ = corner.y + cell * 0.5f;
                    if (!layout.IsFloor(x, y - 1)) walls_.push_back(PlacePiece(models_.wall, midX, wallY, corner.y - halfDepth, 0.0f));
                    if (!layout.IsFloor(x, y + 1)) walls_.push_back(PlacePiece(models_.wall, midX, wallY, corner.y + cell + halfDepth, 0.0f));
                    if (!layout.IsFloor(x - 1, y)) walls_.push_back(PlacePiece(models_.wall, corner.x - halfDepth, wallY, midZ, quarterTurn));
                    if (!layout.IsFloor(x + 1, y)) walls_.push_back(PlacePiece(models_.wall, corner.x + cell + halfDepth, wallY, midZ, quarterTurn));
                }
            }

            // Corners: where only one of the four cells around a grid point is floor, the walls of
            // its two edges leave a gap at the point; a pillar fills it, set back from the floor
            for (std::int32_t y = 0; y <= layout.height; ++y) {
                for (std::int32_t x = 0; x <= layout.width; ++x) {
                    const bool cells[4] = { layout.IsFloor(x - 1, y - 1), layout.IsFloor(x, y - 1), layout.IsFloor(x - 1, y), layout.IsFloor(x, y) };
                    if (std::ranges::count(cells, true) != 1) continue;
                    const float sx = cells[0] || cells[2] ? 1.0f : -1.0f;   // away from the floor cell
                    const float sz = cells[0] || cells[1] ? 1.0f : -1.0f;
                    const auto point = world_.CellCorner(x, y);
                    walls_.push_back(PlacePiece(models_.corner, point.x + sx * halfDepth, cornerY, point.y + sz * halfDepth, 0.0f));
                }
            }
        }

        // Does the segment from -> to pass through the box? (slab test)
        static bool SegmentHitsBox(Systems::Rendering::Vector3 from, Systems::Rendering::Vector3 to,
                                   Systems::Rendering::Vector3 lo, Systems::Rendering::Vector3 hi) noexcept {
            const float start[3] = { from.x, from.y, from.z };
            const float delta[3] = { to.x - from.x, to.y - from.y, to.z - from.z };
            const float minimum[3] = { lo.x, lo.y, lo.z };
            const float maximum[3] = { hi.x, hi.y, hi.z };
            float enter = 0.0f;
            float exit = 1.0f;
            for (int axis = 0; axis < 3; ++axis) {
                if (std::abs(delta[axis]) < 1e-6f) {
                    if (start[axis] < minimum[axis] || start[axis] > maximum[axis]) return false;
                    continue;
                }
                float a = (minimum[axis] - start[axis]) / delta[axis];
                float b = (maximum[axis] - start[axis]) / delta[axis];
                if (a > b) std::swap(a, b);
                enter = std::max(enter, a);
                exit = std::min(exit, b);
                if (enter > exit) return false;
            }
            return true;
        }

        /// <summary>
        /// Fade wall pieces that hide the player from the camera (sight lines to the feet, chest,
        /// and head) and bring the others back, easing over WallFadeSeconds
        /// </summary>
        void UpdateWallFade(float elapsedSeconds) {
            if (walls_.empty()) return;

            const auto eye = camera_.GetView().eye;
            const float step = elapsedSeconds / WallFadeSeconds * (1.0f - FadedWallAlpha);
            for (auto& wall : walls_) {
                bool hides = false;
                if (playerId_) {
                    for (float height : { 0.1f, 0.6f, 1.1f }) {
                        const Systems::Rendering::Vector3 target{ playerPosition_.x, height, playerPosition_.y };
                        if (SegmentHitsBox(eye, target, wall.boundsMin, wall.boundsMax)) {
                            hides = true;
                            break;
                        }
                    }
                }
                const float goal = hides ? FadedWallAlpha : 1.0f;
                wall.opacity = wall.opacity < goal ? std::min(goal, wall.opacity + step) : std::max(goal, wall.opacity - step);
            }
        }

        /// <summary>
        /// Load the UI font. Without a font directory text uses the pixel font; a missing or
        /// invalid font stops startup.
        /// </summary>
        bool LoadFonts(const std::string& fontDirectory) {
            if (fontDirectory.empty()) return true;

            const auto directory = std::filesystem::path(std::u8string(fontDirectory.begin(), fontDirectory.end()));
            const auto result = renderingSystem_.LoadFont(directory / UiFont);
            if (!result.Succeeded()) {
                initializationError_ = std::format("The font in '{}' could not be loaded:\n{}", fontDirectory, result.error);
                return false;
            }
            return true;
        }

        // Enemy animation clips by name (KayKit Rig_Medium). Each enemy type names its attack clip
        // (CombatMelee set); the overhand Throw is the fallback for types that do not.
        void ResolveEnemyClips(std::vector<std::string>& errors) {
            const auto clip = [&](std::string_view name, float* seconds = nullptr) {
                const auto id = renderingSystem_.FindAnimation(name);
                if (!id) {
                    errors.push_back(std::format("animation '{}' not found in the Rig_Medium animation sets", name));
                    return Systems::Rendering::InvalidAnimationId;
                }
                if (seconds) *seconds = std::max(0.01f, renderingSystem_.GetAnimation(*id)->durationSeconds);
                return *id;
            };

            enemyClips_.idle = clip("Idle_A");
            enemyClips_.walk = clip("Walking_A", &enemyClips_.walkSeconds);
            enemyClips_.attack = clip("Throw", &enemyClips_.attackSeconds);
            enemyClips_.hit = clip("Hit_A", &enemyClips_.hitSeconds);
            enemyClips_.death = clip("Death_A");
            enemyClips_.spawn = clip("Spawn_Ground", &enemyClips_.spawnSeconds);

            enemyClips_.attacks.clear();
            for (const auto& enemy : content_->GetEnemies()) {
                if (enemy.attackAnimation.empty()) continue;
                ClipInfo attack{};
                attack.id = clip(enemy.attackAnimation, &attack.seconds);
                enemyClips_.attacks.emplace(enemy.key, attack);
            }
        }

        // Waves start once there is a player to fight (and enemies are enabled)
        void StartEncounter() {
            encounter_.Configure(content_->FindEnemy(LightEnemy), content_->FindEnemy(HeavyEnemy), world_);
            encounter_.SetClips(enemyClips_);
            encounter_.Reset();
            if (spawnEnemies_ && playerId_ && !dungeonMode_) {
                encounter_.StartWave(1, playerPosition_);
            }
        }

        [[nodiscard]] float PlayerAttackSeconds() const noexcept {
            return playerWeapon_ ? playerWeapon_->attackTimeSeconds : 1.0f;
        }

        /// <summary>
        /// The player's melee attack: Attack (left mouse, Space, gamepad A) swings the weapon,
        /// again and again while held. A swing turns toward the nearest enemy in reach and strikes
        /// it part way through (Encounter::AttackImpactFraction); the player stands still until
        /// then. A downed player gets back up after PlayerRespawnSeconds and the waves restart.
        /// </summary>
        void UpdatePlayerCombat(float elapsedSeconds) {
            if (!playerId_) return;
            playerCombat_.hitFlashSeconds = std::max(0.0f, playerCombat_.hitFlashSeconds - elapsedSeconds);

            if (!IsPlayerAlive()) {
                playerCombat_.swinging = false;
                playerCombat_.deadSeconds += elapsedSeconds;
                if (playerCombat_.deadSeconds >= PlayerRespawnSeconds) {
                    RespawnPlayer();
                }
                return;
            }

            const float attackSeconds = PlayerAttackSeconds();
            if (playerCombat_.swinging) {
                playerCombat_.swingSeconds += elapsedSeconds;
                if (!playerCombat_.impactDone && playerCombat_.swingSeconds >= attackSeconds * Encounter::AttackImpactFraction) {
                    playerCombat_.impactDone = true;
                    if (playerCombat_.target) {
                        if (auto event = encounter_.StrikeEnemy(*playerCombat_.target, GetPlayerAttackProfile(), playerPosition_, PlayerReach)) {
                            combatEvents_.push_back(*event);
                            if (event->killed) {
                                OnEnemyKilled(*encounter_.FindEnemy(event->enemyId));
                            }
                        }
                    }
                }
                if (playerCombat_.swingSeconds >= attackSeconds) {
                    playerCombat_.swinging = false;
                }
            }

            const std::string attack(AttackAction);
            if (!playerCombat_.swinging && (inputSystem_.IsActionTriggered(attack) || inputSystem_.IsActionDown(attack))) {
                playerCombat_.swinging = true;
                playerCombat_.swingSeconds = 0.0f;
                playerCombat_.impactDone = false;
                playerCombat_.target = encounter_.FindTarget(playerPosition_, PlayerReach);
                if (const auto* target = playerCombat_.target ? encounter_.FindEnemy(*playerCombat_.target) : nullptr) {
                    playerFacingYaw_ = std::atan2(target->position.x - playerPosition_.x, target->position.y - playerPosition_.y);
                }
            }
        }

        /// <summary>
        /// Enemies act; their hits are applied to the player. A new wave restores the player's health.
        /// </summary>
        void UpdateEncounter(float elapsedSeconds) {
            if (!playerId_) return;

            const PlayerTarget target{ playerPosition_, IsPlayerAlive(), GetPlayerDefense() };
            if (dungeonMode_) {
                UpdateFloorInteractions();
            }
            const auto wave = encounter_.GetWave();
            for (const auto& event : encounter_.Update(elapsedSeconds, target)) {
                combatEvents_.push_back(event);
                if (!event.result.hit || event.result.finalDamage <= 0 || !IsPlayerAlive()) continue;

                characterService_.DamageCharacter(*playerId_, event.result.finalDamage);
                playerCombat_.hitFlashSeconds = Encounter::HitFlashSeconds;
                if (!IsPlayerAlive()) {
                    playerCombat_.swinging = false;
                    playerCombat_.deadSeconds = 0.0f;
                }
            }

            if (encounter_.GetWave() != wave) {
                characterService_.RestCharacter(*playerId_);
            }
            if (dungeonMode_) {
                UpdateRooms();
            }
        }

        // Dungeon floors (DungeonFloor.cpp)
        void StartFloor(std::int32_t depth);
        void UpdateRooms();
        void UpdateFloorInteractions();
        void DrawFloorProps(const Systems::Rendering::Color& tint);
        void BuildTorches();
        void RevealAround();
        void DrawTorches(const Systems::Rendering::Color& tint);
        void DrawMinimap();
        void WriteFloor(Systems::Persistence::SaveData& data) const;
        void ReadFloor(const Systems::Persistence::SaveData& data);

        // Player progression and items (PlayerProgress.cpp)
        void GiveStartingKit();
        void OnEnemyKilled(const Enemy& enemy);
        void UpdateGroundItems(float elapsedSeconds);
        void UpdateQuickUse();
        void UpdateCharacterScreen();
        std::vector<ScreenRow> BuildScreenRows() const;
        void DrawGroundItems(const Systems::Rendering::Color& tint);
        void DrawCharacterScreen();
        Systems::Rendering::ModelId ItemModel(const GroundItem& item) const;
        Systems::Rendering::ModelId HeldWeaponModel() const;
        void AddFloatingText(std::string text, Systems::Rendering::Vector3 at, Systems::Rendering::Color color, float scale);
        void WriteProgress(Systems::Persistence::SaveData& data) const;
        void ReadProgress(const Systems::Persistence::SaveData& data);
        std::string CanEquip(ItemRef item) const;

        // World height just over a character's head
        float EnemyTop(const Enemy& enemy) const {
            if (models_.loaded) {
                const auto it = characterModels_.find(enemy.definition->model);
                if (it != characterModels_.end()) return renderingSystem_.GetModel(it->second)->boundsMax.y * enemy.definition->modelScale + 0.1f;
            }
            return 1.4f;
        }

        float PlayerTop() const {
            if (models_.loaded) {
                const auto* knight = renderingSystem_.GetModel(models_.player);
                return (knight->boundsMax.y - knight->boundsMin.y) * ModelScale + 0.1f;
            }
            return 1.6f;
        }

        /// <summary>
        /// Age the floating combat texts and start new ones for this step's attacks: damage in
        /// white over enemies (gold and larger for criticals, marked "!"), red over the player,
        /// and a grey "MISS" for attacks that did not connect
        /// </summary>
        void UpdateFloatingTexts(float elapsedSeconds) {
            for (auto& text : floatingTexts_) text.ageSeconds += elapsedSeconds;
            std::erase_if(floatingTexts_, [](const FloatingText& text) { return text.ageSeconds >= FloatingTextSeconds; });

            for (const auto& event : combatEvents_) {
                Systems::Rendering::Vector3 at{ playerPosition_.x, PlayerTop(), playerPosition_.y };
                if (event.kind == CombatEventKind::PlayerAttack) {
                    const auto* enemy = encounter_.FindEnemy(event.enemyId);
                    if (!enemy) continue;
                    at = { enemy->position.x, EnemyTop(*enemy), enemy->position.y };
                }
                // Alternate sides a little so quick hits do not stack on top of each other
                at.x += (floatingTextCount_++ % 2 == 0 ? -0.15f : 0.15f);

                FloatingText text{};
                text.position = at;
                if (!event.result.hit) {
                    text.text = "MISS";
                    text.color = { 0.75f, 0.75f, 0.75f, 1.0f };
                    text.scale = 2.0f;
                }
                else if (event.kind == CombatEventKind::PlayerAttack) {
                    text.text = std::format("{}{}", event.result.finalDamage, event.result.critical ? "!" : "");
                    text.color = event.result.critical ? Systems::Rendering::Color{ 1.0f, 0.82f, 0.25f, 1.0f } : Systems::Rendering::Color{ 1.0f, 1.0f, 1.0f, 1.0f };
                    text.scale = event.result.critical ? 4.0f : 3.0f;
                }
                else {
                    text.text = std::format("{}{}", event.result.finalDamage, event.result.critical ? "!" : "");
                    text.color = { 1.0f, 0.32f, 0.25f, 1.0f };
                    text.scale = event.result.critical ? 4.0f : 3.0f;
                }
                floatingTexts_.push_back(std::move(text));
            }
        }

        // Back on the feet with full health: at the arena's center (the waves start over), or at
        // the floor's entrance (rooms not yet cleared raise their enemies again)
        void RespawnPlayer() {
            characterService_.ReviveCharacter(*playerId_, 1.0f);
            playerPosition_ = dungeonMode_ ? world_.RoomCenter(world_.Layout().entranceRoom) : WorldPoint{};
            for (auto& room : rooms_) {
                if (!room.cleared) {
                    room.triggered = false;
                    room.enemies.clear();
                }
            }
            characterService_.PlaceCharacter(*playerId_, ToTile(playerPosition_));
            camera_.SnapTo({ playerPosition_.x, playerPosition_.y });
            playerCombat_ = {};
            groundItems_.clear();
            StartEncounter();
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
            if (!IsPlayerAlive()) return;
            if (playerCombat_.swinging && !playerCombat_.impactDone) return;   // committed to the swing

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
            playerPosition_ = world_.Move(playerPosition_, { direction.x * speed * elapsedSeconds, direction.y * speed * elapsedSeconds }, PlayerRadius);

            // Keep the domain's tile position in sync (at most one tile per step at these speeds)
            const auto tile = ToTile(playerPosition_);
            if (!characterService_.MoveCharacter(*playerId_, tile)) {
                characterService_.PlaceCharacter(*playerId_, tile);
            }
        }

        // The player's body keeps this far (tiles) from walls
        static constexpr float PlayerRadius = PlayerSizeTiles * 0.5f;

        static LB::Position ToTile(WorldPoint point) noexcept {
            return LB::Position{ static_cast<std::int32_t>(std::floor(point.x)), static_cast<std::int32_t>(std::floor(point.y)) };
        }

        // The world with primitives: one floor slab per tile (top at height 0) and wall blocks
        // around the floor. Tile (tx, ty) covers [tx, tx + 1) x [ty, ty + 1); ground y is world Z.
        template<typename Shade>
        void DrawArena(const Shade& shade) {
            if (models_.loaded) {
                DrawArenaModels(shade);
                return;
            }

            using Systems::Rendering::MeshRenderData;
            using Systems::Rendering::MeshShape;

            // Every tile of a floor cell is a flagstone; every solid tile touching one is a wall block
            constexpr float FloorThickness = 0.2f;
            const auto& layout = world_.Layout();
            const auto origin = world_.Origin();
            const auto tiles = static_cast<std::int32_t>(WorldMap::CellTiles);
            const auto isFloorTile = [&](std::int32_t tx, std::int32_t ty) {
                return layout.IsFloor(static_cast<std::int32_t>(std::floor(static_cast<float>(tx) / WorldMap::CellTiles)),
                                      static_cast<std::int32_t>(std::floor(static_cast<float>(ty) / WorldMap::CellTiles)));
            };
            for (std::int32_t ty = 0; ty < layout.height * tiles; ++ty) {
                for (std::int32_t tx = 0; tx < layout.width * tiles; ++tx) {
                    const bool floor = isFloorTile(tx, ty);
                    bool wall = false;
                    for (std::int32_t dy = -1; dy <= 1 && !floor && !wall; ++dy) {
                        for (std::int32_t dx = -1; dx <= 1 && !wall; ++dx) wall = isFloorTile(tx + dx, ty + dy);
                    }
                    if (!floor && !wall) continue;

                    const float worldX = origin.x + static_cast<float>(tx);
                    const float worldZ = origin.y + static_cast<float>(ty);
                    MeshRenderData mesh{};
                    mesh.shape = MeshShape::Box;
                    if (wall) {
                        mesh.position = { worldX + 0.5f, WallHeight * 0.5f - FloorThickness * 0.5f, worldZ + 0.5f };
                        mesh.size = { 1.0f, WallHeight + FloorThickness, 1.0f };
                        mesh.color = shade(0.42f, 0.40f, 0.38f);
                    }
                    else {
                        const auto ix = static_cast<std::int32_t>(std::floor(worldX));
                        const auto iz = static_cast<std::int32_t>(std::floor(worldZ));
                        const bool light = ((ix + iz) & 1) == 0;
                        mesh.position = { worldX + 0.5f, -FloorThickness * 0.5f, worldZ + 0.5f };
                        mesh.size = { 0.96f, FloorThickness, 0.96f };  // thin seams between flagstones
                        mesh.color = light ? shade(0.30f, 0.29f, 0.27f) : shade(0.24f, 0.23f, 0.22f);
                    }
                    renderingSystem_.DrawMesh(mesh);
                }
            }
        }

        // The world from KayKit pieces: a floor tile per tile (top at height 0), wall segments two
        // tiles long whose inner face is on the floor's edge, and pillars filling the corners
        template<typename Shade>
        void DrawArenaModels(const Shade& shade) {
            using Systems::Rendering::ModelRenderData;

            const auto tint = shade(1.0f, 1.0f, 1.0f);
            const auto place = [&](Systems::Rendering::ModelId model, float x, float y, float z, float yaw) {
                renderingSystem_.DrawModel(ModelRenderData{ model, { x, y, z }, yaw, ModelScale, tint });
            };
            const auto topOf = [&](Systems::Rendering::ModelId model) {
                return renderingSystem_.GetModel(model)->boundsMax.y * ModelScale;
            };

            const auto& layout = world_.Layout();
            for (std::int32_t cy = 0; cy < layout.height; ++cy) {
                for (std::int32_t cx = 0; cx < layout.width; ++cx) {
                    if (!layout.IsFloor(cx, cy)) continue;
                    const auto corner = world_.CellCorner(cx, cy);
                    for (std::int32_t dy = 0; dy < static_cast<std::int32_t>(WorldMap::CellTiles); ++dy) {
                        for (std::int32_t dx = 0; dx < static_cast<std::int32_t>(WorldMap::CellTiles); ++dx) {
                            const auto tx = static_cast<std::int32_t>(std::floor(corner.x)) + dx;
                            const auto ty = static_cast<std::int32_t>(std::floor(corner.y)) + dy;
                            // A fixed scatter of the decorated variant breaks up the grid
                            const bool variant = ((tx * 7 + ty * 13) % 11 + 11) % 11 == 0;
                            const auto model = variant ? models_.floorVariant : models_.floor;
                            place(model, static_cast<float>(tx) + 0.5f, -topOf(model), static_cast<float>(ty) + 0.5f, 0.0f);
                        }
                    }
                }
            }

            for (const auto& wall : walls_) {
                auto wallTint = tint;
                wallTint.a = wall.opacity;
                renderingSystem_.DrawModel(ModelRenderData{ wall.model, wall.position, wall.yaw, ModelScale, wallTint });
            }
        }

        // Smooth 0 -> 1 between edge0 and edge1
        static float SmoothStep(float edge0, float edge1, float x) noexcept {
            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        // The sword arm's pose at the shoulder during a swing: a horizontal slash that reads from
        // above. The arm lifts forward (around X, negative raises it) while winding back to the
        // knight's right (around Y), sweeps across the front through the impact, then settles.
        Systems::Rendering::Matrix4 SwordArmPose() const noexcept {
            using Systems::Rendering::Matrix4;
            if (!playerCombat_.swinging) return {};

            constexpr float lifted = -1.0f;
            constexpr float windUp = -1.1f;     // toward the knight's right
            constexpr float followThrough = 1.0f;
            const float phase = playerCombat_.swingSeconds / PlayerAttackSeconds();
            const float impact = Encounter::AttackImpactFraction;
            const float windUpEnd = impact * 0.65f;

            float pitch = 0.0f;
            float sweep = 0.0f;
            if (phase < windUpEnd) {
                const float t = SmoothStep(0.0f, windUpEnd, phase);
                pitch = lifted * t;
                sweep = windUp * t;
            }
            else if (phase < impact) {
                pitch = lifted;
                sweep = windUp + (followThrough - windUp) * SmoothStep(windUpEnd, impact, phase);
            }
            else {
                const float t = 1.0f - SmoothStep(impact, 1.0f, phase);
                pitch = lifted * t;
                sweep = followThrough * t;
            }
            return Matrix4::RotationX(pitch) * Matrix4::RotationY(sweep);
        }

        // Red flash on characters that were just hit (tints multiply, so green and blue drop)
        template<typename Shade>
        static Systems::Rendering::Color HitTint(const Shade& shade, float flashSeconds) {
            const float flash = std::clamp(flashSeconds / Encounter::HitFlashSeconds, 0.0f, 1.0f);
            return shade(1.0f, 1.0f - 0.65f * flash, 1.0f - 0.65f * flash);
        }

        // Placeholder hero: a body, a head, and a visor showing where they face (or the knight model)
        template<typename Shade>
        void DrawPlayer(const Shade& shade) {
            if (!playerId_) return;

            if (models_.loaded) {
                // Feet on the floor, facing the walk direction (glTF characters face +Z)
                using Systems::Rendering::Matrix4;
                const auto* knight = renderingSystem_.GetModel(models_.player);
                const float feet = -knight->boundsMin.y * ModelScale;
                Systems::Rendering::ModelRenderData body{
                    models_.player, { playerPosition_.x, feet, playerPosition_.y }, playerFacingYaw_, ModelScale, HitTint(shade, playerCombat_.hitFlashSeconds) };

                // Down: tip over backwards around the feet, lifted so the back rests on the floor
                if (!IsPlayerAlive()) {
                    const float fall = SmoothStep(0.0f, 0.6f, playerCombat_.deadSeconds);
                    body.transform = Matrix4::RotationX(-1.5f * fall) * Matrix4::Translation({ 0.0f, 0.35f * fall, 0.0f });
                }

                // The sword arm swings at the shoulder; the sword sits in its fist, blade forward
                const auto arm = knight->FindPartIndex(PlayerSwordArmPart);
                const auto armPose = SwordArmPose();
                if (arm) {
                    body.partPoses.push_back({ *arm, armPose });
                    constexpr Systems::Rendering::Vector3 fist{ -0.195f, -0.43f, 0.0f };   // in the arm's space
                    Systems::Rendering::ModelRenderData sword{ HeldWeaponModel(), body.position, body.yawRadians, ModelScale, body.tint };
                    sword.transform = Matrix4::RotationX(std::numbers::pi_v<float> * 0.5f) * Matrix4::Translation(fist)
                                    * armPose * knight->parts[*arm].transform;
                    if (body.transform) *sword.transform = *sword.transform * *body.transform;
                    renderingSystem_.DrawModel(sword);
                }
                renderingSystem_.DrawModel(body);
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

        /// <summary>
        /// Enemies: skinned models posed by their animation, with their weapons in the hand slots
        /// (or placeholder primitives without models). Corpses fade out before they disappear.
        /// </summary>
        template<typename Shade>
        void DrawEnemies(const Shade& shade) {
            const auto modelOf = [this](const std::string& file) {
                const auto it = characterModels_.find(file);
                return it != characterModels_.end() ? it->second : Systems::Rendering::InvalidModelId;
            };

            for (const auto& enemy : encounter_.GetEnemies()) {
                const auto& type = *enemy.definition;
                auto tint = HitTint(shade, enemy.hitFlashSeconds);
                tint.a = Encounter::Opacity(enemy);
                const Systems::Rendering::Vector3 position{ enemy.position.x, 0.0f, enemy.position.y };

                if (models_.loaded) {
                    Systems::Rendering::ModelRenderData body{ modelOf(type.model), position, enemy.facingYaw, type.modelScale, tint };
                    body.animation = enemy.animator.GetState();
                    renderingSystem_.DrawModel(body);

                    for (const auto& [file, slot] : { std::pair{ &type.mainHandModel, "handslot.r" }, std::pair{ &type.offHandModel, "handslot.l" } }) {
                        if (file->empty()) continue;
                        Systems::Rendering::ModelRenderData held{ modelOf(*file), position, enemy.facingYaw, type.modelScale, tint };
                        held.transform = renderingSystem_.GetJointTransform(body.model, slot, body.animation);
                        if (held.transform) renderingSystem_.DrawModel(held);
                    }
                    continue;
                }

                // Placeholder: a bone-colored body and skull, lying flat once dead
                using Systems::Rendering::MeshRenderData;
                using Systems::Rendering::MeshShape;
                const bool dead = enemy.IsDead();
                const float height = dead ? 0.2f : 0.9f;
                const float flash = 1.0f - 0.65f * std::clamp(enemy.hitFlashSeconds / Encounter::HitFlashSeconds, 0.0f, 1.0f);
                auto bone = shade(0.82f, 0.78f * flash, 0.66f * flash);
                bone.a = tint.a;

                MeshRenderData body{};
                body.shape = MeshShape::Cylinder;
                body.position = { position.x, height * 0.5f, position.z };
                body.size = { Encounter::CharacterRadius * 1.6f, height, Encounter::CharacterRadius * 1.6f };
                body.color = bone;
                renderingSystem_.DrawMesh(body);
                if (dead) continue;

                MeshRenderData skull{};
                skull.shape = MeshShape::Sphere;
                skull.position = { position.x, height + 0.18f, position.z };
                skull.size = { 0.4f, 0.4f, 0.4f };
                skull.color = bone;
                renderingSystem_.DrawMesh(skull);
            }
        }

        /// <summary>
        /// HUD: the player's health bar and hit points (bottom left), health bars over hurt enemies,
        /// the wave (top center), and the floating combat texts
        /// </summary>
        void DrawHud() {
            using Systems::Rendering::Color;
            using Systems::Rendering::Rectangle;
            const auto display = [](float r, float g, float b) {
                return Color{ std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), 1.0f };
            };
            const auto bar = [&](std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height, float fraction, Color fill) {
                renderingSystem_.DrawRectangle(Rectangle{ x - 2, y - 2, width + 4, height + 4 }, display(0.05f, 0.04f, 0.04f), 10);
                renderingSystem_.DrawRectangle(Rectangle{ x, y, width, height }, display(0.22f, 0.06f, 0.06f), 11);
                const auto filled = static_cast<std::int32_t>(std::lround(static_cast<float>(width) * std::clamp(fraction, 0.0f, 1.0f)));
                if (filled > 0) renderingSystem_.DrawRectangle(Rectangle{ x, y, filled, height }, fill, 12);
            };

            std::int32_t screenWidth = 0;
            std::int32_t screenHeight = 0;
            renderingSystem_.GetScreenSize(screenWidth, screenHeight);

            const auto label = [&](std::string text, std::int32_t x, std::int32_t y, float scale, Color color, std::int32_t layer) {
                renderingSystem_.DrawText(Systems::Rendering::TextRenderData{ std::move(text), LB::Position{ x, y }, {}, color, scale, layer, true });
            };

            if (playerId_) {
                if (const auto status = characterService_.GetCharacterStatus(*playerId_)) {
                    bar(24, screenHeight - 44, 240, 18, static_cast<float>(status->healthPercentage()), display(0.80f, 0.14f, 0.12f));
                    label(std::format("HP {}/{}", status->currentHitPoints, status->maxHitPoints), 30, screenHeight - 42, 2.0f, display(1.0f, 1.0f, 1.0f), 13);

                    // Experience toward the next level, a thin gold bar under the health bar
                    const float previous = static_cast<float>(characterService_.GetExperienceForLevel(status->level));
                    const float next = static_cast<float>(status->experienceToNextLevel);
                    const float progress = next > previous ? (static_cast<float>(status->currentExperience) - previous) / (next - previous) : 1.0f;
                    bar(24, screenHeight - 20, 240, 5, progress, display(0.95f, 0.75f, 0.30f));
                    label(std::format("LEVEL {}", status->level), 24, screenHeight - 70, 2.0f, display(0.95f, 0.75f, 0.30f), 13);

                    label(std::format("GOLD {}", gold_), 280, screenHeight - 42, 2.0f, display(0.95f, 0.80f, 0.35f), 13);
                    label(std::format("POTIONS {}", CountItem(HealingPotion)), 280, screenHeight - 22, 2.0f, display(0.95f, 0.45f, 0.40f), 13);
                    if (characterService_.GetCharacter(*playerId_)->get().GetUnspentAttributePoints() > 0) {
                        label("POINTS TO SPEND (I)", 24, screenHeight - 92, 2.0f, display(0.55f, 0.95f, 0.55f), 13);
                    }
                }
            }

            for (const auto& enemy : encounter_.GetEnemies()) {
                if (!enemy.IsTargetable() || enemy.hitPoints >= enemy.maxHitPoints) continue;
                float top = 1.4f;
                if (models_.loaded) {
                    const auto it = characterModels_.find(enemy.definition->model);
                    if (it != characterModels_.end()) top = renderingSystem_.GetModel(it->second)->boundsMax.y * enemy.definition->modelScale + 0.1f;
                }
                const auto point = renderingSystem_.WorldToScreen({ enemy.position.x, top, enemy.position.y });
                if (!point.inFront) continue;
                const float fraction = static_cast<float>(enemy.hitPoints) / static_cast<float>(enemy.maxHitPoints);
                bar(static_cast<std::int32_t>(point.x) - 22, static_cast<std::int32_t>(point.y) - 8, 44, 5, fraction, display(0.85f, 0.20f, 0.15f));
            }

            if (encounter_.GetWave() > 0 || depth_ > 0) {
                const auto wave = depth_ > 0 ? std::format("DEPTH {}", depth_) : std::format("WAVE {}", encounter_.GetWave());
                const auto size = renderingSystem_.MeasureText(wave, 3.0f);
                label(wave, screenWidth / 2 - static_cast<std::int32_t>(size.width / 2.0f), 14, 3.0f, display(0.95f, 0.75f, 0.30f), 13);
            }

            // Combat texts: rise with an ease-out and fade over the last 40% of their life
            for (const auto& floating : floatingTexts_) {
                const float t = std::clamp(floating.ageSeconds / FloatingTextSeconds, 0.0f, 1.0f);
                const float rise = FloatingTextRise * (1.0f - (1.0f - t) * (1.0f - t));
                const auto point = renderingSystem_.WorldToScreen({ floating.position.x, floating.position.y + rise, floating.position.z });
                if (!point.inFront) continue;
                auto color = display(floating.color.r, floating.color.g, floating.color.b);
                color.a = 1.0f - SmoothStep(0.6f, 1.0f, t);
                const auto size = renderingSystem_.MeasureText(floating.text, floating.scale);
                label(floating.text, static_cast<std::int32_t>(point.x - size.width / 2.0f), static_cast<std::int32_t>(point.y - size.height),
                      floating.scale, color, 20);
            }
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

        void RegisterGamepadBinding(std::string_view action, Systems::Input::GamepadButton button) {
            Systems::Input::InputAction binding{};
            binding.name = std::string(action);
            binding.deviceType = Systems::Input::InputDevice::Gamepad;
            binding.gamepadButton = button;
            inputSystem_.RegisterAction(binding);
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

            // Character screen (I, C, gamepad View), quick potion (R, gamepad X: use item), and
            // the screen's navigation (WASD/arrows/D-pad, Enter/Space/A, Esc/B)
            RegisterKeyBinding(CharacterScreenAction, Systems::Input::Keys::FromChar('I'));
            RegisterKeyBinding(CharacterScreenAction, Systems::Input::Keys::FromChar('C'));
            RegisterGamepadBinding(CharacterScreenAction, Systems::Input::GamepadButton::Back);
            RegisterKeyBinding(UsePotionAction, Systems::Input::Keys::FromChar('R'));
            RegisterGamepadBinding(UsePotionAction, Systems::Input::GamepadButton::X);
            RegisterKeyBinding(MenuUpAction, Systems::Input::Keys::FromChar('W'));
            RegisterKeyBinding(MenuUpAction, Systems::Input::Keys::Up);
            RegisterGamepadBinding(MenuUpAction, Systems::Input::GamepadButton::DPadUp);
            RegisterKeyBinding(MenuDownAction, Systems::Input::Keys::FromChar('S'));
            RegisterKeyBinding(MenuDownAction, Systems::Input::Keys::Down);
            RegisterGamepadBinding(MenuDownAction, Systems::Input::GamepadButton::DPadDown);
            RegisterKeyBinding(MenuConfirmAction, Systems::Input::Keys::Enter);
            RegisterKeyBinding(MenuConfirmAction, Systems::Input::Keys::Space);
            RegisterGamepadBinding(MenuConfirmAction, Systems::Input::GamepadButton::A);
            RegisterKeyBinding(MenuBackAction, Systems::Input::Keys::Escape);
            RegisterKeyBinding(MapAction, Systems::Input::Keys::FromChar('M'));
            RegisterGamepadBinding(MapAction, Systems::Input::GamepadButton::RightThumb);
            RegisterGamepadBinding(MenuBackAction, Systems::Input::GamepadButton::B);

            // Primary action (design-ui-ux-and-audio.md 4): left click, gamepad A; Space as well
            RegisterKeyBinding(AttackAction, Systems::Input::Keys::Space);
            Systems::Input::InputAction attackMouse{};
            attackMouse.name = std::string(AttackAction);
            attackMouse.deviceType = Systems::Input::InputDevice::Mouse;
            attackMouse.mouseButton = Systems::Input::MouseButton::Left;
            inputSystem_.RegisterAction(attackMouse);
            Systems::Input::InputAction attackGamepad{};
            attackGamepad.name = std::string(AttackAction);
            attackGamepad.deviceType = Systems::Input::InputDevice::Gamepad;
            attackGamepad.gamepadButton = Systems::Input::GamepadButton::A;
            inputSystem_.RegisterAction(attackGamepad);
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
            Systems::Rendering::ModelId playerWeapon{};
        };
        WorldModels models_;
        std::unordered_map<std::string, Systems::Rendering::ModelId> characterModels_;   // enemy bodies and weapons by file
        EnemyClips enemyClips_{};

        // The player's side of combat
        struct PlayerCombat {
            bool swinging{false};
            float swingSeconds{0.0f};
            bool impactDone{false};                // this swing's strike has been resolved
            std::optional<std::uint32_t> target;   // enemy picked when the swing started
            float hitFlashSeconds{0.0f};
            float deadSeconds{0.0f};               // time spent down
        };
        PlayerCombat playerCombat_;
        const Content::WeaponDefinition* playerWeapon_{nullptr};
        Encounter encounter_;
        std::vector<CombatEvent> combatEvents_;
        std::vector<FloatingText> floatingTexts_;
        Services::Inventory::InventoryService inventoryService_;
        std::int32_t gold_{0};
        std::vector<GroundItem> groundItems_;
        WorldMap world_;
        Services::Dungeon::DungeonService dungeonService_;
        bool dungeonMode_{false};
        std::uint32_t baseSeed_{0};
        std::int32_t depth_{0};
        std::vector<RoomState> rooms_;
        std::vector<Torch> torches_;
        std::vector<std::uint8_t> explored_;
        bool mapExpanded_{false};
        std::uint32_t nextGroundItemId_{1};
        LootRoller lootRoller_;
        CharacterScreen characterScreen_;
        std::uint32_t floatingTextCount_{0};
        bool spawnEnemies_{true};
        std::vector<WallPiece> walls_;
        WorldPoint previousRightStick_{};         // for right stick flick detection
        StepTimer timer_;
        double gameTimeSeconds_{0.0};
        bool paused_{false};
        bool active_{true};
        bool initialized_{false};
    };

} // namespace LB::Engine
