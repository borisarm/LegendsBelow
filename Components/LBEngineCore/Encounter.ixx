// Encounter.ixx - Enemies in the arena: waves, Tier 1 AI, and melee combat with the player
export module LB.Engine:Encounter;

import std;
import LB.Core;
import LB.Combat;
import LB.Content;
import LB.CombatService;
import LB.RenderingSystem;
import :Animator;

export namespace LB::Engine {

    /// <summary>
    /// Continuous position in world tiles (tile (0, 0) covers [0, 1) x [0, 1))
    /// </summary>
    struct WorldPoint {
        float x{0.0f};
        float y{0.0f};
    };

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

    /// <summary>
    /// The enemies of the test arena: spawns waves of a light and a heavy enemy type, runs their
    /// AI (notice the player, close in, attack on a timer, reel from hits, die), and resolves
    /// attacks both ways with CombatService. Rendering-independent: animation clips are optional.
    /// </summary>
    class Encounter {
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
        static constexpr float WaveDelaySeconds = 3.0f;     // between clearing a wave and the next
        static constexpr float MinimumSpawnDistance = 5.0f; // from the player
        // Pushing bodies apart is repeated so crowds settle within a step
        static constexpr int SeparationPasses = 4;
        // Walk cycles are authored for this ground speed (tiles per second)
        static constexpr float WalkAnimationSpeed = 2.0f;

        /// <summary>
        /// Encounter with nondeterministic dice and spawn points
        /// </summary>
        Encounter() : combat_(), rng_(std::random_device{}()) {}

        /// <summary>
        /// Encounter whose dice and spawn points are reproducible (tests, replays)
        /// </summary>
        explicit Encounter(std::uint32_t seed) : combat_(seed), rng_(seed) {}

        /// <summary>
        /// Enemy types for waves (heavy may be null: light enemies only) and the arena they fight in
        /// (centered on the origin)
        /// </summary>
        void Configure(const Content::EnemyDefinition* light, const Content::EnemyDefinition* heavy, float arenaHalfWidth, float arenaHalfHeight) {
            light_ = light;
            heavy_ = heavy;
            arenaHalfWidth_ = arenaHalfWidth;
            arenaHalfHeight_ = arenaHalfHeight;
        }

        void SetClips(const EnemyClips& clips) {
            clips_ = clips;
        }

        /// <summary>
        /// Remove every enemy and stop the waves
        /// </summary>
        void Reset() {
            enemies_.clear();
            wave_ = 0;
            waveTimerSeconds_ = 0.0f;
        }

        /// <summary>
        /// Enemies in a wave: light ones grow from 2 to 4, heavy ones from 0 to 2
        /// (placeholder composition until dungeon rooms define their encounters)
        /// </summary>
        static std::pair<std::int32_t, std::int32_t> WaveComposition(std::int32_t wave) noexcept {
            const std::int32_t n = std::max(1, wave);
            return { std::min(2 + (n - 1) / 2, 4), std::min(n / 2, 2) };
        }

        /// <summary>
        /// Start a wave: its enemies rise at random spots at least MinimumSpawnDistance from the player
        /// </summary>
        void StartWave(std::int32_t wave, WorldPoint player) {
            wave_ = wave;
            waveTimerSeconds_ = 0.0f;
            if (!light_) return;

            const auto spawnFacingPlayer = [&](const Content::EnemyDefinition& type) {
                const auto at = FindSpawnPoint(player);
                Spawn(type, at, std::atan2(player.x - at.x, player.y - at.y));
            };
            const auto [lightCount, heavyCount] = WaveComposition(wave);
            for (std::int32_t i = 0; i < heavyCount; ++i) spawnFacingPlayer(heavy_ ? *heavy_ : *light_);
            for (std::int32_t i = 0; i < lightCount; ++i) spawnFacingPlayer(*light_);
        }

