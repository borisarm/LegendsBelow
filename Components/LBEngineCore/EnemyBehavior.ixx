// EnemyBehavior.ixx - How one enemy acts each step (Tier 1 AI, and kiting for ranged enemies): its
// state machine and animations, closing in on the player around walls, turning, attacking on a
// timer, and keeping bodies apart
export module LB.Engine:EnemyBehavior;

import std;
import LB.Content;
import LB.RenderingSystem;
import :Animator;
import :World;
import :Enemies;

export namespace LB::Engine {

    /// <summary>
    /// The enemies' behavior (design-combat-system.md 9.1, Tier 1): notice the player in sight,
    /// close in (straight when in sight, else along a flow field around the walls), attack when in
    /// reach and facing them, reel from hits, die. Ranged enemies only shoot at a player in sight
    /// (7.4) and, while reloading, back away from one closer than their keepAwayRange (9.1 Tier 3,
    /// Kite). Decides when a strike lands or a shot leaves; the encounter resolves them.
    /// Rendering-independent: without clips the animations are simply invalid.
    /// </summary>
    class EnemyBehavior {
    public:
        // Bodies keep this far apart (tiles, center to edge)
        static constexpr float CharacterRadius = 0.3f;
        // The player's hit lands this far through the attack time (wind-up, then swing); enemies
        // use their type's attackImpactFraction, matched to their attack animation
        static constexpr float AttackImpactFraction = 0.45f;
        // A target may step this much past reach during the wind-up and still be hit
        static constexpr float AttackReachSlack = 0.35f;
        // Enemies only start an attack when the player is within this cone (cosine: 60 degrees)
        static constexpr float AttackConeCosine = 0.5f;
        static constexpr float TurnRateRadiansPerSecond = 10.0f;
        static constexpr float SpawnSeconds = 1.3f;
        static constexpr float StaggerSeconds = 0.45f;
        static constexpr float HitFlashSeconds = 0.2f;
        static constexpr float CorpseSeconds = 5.0f;        // after dying, then removed
        static constexpr float CorpseFadeSeconds = 1.0f;    // last part of CorpseSeconds
        // Pushing bodies apart is repeated so crowds settle within a step
        static constexpr int SeparationPasses = 4;
        // Walk cycles are authored for this ground speed (tiles per second)
        static constexpr float WalkAnimationSpeed = 2.0f;
        // Backing away that gains less than this share of a step means a wall is behind
        static constexpr float BackAwayProgress = 0.3f;

        /// <summary>
        /// The world enemies move in (kept by reference; null: open ground, always in sight)
        /// </summary>
        void SetWorld(const WorldMap* world) noexcept { world_ = world; }

        void SetClips(const EnemyClips& clips) { clips_ = clips; }

        /// <summary>
        /// A new step: the flow field toward the player is built again when first needed
        /// </summary>
        void BeginStep() noexcept { flowFieldReady_ = false; }

        /// <summary>
        /// Change an enemy's state and start the state's animation
        /// </summary>
        void Enter(Enemy& enemy, EnemyState state) {
            enemy.state = state;
            enemy.stateSeconds = 0.0f;

            const auto& type = *enemy.definition;
            switch (state) {
            case EnemyState::Spawning:
                enemy.animator.Play(clips_.spawn, false, clips_.spawnSeconds / SpawnSeconds, true, 0.0f);
                break;
            case EnemyState::Idle:
                enemy.animator.Play(clips_.idle, true);
                break;
            case EnemyState::Chasing:
                break;   // walking or waiting in reach, decided every step
            case EnemyState::Attacking: {
                enemy.impactDone = false;
                // The whole clip spans the attack time, so its strike meets attackImpactFraction
                const auto own = clips_.attacks.find(type.key);
                const ClipInfo clip = own != clips_.attacks.end() ? own->second : ClipInfo{ clips_.attack, clips_.attackSeconds };
                enemy.animator.Play(clip.id, false, clip.seconds / type.attackTimeSeconds, true, 0.1f);
                break;
            }
            case EnemyState::Staggered:
                enemy.animator.Play(clips_.hit, false, clips_.hitSeconds / StaggerSeconds, true, 0.05f);
                break;
            case EnemyState::Dead:
                enemy.animator.Play(clips_.death, false, 1.0f, true, 0.1f);
                break;
            }
        }

