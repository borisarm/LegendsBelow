// Encounter.ixx - The enemies of a floor: spawning them, running their behavior each step, the
// test arena's waves, and resolving attacks both ways with CombatService
export module LB.Engine:Encounter;

import std;
import LB.Content;
import LB.CombatService;
import :World;
import :Enemies;
import :EnemyBehavior;
import :Waves;

export namespace LB::Engine {

    /// <summary>
    /// The enemies of a floor: spawns them (waves of a light and a heavy type in the test arena,
    /// groups in dungeon rooms), runs their behavior (EnemyBehavior) each step, and resolves attacks
    /// both ways with CombatService. Rendering-independent: animation clips are optional.
    /// </summary>
    class Encounter {
    public:
        // The enemies' rules, as the rest of the engine knows them (EnemyBehavior, Waves)
        static constexpr float CharacterRadius = EnemyBehavior::CharacterRadius;
        static constexpr float AttackImpactFraction = EnemyBehavior::AttackImpactFraction;
        static constexpr float AttackReachSlack = EnemyBehavior::AttackReachSlack;
        static constexpr float SpawnSeconds = EnemyBehavior::SpawnSeconds;
        static constexpr float HitFlashSeconds = EnemyBehavior::HitFlashSeconds;
        static constexpr float CorpseSeconds = EnemyBehavior::CorpseSeconds;
        static constexpr float CorpseFadeSeconds = EnemyBehavior::CorpseFadeSeconds;
        static constexpr float WaveDelaySeconds = Waves::DelaySeconds;
        static constexpr float MinimumSpawnDistance = Waves::MinimumSpawnDistance;
        using WaveScaling = Waves::Scaling;

        /// <summary>
        /// Encounter with nondeterministic dice and spawn points
        /// </summary>
        Encounter() : combat_(), rng_(std::random_device{}()) {}

        /// <summary>
        /// Encounter whose dice and spawn points are reproducible (tests, replays)
        /// </summary>
        explicit Encounter(std::uint32_t seed) : combat_(seed), rng_(seed) {}

        /// <summary>
        /// Enemy types for waves (heavy may be null: light enemies only) and the world they move in
        /// (kept by reference: it must outlive the encounter's use of it)
        /// </summary>
        void Configure(const Content::EnemyDefinition* light, const Content::EnemyDefinition* heavy, const WorldMap& world) {
            light_ = light;
            heavy_ = heavy;
            world_ = &world;
            behavior_.SetWorld(&world);
        }

        void SetClips(const EnemyClips& clips) {
            behavior_.SetClips(clips);
        }

        /// <summary>
        /// Remove every enemy and stop the waves
        /// </summary>
        void Reset() {
            enemies_.clear();
            wave_ = 0;
            waveTimerSeconds_ = 0.0f;
        }

        [[nodiscard]] static WaveScaling ScalingFor(std::int32_t wave) noexcept { return Waves::ScalingFor(wave); }

        [[nodiscard]] static std::pair<std::int32_t, std::int32_t> WaveComposition(std::int32_t wave) noexcept { return Waves::Composition(wave); }

        /// <summary>
        /// Start a wave: its enemies rise at random spots at least MinimumSpawnDistance from the player
        /// </summary>
        void StartWave(std::int32_t wave, WorldPoint player) {
            wave_ = wave;
            waveTimerSeconds_ = 0.0f;
            if (!light_) return;

            const auto spawnFacingPlayer = [&](const Content::EnemyDefinition& type) {
                const auto at = Waves::WaveSpawnPoint(world_, player, enemies_, rng_);
                Spawn(type, at, std::atan2(player.x - at.x, player.y - at.y), true, wave);
            };
            const auto [lightCount, heavyCount] = Waves::Composition(wave);
            for (std::int32_t i = 0; i < heavyCount; ++i) spawnFacingPlayer(heavy_ ? *heavy_ : *light_);
            for (std::int32_t i = 0; i < lightCount; ++i) spawnFacingPlayer(*light_);
        }

