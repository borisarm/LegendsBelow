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
        // Opportunity attacks (design-combat-system.md 3.2): one per enemy this often at most (a
        // reaction: a hero running from a chase is not struck at every stride)
        static constexpr float OpportunitySeconds = 3.0f;
        // Flanking (design-combat-system.md 7.1): an attacker with another on the far side of its
        // target (more than ~120 degrees around it) gains this to hit
        static constexpr std::int32_t FlankAttackBonus = 2;
        static constexpr float FlankOppositeCosine = -0.5f;
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
                       const Content::EnemyDefinition* ranged = nullptr, const Content::EnemyDefinition* caster = nullptr,
                       const Content::EnemyDefinition* brute = nullptr) {
            light_ = light;
            heavy_ = heavy;
            ranged_ = ranged;
            caster_ = caster;
            brute_ = brute;
            world_ = &world;
            behavior_.SetWorld(&world);
        }

        void SetClips(const EnemyClips& clips) {
            behavior_.SetClips(clips);
        }

        /// <summary>
        /// The content spells and statuses are looked up in (kept by pointer: it must outlive the
        /// encounter's use of it; null: casters cannot cast and spells apply no statuses)
        /// </summary>
        void SetContent(const Content::ContentDatabase* content) noexcept { content_ = content; }

        /// <summary>
        /// Remove every enemy and stop the waves
        /// </summary>
        /// <summary>
        /// What every enemy raised from now on gains besides its wave's scaling: attack bonus,
        /// armour class, and a share more hit points (New Game+, design-loot-and-progression.md
        /// 7.3; nothing on a first descent)
        /// </summary>
        void SetVeterans(std::int32_t attackBonus, std::int32_t armorClass, float extraHitPoints = 0.0f, std::int32_t damageBonus = 0,
                         float lifeSteal = 0.0f) noexcept {
            veteranAttack_ = attackBonus;
            veteranArmorClass_ = armorClass;
            veteranHitPoints_ = extraHitPoints;
            veteranDamage_ = damageBonus;
            veteranLifeSteal_ = lifeSteal;
        }

        void Reset() {
            enemies_.clear();
            projectiles_.Clear();
            bursts_.clear();
            wave_ = 0;
            waveTimerSeconds_ = 0.0f;
        }

        [[nodiscard]] static WaveScaling ScalingFor(std::int32_t wave) noexcept { return Waves::ScalingFor(wave); }

        /// <summary>
        /// Make an enemy a floor's champion: named, tougher, harder hitting, worth more
        /// </summary>
        bool Promote(std::uint32_t id, std::string title, float hitPointsMultiplier, std::int32_t damageBonus, float experienceMultiplier) {
            auto* enemy = Find(id);
            if (!enemy || enemy->IsDead()) return false;
            enemy->champion = true;
            enemy->title = std::move(title);
            enemy->maxHitPoints = static_cast<std::int32_t>(std::lround(static_cast<float>(enemy->maxHitPoints) * hitPointsMultiplier));
            enemy->hitPoints = enemy->maxHitPoints;
            enemy->bonusDamage += damageBonus;
            enemy->experience = static_cast<std::int32_t>(std::lround(static_cast<float>(enemy->experience) * experienceMultiplier));
            return true;
        }

        /// <summary>
        /// How long a knockback carries its body (it covers its distance in this time)
        /// </summary>
        static constexpr float KnockSeconds = 0.18f;

        /// <summary>
        /// Throw an enemy `tiles` straight away from `from` (a heavy blow); bosses and the
        /// unstaggerable stand fast. Returns whether it was thrown.
        /// </summary>
        bool Knock(std::uint32_t id, WorldPoint from, float tiles) {
            auto* enemy = Find(id);
            if (!enemy || enemy->IsDead() || tiles <= 0.0f || enemy->definition->boss || enemy->definition->staggerImmune) return false;
            const float dx = enemy->position.x - from.x;
            const float dy = enemy->position.y - from.y;
            const float length = std::hypot(dx, dy);
            if (length < 1e-4f) return false;
            enemy->knockVelocity = { dx / length * tiles / KnockSeconds, dy / length * tiles / KnockSeconds };
            enemy->knockSeconds = KnockSeconds;
            return true;
        }

        [[nodiscard]] static std::pair<std::int32_t, std::int32_t> WaveComposition(std::int32_t wave) noexcept { return Waves::Composition(wave); }

        [[nodiscard]] static std::int32_t WaveShooters(std::int32_t wave) noexcept { return Waves::Shooters(wave); }

        [[nodiscard]] static std::int32_t WaveCasters(std::int32_t wave) noexcept { return Waves::Casters(wave); }

        [[nodiscard]] static std::int32_t WaveBrutes(std::int32_t wave) noexcept { return Waves::Brutes(wave); }

        /// <summary>
        /// Start a wave: its enemies rise at random spots at least MinimumSpawnDistance from the player
        /// </summary>
        void StartWave(std::int32_t wave, WorldPoint player) {
            wave_ = wave;
            waveTimerSeconds_ = 0.0f;
            if (!light_) return;

            // A wave is one group: they notice the player together
            const auto group = NextGroup();
            const auto spawnFacingPlayer = [&](const Content::EnemyDefinition& type) {
                const auto at = Waves::WaveSpawnPoint(world_, player, enemies_, rng_);
                Spawn(type, at, std::atan2(player.x - at.x, player.y - at.y), true, wave, group);
            };
            const auto [lightCount, heavyCount] = Waves::Composition(wave);
            for (std::int32_t i = 0; i < heavyCount; ++i) spawnFacingPlayer(heavy_ ? *heavy_ : *light_);
            for (std::int32_t i = 0; i < lightCount; ++i) spawnFacingPlayer(*light_);
            if (ranged_) {
                for (std::int32_t i = 0; i < Waves::Shooters(wave); ++i) spawnFacingPlayer(*ranged_);
            }
            if (caster_) {
                for (std::int32_t i = 0; i < Waves::Casters(wave); ++i) spawnFacingPlayer(*caster_);
            }
            if (brute_) {
                for (std::int32_t i = 0; i < Waves::Brutes(wave); ++i) spawnFacingPlayer(*brute_);
            }
        }

        /// <summary>
        /// Add one enemy at a point, facing a yaw, as tough as a wave makes it. Without rising (the
        /// spawn animation) it is ready at once (tests, scripted placements).
        /// </summary>
        std::uint32_t Spawn(const Content::EnemyDefinition& type, WorldPoint at, float facingYaw, bool rise = true, std::int32_t wave = 1,
                            std::uint32_t group = 0) {
            const auto scaling = Waves::ScalingFor(wave);
            Enemy enemy;
            enemy.id = nextId_++;
            enemy.definition = &type;
            enemy.position = at;
            enemy.facingYaw = facingYaw;
            enemy.maxHitPoints = std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(type.hitPoints) * scaling.hitPoints * (1.0f + veteranHitPoints_))));
            enemy.hitPoints = enemy.maxHitPoints;
            enemy.bonusDamage = scaling.damage + veteranDamage_;
            enemy.bonusLifeSteal = veteranLifeSteal_;
            enemy.bonusAttack = veteranAttack_;
            enemy.bonusArmorClass = veteranArmorClass_;
            enemy.experience = static_cast<std::int32_t>(std::lround(static_cast<float>(type.experience) * scaling.experience));
            enemy.wave = wave;
            enemy.group = group;
            enemy.summonSeconds = type.summonCooldownSeconds * 0.5f;   // the first raising comes early
            behavior_.Enter(enemy, rise ? EnemyState::Spawning : EnemyState::Idle);
            enemies_.push_back(std::move(enemy));
            return enemies_.back().id;
        }

        /// <summary>
        /// One simulation step: every enemy acts, the strikes that land on the player are resolved,
        /// bodies are pushed apart, corpses cleared, and the next wave starts once one is cleared.
        /// Returns the enemy attacks resolved this step (the engine applies their damage).
        /// </summary>
        std::vector<CombatEvent> Update(float elapsedSeconds, const PlayerTarget& player, std::span<const PlayerTarget> allies = {}) {
            std::vector<CombatEvent> events = std::move(opportunities_);
            opportunities_.clear();
            for (auto& enemy : enemies_) enemy.opportunitySeconds = std::max(0.0f, enemy.opportunitySeconds - elapsedSeconds);
            shots_.clear();
            wallHits_.clear();
            bursts_.clear();
            phaseChanges_.clear();
            behavior_.BeginStep();
            AlertGroups();
            const auto formations = Formations(player.position);
            for (auto& enemy : enemies_) {
                UpdateStatuses(enemy, elapsedSeconds, events);
                const auto formation = formations.find(enemy.group);
                const auto& target = TargetOf(enemy, player, allies);
                switch (behavior_.Update(enemy, elapsedSeconds, target, formation != formations.end() ? formation->second : Formation{})) {
                case EnemyAction::Strike: {
                    auto attack = AttackOf(enemy);
                    if (Flanking(enemy, target.position)) attack.attackBonus += FlankAttackBonus;
                    events.push_back({ CombatEventKind::EnemyAttack, enemy.id, combat_.ResolveWeaponAttack(attack, target.defense), false, nullptr, target.ally });
                    // Life drain: its blow heals it
                    const auto& dealt = events.back().result;
                    const float lifeSteal = enemy.definition->lifeSteal + enemy.bonusLifeSteal;
                    if (lifeSteal > 0.0f && dealt.hit && dealt.finalDamage > 0) {
                        const auto healed = std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(dealt.finalDamage) * lifeSteal)));
                        enemy.hitPoints = std::min(enemy.maxHitPoints, enemy.hitPoints + healed);
                    }
                    break;
                }
                case EnemyAction::Shoot:
                    if (enemy.definition->IsCaster()) CastAtPlayer(enemy, target.position);
                    else ShootAtPlayer(enemy, target.position);
                    break;
                case EnemyAction::Slam: {
                    // Everyone inside the ring is struck (through armour, like a spell)
                    const auto slam = enemy.ActiveSlam();
                    bursts_.push_back({ enemy.slamCenter, slam.radius, slam.damageType });
                    const auto strike = [&](const PlayerTarget& body) {
                        if (!body.alive || Distance(body.position, enemy.slamCenter) > slam.radius + body.radius) return;
                        auto dice = slam.damage;
                        dice.bonus += enemy.bonusDamage;
                        const auto damage = combat_.RollDice(dice);
                        events.push_back({ CombatEventKind::EnemyAttack, enemy.id, Services::Combat::CombatService::ResolveSpellHit(damage, slam.damageType, body.defense),
                                           false, nullptr, body.ally });
                    };
                    strike(player);
                    for (const auto& ally : allies) strike(ally);
                    break;
                }
                case EnemyAction::None:
                    break;
                }
            }
            for (int pass = 0; pass < EnemyBehavior::SeparationPasses; ++pass) {
                behavior_.Separate(enemies_, player, allies);
            }
            UpdateProjectiles(elapsedSeconds, player, allies, events);
            UpdateSummoners(elapsedSeconds, player);

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
                ids.push_back(Spawn(type, at, std::atan2(player.x - at.x, player.y - at.y), true, depth, RoomGroup(room)));
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
            return Hit(*enemy, combat_.ResolveWeaponAttack(attack, DefenseOf(*enemy)));
        }

        /// <summary>
        /// The player's spell missile leaving `from` along `direction` (it starts just outside the
        /// caster's body): it flies the spell's range at its speed and is resolved as the spell
        /// where it lands, bursting there when the spell has a radius. Returns its id.
        /// </summary>
        std::uint32_t CastMissile(const SpellPayload& payload, WorldPoint from, WorldPoint direction, float bodyRadius) {
            const auto& spell = *payload.spell;
            Services::Combat::WeaponAttackProfile attack{};
            attack.damageType = spell.damageType;
            const auto id = Shoot(attack, from, direction, spell.projectileSpeed, spell.range, std::string{}, bodyRadius);
            projectiles_.Find(id)->spell = payload;
            return id;
        }

        /// <summary>
        /// Opportunity attacks (design-combat-system.md 3.2): the hero walked from `from` to `to`,
        /// and every awake melee enemy not swinging whose reach they left strikes once (its basic attack, at
        /// most once every OpportunitySeconds, without breaking off what it is doing). The strikes
        /// come out of the next Update with the other enemy attacks. Dashes and teleports do not
        /// walk, so they leave freely.
        /// </summary>
        void CheckOpportunity(WorldPoint from, WorldPoint to, const Services::Combat::DefenseProfile& defense) {
            for (auto& enemy : enemies_) {
                // A reaction: not from one in the middle of its own swing
                if (!Threatens(enemy) || enemy.state != EnemyState::Chasing || enemy.opportunitySeconds > 0.0f) continue;
                const float reach = enemy.definition->attackRange + AttackReachSlack;
                if (Distance(enemy.position, from) > reach || Distance(enemy.position, to) <= reach) continue;
                enemy.opportunitySeconds = OpportunitySeconds;
                opportunities_.push_back({ CombatEventKind::EnemyAttack, enemy.id, combat_.ResolveWeaponAttack(AttackOf(enemy), defense), false });
            }
        }

        /// <summary>
        /// Would this enemy's blow at a target be flanking it (another enemy threatening it from
        /// its far side)? False for an enemy that is gone.
        /// </summary>
        [[nodiscard]] bool IsFlanking(std::uint32_t attacker, WorldPoint target) const noexcept {
            const auto* enemy = FindEnemy(attacker);
            return enemy && Flanking(*enemy, target);
        }

        /// <summary>
        /// Whom an enemy would go after among the player and the allies: an ally's id, 0 for the
        /// player (and for an enemy that is gone)
        /// </summary>
        [[nodiscard]] std::uint32_t TargetOf(std::uint32_t id, const PlayerTarget& player, std::span<const PlayerTarget> allies) const {
            const auto* enemy = FindEnemy(id);
            return enemy ? TargetOf(*enemy, player, allies).ally : 0;
        }

        /// <summary>
        /// Damage dealt straight to an enemy (a legendary power: lightning, thorns), through its
        /// resistances like a spell; null when it is gone
        /// </summary>
        std::optional<CombatEvent> DamageEnemy(std::uint32_t id, std::int32_t rawDamage, Services::Combat::DamageType type) {
            auto* enemy = Find(id);
            if (!enemy || !enemy->IsTargetable()) return std::nullopt;
            return Hit(*enemy, Services::Combat::CombatService::ResolveSpellHit(rawDamage, type, DefenseOf(*enemy)));
        }

        /// <summary>
        /// A status (statuses.json) on an enemy
        /// </summary>
        void ApplyStatus(std::uint32_t id, std::string_view statusKey) {
            auto* enemy = Find(id);
            if (!enemy || !enemy->IsTargetable() || !content_) return;
            if (const auto* status = content_->FindStatus(statusKey)) enemy->statuses.Apply(*status);
        }

        /// <summary>
        /// A spell striking an enemy at once (the player's smite); null when it is gone
        /// </summary>
        std::optional<CombatEvent> StrikeWithSpell(const SpellPayload& payload, std::uint32_t id) {
            auto* enemy = Find(id);
            if (!enemy || !enemy->IsTargetable()) return std::nullopt;
            return SpellHit(*enemy, payload);
        }

        /// <summary>
        /// A spell bursting around a point (the player's frost nova): every enemy within its radius
        /// takes it. Returns the hits.
        /// </summary>
        std::vector<CombatEvent> CastBurst(const SpellPayload& payload, WorldPoint at) {
            std::vector<CombatEvent> events;
            Burst(payload, at, events);
            return events;
        }

        /// <summary>
        /// Where spells burst in the last Update (and the last CastBurst)
        /// </summary>
        [[nodiscard]] const std::vector<SpellBurst>& GetBursts() const noexcept { return bursts_; }

        /// <summary>
        /// Bosses that turned to their second phase in the last Update
        /// </summary>
        [[nodiscard]] const std::vector<std::uint32_t>& GetPhaseChanges() const noexcept { return phaseChanges_; }

        /// <summary>
        /// The player's shot: an arrow or bolt leaving `from` (it starts just outside the shooter's
        /// body) along `direction`; it hits the first enemy in its way within `range` tiles, as
        /// resolved when it arrives (Update returns that attack). Returns its id.
        /// </summary>
        std::uint32_t Shoot(const Services::Combat::WeaponAttackProfile& attack, WorldPoint from, WorldPoint direction, float speed, float range,
                            std::string model, float bodyRadius, std::uint32_t shooter = 0) {
            const float length = std::max(1e-5f, std::hypot(direction.x, direction.y));
            const float offset = bodyRadius + ProjectileSet::Radius;
            Projectile shot{};
            shot.side = ProjectileSide::Player;
            shot.shooter = shooter;
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
        static Services::Combat::DefenseProfile DefenseOf(const Enemy& enemy) {
            const auto& type = *enemy.definition;
            return { type.armorClass + enemy.bonusArmorClass, type.physicalArmor, type.resistances };
        }

        // Apply a resolved attack (or spell) to an enemy: a hit that does not kill staggers it
        // unless it is mid-attack; a spell's statuses take hold
        CombatEvent Hit(Enemy& enemy, const Services::Combat::WeaponAttackResult& result, const Content::SpellDefinition* spell = nullptr) {
            CombatEvent event{ CombatEventKind::PlayerAttack, enemy.id, result, false, spell };
            if (event.result.hit) {
                event.result.finalDamage = enemy.statuses.Taken(event.result.finalDamage);   // a curse of frailty
                ApplyStatuses(enemy.statuses, spell);
                enemy.hitPoints = std::max(0, enemy.hitPoints - event.result.finalDamage);
                enemy.hitFlashSeconds = HitFlashSeconds;
                if (enemy.hitPoints == 0) {
                    event.killed = true;
                    behavior_.Enter(enemy, EnemyState::Dead);
                }
                else if (enemy.state != EnemyState::Attacking && !enemy.definition->staggerImmune) {
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
        void UpdateProjectiles(float elapsedSeconds, const PlayerTarget& player, std::span<const PlayerTarget> allies, std::vector<CombatEvent>& events) {
            std::vector<ProjectileTarget> targets;
            if (player.alive) targets.push_back({ 0, ProjectileSide::Player, player.position, player.radius });
            for (const auto& ally : allies) {
                if (ally.alive) targets.push_back({ ally.ally, ProjectileSide::Player, ally.position, ally.radius });
            }
            for (const auto& enemy : enemies_) {
                if (enemy.IsTargetable()) targets.push_back({ enemy.id, ProjectileSide::Enemies, enemy.position, CharacterRadius });
            }
            for (const auto& impact : projectiles_.Update(elapsedSeconds, world_, targets)) {
                if (!impact.target) {
                    wallHits_.push_back(impact.at);
                }
                else if (impact.projectile.side == ProjectileSide::Enemies) {
                    // The player (id 0) or the ally it reached
                    const auto ally = std::ranges::find(allies, *impact.target, &PlayerTarget::ally);
                    const auto& body = *impact.target != 0 && ally != allies.end() ? *ally : player;
                    if (impact.projectile.spell) {
                        const auto& payload = *impact.projectile.spell;
                        events.push_back({ CombatEventKind::EnemyAttack, impact.projectile.shooter,
                            Services::Combat::CombatService::ResolveSpellHit(payload.rawDamage, payload.spell->damageType, body.defense), false, payload.spell, body.ally });
                    }
                    else {
                        events.push_back({ CombatEventKind::EnemyAttack, impact.projectile.shooter, combat_.ResolveWeaponAttack(impact.projectile.attack, body.defense),
                                           false, nullptr, body.ally });
                    }
                }
                else if (auto* enemy = Find(*impact.target)) {
                    if (!impact.projectile.spell) {
                        events.push_back(Hit(*enemy, combat_.ResolveWeaponAttack(impact.projectile.attack, DefenseOf(*enemy))));
                        events.back().byAlly = impact.projectile.shooter != 0;   // an ally's arrow
                    }
                    else if (impact.projectile.spell->spell->radius <= 0.0f) events.push_back(SpellHit(*enemy, *impact.projectile.spell));
                }
                // A spell with a radius bursts where it lands, against a body or a wall
                if (impact.projectile.spell && impact.projectile.spell->spell->radius > 0.0f) {
                    if (impact.projectile.side == ProjectileSide::Player) Burst(*impact.projectile.spell, impact.at, events);
                    else bursts_.push_back({ impact.at, impact.projectile.spell->spell->radius, impact.projectile.spell->spell->damageType });
                }
            }
        }

        CombatEvent SpellHit(Enemy& enemy, const SpellPayload& payload) {
            auto result = Services::Combat::CombatService::ResolveSpellHit(payload.rawDamage, payload.spell->damageType, DefenseOf(enemy));
            result.critical = payload.critical;
            return Hit(enemy, result, payload.spell);
        }

        // The player's spell bursting: every targetable enemy whose body is within the radius
        void Burst(const SpellPayload& payload, WorldPoint at, std::vector<CombatEvent>& events) {
            const auto& spell = *payload.spell;
            bursts_.push_back({ at, spell.radius, spell.damageType });
            for (auto& enemy : enemies_) {
                if (enemy.IsTargetable() && Distance(enemy.position, at) <= spell.radius + CharacterRadius) events.push_back(SpellHit(enemy, payload));
            }
        }

        // A caster's spell missile at where the player stands now: the spell's damage from the
        // enemy's spell power
        void CastAtPlayer(const Enemy& enemy, WorldPoint player) {
            const auto* spell = content_ ? content_->FindSpell(enemy.definition->spell) : nullptr;
            if (!spell) return;
            WorldPoint direction{ player.x - enemy.position.x, player.y - enemy.position.y };
            if (std::hypot(direction.x, direction.y) < 1e-4f) direction = Forward(enemy.facingYaw);
            const float length = std::hypot(direction.x, direction.y);
            const float offset = CharacterRadius + ProjectileSet::Radius;
            Projectile shot{};
            shot.side = ProjectileSide::Enemies;
            shot.shooter = enemy.id;
            shot.position = { enemy.position.x + direction.x / length * offset, enemy.position.y + direction.y / length * offset };
            shot.direction = direction;
            shot.speed = spell->projectileSpeed;
            shot.rangeLeft = enemy.definition->attackRange * ShotOvershoot;
            shot.attack.damageType = spell->damageType;
            shot.spell = SpellPayload{ spell, SpellDamage(*spell, enemy.definition->spellPower) };
            projectiles_.Launch(std::move(shot));
            shots_.push_back(enemy.position);
        }

        // An enemy that can strike whoever is within its reach now: awake, melee, free to act
        static bool Threatens(const Enemy& enemy) noexcept {
            return (enemy.state == EnemyState::Chasing || enemy.state == EnemyState::Attacking) && !enemy.definition->IsRanged()
                && !enemy.statuses.PreventsActions();
        }

        // Another enemy threatens the target from its far side (design-combat-system.md 7.1)
        [[nodiscard]] bool Flanking(const Enemy& attacker, WorldPoint target) const noexcept {
            const float ax = attacker.position.x - target.x;
            const float ay = attacker.position.y - target.y;
            const float a = std::hypot(ax, ay);
            if (a < 1e-4f) return false;
            for (const auto& other : enemies_) {
                if (other.id == attacker.id || !Threatens(other)) continue;
                const float ox = other.position.x - target.x;
                const float oy = other.position.y - target.y;
                const float o = std::hypot(ox, oy);
                if (o < 1e-4f || o > other.definition->attackRange + AttackReachSlack) continue;
                if ((ax * ox + ay * oy) / (a * o) <= FlankOppositeCosine) return true;
            }
            return false;
        }

        // Whom an enemy goes after: the nearest of the player and their allies still standing; a
        // shooter, the most wounded of those within its range (focus fire), else the nearest
        static const PlayerTarget& TargetOf(const Enemy& enemy, const PlayerTarget& player, std::span<const PlayerTarget> allies) {
            if (enemy.definition->IsRanged()) {
                const PlayerTarget* weakest = nullptr;
                const auto consider = [&](const PlayerTarget& body) {
                    if (!body.alive || Distance(enemy.position, body.position) > enemy.definition->attackRange) return;
                    if (!weakest || body.health < weakest->health) weakest = &body;
                };
                consider(player);
                for (const auto& ally : allies) consider(ally);
                if (weakest) return *weakest;
            }
            const PlayerTarget* best = &player;
            float bestDistance = player.alive ? Distance(enemy.position, player.position) : std::numeric_limits<float>::infinity();
            for (const auto& ally : allies) {
                if (!ally.alive) continue;
                const float distance = Distance(enemy.position, ally.position);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = &ally;
                }
            }
            return *best;
        }

        // Each group's formation: its nearest tank and nearest melee fighter closing in on the player
        [[nodiscard]] std::unordered_map<std::uint32_t, Formation> Formations(WorldPoint player) const {
            std::unordered_map<std::uint32_t, Formation> formations;
            for (const auto& enemy : enemies_) {
                if (enemy.group == 0 || enemy.IsDead() || enemy.definition->IsRanged()) continue;
                if (enemy.state != EnemyState::Chasing && enemy.state != EnemyState::Attacking) continue;
                auto& formation = formations[enemy.group];
                const float distance = Distance(enemy.position, player);
                formation.front = std::min(formation.front, distance);
                if (enemy.definition->tank) formation.tank = std::min(formation.tank, distance);
            }
            return formations;
        }

        // Burning and other damage over time; a status that kills counts as the player's (only the
        // player's spells set them on enemies)
        void UpdateStatuses(Enemy& enemy, float elapsedSeconds, std::vector<CombatEvent>& events) {
            if (enemy.IsDead()) {
                enemy.statuses.Clear();
                return;
            }
            for (const auto& tick : enemy.statuses.Update(elapsedSeconds)) {
                CombatEvent event{ CombatEventKind::PlayerAttack, enemy.id, Services::Combat::CombatService::ResolveSpellHit(tick.damage, tick.damageType, DefenseOf(enemy)), false };
                enemy.hitPoints = std::max(0, enemy.hitPoints - event.result.finalDamage);
                if (enemy.hitPoints == 0) {
                    event.killed = true;
                    behavior_.Enter(enemy, EnemyState::Dead);
                }
                events.push_back(event);
                if (event.killed) break;
            }
        }

        // A room's group (rooms come first; waves draw new ones after them)
        static std::uint32_t RoomGroup(std::int32_t room) noexcept { return static_cast<std::uint32_t>(room) + 1; }

        std::uint32_t NextGroup() noexcept { return nextGroup_++; }

        // Design-combat-system.md 9.3 (group dynamics): when one of a group notices the player, the
        // rest come too
        void AlertGroups() {
            std::vector<std::uint32_t> alerted;
            for (const auto& enemy : enemies_) {
                if (enemy.group != 0 && (enemy.state == EnemyState::Chasing || enemy.state == EnemyState::Attacking || enemy.state == EnemyState::Staggered)) {
                    alerted.push_back(enemy.group);
                }
            }
            if (alerted.empty()) return;
            for (auto& enemy : enemies_) {
                if (enemy.state == EnemyState::Idle && std::ranges::find(alerted, enemy.group) != alerted.end()) behavior_.Enter(enemy, EnemyState::Chasing);
            }
        }

        // Summoners raise enemies around them while fighting (from their summonFromPhase on, twice
        // as often from their second phase; several kinds in turn); a boss turns to its second
        // and third phases below their health; what a summoner raised crumbles when it falls
        void UpdateSummoners(float elapsedSeconds, const PlayerTarget& player) {
            struct Raise { const Content::EnemyDefinition* type; WorldPoint at; std::int32_t wave; std::uint32_t group; std::uint32_t summoner; };
            std::vector<Raise> raises;
            for (auto& enemy : enemies_) {
                const auto& type = *enemy.definition;
                if (enemy.IsDead() || enemy.state == EnemyState::Spawning) continue;
                if (!enemy.phaseTwo && type.phaseTwoHealthFraction > 0.0f && enemy.hitPoints <= static_cast<float>(enemy.maxHitPoints) * type.phaseTwoHealthFraction) {
                    enemy.phaseTwo = true;
                    enemy.summonSeconds = 0.0f;   // it raises at once
                    phaseChanges_.push_back(enemy.id);
                }
                if (enemy.phaseTwo && !enemy.phaseThree && type.phaseThreeHealthFraction > 0.0f
                    && enemy.hitPoints <= static_cast<float>(enemy.maxHitPoints) * type.phaseThreeHealthFraction) {
                    enemy.phaseThree = true;
                    enemy.lastSlamCooldown = 0.0f;   // its last attack comes at once
                    phaseChanges_.push_back(enemy.id);
                }
                const bool fighting = enemy.state == EnemyState::Chasing || enemy.state == EnemyState::Attacking || enemy.state == EnemyState::Staggered;
                if (!type.IsSummoner() || !content_ || enemy.Phase() < type.summonFromPhase || !fighting || !player.alive) continue;
                enemy.summonSeconds -= elapsedSeconds;
                if (enemy.summonSeconds > 0.0f) continue;
                enemy.summonSeconds = type.summonCooldownSeconds * (enemy.phaseTwo ? 0.5f : 1.0f);

                const auto alive = std::ranges::count_if(enemies_, [&](const Enemy& e) { return e.summoner == enemy.id && !e.IsDead(); });
                const auto count = std::min<std::int64_t>(type.summonCount, type.maxSummons - alive);
                for (std::int64_t i = 0; i < count; ++i) {
                    const auto* summoned = content_->FindEnemy(type.SummonFor(enemy.summonTurn++));
                    if (!summoned) continue;
                    const float angle = std::uniform_real_distribution<float>(0.0f, 2.0f * std::numbers::pi_v<float>)(rng_);
                    WorldPoint at{ enemy.position.x + std::sin(angle) * 1.6f, enemy.position.y + std::cos(angle) * 1.6f };
                    if (world_ && !world_->CircleFits(at, CharacterRadius)) at = enemy.position;
                    raises.push_back({ summoned, at, enemy.wave, enemy.group, enemy.id });
                }
                if (count > 0) bursts_.push_back({ enemy.position, 1.8f, Services::Combat::DamageType::Shadow });
            }
            for (const auto& raise : raises) {
                const auto id = Spawn(*raise.type, raise.at, std::atan2(player.position.x - raise.at.x, player.position.y - raise.at.y), true, raise.wave, raise.group);
                Find(id)->summoner = raise.summoner;
            }

            for (auto& enemy : enemies_) {
                if (enemy.summoner == 0 || enemy.IsDead()) continue;
                const auto* master = FindEnemy(enemy.summoner);
                if (!master || master->IsDead()) behavior_.Enter(enemy, EnemyState::Dead);
            }
        }

        void ApplyStatuses(StatusSet& statuses, const Content::SpellDefinition* spell) const {
            if (!spell || !content_) return;
            for (const auto& key : spell->appliedStatusIds) {
                if (const auto* status = content_->FindStatus(key)) statuses.Apply(*status);
            }
        }

    public:
        /// <summary>
        /// A spell's raw damage (design-spells-system.md 5.1): base + coefficient x spell power
        /// </summary>
        [[nodiscard]] static std::int32_t SpellDamage(const Content::SpellDefinition& spell, std::int32_t spellPower) noexcept {
            return std::max(0, static_cast<std::int32_t>(std::lround(static_cast<float>(spell.baseDamage) + spell.powerCoefficient * static_cast<float>(spellPower))));
        }

    private:

        Enemy* Find(std::uint32_t id) noexcept {
            const auto it = std::ranges::find(enemies_, id, &Enemy::id);
            return it != enemies_.end() ? &*it : nullptr;
        }

        static Services::Combat::WeaponAttackProfile AttackOf(const Enemy& enemy) {
            const auto& type = *enemy.definition;
            return { type.attackBonus + enemy.bonusAttack, type.damageDice, type.damageBonus + enemy.bonusDamage, type.damageType, type.critRangeMin, type.critMultiplier };
        }

        Services::Combat::CombatService combat_;
        std::mt19937 rng_;
        std::int32_t veteranAttack_{0};
        std::int32_t veteranArmorClass_{0};
        float veteranHitPoints_{0.0f};
        std::int32_t veteranDamage_{0};
        float veteranLifeSteal_{0.0f};
        EnemyBehavior behavior_;
        ProjectileSet projectiles_;
        std::vector<WorldPoint> shots_;
        std::vector<WorldPoint> wallHits_;
        const Content::EnemyDefinition* light_{nullptr};
        const Content::EnemyDefinition* heavy_{nullptr};
        const Content::EnemyDefinition* ranged_{nullptr};
        const Content::EnemyDefinition* caster_{nullptr};
        const Content::EnemyDefinition* brute_{nullptr};
        std::uint32_t nextGroup_{1000};
        std::vector<std::uint32_t> phaseChanges_;
        const Content::ContentDatabase* content_{nullptr};
        std::vector<SpellBurst> bursts_;
        std::vector<CombatEvent> opportunities_;   // strikes at the hero walking away, for the next Update
        const WorldMap* world_{nullptr};
        std::vector<Enemy> enemies_;
        std::uint32_t nextId_{1};
        std::int32_t wave_{0};
        float waveTimerSeconds_{0.0f};
    };

} // namespace LB::Engine
