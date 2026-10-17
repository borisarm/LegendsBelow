// EngineTunables.ixx - The engine's tunables and asset names (GameEngine inherits them, so they
// stay reachable as GameEngine::Name): windows and timing, input actions, models and animations,
// rosters and bosses, the tiers' air, and the rules' numbers
export module LB.Engine:EngineTunables;

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
import :Objectives;
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
    /// The engine's constants and asset names, as a base of GameEngine (GameEngine::Name)
    /// </summary>
    struct EngineTunables {
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
        static constexpr float SavedNoticeSeconds = 2.0f;   // "Saved" shows this long after an autosave

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
        static constexpr std::array<std::string_view, 4> BossEnemies = { "enemy_necromancer", "enemy_golem_warlord", "enemy_frost_golem", "enemy_abomination" };

        /// <summary>
        /// Who fills a tier's rooms (design-dungeon-and-world.md 5: each theme its inhabitants): the
        /// crypts' skeletons; the caves' werewolves, orcs, and witches; the ruins' vampires,
        /// tieflings, and black knights (skeletons shoot in the first three); the archive's shadow
        /// blades, avian blademasters, thornbow wardens, and lorekeepers
        /// </summary>
        struct Roster {
            std::string_view light;
            std::string_view heavy;
            std::string_view ranged;
            std::string_view caster;
            std::string_view brute;
        };
        static constexpr std::array<Roster, 4> Rosters{ {
            { "enemy_skeleton_minion", "enemy_skeleton_warrior", "enemy_skeleton_rogue", "enemy_skeleton_mage", "enemy_skeleton_golem" },
            { "enemy_werewolf", "enemy_orc_raider", "enemy_skeleton_rogue", "enemy_witch", "enemy_orc_brute" },
            { "enemy_vampire", "enemy_skeleton_warrior", "enemy_skeleton_rogue", "enemy_tiefling", "enemy_black_knight" },
            { "enemy_shadow_blade", "enemy_avian_blademaster", "enemy_thornbow_warden", "enemy_lorekeeper", "enemy_black_knight" },
        } };

        [[nodiscard]] static constexpr const Roster& RosterForDepth(std::int32_t depth) noexcept {
            return Rosters[std::min(static_cast<std::size_t>(std::max(0, depth - 1) / BossFloorInterval), Rosters.size() - 1)];
        }

        /// <summary>
        /// The boss at the end of a depth's tier
        /// </summary>
        [[nodiscard]] static constexpr std::string_view BossForDepth(std::int32_t depth) noexcept {
            return depth == FinalDepth ? FinalBoss : BossEnemies[std::min(static_cast<std::size_t>(std::max(0, depth - 1) / BossFloorInterval), BossEnemies.size() - 1)];
        }
        // A boss guards the exit at the end of each tier of floors (design-dungeon-and-world.md 7.1-7.2)
        static constexpr std::int32_t BossFloorInterval = 3;
        // Below the last tier's resting place, the final floor (design-dungeon-and-world.md 7.4): an
        // entrance, a corridor, and a great hall where the last boss waits
        static constexpr std::int32_t FinalDepth = 14;
        static constexpr std::string_view FinalBoss = "enemy_first_archivist";
        static constexpr std::string_view FinalFloorName = "THE RED LIBRARY";
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

        // Objectives (objectives.json holds their tunables): how near the hero picks up a relic
        // or frees a captive, and how much bigger a champion stands
        static constexpr float RelicReach = 1.0f;
        static constexpr float CaptiveReach = 1.5f;
        static constexpr float ChampionScale = 1.2f;
        static constexpr double VoiceCooldownSeconds = 1.5;   // a creature cries out when hurt at most this often
        static constexpr float CageHeightStretch = 2.6f;   // the cage's bars stand this many times their height
        static constexpr std::string_view LoreBookModel = "book_grey.gltf";
        static constexpr float LoreReach = 1.3f;                // how near the hero reads a book
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
        static constexpr std::array<Atmosphere, 4> Atmospheres{ {
            { ParticleEffect::CryptDust, 18.0f, 0.3f, 2.6f },
            { ParticleEffect::CaveMist, 6.0f, 0.05f, 0.3f },
            { ParticleEffect::RuinEmbers, 8.0f, 0.1f, 1.2f },
            { ParticleEffect::ArchiveMotes, 10.0f, 0.4f, 2.4f },
        } };
        static constexpr float AtmosphereHalfExtent = 7.0f;   // tiles around the hero
        static constexpr float HeavyKnockbackTiles = 1.0f;

        // Damage numbers rise and fade over this time (CombatTextLog)
        static constexpr float FloatingTextSeconds = CombatTextLog::Seconds;

        // Walls between the camera and the player fade (WallSet)
        static constexpr float FadedWallAlpha = WallSet::FadedAlpha;
        static constexpr float WallFadeSeconds = WallSet::FadeSeconds;
    };

} // namespace LB::Engine