        /// <summary>
        /// One step of an enemy. Returns Strike when its melee attack lands on the player this step
        /// (in reach at the impact moment), Shoot when a ranged enemy's shot leaves; the caller
        /// resolves the strike or launches the shot.
        /// </summary>
        [[nodiscard]] EnemyAction Update(Enemy& enemy, float dt, const PlayerTarget& player) {
            enemy.stateSeconds += dt;
            enemy.hitFlashSeconds = std::max(0.0f, enemy.hitFlashSeconds - dt);
            enemy.animator.Update(dt);

            const auto& type = *enemy.definition;
            const float dx = player.position.x - enemy.position.x;
            const float dy = player.position.y - enemy.position.y;
            const float distance = std::hypot(dx, dy);

            switch (enemy.state) {
            case EnemyState::Spawning:
                if (enemy.stateSeconds >= SpawnSeconds) Enter(enemy, EnemyState::Idle);
                return EnemyAction::None;

            case EnemyState::Dead:
                return EnemyAction::None;

            case EnemyState::Staggered:
                if (enemy.stateSeconds >= StaggerSeconds) Enter(enemy, EnemyState::Chasing);
                return EnemyAction::None;

            case EnemyState::Attacking: {
                TurnToward(enemy, dx, dy, dt);
                auto action = EnemyAction::None;
                if (!enemy.impactDone && enemy.stateSeconds >= type.attackTimeSeconds * type.attackImpactFraction) {
                    enemy.impactDone = true;
                    if (type.IsRanged()) {
                        if (player.alive) action = EnemyAction::Shoot;
                    }
                    else if (player.alive && distance <= type.attackRange + AttackReachSlack) {
                        action = EnemyAction::Strike;
                    }
                }
                if (enemy.stateSeconds >= type.attackTimeSeconds) {
                    enemy.recoverySeconds = type.attackRecoverySeconds;
                    Enter(enemy, EnemyState::Chasing);
                }
                return action;
            }

            case EnemyState::Idle:
            case EnemyState::Chasing:
                break;
            }

            enemy.recoverySeconds = std::max(0.0f, enemy.recoverySeconds - dt);
            if (!player.alive) {
                if (enemy.state != EnemyState::Idle) Enter(enemy, EnemyState::Idle);
                return EnemyAction::None;
            }
            const bool inSight = !world_ || world_->LineOfSight(enemy.position, player.position);
            if (enemy.state == EnemyState::Idle) {
                if (distance > type.aggroRange || !inSight) return EnemyAction::None;
                Enter(enemy, EnemyState::Chasing);
            }

            // Too close while reloading: a ranged enemy backs away, unless a wall is behind it
            if (type.IsRanged() && distance < type.keepAwayRange && enemy.recoverySeconds > 0.0f && distance > 0.0f) {
                const float step = type.movementSpeed * dt;
                const WorldPoint away{ -dx / distance * step, -dy / distance * step };
                const auto before = enemy.position;
                enemy.position = MoveBody(enemy.position, away);
                if (Distance(before, enemy.position) >= step * BackAwayProgress) {
                    TurnToward(enemy, away.x, away.y, dt);
                    enemy.animator.Play(clips_.walk, true, type.movementSpeed / WalkAnimationSpeed);
                    return EnemyAction::None;
                }
                enemy.position = before;   // cornered: stand and fight
            }

            // In reach (in sight, for a shot): face the player and attack when ready
            if (distance <= type.attackRange && (inSight || !type.IsRanged())) {
                TurnToward(enemy, dx, dy, dt);
                const auto forward = Forward(enemy.facingYaw);
                const bool facing = distance <= 0.0f || (forward.x * dx + forward.y * dy) / distance >= AttackConeCosine;
                if (enemy.recoverySeconds <= 0.0f && facing) {
                    Enter(enemy, EnemyState::Attacking);
                    return EnemyAction::None;
                }
                enemy.animator.Play(clips_.idle, true);
                return EnemyAction::None;
            }

            // Close in: straight at the player when in sight (stopping a little inside reach), else
            // along the flow field around the walls
            float tx = dx, ty = dy;
            float step = std::min(type.movementSpeed * dt, std::max(0.0f, distance - type.attackRange * 0.9f));
            if (!inSight && world_) {
                if (!flowFieldReady_) {
                    world_->BuildFlowField(player.position);
                    flowFieldReady_ = true;
                }
                const auto waypoint = world_->NextWaypoint(enemy.position);
                if (!waypoint) {
                    enemy.animator.Play(clips_.idle, true);
                    return EnemyAction::None;
                }
                tx = waypoint->x - enemy.position.x;
                ty = waypoint->y - enemy.position.y;
                step = type.movementSpeed * dt;
            }
            const float length = std::max(1e-4f, std::hypot(tx, ty));
            enemy.position = MoveBody(enemy.position, { tx / length * step, ty / length * step });
            TurnToward(enemy, tx, ty, dt);
            enemy.animator.Play(clips_.walk, true, type.movementSpeed / WalkAnimationSpeed);
            return EnemyAction::None;
        }

