// Enemies.ixx - What an enemy is: its state, animation clips, and the combat events between the
// enemies and the player
export module LB.Engine:Enemies;

import std;
import LB.Content;
import LB.CombatService;
import LB.RenderingSystem;
import :Animator;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// What an enemy is doing (design-combat-system.md 9.1, Tier 1: move to the target and attack)
    /// </summary>
    enum class EnemyState {
        Spawning,    // rising from the ground; cannot act or be hit
        Idle,        // has not noticed the player (or the player is down)
        Chasing,     // closing in, or waiting in reach for the next attack
        Attacking,   // winding up and swinging; the hit lands part way through
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
        std::int32_t experience{0};         // awarded for the kill, scaled for the wave
        bool impactDone{false};             // the current attack's hit has been resolved
        float recoverySeconds{0.0f};        // until the next attack may start
        float hitFlashSeconds{0.0f};        // > 0 right after being hit
        Animator animator;

        [[nodiscard]] bool IsDead() const noexcept { return state == EnemyState::Dead; }

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
    };

    /// <summary>
    /// The player as enemies see it during a step
    /// </summary>
    struct PlayerTarget {
        WorldPoint position{};
        bool alive{true};
        Services::Combat::DefenseProfile defense{};
    };

} // namespace LB::Engine