        /// <summary>
        /// Add one enemy at a point, facing a yaw. Without rising (the spawn animation) it is ready
        /// at once (tests, scripted placements).
        /// </summary>
        std::uint32_t Spawn(const Content::EnemyDefinition& type, WorldPoint at, float facingYaw, bool rise = true) {
            Enemy enemy;
            enemy.id = nextId_++;
            enemy.definition = &type;
            enemy.position = ClampToArena(at);
            enemy.facingYaw = facingYaw;
            enemy.hitPoints = type.hitPoints;
            Enter(enemy, rise ? EnemyState::Spawning : EnemyState::Idle);
            enemies_.push_back(std::move(enemy));
            return enemies_.back().id;
        }

        /// <summary>
        /// One simulation step: AI and movement for every enemy, attacks against the player, corpse
        /// clean-up, and the next wave once one is cleared. Returns the enemy attacks resolved this
        /// step (the engine applies their damage to the player).
        /// </summary>
        std::vector<CombatEvent> Update(float elapsedSeconds, const PlayerTarget& player) {
            std::vector<CombatEvent> events;
            for (auto& enemy : enemies_) {
                UpdateEnemy(enemy, elapsedSeconds, player, events);
            }
            for (int pass = 0; pass < SeparationPasses; ++pass) {
                Separate(player);
            }

            std::erase_if(enemies_, [](const Enemy& e) { return e.IsDead() && e.stateSeconds >= CorpseSeconds; });

            if (wave_ > 0 && LivingEnemyCount() == 0) {
                waveTimerSeconds_ += elapsedSeconds;
                if (waveTimerSeconds_ >= WaveDelaySeconds) {
                    StartWave(wave_ + 1, player.position);
                }
            }
            return events;
        }

        /// <summary>
        /// The nearest enemy that can be struck from a point within reach (between centers), if any
        /// </summary>
        [[nodiscard]] std::optional<std::uint32_t> FindTarget(WorldPoint from, float reach) const {
            std::optional<std::uint32_t> best;
            float bestDistance = reach;
            for (const auto& enemy : enemies_) {
                if (!enemy.IsTargetable()) continue;
                const float distance = Distance(from, enemy.position);
                if (distance <= bestDistance) {
                    bestDistance = distance;
                    best = enemy.id;
                }
            }
            return best;
        }

        /// <summary>
        /// Resolve the player's attack on an enemy from a point. Null when the enemy is gone, not
        /// targetable, or out of reach (the swing whiffs). A hit that does not kill staggers the
        /// enemy unless it is mid-attack.
        /// </summary>
        std::optional<CombatEvent> StrikeEnemy(std::uint32_t id, const Services::Combat::WeaponAttackProfile& attack, WorldPoint from, float reach) {
            auto* enemy = Find(id);
            if (!enemy || !enemy->IsTargetable() || Distance(from, enemy->position) > reach + AttackReachSlack) {
                return std::nullopt;
            }

            const auto& type = *enemy->definition;
            const Services::Combat::DefenseProfile defense{ type.armorClass, type.physicalArmor, type.resistances };

            CombatEvent event{ CombatEventKind::PlayerAttack, id, combat_.ResolveWeaponAttack(attack, defense), false };
            if (event.result.hit) {
                enemy->hitPoints = std::max(0, enemy->hitPoints - event.result.finalDamage);
                enemy->hitFlashSeconds = HitFlashSeconds;
                if (enemy->hitPoints == 0) {
                    event.killed = true;
                    Enter(*enemy, EnemyState::Dead);
                }
                else if (enemy->state != EnemyState::Attacking) {
                    Enter(*enemy, EnemyState::Staggered);
                }
            }
            return event;
        }

        [[nodiscard]] const std::vector<Enemy>& GetEnemies() const noexcept { return enemies_; }

        [[nodiscard]] const Enemy* FindEnemy(std::uint32_t id) const noexcept {
            const auto it = std::ranges::find(enemies_, id, &Enemy::id);
            return it != enemies_.end() ? &*it : nullptr;
        }