        /// <summary>
        /// Add one enemy at a point, facing a yaw, as tough as a wave makes it. Without rising (the
        /// spawn animation) it is ready at once (tests, scripted placements).
        /// </summary>
        std::uint32_t Spawn(const Content::EnemyDefinition& type, WorldPoint at, float facingYaw, bool rise = true, std::int32_t wave = 1) {
            const auto scaling = Waves::ScalingFor(wave);
            Enemy enemy;
            enemy.id = nextId_++;
            enemy.definition = &type;
            enemy.position = at;
            enemy.facingYaw = facingYaw;
            enemy.maxHitPoints = std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(type.hitPoints) * scaling.hitPoints)));
            enemy.hitPoints = enemy.maxHitPoints;
            enemy.bonusDamage = scaling.damage;
            enemy.experience = static_cast<std::int32_t>(std::lround(static_cast<float>(type.experience) * scaling.experience));
            behavior_.Enter(enemy, rise ? EnemyState::Spawning : EnemyState::Idle);
            enemies_.push_back(std::move(enemy));
            return enemies_.back().id;
        }

        /// <summary>
        /// One simulation step: every enemy acts, the strikes that land on the player are resolved,
        /// bodies are pushed apart, corpses cleared, and the next wave starts once one is cleared.
        /// Returns the enemy attacks resolved this step (the engine applies their damage).
        /// </summary>
        std::vector<CombatEvent> Update(float elapsedSeconds, const PlayerTarget& player) {
            std::vector<CombatEvent> events;
            behavior_.BeginStep();
            for (auto& enemy : enemies_) {
                if (behavior_.Update(enemy, elapsedSeconds, player)) {
                    events.push_back({ CombatEventKind::EnemyAttack, enemy.id, combat_.ResolveWeaponAttack(AttackOf(enemy), player.defense), false });
                }
            }
            for (int pass = 0; pass < EnemyBehavior::SeparationPasses; ++pass) {
                behavior_.Separate(enemies_, player);
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
        /// Raise a group in a room of the world: each enemy at a random spot of the room away from
        /// the player (when the room allows), facing them, as tough as `depth` makes it. Returns
        /// the new enemies' ids.
        /// </summary>
        std::vector<std::uint32_t> SpawnInRoom(const Content::EnemyDefinition& type, std::int32_t count, std::int32_t room,
                                               WorldPoint player, std::int32_t depth) {
            std::vector<std::uint32_t> ids;
            if (!world_) return ids;
            for (std::int32_t i = 0; i < count; ++i) {
                const auto at = Waves::RoomSpawnPoint(*world_, room, player, enemies_, CharacterRadius, rng_);
                ids.push_back(Spawn(type, at, std::atan2(player.x - at.x, player.y - at.y), true, depth));
            }
            return ids;
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
                    behavior_.Enter(*enemy, EnemyState::Dead);
                }
                else if (enemy->state != EnemyState::Attacking) {
                    behavior_.Enter(*enemy, EnemyState::Staggered);
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
        [[nodiscard]] static float Opacity(const Enemy& enemy) noexcept { return EnemyBehavior::Opacity(enemy); }

        [[nodiscard]] static WorldPoint Forward(float yaw) noexcept { return LB::Engine::Forward(yaw); }

        [[nodiscard]] static float Distance(WorldPoint a, WorldPoint b) noexcept { return LB::Engine::Distance(a, b); }

    private:
        Enemy* Find(std::uint32_t id) noexcept {
            const auto it = std::ranges::find(enemies_, id, &Enemy::id);
            return it != enemies_.end() ? &*it : nullptr;
        }

        static Services::Combat::WeaponAttackProfile AttackOf(const Enemy& enemy) {
            const auto& type = *enemy.definition;
            return { type.attackBonus, type.damageDice, type.damageBonus + enemy.bonusDamage, type.damageType, type.critRangeMin, type.critMultiplier };
        }

        Services::Combat::CombatService combat_;
        std::mt19937 rng_;
        EnemyBehavior behavior_;
        const Content::EnemyDefinition* light_{nullptr};
        const Content::EnemyDefinition* heavy_{nullptr};
        const WorldMap* world_{nullptr};
        std::vector<Enemy> enemies_;
        std::uint32_t nextId_{1};
        std::int32_t wave_{0};
        float waveTimerSeconds_{0.0f};
    };

} // namespace LB::Engine
