// GameEngine.ixx - Game loop orchestration
//
// GameEngine owns the systems (rendering, input, audio, persistence) and one game run (Session),
// and coordinates them: Tick -> Update (Simulation.cpp) -> Render (WorldView.cpp). Startup,
// shutdown and saves are in EngineSetup.cpp; dungeon floors in DungeonFloor.cpp; the player's
// items and progress in PlayerProgress.cpp; effects in Effects.cpp. Self-contained parts live in
// their own partitions: WallSet, TorchSet, Minimap, EffectsSystem, CombatTextLog, Hud.
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
import :Particles;
import :Effects;
import :Walls;
import :Torches;
import :Minimap;
import :CombatText;
import :Hud;
import :Sounds;
import :Music;
import :Menus;
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
        // Sound effects (SoundBank's .ogg clips); empty: the game is silent
        std::string soundDirectory;
        // Music (MusicTracks); empty: no music
        std::string musicDirectory;
        // Open the title menu at startup (the game); off: straight into play (tests, tools)
        bool titleScreen{false};
        // Waves of enemies from enemies.json fight the player (needs content with a player)
        bool spawnEnemies{true};
        // Fixed seed for dice, spawn points, and dungeon floors (tests, replays); empty: random
        std::optional<std::uint32_t> randomSeed;
        // Generated dungeon floors (rooms, corridors, a trapdoor to the next floor); off: the
        // placeholder test arena with waves of enemies
        bool dungeon{false};
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

        /// <summary>
        /// The HUD and menus are laid out for the default window size and scaled with the window:
        /// as large as fits both ways (the layout keeps its proportions)
        /// </summary>
        [[nodiscard]] static float UiScaleFor(std::int32_t width, std::int32_t height) noexcept {
            const float byWidth = static_cast<float>(width) / static_cast<float>(DefaultWindowWidth);
            const float byHeight = static_cast<float>(height) / static_cast<float>(DefaultWindowHeight);
            return std::max(0.5f, std::min(byWidth, byHeight));
        }

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
        static constexpr std::string_view MenuLeftAction = "MenuLeft";
        static constexpr std::string_view MenuRightAction = "MenuRight";

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

        // Torches on room walls (TorchSet), and how dark the dungeon is without them (the
        // directional lights' share)
        static constexpr std::string_view TorchModel = "torchWall.glb";
        static constexpr float TorchLightRadius = TorchSet::LightRadius;
        static constexpr float TorchFireRange = TorchSet::FireRange;
        static constexpr float DungeonDirectionalLight = 0.3f;

        // Damage numbers rise and fade over this time (CombatTextLog)
        static constexpr float FloatingTextSeconds = CombatTextLog::Seconds;

        // Walls between the camera and the player fade (WallSet)
        static constexpr float FadedWallAlpha = WallSet::FadedAlpha;
        static constexpr float WallFadeSeconds = WallSet::FadeSeconds;

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
        bool Initialize(void* nativeWindow, std::int32_t width, std::int32_t height, const EngineOptions& options = {});

        /// <summary>
        /// Shut down all systems. Safe to call more than once.
        /// </summary>
        void Shutdown();

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
        [[nodiscard]] const Services::Character::CharacterService& GetCharacters() const noexcept { return session_->characterService; }
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
        [[nodiscard]] std::optional<LB::Character::CharacterId> GetPlayerId() const noexcept { return session_->playerId; }

        /// <summary>
        /// Player position in world tiles
        /// </summary>
        [[nodiscard]] WorldPoint GetPlayerPosition() const noexcept { return session_->playerPosition; }

        /// <summary>
        /// Where the player faces: radians around the vertical axis, 0 faces +Z (south)
        /// </summary>
        [[nodiscard]] float GetPlayerFacingYaw() const noexcept { return session_->playerFacingYaw; }

        /// <summary>
        /// True when gamepads are read from the platform (GameInput runtime available)
        /// </summary>
        [[nodiscard]] bool HasGamepadSupport() const noexcept { return inputSystem_.HasDeviceInput(); }

        /// <summary>
        /// Tactical pause (design-combat-system.md): the loop keeps running and reading input,
        /// but the simulation and the game clock stop.
        /// </summary>
        [[nodiscard]] bool IsPaused() const noexcept { return session_->paused; }

        /// <summary>
        /// Simulated time, excluding time spent paused
        /// </summary>
        [[nodiscard]] double GetGameTimeSeconds() const noexcept { return session_->gameTimeSeconds; }

        /// <summary>
        /// The walkable world: the test arena, or the current dungeon floor
        /// </summary>
        [[nodiscard]] const WorldMap& GetWorld() const noexcept { return session_->world; }

        /// <summary>
        /// The dungeon floor the player is on (1 for the first; 0 in the test arena)
        /// </summary>
        [[nodiscard]] std::int32_t GetDepth() const noexcept { return session_->depth; }

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

        [[nodiscard]] const std::vector<RoomState>& GetRoomStates() const noexcept { return session_->rooms; }

        using Torch = LB::Engine::Torch;

        /// <summary>
        /// The torches on the room walls of the current floor
        /// </summary>
        [[nodiscard]] const std::vector<Torch>& GetTorches() const noexcept { return session_->torches.Torches(); }

        /// <summary>
        /// The menus: the title screen, the pause menu, options, and credits (any open menu pauses
        /// the game)
        /// </summary>
        [[nodiscard]] const GameMenu& GetMenu() const noexcept { return menu_; }

        /// <summary>
        /// The player chose Quit: the host should close the game
        /// </summary>
        [[nodiscard]] bool IsQuitRequested() const noexcept { return quitRequested_; }

        /// <summary>
        /// The credits (credits.txt in the content directory), one line each
        /// </summary>
        [[nodiscard]] const std::vector<std::string>& GetCredits() const noexcept { return credits_; }

        /// <summary>
        /// How the music is mixed (the calm and combat layers)
        /// </summary>
        [[nodiscard]] const MusicDirector& GetMusic() const noexcept { return session_->music; }

        /// <summary>
        /// Particles in the world: torch fire, hit sparks, dust, sparkles
        /// </summary>
        [[nodiscard]] const ParticleField& GetParticles() const noexcept { return session_->effects.Particles(); }

        /// <summary>
        /// Has the player seen this floor cell? (what the minimap shows)
        /// </summary>
        [[nodiscard]] bool IsCellExplored(std::int32_t x, std::int32_t y) const noexcept { return session_->minimap.IsExplored(x, y); }

        /// <summary>
        /// The minimap shown large (M or the right stick click toggles it)
        /// </summary>
        [[nodiscard]] bool IsMapExpanded() const noexcept { return session_->minimap.IsExpanded(); }

        /// <summary>
        /// Go down to the next floor (normally by stepping on the trapdoor of a cleared exit room)
        /// </summary>
        void Descend();

        /// <summary>
        /// The enemies and waves of the arena
        [[nodiscard]] const Encounter& GetEncounter() const noexcept { return session_->encounter; }

        /// <summary>
        /// Attacks resolved during the last simulation step (the player's and the enemies')
        /// </summary>
        [[nodiscard]] const std::vector<CombatEvent>& GetCombatEvents() const noexcept { return session_->combatEvents; }

        /// <summary>
        /// Damage numbers and misses currently floating over characters
        /// </summary>
        [[nodiscard]] const std::vector<FloatingText>& GetFloatingTexts() const noexcept { return session_->combatText.Texts(); }

        /// <summary>
        /// True while there is a player with hit points left
        /// </summary>
        [[nodiscard]] bool IsPlayerAlive() const {
            if (!session_->playerId) return false;
            const auto character = session_->characterService.GetCharacter(*session_->playerId);
            return character && character->get().IsAlive();
        }

        /// <summary>
        /// True while the player's weapon swing plays (attacks repeat while Attack is held)
        /// </summary>
        [[nodiscard]] bool IsPlayerAttacking() const noexcept { return session_->playerCombat.swinging; }

        /// <summary>
        /// The player's weapon attack (design-combat-system.md 4.1, 4.3): class attack bonus with the
        /// weapon's category, the weapon's dice, and its scaling attribute's modifier
        /// </summary>
        [[nodiscard]] Services::Combat::WeaponAttackProfile GetPlayerAttackProfile() const {
            Services::Combat::WeaponAttackProfile attack{};
            if (!session_->playerId || !session_->playerWeapon) return attack;
            const auto character = session_->characterService.GetCharacter(*session_->playerId);
            if (!character) return attack;

            const auto& weapon = *session_->playerWeapon;
            const auto& attributes = character->get().GetAttributes();
            const std::int32_t modifier = weapon.attributeScale == "DEX" ? attributes.GetDexterityModifier()
                                        : weapon.attributeScale == "STR" ? attributes.GetStrengthModifier() : 0;
            attack.attackBonus = session_->characterService.GetAttackBonus(*session_->playerId, weapon.category).value_or(0);
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
            if (!session_->playerId) return defense;
            const auto character = session_->characterService.GetCharacter(*session_->playerId);
            if (!character) return defense;

            defense.armorClass = character->get().GetArmorClass() + character->get().GetAttributes().GetDexterityModifier();
            for (const auto& piece : session_->characterService.GetEquippedArmor(*session_->playerId)) {
                if (const auto* armor = content_->FindArmor(piece.armorId)) {
                    defense.physicalArmor += armor->physicalAR + armor->armorBonus;
                }
            }
            return defense;
        }

        /// <summary>
        /// Gold carried
        /// </summary>
        [[nodiscard]] std::int32_t GetGold() const noexcept { return session_->gold; }

        /// <summary>
        /// The bag (unequipped items); item ids are packed ItemRefs
        /// </summary>
        [[nodiscard]] std::vector<Inventory::InventoryItem> GetInventory() const {
            return session_->playerId ? session_->inventoryService.GetInventory(*session_->playerId) : std::vector<Inventory::InventoryItem>{};
        }

        /// <summary>
        /// How many of an item (by content key) the bag holds
        /// </summary>
        [[nodiscard]] std::int32_t CountItem(std::string_view key) const;

        /// <summary>
        /// The weapon the player attacks with
        /// </summary>
        [[nodiscard]] const Content::WeaponDefinition* GetPlayerWeapon() const noexcept { return session_->playerWeapon; }

        /// <summary>
        /// Gold and items lying on the floor, waiting to be picked up
        /// </summary>
        [[nodiscard]] const std::vector<GroundItem>& GetGroundItems() const noexcept { return session_->groundItems; }

        /// <summary>
        /// The character and inventory screen (I, C, or the gamepad View button)
        /// </summary>
        [[nodiscard]] const CharacterScreen& GetCharacterScreen() const noexcept { return session_->characterScreen; }

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
            return session_->encounter.Spawn(*type, at, std::atan2(session_->playerPosition.x - at.x, session_->playerPosition.y - at.y), false);
        }

        /// <summary>
        /// Put an armour piece from armour.json on the player (replacing the piece in its slot).
        /// Returns false without a player or for an unknown key.
        /// </summary>
        bool EquipPlayerArmor(std::string_view armorKey) {
            const auto* armor = content_->FindArmor(armorKey);
            if (!session_->playerId || !armor) return false;

            Services::Character::ArmorPiece piece{};
            piece.armorId = armor->id;
            piece.slot = armor->slot;
            piece.category = armor->category;
            piece.movementPenalty = armor->movementPenalty;
            piece.castPenalty = armor->castPenalty;
            return session_->characterService.EquipArmor(*session_->playerId, piece);
        }

        /// <summary>
        /// Opacity of each wall piece (1 opaque, FadedWallAlpha while hiding the player); empty
        /// when the world is drawn with primitives
        /// </summary>
        [[nodiscard]] std::vector<float> GetWallOpacities() const {
            std::vector<float> opacities;
            for (const auto& wall : session_->walls.Pieces()) opacities.push_back(wall.opacity);
            return opacities;
        }

        /// <summary>
        /// The isometric camera following the player
        /// </summary>
        [[nodiscard]] const Systems::Rendering::IsometricCamera& GetCamera() const noexcept {
            return session_->camera;
        }

        /// <summary>
        /// Save the game state to a slot
        /// </summary>
        bool SaveGame(std::int32_t slotIndex);

        /// <summary>
        /// Restore the game state from a slot. Returns false (state unchanged) if the slot is
        /// missing or unreadable.
        /// </summary>
        bool LoadGame(std::int32_t slotIndex);

    private:
        // Display (sRGB) colors for the frame, dimmed while paused, handed to the renderer linear
        struct Shade {
            float dim{1.0f};
            [[nodiscard]] Systems::Rendering::Color operator()(float r, float g, float b) const {
                const auto linear = [this](float c) { return std::pow(c * dim, 2.2f); };
                return Systems::Rendering::Color{ linear(r), linear(g), linear(b), 1.0f };
            }
        };

        // Startup, shutdown, preferences, input bindings (EngineSetup.cpp)
        bool StartRun();
        bool LoadContent(const std::string& contentDirectory);
        bool LoadModels(const std::string& modelDirectory);
        bool LoadFonts(const std::string& fontDirectory);
        void ResolveEnemyClips(std::vector<std::string>& errors);
        void ConfigureClassRules();
        bool SpawnPlayer();
        void SetPaused(bool paused);
        void ApplyPreferences(const Systems::Persistence::PlayerPreferences& prefs);
        void SaveVolumePreferences();
        static bool ParseDouble(const Systems::Persistence::SaveData& data, const std::string& key, double& value);
        void RegisterGamepadBinding(std::string_view action, Systems::Input::GamepadButton button);
        void RegisterKeyBinding(std::string_view action, std::int32_t keyCode);
        void RegisterDefaultBindings();
        void OnDeviceLost() {}       // nothing device-dependent is held by the game yet
        void OnDeviceRestored() {}

        // One simulation step (Simulation.cpp)
        void Update(double elapsedSeconds);
        void UpdateCameraControls();
        void UpdatePlayerCombat(float elapsedSeconds);
        void MovePlayer(float elapsedSeconds);
        void UpdateEncounter(float elapsedSeconds);
        void UpdateCombatTexts(float elapsedSeconds);
        void StartEncounter();
        void RespawnPlayer();

        // Drawing a frame (WorldView.cpp)
        void Render();
        void DrawArena(const Shade& shade);
        void DrawArenaModels(const Shade& shade);
        void DrawPlayer(const Shade& shade);
        void DrawEnemies(const Shade& shade);
        void DrawHud();
        Systems::Rendering::Matrix4 SwordArmPose() const noexcept;
        static Systems::Rendering::Color HitTint(const Shade& shade, float flashSeconds);
        float EnemyTop(const Enemy& enemy) const;
        float PlayerTop() const;
        // A loaded prop, character, or weapon model by file (invalid when not loaded)
        Systems::Rendering::ModelId ModelOf(std::string_view file) const;

        // Dungeon floors (DungeonFloor.cpp)
        void StartFloor(std::int32_t depth);
        void UpdateRooms();
        void UpdateFloorInteractions();
        void DrawFloorProps(const Systems::Rendering::Color& tint);
        void DrawMinimap();
        void WriteFloor(Systems::Persistence::SaveData& data) const;
        void ReadFloor(const Systems::Persistence::SaveData& data);

        // Menus (GameMenus.cpp)
        void LoadCredits(const std::string& contentDirectory);
        void OpenMenu(MenuPage page);
        void CloseMenu();
        std::vector<MenuItem> BuildMenuItems() const;
        void UpdateMenu();
        void AdjustMenuItem(MenuCommand command, int direction);
        void ChooseMenuItem(MenuCommand command);
        void DrawMenu();

        // Sounds (GameSounds.cpp): `at` places a sound in the world (heard from the player)
        bool LoadSounds(const std::string& soundDirectory);
        bool LoadMusic(const std::string& musicDirectory);
        void StartMusic();
        void UpdateMusic(float elapsedSeconds);
        void PlaySound(GameSound sound, std::optional<WorldPoint> at = std::nullopt);
        void PlayCombatSounds();
        void UpdateSounds(float elapsedSeconds, bool running);

        // When and where effects play (Effects.cpp)
        void EmitCombatEffects();
        void EmitHealingEffect();
        void EmitLevelUpEffect();
        void EmitChestEffect(WorldPoint chest);
        void UpdateEffects(float elapsedSeconds, bool running);

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
        void WriteProgress(Systems::Persistence::SaveData& data) const;
        void ReadProgress(const Systems::Persistence::SaveData& data);
        std::string CanEquip(ItemRef item) const;

        // The player's character; null before Initialize, without class content, or if it is gone
        [[nodiscard]] const LB::Character::Character* PlayerCharacter() const {
            if (!session_->playerId) return nullptr;
            const auto character = session_->characterService.GetCharacter(*session_->playerId);
            return character ? &character->get() : nullptr;
        }

        [[nodiscard]] float PlayerAttackSeconds() const noexcept {
            return session_->playerWeapon ? session_->playerWeapon->attackTimeSeconds : 1.0f;
        }

        // The player's body keeps this far (tiles) from walls
        static constexpr float PlayerRadius = PlayerSizeTiles * 0.5f;

        static LB::Position ToTile(WorldPoint point) noexcept {
            return LB::Position{ static_cast<std::int32_t>(std::floor(point.x)), static_cast<std::int32_t>(std::floor(point.y)) };
        }

        Systems::Rendering::RenderingSystem renderingSystem_;
        Systems::Input::InputSystem inputSystem_;
        Systems::Audio::AudioSystem audioSystem_;
        Systems::Persistence::PersistenceSystem persistenceSystem_;
        std::shared_ptr<const Content::ContentDatabase> content_ = std::make_shared<Content::ContentDatabase>();
        Services::Dungeon::DungeonService dungeonService_;
        std::string initializationError_;

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

        /// <summary>
        /// Handles to what the renderer loaded for this run: world pieces, characters and props by
        /// file, and the enemies' animation clips. Dropped as a whole on Shutdown.
        /// </summary>
        struct StringHash {
            using is_transparent = void;
            [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept { return std::hash<std::string_view>{}(text); }
        };
        struct Assets {
            WorldModels models;
            std::unordered_map<std::string, Systems::Rendering::ModelId, StringHash, std::equal_to<>> characterModels;   // enemy bodies, weapons, props by file
            EnemyClips enemyClips{};
            bool soundsLoaded{false};
            bool musicLoaded{false};
        };

        // The player's side of combat
        struct PlayerCombat {
            bool swinging{false};
            float swingSeconds{0.0f};
            bool impactDone{false};                // this swing's strike has been resolved
            std::optional<std::uint32_t> target;   // enemy picked when the swing started
            float hitFlashSeconds{0.0f};
            float deadSeconds{0.0f};               // time spent down
        };

        /// <summary>
        /// Everything one game run owns, from Initialize to Shutdown: the player, the world and
        /// its floor, enemies, loot, effects, and the clock. Shutdown replaces it with a fresh one,
        /// so state added later cannot leak from one run into the next.
        /// </summary>
        struct Session {
            Services::Character::CharacterService characterService;
            std::optional<LB::Character::CharacterId> playerId;
            WorldPoint playerPosition{};
            float playerFacingYaw{0.0f};             // radians around the vertical axis; 0 faces +Z (south)
            PlayerCombat playerCombat;
            const Content::WeaponDefinition* playerWeapon{nullptr};   // into content_, which outlives the session
            Services::Inventory::InventoryService inventoryService;
            std::int32_t gold{0};
            std::vector<GroundItem> groundItems;
            std::uint32_t nextGroundItemId{1};
            LootRoller lootRoller;
            CharacterScreen characterScreen;

            Encounter encounter;
            bool spawnEnemies{true};
            std::vector<CombatEvent> combatEvents;
            CombatTextLog combatText;

            WorldMap world;
            WallSet walls;
            bool dungeonMode{false};
            std::uint32_t baseSeed{0};
            std::int32_t depth{0};
            std::vector<RoomState> rooms;
            TorchSet torches;
            Minimap minimap;

            EffectsSystem effects;
            SoundBank sounds;
            WorldPoint soundPlayerPosition{};        // where the player was last step (footsteps)
            float strideTiles{0.0f};                 // walked since the last footstep
            std::unordered_map<std::uint32_t, EnemyState> enemyStatesHeard;   // last step's, for state sounds
            MusicDirector music;
            std::int32_t calmMusic{-1};              // the playing layers (audio source ids)
            std::int32_t combatMusic{-1};
            WorldPoint effectsPlayerPosition{};      // where the player was last step (footstep dust)

            Systems::Rendering::IsometricCamera camera;
            WorldPoint previousRightStick{};         // for right stick flick detection
            double gameTimeSeconds{0.0};
            bool paused{false};
        };

        Assets assets_;
        EngineOptions options_;                     // what Initialize was given (new runs start from it)
        GameMenu menu_;                             // spans runs: New Game and Quit to Title start new ones
        bool quitRequested_{false};
        std::vector<std::string> credits_;
        std::unique_ptr<Session> session_ = std::make_unique<Session>();   // on the heap: it is large (~20 KB)
        StepTimer timer_;
        bool active_{true};
        bool initialized_{false};
    };

} // namespace LB::Engine