        /// <summary>
        /// Enemies still standing (spawning included)
        /// </summary>
        [[nodiscard]] std::size_t LivingEnemyCount() const noexcept {
            return static_cast<std::size_t>(std::ranges::count_if(enemies_, [](const Enemy& e) { return !e.IsDead(); }));
        }

        /// <summary>
        /// The current wave (0 before the first)
        /// </summary>
        [[nodiscard]] std::int32_t GetWave() const noexcept { return wave_; }

        /// <summary>
        /// How visible a corpse still is (1 until it starts fading)
        /// </summary>
        [[nodiscard]] static float Opacity(const Enemy& enemy) noexcept {
            if (!enemy.IsDead()) return 1.0f;
            const float fadeStart = CorpseSeconds - CorpseFadeSeconds;
            return std::clamp(1.0f - (enemy.stateSeconds - fadeStart) / CorpseFadeSeconds, 0.0f, 1.0f);
        }

        /// <summary>
        /// The ground direction a yaw faces
        /// </summary>
        [[nodiscard]] static WorldPoint Forward(float yaw) noexcept {
            return { std::sin(yaw), std::cos(yaw) };
        }

        [[nodiscard]] static float Distance(WorldPoint a, WorldPoint b) noexcept {
            return std::hypot(b.x - a.x, b.y - a.y);
        }

    private:
        Enemy* Find(std::uint32_t id) noexcept {
            const auto it = std::ranges::find(enemies_, id, &Enemy::id);
            return it != enemies_.end() ? &*it : nullptr;
        }

        static Services::Combat::WeaponAttackProfile AttackOf(const Content::EnemyDefinition& type) {
            return { type.attackBonus, type.damageDice, type.damageBonus, type.damageType, type.critRangeMin, type.critMultiplier };
        }

        // Change state and start the state's animation
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

        void UpdateEnemy(Enemy& enemy, float dt, const PlayerTarget& player, std::vector<CombatEvent>& events) {
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
                return;

            case EnemyState::Dead:
                return;

            case EnemyState::Staggered:
                if (enemy.stateSeconds >= StaggerSeconds) Enter(enemy, EnemyState::Chasing);
                return;

            case EnemyState::Attacking:
                TurnToward(enemy, dx, dy, dt);
                if (!enemy.impactDone && enemy.stateSeconds >= type.attackTimeSeconds * type.attackImpactFraction) {
                    enemy.impactDone = true;
                    if (player.alive && distance <= type.attackRange + AttackReachSlack) {
                        events.push_back({ CombatEventKind::EnemyAttack, enemy.id, combat_.ResolveWeaponAttack(AttackOf(type), player.defense), false });
                    }
                }
                if (enemy.stateSeconds >= type.attackTimeSeconds) {
                    enemy.recoverySeconds = type.attackRecoverySeconds;
                    Enter(enemy, EnemyState::Chasing);
                }
                return;

            case EnemyState::Idle:
            case EnemyState::Chasing:
                break;
            }

            enemy.recoverySeconds = std::max(0.0f, enemy.recoverySeconds - dt);
            if (!player.alive) {
                if (enemy.state != EnemyState::Idle) Enter(enemy, EnemyState::Idle);
                return;
            }
            if (enemy.state == EnemyState::Idle) {
                if (distance > type.aggroRange) return;
                Enter(enemy, EnemyState::Chasing);
            }

            // In reach: face the player and attack when ready
            if (distance <= type.attackRange) {
                TurnToward(enemy, dx, dy, dt);
                const auto forward = Forward(enemy.facingYaw);
                const bool facing = distance <= 0.0f || (forward.x * dx + forward.y * dy) / distance >= AttackConeCosine;
                if (enemy.recoverySeconds <= 0.0f && facing) {
                    Enter(enemy, EnemyState::Attacking);
                    return;
                }
                enemy.animator.Play(clips_.idle, true);
                return;
            }

