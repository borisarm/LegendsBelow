// Encounter.ixx - The enemies of a floor: spawning them, running their behavior each step, the
// test arena's waves, the arrows and bolts in flight, and resolving attacks both ways with
// CombatService
export module LB.Engine:Encounter;

import std;
import LB.Content;
import LB.CombatService;
import :World;
import :Enemies;
import :EnemyBehavior;
import :Projectiles;
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
        // Enemy shots fly this much past the shooter's range (a target stepping back is still hit)
        static constexpr float ShotOvershoot = 1.3f;
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
        /// Enemy types for waves (heavy may be null: light enemies only; ranged may be null: no
        /// shooters) and the world they move in (kept by reference: it must outlive the
        /// encounter's use of it)
        /// </summary>
        void Configure(const Content::EnemyDefinition* light, const Content::EnemyDefinition* heavy, const WorldMap& world,
                       const Content::EnemyDefinition* ranged = nullptr) {
            light_ = light;
            heavy_ = heavy;
            ranged_ = ranged;
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
            projectiles_.Clear();
            wave_ = 0;
            waveTimerSeconds_ = 0.0f;
        }

        [[nodiscard]] static WaveScaling ScalingFor(std::int32_t wave) noexcept { return Waves::ScalingFor(wave); }

        [[nodiscard]] static std::pair<std::int32_t, std::int32_t> WaveComposition(std::int32_t wave) noexcept { return Waves::Composition(wave); }

        [[nodiscard]] static std::int32_t WaveShooters(std::int32_t wave) noexcept { return Waves::Shooters(wave); }

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
            if (ranged_) {
                for (std::int32_t i = 0; i < Waves::Shooters(wave); ++i) spawnFacingPlayer(*ranged_);
            }
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
            shots_.clear();
            wallHits_.clear();
            behavior_.BeginStep();
            for (auto& enemy : enemies_) {
                switch (behavior_.Update(enemy, elapsedSeconds, player)) {
                case EnemyAction::Strike:
                    events.push_back({ CombatEventKind::EnemyAttack, enemy.id, combat_.ResolveWeaponAttack(AttackOf(enemy), player.defense), false });
                    break;
                case EnemyAction::Shoot:
                    ShootAtPlayer(enemy, player.position);
                    break;
                case EnemyAction::None:
                    break;
                }
            }
            for (int pass = 0; pass < EnemyBehavior::SeparationPasses; ++pass) {
                behavior_.Separate(enemies_, player);
            }
            UpdateProjectiles(elapsedSeconds, player, events);

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
        /// The nearest enemy that can be struck from a point within reach (between centers), if
        /// any; with `inSight`, only one the world does not hide (a shot's target)
        /// </summary>
        [[nodiscard]] std::optional<std::uint32_t> FindTarget(WorldPoint from, float reach, bool inSight = false) const {
            std::optional<std::uint32_t> best;
            float bestDistance = reach;
            for (const auto& enemy : enemies_) {
                if (!enemy.IsTargetable()) continue;
                if (inSight && world_ && !world_->LineOfSight(from, enemy.position)) continue;
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
            return Hit(*enemy, attack);
        }

        /// <summary>
        /// The player's shot: an arrow or bolt leaving `from` (it starts just outside the shooter's
        /// body) along `direction`; it hits the first enemy in its way within `range` tiles, as
        /// resolved when it arrives (Update returns that attack). Returns its id.
        /// </summary>
        std::uint32_t Shoot(const Services::Combat::WeaponAttackProfile& attack, WorldPoint from, WorldPoint direction, float speed, float range,
                            std::string model, float bodyRadius) {
            const float length = std::max(1e-5f, std::hypot(direction.x, direction.y));
            const float offset = bodyRadius + ProjectileSet::Radius;
            Projectile shot{};
            shot.side = ProjectileSide::Player;
            shot.position = { from.x + direction.x / length * offset, from.y + direction.y / length * offset };
            shot.direction = direction;
            shot.speed = speed;
            shot.rangeLeft = range;
            shot.attack = attack;
            shot.model = std::move(model);
            return projectiles_.Launch(std::move(shot));
        }

        /// <summary>
        /// The arrows and bolts in flight, and those stuck in walls
        /// </summary>
        [[nodiscard]] const std::vector<Projectile>& GetProjectiles() const noexcept { return projectiles_.Get(); }

        /// <summary>
        /// Where enemies shot from, and where projectiles struck walls, in the last Update
        /// </summary>
        [[nodiscard]] const std::vector<WorldPoint>& GetShots() const noexcept { return shots_; }
        [[nodiscard]] const std::vector<WorldPoint>& GetWallHits() const noexcept { return wallHits_; }

    private:
        // Resolve an attack that reached an enemy: a hit that does not kill staggers it unless it
        // is mid-attack
        CombatEvent Hit(Enemy& enemy, const Services::Combat::WeaponAttackProfile& attack) {
            const auto id = enemy.id;
            const auto& type = *enemy.definition;
            const Services::Combat::DefenseProfile defense{ type.armorClass, type.physicalArmor, type.resistances };

            CombatEvent event{ CombatEventKind::PlayerAttack, id, combat_.ResolveWeaponAttack(attack, defense), false };
            if (event.result.hit) {
                enemy.hitPoints = std::max(0, enemy.hitPoints - event.result.finalDamage);
                enemy.hitFlashSeconds = HitFlashSeconds;
                if (enemy.hitPoints == 0) {
                    event.killed = true;
                    behavior_.Enter(enemy, EnemyState::Dead);
                }
                else if (enemy.state != EnemyState::Attacking) {
                    behavior_.Enter(enemy, EnemyState::Staggered);
                }
            }
            return event;
        }

    public:
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
        // A ranged enemy's shot at where the player stands now (it can be dodged), flying a little
        // past the enemy's range
        void ShootAtPlayer(const Enemy& enemy, WorldPoint player) {
            const auto& type = *enemy.definition;
            WorldPoint direction{ player.x - enemy.position.x, player.y - enemy.position.y };
            if (std::hypot(direction.x, direction.y) < 1e-4f) direction = Forward(enemy.facingYaw);
            const float length = std::hypot(direction.x, direction.y);
            const float offset = CharacterRadius + ProjectileSet::Radius;
            Projectile shot{};
            shot.side = ProjectileSide::Enemies;
            shot.shooter = enemy.id;
            shot.position = { enemy.position.x + direction.x / length * offset, enemy.position.y + direction.y / length * offset };
            shot.direction = direction;
            shot.speed = type.projectileSpeed;
            shot.rangeLeft = type.attackRange * ShotOvershoot;
            shot.attack = AttackOf(enemy);
            shot.model = type.projectileModel;
            projectiles_.Launch(std::move(shot));
            shots_.push_back(enemy.position);
        }

        // Move the projectiles; resolve those that reached a body (the player, or an enemy for the
        // player's shots) and note those that struck walls
        void UpdateProjectiles(float elapsedSeconds, const PlayerTarget& player, std::vector<CombatEvent>& events) {
            std::vector<ProjectileTarget> targets;
            if (player.alive) targets.push_back({ 0, ProjectileSide::Player, player.position, player.radius });
            for (const auto& enemy : enemies_) {
                if (enemy.IsTargetable()) targets.push_back({ enemy.id, ProjectileSide::Enemies, enemy.position, CharacterRadius });
            }
            for (const auto& impact : projectiles_.Update(elapsedSeconds, world_, targets)) {
                if (!impact.target) {
                    wallHits_.push_back(impact.at);
                }
                else if (impact.projectile.side == ProjectileSide::Enemies) {
                    events.push_back({ CombatEventKind::EnemyAttack, impact.projectile.shooter, combat_.ResolveWeaponAttack(impact.projectile.attack, player.defense), false });
                }
                else if (auto* enemy = Find(*impact.target)) {
                    events.push_back(Hit(*enemy, impact.projectile.attack));
                }
            }
        }

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
        ProjectileSet projectiles_;
        std::vector<WorldPoint> shots_;
        std::vector<WorldPoint> wallHits_;
        const Content::EnemyDefinition* light_{nullptr};
        const Content::EnemyDefinition* heavy_{nullptr};
        const Content::EnemyDefinition* ranged_{nullptr};
        const WorldMap* world_{nullptr};
        std::vector<Enemy> enemies_;
        std::uint32_t nextId_{1};
        std::int32_t wave_{0};
        float waveTimerSeconds_{0.0f};
    };

} // namespace LB::Engine
