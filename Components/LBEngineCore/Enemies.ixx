// Enemies.ixx - What an enemy is: its state, animation clips, and the combat events between the
// enemies and the player
export module LB.Engine:Enemies;

import std;
import LB.Content;
import LB.CombatService;
import LB.RenderingSystem;
import :Animator;
import :Statuses;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// What an enemy is doing (design-combat-system.md 9.1, Tier 1: move to the target and attack)
    /// </summary>
    enum class EnemyState {
        Spawning,    // rising from the ground; cannot act or be hit
        Idle,        // has not noticed the player (or the player is down)
        Chasing,     // closing in (or backing away, for ranged enemies), or waiting in reach for the next attack
        Attacking,   // winding up and swinging (or aiming); the hit lands (or the shot leaves) part way through
        Staggered,   // reeling from a hit (not while attacking)
        Dead         // corpse, removed after a while
    };

    /// <summary>
    /// A clip and its length
    /// </summary>
    struct ClipInfo {
        Systems::Rendering::AnimationId id{};
        float seconds{1.0f};
    };

    /// <summary>
    /// Animation clips enemies play (KayKit Rig_Medium), with their lengths so playback can be
    /// fitted to gameplay timings. All invalid when no animations are loaded (headless).
    /// </summary>
    struct EnemyClips {
        // Rig_Large clips, for enemies on that rig (EnemyDefinition::rig); empty: they use these
        std::shared_ptr<const EnemyClips> large;
        Systems::Rendering::AnimationId idle{};
        Systems::Rendering::AnimationId walk{};
        Systems::Rendering::AnimationId attack{};
        Systems::Rendering::AnimationId hit{};
        Systems::Rendering::AnimationId death{};
        Systems::Rendering::AnimationId spawn{};
        float walkSeconds{1.0f};
        float attackSeconds{1.0f};
        float hitSeconds{1.0f};
        float spawnSeconds{1.0f};
        // Attack clips named by enemy types (EnemyDefinition::attackAnimation), by enemy key;
        // types without one use `attack`
        std::map<std::string, ClipInfo, std::less<>> attacks;
    };

    /// <summary>
    /// One enemy in the arena. Stats come from its EnemyDefinition (enemies.json).
    /// </summary>
    struct Enemy {
        std::uint32_t id{0};
        const Content::EnemyDefinition* definition{nullptr};
        WorldPoint position{};
        float facingYaw{0.0f};              // radians around the vertical axis; 0 faces +Z
        EnemyState state{EnemyState::Spawning};
        float stateSeconds{0.0f};           // time spent in the current state
        std::int32_t hitPoints{1};
        std::int32_t maxHitPoints{1};       // the type's hit points scaled for the wave
        std::int32_t bonusDamage{0};        // added to the type's damage for the wave
        std::int32_t bonusAttack{0};        // added to the type's attack bonus (New Game+)
        std::int32_t bonusArmorClass{0};    // added to the type's armour class (New Game+)
        std::int32_t experience{0};         // awarded for the kill, scaled for the wave
        bool impactDone{false};             // the current attack's hit has been resolved
        float recoverySeconds{0.0f};        // until the next attack may start
        float hitFlashSeconds{0.0f};        // > 0 right after being hit
        Animator animator;
        StatusSet statuses;                 // slowed, rooted, frozen, burning
        std::int32_t wave{1};               // the wave or depth it was scaled for
        std::uint32_t group{0};             // its room's or wave's group: they notice the player together (0: alone)
        std::uint32_t summoner{0};          // the enemy that raised it (it crumbles with it); 0: none
        float summonSeconds{0.0f};          // until it raises enemies again (summoners)
        std::size_t summonTurn{0};          // raisings so far (which of EnemyDefinition::summons comes next)
        bool phaseTwo{false};               // a boss below its phase-two health (and still in its third)
        bool phaseThree{false};             // a boss below its phase-three health
        float slamSeconds{0.0f};            // > 0: winding up a telegraphed attack, landing when it runs out
        float slamCooldown{0.0f};           // until it may start another
        float lastSlamCooldown{0.0f};       // until it may start its phase-three one
        bool lastSlam{false};               // the one winding up is its phase-three one
        WorldPoint slamCenter{};            // where the ring is
        bool champion{false};               // a floor's named elite (its objective)
        std::string title;                  // its name ("Grishnak the Cruel")
        WorldPoint knockVelocity{};         // thrown back by a heavy blow (tiles per second)...
        float knockSeconds{0.0f};           // ...for this long
        float lungeSeconds{0.0f};           // > 0: rushing at the player
        float lungeCooldown{0.0f};
        WorldPoint lungeDirection{};

        [[nodiscard]] bool IsDead() const noexcept { return state == EnemyState::Dead; }

        // Its phase: 1, 2 below its phase-two health, 3 below its phase-three health
        [[nodiscard]] std::int32_t Phase() const noexcept { return phaseThree ? 3 : phaseTwo ? 2 : 1; }

        // The telegraphed attack winding up (or last wound up): its phase-three one, or its slam
        [[nodiscard]] Content::TelegraphDefinition ActiveSlam() const noexcept {
            return lastSlam && definition->phaseThreeSlam ? *definition->phaseThreeSlam : definition->Slam();
        }

        // Can be attacked: on its feet and done spawning
        [[nodiscard]] bool IsTargetable() const noexcept {
            return state != EnemyState::Dead && state != EnemyState::Spawning;
        }
    };

    /// <summary>
    /// Who attacked whom in a combat event
    /// </summary>
    enum class CombatEventKind {
        PlayerAttack,   // the player struck at an enemy
        EnemyAttack     // an enemy struck at the player
    };

    /// <summary>
    /// A resolved attack: the roll, the damage after defenses, and whether it killed the enemy
    /// (player attacks). Enemy attacks are applied to the player by the engine.
    /// </summary>
    struct CombatEvent {
        CombatEventKind kind{CombatEventKind::PlayerAttack};
        std::uint32_t enemyId{0};
        Services::Combat::WeaponAttackResult result{};
        bool killed{false};
        const Content::SpellDefinition* spell{nullptr};   // a spell's hit (its statuses go to the target); null: an attack
        std::uint32_t ally{0};              // an enemy attack's target when it was an ally (0: the player)
        bool byAlly{false};                 // a blow of the hero's side struck by an ally (not the hero)
    };

    /// <summary>
    /// The player (or one of their allies) as enemies see them during a step
    /// </summary>
    struct PlayerTarget {
        WorldPoint position{};
        bool alive{true};
        Services::Combat::DefenseProfile defense{};
        float radius{0.375f};               // the body projectiles hit (tiles)
        std::uint32_t ally{0};              // an ally's id (0: the player)
    };

    /// <summary>
    /// What an enemy's step ends with: nothing, a melee strike landing on the player, or a shot
    /// leaving toward them
    /// </summary>
    enum class EnemyAction {
        None,
        Strike,
        Shoot,
        Slam        // a telegraphed attack lands (at Enemy::slamCenter)
    };

    /// <summary>
    /// Where an enemy's group stands this step (design-combat-system.md 9.1): how close its
    /// nearest tank and its nearest melee fighter are to the player (infinity: none closing in)
    /// </summary>
    struct Formation {
        float tank{std::numeric_limits<float>::infinity()};
        float front{std::numeric_limits<float>::infinity()};
    };

} // namespace LB::Engine