            // Close in, stopping a little inside reach
            const float step = std::min(type.movementSpeed * dt, distance - type.attackRange * 0.9f);
            enemy.position = ClampToArena({ enemy.position.x + dx / distance * step, enemy.position.y + dy / distance * step });
            TurnToward(enemy, dx, dy, dt);
            enemy.animator.Play(clips_.walk, true, type.movementSpeed / WalkAnimationSpeed);
        }

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

        // Push overlapping enemies apart, and out of the player (the player is never pushed)
        void Separate(const PlayerTarget& player) {
            constexpr float minimum = CharacterRadius * 2.0f;
            for (std::size_t i = 0; i < enemies_.size(); ++i) {
                auto& a = enemies_[i];
                if (a.IsDead()) continue;
                for (std::size_t j = i + 1; j < enemies_.size(); ++j) {
                    auto& b = enemies_[j];
                    if (b.IsDead()) continue;
                    const float dx = b.position.x - a.position.x;
                    const float dy = b.position.y - a.position.y;
                    const float distance = std::hypot(dx, dy);
                    if (distance >= minimum) continue;
                    // Coincident bodies separate along a fixed axis
                    const float nx = distance > 1e-4f ? dx / distance : 1.0f;
                    const float ny = distance > 1e-4f ? dy / distance : 0.0f;
                    const float push = (minimum - distance) * 0.5f;
                    a.position = ClampToArena({ a.position.x - nx * push, a.position.y - ny * push });
                    b.position = ClampToArena({ b.position.x + nx * push, b.position.y + ny * push });
                }

                if (player.alive) {
                    const float dx = a.position.x - player.position.x;
                    const float dy = a.position.y - player.position.y;
                    const float distance = std::hypot(dx, dy);
                    if (distance < minimum) {
                        const float nx = distance > 1e-4f ? dx / distance : 1.0f;
                        const float ny = distance > 1e-4f ? dy / distance : 0.0f;
                        a.position = ClampToArena({ player.position.x + nx * minimum, player.position.y + ny * minimum });
                    }
                }
            }
        }

        WorldPoint ClampToArena(WorldPoint point) const noexcept {
            const float x = std::max(0.0f, arenaHalfWidth_ - CharacterRadius);
            const float y = std::max(0.0f, arenaHalfHeight_ - CharacterRadius);
            return { std::clamp(point.x, -x, x), std::clamp(point.y, -y, y) };
        }

        // A random spot in the arena away from the player and the other enemies (the farthest
        // candidate when none qualifies)
        WorldPoint FindSpawnPoint(WorldPoint player) {
            constexpr float margin = 1.5f;
            std::uniform_real_distribution<float> x(-arenaHalfWidth_ + margin, arenaHalfWidth_ - margin);
            std::uniform_real_distribution<float> y(-arenaHalfHeight_ + margin, arenaHalfHeight_ - margin);

            WorldPoint best{};
            float bestScore = -1.0f;
            for (int attempt = 0; attempt < 32; ++attempt) {
                const WorldPoint candidate{ x(rng_), y(rng_) };
                float nearestEnemy = std::numeric_limits<float>::max();
                for (const auto& enemy : enemies_) {
                    if (!enemy.IsDead()) nearestEnemy = std::min(nearestEnemy, Distance(candidate, enemy.position));
                }
                const float fromPlayer = Distance(candidate, player);
                if (fromPlayer >= MinimumSpawnDistance && nearestEnemy >= 1.0f) return candidate;

                const float score = std::min(fromPlayer, nearestEnemy * MinimumSpawnDistance);
                if (score > bestScore) {
                    bestScore = score;
                    best = candidate;
                }
            }
            return best;
        }

        Services::Combat::CombatService combat_;
        std::mt19937 rng_;
        const Content::EnemyDefinition* light_{nullptr};
        const Content::EnemyDefinition* heavy_{nullptr};
        float arenaHalfWidth_{10.0f};
        float arenaHalfHeight_{7.0f};
        EnemyClips clips_{};
        std::vector<Enemy> enemies_;
        std::uint32_t nextId_{1};
        std::int32_t wave_{0};
        float waveTimerSeconds_{0.0f};
    };

} // namespace LB::Engine
