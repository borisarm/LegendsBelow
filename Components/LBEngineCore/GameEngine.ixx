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
import :Projectiles;
import :Statuses;
import :Decor;
import :GameEvents;
import :Allies;
import :Ceremony;
import :Features;
import :Legacy;
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
        // Images that frame the menus (MenuArt); empty: plain panels
        std::string uiDirectory;
        // Waves of enemies from enemies.json fight the player (needs content with a player)
        bool spawnEnemies{true};
        // Fixed seed for dice, spawn points, and dungeon floors (tests, replays); empty: random
        std::optional<std::uint32_t> randomSeed;
        // Generated dungeon floors (rooms, corridors, a trapdoor to the next floor); off: the
        // placeholder test arena with waves of enemies
        bool dungeon{false};
        // The hero's class (classes.json): Warrior or Mage are playable; New Game asks
        LB::Character::CharacterClass playerClass{LB::Character::CharacterClass::Warrior};
        // Heavy blows freeze the action for a moment (hit-stop); off: steps are never skipped (tests count them)
        bool hitStop{false};
        // A fallen hero sees the run's summary and chooses to rise again; off: they get up at once (tests, tools)
        bool deathScreen{false};
        // Save the run on its own (a new game, each floor down, quitting, closing, suspending) so
        // Continue picks it up; off: only Save Game and the quick save write saves (tests)
        bool autoSave{false};
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
        // Talk to the merchant: F, gamepad d-pad up
        static constexpr std::string_view InteractAction = "Interact";
        static constexpr std::string_view DropAction = "Drop";               // the character screen: drop the bag item under the cursor
        static constexpr std::string_view AllyStanceAction = "AllyStance";   // the party: follow, or hold here (G, d-pad down)
        static constexpr std::string_view AllyFocusAction = "AllyFocus";     // the party: after the nearest enemy (T, d-pad right)
        // The tactical pause (design-combat-system.md 2.1): Tab, gamepad left stick click
        static constexpr std::string_view TacticalPauseAction = "TacticalPause";
        // Orders: right click (the secondary action, design-ui-ux-and-audio.md 4.1); the target
        // under the mouse is the enemy whose body is this close to the floor point under it
        static constexpr std::string_view OrderAction = "Order";
        static constexpr float PickRadius = 0.8f;
        static constexpr float ArriveTiles = 0.25f;             // a move order is done this close
        // The spell bar's slots (keys 1-4; gamepad LB, RB, Y, B)
        static constexpr std::array<std::string_view, 4> SpellActions = { "Spell1", "Spell2", "Spell3", "Spell4" };
        static constexpr std::size_t SpellSlots = SpellActions.size();

        /// <summary>
        /// The actions a player may bind to other keys and buttons on the Controls page (moving,
        /// the menus' keys, and Esc / Start stay as they are)
        /// </summary>
        static constexpr std::array<std::string_view, 18> RemappableActions = {
            InteractAction, MapAction, RotateCameraLeftAction, RotateCameraRightAction, AttackAction,
            SpellActions[0], SpellActions[1], SpellActions[2], SpellActions[3], UsePotionAction, AllyStanceAction, AllyFocusAction,
            TacticalPauseAction, CharacterScreenAction, DropAction, QuickSaveAction, QuickLoadAction, OrderAction };
        // The Controls page's last row: every binding back to the defaults
        static constexpr std::string_view ResetControlsRow = "ResetControls";
        // A spell's release pose (Ranged_Magic_Shoot) plays this long after it leaves
        static constexpr float SpellReleaseSeconds = 0.45f;

        // Right stick flicks past this deflection rotate (X) or zoom (Y) the camera
        static constexpr float CameraStickThreshold = 0.6f;

        // Save slot used by quick save/load (F5/F9)
        static constexpr std::int32_t QuickSaveSlot = 0;
        static constexpr std::int32_t AutoSaveSlot = 1;
        // The slots Save Game writes to (the player's own: "Slot 1" to "Slot 3")
        static constexpr std::array<std::int32_t, 3> SaveSlots = { 2, 3, 4 };

        /// <summary>
        /// A save's summary for the slot pages ("Warrior, level 5, depth 7 - 12:34"); empty: no save there
        /// </summary>
        [[nodiscard]] std::string SlotSummary(std::int32_t slot) const;

        /// <summary>
        /// The records of every run and the boons they unlock (the profile)
        /// </summary>
        [[nodiscard]] const Legacy& GetLegacy() const noexcept { return legacy_; }
        static constexpr float SavedNoticeSeconds = 2.0f;   // "Saved" shows this long after an autosave

        /// <summary>
        /// Save the run in the autosave slot (EngineOptions::autoSave): not behind the title screen,
        /// not while the hero is down. Returns whether it saved.
        /// </summary>
        bool AutoSave();

        /// <summary>
        /// The newest save, of any slot (what Continue and the death screen load); none: no save yet
        /// </summary>
        [[nodiscard]] std::optional<std::int32_t> LatestSaveSlot() const;

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
        // The player: the KayKit Adventurers knight on the Rig_Medium (the skeletons' rig and
        // clips), at the enemies' scale, holding the equipped weapon (and shield) in its hand slots
        static constexpr std::string_view PlayerModel = "Knight.glb";
        static constexpr std::string_view MageModel = "Mage.glb";
        static constexpr std::string_view RogueModel = "Rogue_Hooded.glb";
        static constexpr std::string_view ClericModel = "Cleric.gltf";   // the knight in its gold texture (knight_texture_alt_B)
        // After a shadowstep, blows within this long are sneak attacks
        static constexpr float SneakWindowSeconds = 2.0f;
        // Struck from behind (flanked): from more than ~105 degrees off the way the enemy faces
        static constexpr float FlankCosine = -0.25f;
        static constexpr float PlayerModelScale = 0.42f;
        // The sword lying on the floor as loot (the knight holds HeldModels)
        static constexpr std::string_view PlayerWeaponModel = "sword_common.glb";
        // Weapons and the shield in the knight's hands, and the arrows and bolts it shoots
        // (KayKit Adventurers; the mace is the skeletons')
        static constexpr std::string_view HeldSword = "sword_1handed.gltf";
        static constexpr std::string_view HeldShield = "shield_round.gltf";
        static constexpr std::string_view ArrowModel = "arrow_bow.gltf";
        static constexpr std::string_view BoltModel = "arrow_crossbow.gltf";
        static constexpr std::array<std::string_view, 11> HeldModels = {
            HeldSword, "axe_1handed.gltf", "dagger.gltf", "Skeleton_Mace.gltf", "staff.gltf", "wand.gltf", "bow_withString.gltf",
            "crossbow_1handed.gltf", HeldShield, ArrowModel, BoltModel };
        // Held like a pistol (see HeldTransform)
        static constexpr std::array<std::string_view, 2> PistolGripModels = { "crossbow_1handed.gltf", "Skeleton_Crossbow.gltf" };
        // Font of the HUD and the damage numbers (Assets/Fonts, SIL Open Font License)
        static constexpr std::string_view UiFont = "MedievalSharp-Regular.ttf";
        // Props for loot on the floor and the weapon categories the knight can hold
        static constexpr std::array<std::string_view, 10> PropModels = {
            "lootSackA.glb", "coinsSmall.glb", "coinsMedium.glb", "coinsLarge.glb", "axe_common.glb",
            "dagger_common.glb", "hammer_common.glb", "staff_common.glb", "crossbow_common.glb", "shield_common.glb" };
        // Animation sets of the skinned characters (KayKit Rig_Medium), in the model directory
        static constexpr std::array<std::string_view, 5> AnimationFiles = {
            "Animations/Rig_Medium_General.glb", "Animations/Rig_Medium_MovementBasic.glb", "Animations/Rig_Medium_CombatMelee.glb",
            "Animations/Rig_Medium_CombatRanged.glb", "Animations/Rig_Medium_MovementAdvanced.glb" };
        // Rig_Large animation sets (the golem's), loaded with clip names under "Large/"
        static constexpr std::array<std::string_view, 3> LargeAnimationFiles = {
            "Animations/Rig_Large_General.glb", "Animations/Rig_Large_MovementBasic.glb", "Animations/Rig_Large_CombatMelee.glb" };
        static constexpr std::string_view LargeClipPrefix = "Large/";
        // Run cycles are authored for this ground speed (tiles per second, at PlayerModelScale)
        static constexpr float RunAnimationSpeed = 4.0f;

        // Combat in the test arena: the player's starting weapon (weapons.json) and the enemies of
        // the waves (enemies.json)
        static constexpr std::string_view StartingWeapon = "weapon_sword_rusted_shortsword_t1";
        static constexpr std::string_view LightEnemy = "enemy_skeleton_minion";
        static constexpr std::string_view HeavyEnemy = "enemy_skeleton_warrior";
        static constexpr std::string_view RangedEnemy = "enemy_skeleton_rogue";
        static constexpr std::string_view CasterEnemy = "enemy_skeleton_mage";
        static constexpr std::string_view BruteEnemy = "enemy_skeleton_golem";
        static constexpr std::string_view BossEnemy = "enemy_necromancer";
        // The bosses of the tiers, in turn (design-dungeon-and-world.md 7.1)
        static constexpr std::array<std::string_view, 3> BossEnemies = { "enemy_necromancer", "enemy_golem_warlord", "enemy_frost_golem" };

        /// <summary>
        /// Who fills a tier's rooms (design-dungeon-and-world.md 5: each theme its inhabitants): the
        /// crypts' skeletons; the caves' werewolves, orcs, and witches; the ruins' vampires,
        /// tieflings, and black knights (skeletons still shoot in all of them)
        /// </summary>
        struct Roster {
            std::string_view light;
            std::string_view heavy;
            std::string_view ranged;
            std::string_view caster;
            std::string_view brute;
        };
        static constexpr std::array<Roster, 3> Rosters{ {
            { "enemy_skeleton_minion", "enemy_skeleton_warrior", "enemy_skeleton_rogue", "enemy_skeleton_mage", "enemy_skeleton_golem" },
            { "enemy_werewolf", "enemy_orc_raider", "enemy_skeleton_rogue", "enemy_witch", "enemy_orc_brute" },
            { "enemy_vampire", "enemy_skeleton_warrior", "enemy_skeleton_rogue", "enemy_tiefling", "enemy_black_knight" },
        } };

        [[nodiscard]] static constexpr const Roster& RosterForDepth(std::int32_t depth) noexcept {
            return Rosters[static_cast<std::size_t>(std::max(0, depth - 1) / BossFloorInterval) % Rosters.size()];
        }

        /// <summary>
        /// The boss at the end of a depth's tier
        /// </summary>
        [[nodiscard]] static constexpr std::string_view BossForDepth(std::int32_t depth) noexcept {
            return BossEnemies[static_cast<std::size_t>(std::max(0, depth - 1) / BossFloorInterval) % BossEnemies.size()];
        }
        // A boss guards the exit at the end of each tier of floors (design-dungeon-and-world.md 7.1-7.2)
        static constexpr std::int32_t BossFloorInterval = 3;
        // The floor after a boss is a resting place (design-dungeon-and-world.md 7.3): no enemies, a
        // shrine that restores the hero once, and a merchant
        static constexpr std::string_view MerchantModel = "Engineer.glb";
        static constexpr std::string_view ShrineModel = "pillar_decorated.gltf";
        static constexpr std::string_view CandlesModel = "candle_triple.gltf";
        static constexpr std::string_view MerchantStockTable = "stock_merchant";
        static constexpr std::int32_t MerchantGearCount = 6;
        static constexpr std::int32_t MerchantArrowBundle = 20;
        static constexpr float SellFraction = 0.25f;          // of an item's value, what the merchant pays
        static constexpr float MerchantRadius = 1.6f;         // close enough to trade
        static constexpr float PlayerReach = 1.4f;              // tiles between centers
        // A bow or crossbow shot leaves this far through the attack time (drawn, or aimed); the
        // player stands still until then
        static constexpr float ShotReleaseFraction = 0.5f;
        // The Warrior's starting bag holds this bow (classes.json startingItems), to switch to on the
        // character screen
        static constexpr std::string_view StartingBow = "weapon_bow_worn_shortbow_t1";
        static constexpr float PlayerRespawnSeconds = 3.0f;     // after going down

        // Loot: the potion the quick-use button drinks, the bag size, and how close the player
        // walks to pick things up
        static constexpr std::string_view HealingPotion = "consumable_potion_healing_minor";
        static constexpr std::int32_t StartingPotions = 2;
        static constexpr std::int32_t BagCapacity = 20;
        static constexpr float PickupRadius = 0.7f;

        // Dungeon floors: props, how close the player steps to open a chest or take the trapdoor,
        // and the loot table of treasure chests
        static constexpr std::string_view ChestModel = "chest_common.glb";
        static constexpr std::string_view OpenChestModel = "chest_common_empty.glb";
        static constexpr std::string_view TrapdoorModel = "trapdoor.glb";
        static constexpr std::string_view ChestLootTable = "loot_chest";
        // Behind a gate or a puzzle: a greater chest, rolled this many times, its finds as rare as
        // this many floors deeper
        static constexpr std::int32_t GreaterChestRolls = 3;
        static constexpr std::int32_t GreaterChestDepthBonus = 4;
        static constexpr std::string_view GreaterChestModel = "chest_large_gold.gltf";
        static constexpr std::string_view KeyItem = "consumable_key_iron";
        // The item icons in the UI directory (icon_<name>.png)
        static constexpr std::array<std::string_view, 17> ItemIcons = {
            "sword", "axe", "dagger", "mace", "staff", "wand", "bow", "crossbow", "shield", "helmet", "chest", "gloves", "boots", "potion", "arrows", "bolts", "key" };
        static constexpr std::string_view GateModel = "wall_gated.gltf";
        static constexpr std::string_view SecretWallModel = "wall_cracked.gltf";
        static constexpr std::string_view CandleModel = "candle.gltf";
        static constexpr std::string_view LitCandleModel = "candle_lit.gltf";
        static constexpr float GateReach = 1.6f;          // the hero this close to a gate tries it
        static constexpr float GateSinkSeconds = 0.8f;     // an opened gate sinks into the floor
        static constexpr float WallCrumbleSeconds = 0.6f;  // a broken secret wall falls in on itself
        static constexpr float LidSwingSeconds = 0.5f;     // a greater chest's lid swings open
        static constexpr float LidOpenRadians = 1.9f;      // back past upright
        static constexpr std::string_view GreaterChestLid = "chest_large_gold_lid";   // the lid's part (node) in GreaterChestModel
        static constexpr float CandleReach = 1.2f;
        static constexpr float InteractRadius = 1.0f;

        // Torches on room walls (TorchSet), and how dark the dungeon is without them (the
        // directional lights' share)
        static constexpr std::string_view TorchModel = "torchWall.glb";
        static constexpr float TorchLightRadius = TorchSet::LightRadius;
        static constexpr float TorchFireRange = TorchSet::FireRange;
        static constexpr float DungeonDirectionalLight = 0.45f;   // the key light, which casts the shadows

        // Knockback (feel): the hero's critical hits throw an enemy back, a boss's blow or a
        // heavy one (HeavyHitShare) throws the hero back; a spell's own knockbackTiles too
        static constexpr float CritKnockbackTiles = 0.8f;

        /// <summary>
        /// Each tier's air around the hero (FloorThemes order): what drifts there, how much each
        /// second, and between which heights
        /// </summary>
        struct Atmosphere {
            ParticleEffect effect;
            float perSecond;
            float lowest;
            float highest;
        };
        static constexpr std::array<Atmosphere, 3> Atmospheres{ {
            { ParticleEffect::CryptDust, 18.0f, 0.3f, 2.6f },
            { ParticleEffect::CaveMist, 6.0f, 0.05f, 0.3f },
            { ParticleEffect::RuinEmbers, 8.0f, 0.1f, 1.2f },
        } };
        static constexpr float AtmosphereHalfExtent = 7.0f;   // tiles around the hero
        static constexpr float HeavyKnockbackTiles = 1.0f;

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
            AutoSave();   // the console may not resume the game
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
            double chestOpenedAt{0.0};   // game time it opened (its lid's swing); 0: long open (a loaded save)
            std::vector<std::uint32_t> enemies;
        };

        [[nodiscard]] const std::vector<RoomState>& GetRoomStates() const noexcept { return session_->rooms; }

        using Torch = LB::Engine::Torch;

        /// <summary>
        /// The torches on the room walls of the current floor
        /// </summary>
        [[nodiscard]] const std::vector<Torch>& GetTorches() const noexcept { return session_->torches.Torches(); }
        [[nodiscard]] const WallSet& GetWalls() const noexcept { return session_->walls; }

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
        /// Throw an enemy back from a point, as a heavy blow does (tools and tests); false for a boss
        /// </summary>
        bool KnockEnemy(std::uint32_t id, WorldPoint from, float tiles) { return session_->encounter.Knock(id, from, tiles); }

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
        /// True while the player's attack plays: a weapon swing, or drawing and loosing a shot with
        /// a bow or crossbow (attacks repeat while Attack is held)
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
            const auto attributes = session_->characterService.GetEffectiveAttributes(*session_->playerId).value_or(character->get().GetAttributes());
            const std::int32_t modifier = weapon.attributeScale == "DEX" ? attributes.GetDexterityModifier()
                                        : weapon.attributeScale == "STR" ? attributes.GetStrengthModifier() : 0;
            const auto bonuses = EquippedBonuses();
            attack.attackBonus = session_->characterService.GetAttackBonus(*session_->playerId, weapon.category).value_or(0) + bonuses.attackBonus;
            attack.damageDice = weapon.damageDice;
            attack.damageBonus = weapon.damageBonus + static_cast<std::int32_t>(std::lround(static_cast<float>(modifier) * weapon.scaleFactor)) + bonuses.damage;
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

            defense.armorClass = character->get().GetArmorClass()
                               + session_->characterService.GetEffectiveAttributes(*session_->playerId).value_or(character->get().GetAttributes()).GetDexterityModifier();
            for (const auto& piece : session_->characterService.GetEquippedArmor(*session_->playerId)) {
                if (const auto* armor = content_->FindArmor(piece.armorId)) {
                    defense.physicalArmor += armor->physicalAR + armor->armorBonus;
                }
            }
            const auto bonuses = EquippedBonuses();
            defense.armorClass += bonuses.armorClass;
            defense.physicalArmor += bonuses.armor;
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
        /// What the equipped weapon and armour add (their affixes)
        /// </summary>
        [[nodiscard]] ItemBonuses EquippedBonuses() const;

        /// <summary>
        /// An item's name (a rolled one's, with its affixes)
        /// </summary>
        [[nodiscard]] std::string ItemName(ItemRef item) const;

        /// <summary>
        /// The run's rolled items (a Rolled ItemRef's rarity and affixes)
        /// </summary>
        [[nodiscard]] const ItemRolls& GetItemRolls() const noexcept { return session_->rolls; }

        /// <summary>
        /// The equipped weapon as an item (rolled or not)
        /// </summary>
        [[nodiscard]] ItemRef GetPlayerWeaponItem() const { return EquippedItem(LB::Combat::EquipmentSlot::MainHand); }

        /// <summary>
        /// The item equipped in a slot (invalid: none). The inventory's equipment slots are the one
        /// record of what is equipped; the weapon and CharacterService's armour follow from them.
        /// </summary>
        [[nodiscard]] ItemRef EquippedItem(LB::Combat::EquipmentSlot slot) const;

        /// <summary>
        /// The armour worn (every slot but the main hand), by slot
        /// </summary>
        [[nodiscard]] std::vector<std::pair<LB::Combat::EquipmentSlot, ItemRef>> WornItems() const;

        /// <summary>
        /// Ammunition of a type in the bag (arrows for "Arrow")
        /// </summary>
        [[nodiscard]] std::int32_t AmmoCount(std::string_view ammoType) const;

        /// <summary>
        /// What the player was told to do (design-combat-system.md 2.1: orders given during the
        /// tactical pause are carried out when it ends): walk to a point, go after an enemy and
        /// attack it until it falls, or cast a spell at one once in range
        /// </summary>
        enum class OrderKind { None, Move, Attack, Cast };
        struct PlayerOrder {
            OrderKind kind{OrderKind::None};
            WorldPoint point{};                       // Move
            std::optional<std::uint32_t> target;      // Attack, Cast (empty Cast: straight ahead)
            std::size_t slot{0};                      // Cast
        };

        [[nodiscard]] const PlayerOrder& GetPlayerOrder() const noexcept { return session_->order; }

        /// <summary>
        /// Order a walk to a point, as a right click on the floor does (tools and tests)
        /// </summary>
        void OrderMoveTo(WorldPoint at) noexcept {
            session_->order = PlayerOrder{ OrderKind::Move, at, std::nullopt, 0 };
            session_->route = {};
        }

        // What the camera can see, generously: its distance times this, plus a margin (tiles) for
        // tall walls whose feet are just off screen
        static constexpr float ViewDistanceFactor = 1.5f;
        static constexpr float ViewMargin = 4.0f;

        /// <summary>
        /// The ground the camera can see; floors, walls, torches, and props outside it are not drawn
        /// </summary>
        [[nodiscard]] ViewArea GetViewArea() const noexcept {
            const auto focus = session_->camera.GetFocus();
            return { { focus.x, focus.y }, session_->camera.GetDistance() * ViewDistanceFactor + ViewMargin };
        }

        /// <summary>
        /// The way an order is walking: the points still ahead (empty: straight at the goal)
        /// </summary>
        [[nodiscard]] std::span<const WorldPoint> GetOrderRoute() const noexcept {
            const auto& route = session_->route;
            return route.next < route.points.size() ? std::span<const WorldPoint>(route.points).subspan(route.next) : std::span<const WorldPoint>{};
        }

        /// <summary>
        /// A merchant's ware: an item (rolled or not), how many come with it, and its price
        /// </summary>
        struct StockEntry {
            ItemRef item;
            std::int32_t quantity{1};                 // per purchase (arrows come in bundles)
            std::int32_t price{0};
            bool unlimited{false};                    // potions and ammunition never run out
            const Content::AllyDefinition* ally{nullptr};   // a companion to hire instead of an item
        };

        /// <summary>
        /// This floor is a resting place (a merchant and a shrine, no enemies)
        /// </summary>
        [[nodiscard]] bool IsRestFloor() const noexcept { return session_->restFloor; }
        [[nodiscard]] static bool IsRestDepth(std::int32_t depth) noexcept { return depth > 1 && depth % BossFloorInterval == 1; }

        /// <summary>
        /// Go straight to a floor of the dungeon (tools and tests; the game descends by the trapdoor)
        /// </summary>
        void EnterFloor(std::int32_t depth) {
            if (session_->dungeonMode && session_->playerId && depth >= 1) StartFloor(depth);
        }

        /// <summary>
        /// Put the player at a point of the floor (tools and tests)
        /// </summary>
        void EnterFloorPosition(WorldPoint at) {
            if (!session_->playerId || !session_->world.CircleFits(at, PlayerRadius)) return;
            session_->playerPosition = at;
            session_->characterService.PlaceCharacter(*session_->playerId, ToTile(at));
        }
        [[nodiscard]] std::optional<WorldPoint> GetMerchantPosition() const noexcept { return session_->merchant; }
        [[nodiscard]] std::optional<WorldPoint> GetShrinePosition() const noexcept { return session_->shrine; }
        [[nodiscard]] bool IsShrineUsed() const noexcept { return session_->shrineUsed; }
        [[nodiscard]] const std::vector<StockEntry>& GetMerchantStock() const noexcept { return session_->stock; }

        /// <summary>
        /// The floor's props and spike traps, and the look of its tier
        /// </summary>
        [[nodiscard]] const FloorDecor& GetDecor() const noexcept { return session_->decor; }

        /// <summary>
        /// The floor's gates (locked and secret) and candle puzzles
        /// </summary>
        [[nodiscard]] const FloorFeatures& GetFeatures() const noexcept { return session_->features; }

        /// <summary>
        /// An item from its description as saves write it: a content key, or
        /// "~base|rarity|affix,affix|level" for a rolled one (tools and tests; it is not given)
        /// </summary>
        ItemRef MakeItem(std::string_view description) { return ItemFromSaveKey(description); }

        /// <summary>
        /// A rolled item's level (the depth it dropped at); 0 for anything else
        /// </summary>
        [[nodiscard]] ItemRef BaseItem(ItemRef item) const noexcept { return session_->rolls.BaseOf(item); }   // a rolled item's weapon or armour

        [[nodiscard]] std::int32_t ItemLevel(ItemRef item) const noexcept {
            const auto* rolled = session_->rolls.Find(item);
            return rolled ? rolled->level : 0;
        }

        /// <summary>
        /// Touch a puzzle candle by index (as Interact beside it does; tools and tests)
        /// </summary>
        void TouchCandle(std::size_t puzzle, std::size_t candle);
        [[nodiscard]] const FloorTheme& GetTheme() const noexcept { return session_->dungeonMode ? ThemeForDepth(session_->depth) : FloorThemes[0]; }
        [[nodiscard]] const CharacterScreen& GetTradeScreen() const noexcept { return session_->trade; }

        /// <summary>
        /// What a merchant asks for an item (its value times its rarity's multiplier), and pays for it
        /// </summary>
        [[nodiscard]] std::int32_t PriceOf(ItemRef item) const;
        [[nodiscard]] std::int32_t SellPriceOf(ItemRef item) const {
            return std::max(1, static_cast<std::int32_t>(std::floor(static_cast<float>(PriceOf(item)) * SellFraction)));
        }

        /// <summary>
        /// Buy a stock entry / sell one item from the bag (at the merchant); false when it cannot be done
        /// </summary>
        bool BuyFromMerchant(std::size_t index);
        bool SellToMerchant(ItemRef item);

        /// <summary>
        /// The enemy the orders would be about: under the mouse, or picked with the d-pad while paused
        /// </summary>
        [[nodiscard]] std::optional<std::uint32_t> GetPickedEnemy() const noexcept { return session_->picked; }

        /// <summary>
        /// Paused with Tab (or the left stick click): no menu, the game waits for orders
        /// </summary>
        [[nodiscard]] bool IsTacticallyPaused() const noexcept { return session_->paused && !menu_.IsOpen(); }

        /// <summary>
        /// The hero's class this run
        /// </summary>
        [[nodiscard]] LB::Character::CharacterClass GetPlayerClass() const noexcept { return session_->playerClass; }

        /// <summary>
        /// The spells on the player's bar (classes.json spellBar), slot by slot; empty for classes
        /// without spells
        /// </summary>
        [[nodiscard]] std::vector<const Content::SpellDefinition*> GetSpellBar() const;

        /// <summary>
        /// Why the spell in a slot cannot be cast now ("level 5", "mana", "cooldown", ...), or
        /// empty when it can
        /// </summary>
        [[nodiscard]] std::string SpellBlocker(std::size_t slot) const;

        /// <summary>
        /// Seconds until a slot's spell can be cast again
        /// </summary>
        [[nodiscard]] float GetSpellCooldown(std::size_t slot) const noexcept {
            return slot < SpellSlots ? session_->spellCooldowns[slot] : 0.0f;
        }

        /// <summary>
        /// A spell is being cast (the player stands still; moving cancels it)
        /// </summary>
        [[nodiscard]] bool IsPlayerCasting() const noexcept { return session_->casting.active; }

        /// <summary>
        /// Slowed, rooted, burning, ... on the player
        /// </summary>
        [[nodiscard]] const StatusSet& GetPlayerStatuses() const noexcept { return session_->playerStatuses; }

        /// <summary>
        /// The player's spell power (design-character-system.md 4.2): SP_Base + caster modifier x
        /// level (INT for the Mage, WIS for the Cleric, half INT for the Rogue)
        /// </summary>
        [[nodiscard]] std::int32_t GetPlayerSpellPower() const;

        /// <summary>
        /// The path the hero chose at SpecializationLevel (null before)
        /// </summary>
        [[nodiscard]] const Content::SpecializationDefinition* GetPlayerPath() const;

        /// <summary>
        /// Take a path (its key from classes.json); false before the level, with one already, or for another class's
        /// </summary>
        bool ChoosePath(std::string_view key);

        /// <summary>
        /// Spend an unspent attribute point, as the character screen does; false with none (or at the cap)
        /// </summary>
        bool SpendAttributePoint(LB::AttributeType attribute) {
            return session_->playerId && session_->characterService.SpendAttributePoint(*session_->playerId, attribute);
        }

        /// <summary>
        /// Award the player experience as a kill would (levels and their fanfare; tools and tests)
        /// </summary>
        void GrantExperience(std::int32_t experience);

        /// <summary>
        /// The run's events (GameEvents: kills, levels, paths), delivered at the end of each step;
        /// tools and tests may subscribe (until the run ends)
        /// </summary>
        [[nodiscard]] EventBus& GetEvents() noexcept { return session_->events; }

        /// <summary>
        /// What a run has seen so far: the deepest floor, enemies slain, gold found, times fallen
        /// (the death screen's summary; not kept in saves)
        /// </summary>
        struct RunStats {
            std::int32_t deepest{1};
            std::int32_t kills{0};
            std::int32_t goldFound{0};
            std::int32_t falls{0};
        };
        [[nodiscard]] const RunStats& GetRunStats() const noexcept { return session_->stats; }

        /// <summary>
        /// Put a bag item on the floor at the hero's feet (the whole stack); it stays there until
        /// they walk away from it and back. False when it is not in the bag.
        /// </summary>
        bool DropItem(ItemRef item);

        /// <summary>
        /// The banner across the screen (a level reached, a path taken), and the path cards
        /// </summary>
        [[nodiscard]] const Fanfare& GetFanfare() const noexcept { return session_->fanfare; }
        [[nodiscard]] const PathChoice& GetPathChoice() const noexcept { return session_->pathChoice; }
        [[nodiscard]] std::vector<PathCard> PathCards() const;

        // Feel: heavy blows freeze the action a moment (EngineOptions::hitStop) and shake the camera
        static constexpr float CritHitStopSeconds = 0.05f;
        static constexpr float HeavyHitStopSeconds = 0.08f;
        static constexpr float HeavyHitShare = 0.25f;           // a blow taking this share of the hero's health is heavy
        [[nodiscard]] float GetHitStopSeconds() const noexcept { return session_->hitStopSeconds; }

        /// <summary>
        /// How far the camera is shaking now (tiles; 0: still)
        /// </summary>
        [[nodiscard]] float GetShake() const noexcept {
            return session_->shakeDuration > 0.0f ? session_->shakeStrength * session_->shakeSeconds / session_->shakeDuration : 0.0f;
        }

        // Shadows under characters: darkest at the feet, stretched away from a torch within its light
        static constexpr float ShadowOpacity = 0.6f;
        static constexpr float ShadowHeight = 0.03f;

        /// <summary>
        /// The controls page's rows: each action with its keys and mouse buttons and its
        /// controller buttons, from the bindings (sections are rows with only a title)
        /// </summary>
        [[nodiscard]] std::vector<MenuModel::ControlRow> GetControls() const;

        /// <summary>
        /// A companion from allies.json joins the hero (beside them, at their level); null for an
        /// unknown key, without a hero, or with the party full. The merchant hires them for gold.
        /// </summary>
        std::optional<std::uint32_t> HireAlly(std::string_view key);

        [[nodiscard]] const AllyParty& GetAllies() const noexcept { return session_->allies; }

        /// <summary>
        /// Bind an action (one of RemappableActions) to a key or button in place of its first one
        /// of that device. An action that had it takes the action's old one instead (a swap).
        /// Moving's keys (WASD, arrows), Esc, and Start are refused. Kept in the profile.
        /// </summary>
        bool Rebind(std::string_view action, Systems::Input::InputAction binding);

        /// <summary>
        /// Every binding back to the defaults (and the profile's forgotten)
        /// </summary>
        void ResetBindings();

        /// <summary>
        /// The Controls page's chosen row, and whether it waits for a key or button
        /// </summary>
        [[nodiscard]] std::size_t ControlsCursor() const noexcept { return controlsCursor_; }
        [[nodiscard]] bool IsListeningForBinding() const noexcept { return listening_; }

        /// <summary>
        /// Give an ally a weapon or armour piece from the bag (what they had of that kind goes
        /// back to it), or take an ally's gear back; false when it cannot be
        /// </summary>
        bool GiveToAlly(std::uint32_t ally, ItemRef item);
        bool TakeFromAlly(std::uint32_t ally);

        /// <summary>
        /// The items an ally carries (weapon, armour; invalid: none)
        /// </summary>
        [[nodiscard]] std::pair<ItemRef, ItemRef> AllyItems(std::uint32_t ally) const {
            const auto it = session_->allyItems.find(ally);
            return it != session_->allyItems.end() ? it->second : std::pair<ItemRef, ItemRef>{};
        }

        /// <summary>
        /// Wound an ally (tools and tests); returns whether it took them down
        /// </summary>
        bool DamageAlly(std::uint32_t id, std::int32_t amount) { return session_->allies.Damage(id, amount); }

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
        /// Put an item (rolled or not) in the bag; returns how many fit
        /// </summary>
        std::int32_t GiveItemRef(ItemRef item, std::int32_t quantity = 1);

        /// <summary>
        /// Equip a weapon or armour piece from the bag (what was in its slot goes back to the bag).
        /// Fails for items not in the bag, not usable by the player's class, or above their level.
        /// </summary>
        bool EquipItem(std::string_view key);

        /// <summary>
        /// Equip an item from the bag (rolled or not): what it replaces goes back to the bag
        /// </summary>
        bool EquipItemRef(ItemRef item);

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
        void DropLoot(std::string_view lootTable, WorldPoint at, std::int32_t rolls = 1, std::int32_t depthBonus = 0);

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
            return PutOn(ItemRef{ ItemKind::Armor, armor->id }, armor->slot);
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
        void ResolveClips(std::vector<std::string>& errors);
        void ConfigureClassRules();
        bool SpawnPlayer();
        void SetPaused(bool paused);
        void ApplyPreferences(const Systems::Persistence::PlayerPreferences& prefs);
        void SaveVolumePreferences();
        static bool ParseDouble(const Systems::Persistence::SaveData& data, const std::string& key, double& value);
        void RegisterGamepadBinding(std::string_view action, Systems::Input::GamepadButton button);
        void RegisterKeyBinding(std::string_view action, std::int32_t keyCode);
        void ApplyBindingProfile();
        void SaveBindings();
        bool UpdateControlsPage();
        void RegisterDefaultBindings();
        void OnDeviceLost() {}       // nothing device-dependent is held by the game yet
        void OnDeviceRestored() {}

        // One simulation step (Simulation.cpp)
        void Update(double elapsedSeconds);
        void UpdateCameraControls();
        void UpdatePlayerCombat(float elapsedSeconds);
        void UpdatePlayerCasting(float elapsedSeconds);
        void StartCast(std::size_t slot, std::optional<std::uint32_t> target = std::nullopt);
        void ReleaseCast();
        void CancelCast();
        void UpdatePlayerStatuses(float elapsedSeconds);
        void ApplySpellStatusesToPlayer(const Content::SpellDefinition& spell);
        const Content::ClassDefinition* PlayerClassDefinition() const;
        Services::Combat::WeaponAttackProfile PlayerAttackAgainst(const Enemy* target) const;
        void HealPlayer(std::int32_t amount);
        void StartPlayerAttack(std::optional<std::uint32_t> aim = std::nullopt);
        void UpdateOrders();
        void FollowOrder();
        std::optional<WorldPoint> OrderStep() const;

        // Equipment: record an item in a slot (or clear it), then bring the weapon and
        // CharacterService's armour pieces in line with the inventory's slots
        void SubscribeToEvents();
        [[nodiscard]] Systems::Rendering::ImageId IconFor(ItemRef item) const;
        void ApplyItemPowers();
        void UpdateFeatures();
        void DrawFeatures(const Systems::Rendering::Color& tint);
        void UpdatePathChoice();
        void DrawFanfare();
        [[nodiscard]] std::vector<std::string> PathBonuses(const Content::SpecializationDefinition& path) const;
        void UpdateAmbience();
        void FeelCombat();
        void Jolt(float hitStopSeconds, float shakeSeconds, float shakeStrength);
        void KnockPlayer(WorldPoint from, float tiles);
        void DrawShadows();
        [[nodiscard]] std::vector<std::string> FallenSummary() const;
        void UpdateAllies(float elapsedSeconds);
        void UpdateAllyOrders();
        void SyncAllyGear(std::uint32_t ally);
        void DrawAllies(const Shade& shade);
        void HealParty(const Content::SpellDefinition& spell, std::int32_t amount);
        bool PutOn(ItemRef item, LB::Combat::EquipmentSlot slot);
        bool TakeOff(LB::Combat::EquipmentSlot slot);
        void SyncEquipment();
        bool OrderTargetInReach(const Enemy& target) const;
        void DrawOrderMarks();
        void ReleasePlayerAttack();
        void AnimatePlayer(float elapsedSeconds, bool moving);
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
        void DrawProjectiles(const Shade& shade);
        Systems::Rendering::ModelId PlayerBodyModel() const;
        void DrawHud();
        static Systems::Rendering::Color HitTint(const Shade& shade, float flashSeconds);
        float EnemyTop(const Enemy& enemy) const;
        float PlayerTop() const;
        std::optional<Systems::Rendering::Matrix4> HeldTransform(Systems::Rendering::ModelId body, std::string_view file, const char* slot,
                                                                 const Systems::Rendering::AnimationState& animation) const;
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
        bool LoadMenuArt(const std::string& uiDirectory);
        void OpenMenu(MenuPage page);
        void CloseMenu();
        std::vector<MenuItem> BuildMenuItems() const;
        void UpdateMenu();
        void AdjustMenuItem(MenuCommand command, int direction);
        void ChooseMenuItem(MenuCommand command, std::int32_t slot = -1);
        void UpdateLegacy(const Legacy& next);
        void SaveLegacy();
        void GrantBoons();
        [[nodiscard]] std::vector<std::string> LegacyLines() const;
        void DrawMenu();

        // Sounds (GameSounds.cpp): `at` places a sound in the world (heard from the player)
        bool LoadSounds(const std::string& soundDirectory);
        bool LoadMusic(const std::string& musicDirectory);
        void StartMusic();
        void UpdateMusic(float elapsedSeconds);
        void PlaySound(GameSound sound, std::optional<WorldPoint> at = std::nullopt);
        void PlayCombatSounds();
        void PlayProjectileSounds();
        void PlaySpellSounds();
        void UpdateSounds(float elapsedSeconds, bool running);

        // When and where effects play (Effects.cpp)
        void EmitCombatEffects();
        void EmitProjectileEffects();
        void EmitSpellEffects(float elapsedSeconds);
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
        void UpdateTradeScreen();
        std::vector<ScreenRow> BuildTradeRows() const;
        void DrawRowScreen(const CharacterScreen& screen, std::string_view title);
        void StockMerchant();
        void DrawRestPlace(const Systems::Rendering::Color& tint);
        void DrawDecor(const Systems::Rendering::Color& tint);
        void DrawTelegraphs();
        void UpdateTraps();
        [[nodiscard]] bool ScreenOpen() const noexcept { return session_->characterScreen.IsOpen() || session_->trade.IsOpen(); }
        [[nodiscard]] bool NearMerchant() const noexcept {
            return session_->merchant && IsPlayerAlive() && Distance(*session_->merchant, session_->playerPosition) <= MerchantRadius;
        }
        std::vector<ScreenRow> BuildScreenRows() const;
        void DrawGroundItems(const Systems::Rendering::Color& tint);
        void DrawCharacterScreen();
        Systems::Rendering::ModelId ItemModel(const GroundItem& item) const;
        std::string_view HeldWeaponFile() const;
        bool PlayerHoldsShield() const;
        void WriteProgress(Systems::Persistence::SaveData& data) const;
        void ReadProgress(const Systems::Persistence::SaveData& data);
        std::string CanEquip(ItemRef item) const;
        std::string ItemSaveKey(ItemRef item) const;
        ItemRef ItemFromSaveKey(std::string_view text);
        std::optional<Systems::Rendering::Color> RarityColorOf(ItemRef item) const;
        bool SpendAmmo(std::string_view ammoType);
        ItemRef RollDrop(ItemRef item, std::int32_t depthBonus = 0);

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
            Systems::Rendering::ModelId player{};           // the knight (Warrior)
            Systems::Rendering::ModelId mage{};
            Systems::Rendering::ModelId rogue{};
            Systems::Rendering::ModelId cleric{};
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
        // Clips the player plays (KayKit Rig_Medium); all invalid without animations (headless)
        struct PlayerClips {
            ClipInfo idle;
            ClipInfo bowIdle;       // standing with a bow
            ClipInfo run;
            ClipInfo slash;         // one-handed weapons
            ClipInfo slashTwoHanded;
            ClipInfo stab;          // daggers
            ClipInfo bowDraw;       // until the arrow leaves
            ClipInfo bowRelease;    // after it
            ClipInfo shoot;         // a crossbow
            ClipInfo hit;
            ClipInfo death;
            ClipInfo castLoop;      // a spell being cast
            ClipInfo castRelease;   // the spell leaving (and a wand's shot)
            ClipInfo dodge;         // a rogue's evasion
        };

        struct Assets {
            WorldModels models;
            std::unordered_map<std::string, Systems::Rendering::ModelId, StringHash, std::equal_to<>> characterModels;   // enemy bodies, weapons, props by file
            EnemyClips enemyClips{};
            PlayerClips playerClips{};
            bool soundsLoaded{false};
            bool musicLoaded{false};
            MenuArt menuArt;
            std::unordered_map<std::string, Systems::Rendering::ImageId, StringHash, std::equal_to<>> itemIcons;   // "sword" -> its icon (Scripts/MakeIcons.py)
        };

        // The player's side of combat
        struct PlayerCombat {
            bool swinging{false};                  // attacking: a swing, or a shot being drawn and loosed
            float swingSeconds{0.0f};
            bool impactDone{false};                // this swing's strike has been resolved (or the shot has left)
            bool shooting{false};                  // this attack is a shot (a bow or crossbow)
            std::optional<std::uint32_t> target;   // enemy picked when the attack started
            float hurtSeconds{0.0f};               // the flinch from a hit still playing
            float hitFlashSeconds{0.0f};
            float deadSeconds{0.0f};               // time spent down
        };

        /// <summary>
        /// Everything one game run owns, from Initialize to Shutdown: the player, the world and
        /// its floor, enemies, loot, effects, and the clock. Shutdown replaces it with a fresh one,
        /// so state added later cannot leak from one run into the next.
        /// </summary>
        // The spell the player is casting (design-spells-system.md 4.2-4.3)
        struct PlayerCasting {
            bool active{false};
            std::size_t slot{0};
            const Content::SpellDefinition* spell{nullptr};   // into content_, which outlives the session
            float seconds{0.0f};
            std::optional<std::uint32_t> target;
            float releaseSeconds{0.0f};                       // the release pose still playing
        };

        struct Session {
            Services::Character::CharacterService characterService;
            std::optional<LB::Character::CharacterId> playerId;
            LB::Character::CharacterClass playerClass{LB::Character::CharacterClass::Warrior};
            WorldPoint playerPosition{};
            float playerFacingYaw{0.0f};             // radians around the vertical axis; 0 faces +Z (south)
            PlayerCombat playerCombat;
            Animator playerAnimator;
            PlayerCasting casting;
            PlayerOrder order;
            std::optional<std::uint32_t> picked;     // under the mouse, or cycled to while paused
            std::array<float, SpellSlots> spellCooldowns{};
            float manaCarry{0.0f};                   // regenerated mana not yet a whole point
            float evadeSeconds{0.0f};                // no attack touches the player (an evasion)
            float sneakWindowSeconds{0.0f};          // blows are sneak attacks (after a shadowstep)
            std::string path;                        // the specialization's key (empty: none yet)
            StatusSet playerStatuses;
            // An order's walk around the walls (design-combat-system.md 2.1, orders given in the tactical pause)
            struct Route {
                std::vector<WorldPoint> points;
                std::size_t next{0};
                std::pair<std::int32_t, std::int32_t> goalCell{ -1, -1 };
            };
            Route route;
            const Content::WeaponDefinition* playerWeapon{nullptr};   // the main hand's (follows the inventory: SyncEquipment); into content_
            ItemRolls rolls;                         // rolled items of this run
            Services::Inventory::InventoryService inventoryService;
            std::int32_t gold{0};
            std::vector<GroundItem> groundItems;
            std::uint32_t nextGroundItemId{1};
            LootRoller lootRoller;
            CharacterScreen characterScreen;
            CharacterScreen trade;                   // the merchant's wares and the bag
            bool restFloor{false};
            std::optional<WorldPoint> merchant;
            std::optional<WorldPoint> shrine;
            bool shrineUsed{false};
            std::vector<StockEntry> stock;
            FloorDecor decor;                        // props and traps of the floor
            FloorFeatures features;                  // gates, secret walls, candle puzzles
            EventBus events;                         // what happens in the run (GameEvents), dispatched after each step
            AllyParty allies;                        // the hero's companions
            std::map<std::uint32_t, std::pair<ItemRef, ItemRef>> allyItems;   // what each carries: weapon, armour
            std::optional<std::uint32_t> givingTo;   // the character screen is choosing an item for this ally
            std::string ambienceClip;                // the loop playing under the floor (empty: none)
            Systems::Audio::AudioSourceId ambienceSource{};
            float hitStopSeconds{0.0f};              // the action is frozen (a heavy blow)
            WorldPoint knockVelocity{};              // the hero thrown back by a heavy blow...
            float knockSeconds{0.0f};                // ...for this long
            float shakeSeconds{0.0f};                // the camera shakes, dying away over shakeDuration
            float shakeDuration{0.0f};
            float shakeStrength{0.0f};               // tiles at its start
            RunStats stats;
            bool playing{false};                     // a run being played (not the one behind the title screen)
            float savedNoticeSeconds{0.0f};
            Fanfare fanfare;                         // a level or a path, across the screen
            PathChoice pathChoice;                   // the path cards (over the character screen)

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
        Legacy legacy_;                             // outlives runs: loaded from the profile at startup
        std::size_t controlsCursor_{0};          // the Controls page's chosen row
        bool listening_{false};                  // waiting for the chosen row's new key or button
        StepTimer timer_;
        bool active_{true};
        bool initialized_{false};
    };

} // namespace LB::Engine