        /// <summary>
        /// Push overlapping enemies apart, and out of the player (the player is never pushed)
        /// </summary>
        void Separate(std::vector<Enemy>& enemies, const PlayerTarget& player) const {
            constexpr float minimum = CharacterRadius * 2.0f;
            for (std::size_t i = 0; i < enemies.size(); ++i) {
                auto& a = enemies[i];
                if (a.IsDead()) continue;
                for (std::size_t j = i + 1; j < enemies.size(); ++j) {
                    auto& b = enemies[j];
                    if (b.IsDead()) continue;
                    const float dx = b.position.x - a.position.x;
                    const float dy = b.position.y - a.position.y;
                    const float distance = std::hypot(dx, dy);
                    if (distance >= minimum) continue;
                    // Coincident bodies separate along a fixed axis
                    const float nx = distance > 1e-4f ? dx / distance : 1.0f;
                    const float ny = distance > 1e-4f ? dy / distance : 0.0f;
                    const float push = (minimum - distance) * 0.5f;
                    a.position = MoveBody(a.position, { -nx * push, -ny * push });
                    b.position = MoveBody(b.position, { nx * push, ny * push });
                }

                if (player.alive) {
                    const float dx = a.position.x - player.position.x;
                    const float dy = a.position.y - player.position.y;
                    const float distance = std::hypot(dx, dy);
                    if (distance < minimum) {
                        const float nx = distance > 1e-4f ? dx / distance : 1.0f;
                        const float ny = distance > 1e-4f ? dy / distance : 0.0f;
                        a.position = MoveBody(a.position, { player.position.x + nx * minimum - a.position.x, player.position.y + ny * minimum - a.position.y });
                    }
                }
            }
        }

        /// <summary>
        /// How visible a corpse still is (1 until it starts fading)
        /// </summary>
        [[nodiscard]] static float Opacity(const Enemy& enemy) noexcept {
            if (!enemy.IsDead()) return 1.0f;
            const float fadeStart = CorpseSeconds - CorpseFadeSeconds;
            return std::clamp(1.0f - (enemy.stateSeconds - fadeStart) / CorpseFadeSeconds, 0.0f, 1.0f);
        }

    private:
        // Turn toward a ground direction, at most TurnRateRadiansPerSecond
        static void TurnToward(Enemy& enemy, float dx, float dy, float dt) {
            if (dx == 0.0f && dy == 0.0f) return;
            constexpr float pi = std::numbers::pi_v<float>;
            float delta = std::atan2(dx, dy) - enemy.facingYaw;
            while (delta > pi) delta -= 2.0f * pi;
            while (delta < -pi) delta += 2.0f * pi;
            const float maxTurn = TurnRateRadiansPerSecond * dt;
            enemy.facingYaw += std::clamp(delta, -maxTurn, maxTurn);
        }

        // A body's move, sliding along the world's walls (unbounded without a world)
        WorldPoint MoveBody(WorldPoint from, WorldPoint step) const noexcept {
            if (!world_) return { from.x + step.x, from.y + step.y };
            return world_->Move(from, step, CharacterRadius);
        }

        const WorldMap* world_{nullptr};
        EnemyClips clips_{};
        bool flowFieldReady_{false};
    };

} // namespace LB::Engine
