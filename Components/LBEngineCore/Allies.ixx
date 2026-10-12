// Allies.ixx - The hero's companions: following them, fighting what threatens them, going down and
// getting back up when the fight is over
export module LB.Engine:Allies;

import std;
import LB.Content;
import LB.CombatService;
import LB.RenderingSystem;
import :World;
import :Animator;
import :Enemies;
import :Encounter;

export namespace LB::Engine {

    /// <summary>
    /// The clips allies play (KayKit Adventurers bodies share the player's Rig_Medium clips)
    /// </summary>
    struct AllyClips {
        ClipInfo idle;
        ClipInfo run;
        ClipInfo slash;      // a two-handed swing
        ClipInfo shoot;      // a bow's draw and release
        ClipInfo hit;
        ClipInfo death;
    };

    enum class AllyState {
        Following,   // near the hero, or on the way to them
        Fighting,    // closing on a foe, or striking it
        Down         // out of the fight until it is over
    };

    /// <summary>
    /// A companion: an AllyDefinition grown to the hero's level, where they stand, and what they do
    /// </summary>
    struct Ally {
        std::uint32_t id{0};
        const Content::AllyDefinition* definition{nullptr};
        WorldPoint position{};
        float facingYaw{0.0f};
        AllyState state{AllyState::Following};
        std::int32_t hitPoints{1};
        std::int32_t maxHitPoints{1};
        std::int32_t level{1};                  // the hero's, which they grow with
        std::optional<std::uint32_t> target;    // the enemy they are after
        bool swinging{false};
        bool impactDone{false};
        float swingSeconds{0.0f};
        float recoverySeconds{0.0f};
        float hitFlashSeconds{0.0f};
        float quietSeconds{0.0f};               // down, with no enemy awake nearby
        Animator animator;
        // Their way around the walls (found again when the goal moves to another cell)
        std::vector<WorldPoint> route;
        std::size_t routeNext{0};
        std::pair<std::int32_t, std::int32_t> routeGoal{ -1, -1 };

        [[nodiscard]] bool IsDown() const noexcept { return state == AllyState::Down; }

        /// <summary>
        /// Their attack: the definition's, better with the hero's level (+1 to hit every two levels,
        /// +1 damage every three)
        /// </summary>
        [[nodiscard]] Services::Combat::WeaponAttackProfile Attack() const {
            Services::Combat::WeaponAttackProfile attack{};
            attack.attackBonus = definition->attackBonus + (level - 1) / 2;
            attack.damageDice = definition->damageDice;
            attack.damageBonus = definition->damageBonus + (level - 1) / 3;
            attack.damageType = definition->damageType;
            return attack;
        }

        [[nodiscard]] Services::Combat::DefenseProfile Defense() const {
            return { definition->armorClass, definition->physicalArmor, {} };
        }
    };

    /// <summary>
    /// The hero's companions (design-character-system.md 7). Each step they follow the hero, go after
    /// enemies that are awake near the hero (melee ones close in, shooters keep their distance),
    /// strike or shoot through the Encounter (their blows count as the hero's side), and, when
    /// beaten, lie down until no enemy has been awake near them for a while.
    /// </summary>
    class AllyParty {
    public:
        static constexpr std::size_t MaxAllies = 1;
        static constexpr float FollowDistance = 1.8f;     // they keep about this close to the hero
        static constexpr float GuardRadius = 7.0f;        // they go after awake enemies this near the hero
        static constexpr float CatchUpDistance = 14.0f;   // farther than this (and not fighting) they rejoin the hero at once
        static constexpr float ReviveQuietSeconds = 3.0f; // a downed ally gets up after this long without enemies awake nearby
        static constexpr float ReviveFraction = 0.5f;
        static constexpr float Radius = 0.375f;           // their body (the player's)
        static constexpr float BlowFraction = 0.5f;       // of the attack time, when the blow lands (or the arrow leaves)

        void SetClips(const AllyClips& clips) { clips_ = clips; }

        /// <summary>
        /// A new ally at a point, at the hero's level, at full health; none past MaxAllies
        /// </summary>
        std::optional<std::uint32_t> Add(const Content::AllyDefinition& definition, WorldPoint at, std::int32_t level) {
            if (allies_.size() >= MaxAllies) return std::nullopt;
            Ally ally;
            ally.id = nextId_++;
            ally.definition = &definition;
            ally.position = at;
            ally.level = std::max(1, level);
            ally.maxHitPoints = MaxHitPointsAt(definition, ally.level);
            ally.hitPoints = ally.maxHitPoints;
            ally.animator.Play(clips_.idle.id, true);
            allies_.push_back(std::move(ally));
            return allies_.back().id;
        }

        void Clear() noexcept { allies_.clear(); }

        /// <summary>
        /// The hero reached a level: their allies grow with them (the hit points gained are healed)
        /// </summary>
        void SetLevel(std::int32_t level) {
            for (auto& ally : allies_) {
                const auto before = ally.maxHitPoints;
                ally.level = std::max(1, level);
                ally.maxHitPoints = MaxHitPointsAt(*ally.definition, ally.level);
                if (!ally.IsDown()) ally.hitPoints = std::min(ally.maxHitPoints, ally.hitPoints + std::max(0, ally.maxHitPoints - before));
            }
        }

        /// <summary>
        /// Beside the hero (a new floor, the hero back on their feet): up again if they were down
        /// </summary>
        void PlaceAround(const WorldMap& world, WorldPoint hero, bool revive) {
            for (std::size_t i = 0; i < allies_.size(); ++i) {
                auto& ally = allies_[i];
                const float angle = 2.4f + static_cast<float>(i) * 1.2f;
                const WorldPoint spot{ hero.x + std::cos(angle) * 1.2f, hero.y + std::sin(angle) * 1.2f };
                ally.position = world.CircleFits(spot, Radius) ? spot : hero;
                ally.target.reset();
                ally.swinging = false;
                ally.route.clear();
                if (revive && ally.IsDown()) GetUp(ally, ReviveFraction);
            }
        }

        /// <summary>
        /// Damage taken (an enemy's blow, a trap); at 0 they go down. Returns whether it took them down.
        /// </summary>
        bool Damage(std::uint32_t id, std::int32_t amount) {
            auto* ally = Mutable(id);
            if (!ally || ally->IsDown() || amount <= 0) return false;
            ally->hitPoints = std::max(0, ally->hitPoints - amount);
            ally->hitFlashSeconds = HitFlashSeconds;
            if (ally->hitPoints > 0) {
                if (!ally->swinging) ally->animator.Play(clips_.hit.id, false, 1.0f, true, 0.05f);
                return false;
            }
            ally->state = AllyState::Down;
            ally->swinging = false;
            ally->target.reset();
            ally->quietSeconds = 0.0f;
            ally->animator.Play(clips_.death.id, false, 1.0f, true, 0.1f);
            return true;
        }

        /// <summary>
        /// Healing (not of the downed); returns what was healed
        /// </summary>
        std::int32_t Heal(std::uint32_t id, std::int32_t amount) {
            auto* ally = Mutable(id);
            if (!ally || ally->IsDown() || amount <= 0) return 0;
            const auto before = ally->hitPoints;
            ally->hitPoints = std::min(ally->maxHitPoints, ally->hitPoints + amount);
            return ally->hitPoints - before;
        }

        /// <summary>
        /// Everyone up and whole (a shrine, a resting place)
        /// </summary>
        void RestoreAll() {
            for (auto& ally : allies_) GetUp(ally, 1.0f);
        }

        /// <summary>
        /// One step. Blows land through the Encounter at once (returned as the hero's side's
        /// events); arrows leave into it and land in its next steps.
        /// </summary>
        std::vector<CombatEvent> Update(float dt, const WorldMap& world, Encounter& encounter, WorldPoint hero, bool heroAlive) {
            std::vector<CombatEvent> events;
            for (auto& ally : allies_) {
                ally.hitFlashSeconds = std::max(0.0f, ally.hitFlashSeconds - dt);
                ally.recoverySeconds = std::max(0.0f, ally.recoverySeconds - dt);
                ally.animator.Update(dt);

                if (ally.IsDown()) {
                    const bool threatened = std::ranges::any_of(encounter.GetEnemies(), [&](const Enemy& enemy) {
                        return Awake(enemy) && Distance(enemy.position, ally.position) <= CatchUpDistance;
                    });
                    ally.quietSeconds = threatened ? 0.0f : ally.quietSeconds + dt;
                    if (ally.quietSeconds >= ReviveQuietSeconds && heroAlive) GetUp(ally, ReviveFraction);
                    continue;
                }

                if (ally.swinging) {
                    Swing(ally, dt, encounter, events);
                    continue;
                }

                // A foe: theirs if still awake and near the hero, else the nearest awake one there
                const Enemy* foe = ally.target ? encounter.FindEnemy(*ally.target) : nullptr;
                if (foe && (!Awake(*foe) || Distance(foe->position, hero) > GuardRadius + 2.0f)) foe = nullptr;
                if (!foe && heroAlive) foe = NearestFoe(encounter, ally, hero);
                ally.target = foe ? std::optional{ foe->id } : std::nullopt;
                ally.state = foe ? AllyState::Fighting : AllyState::Following;

                if (foe) {
                    const float distance = Distance(ally.position, foe->position);
                    const bool inSight = world.LineOfSight(ally.position, foe->position);
                    if (distance <= ally.definition->attackRange && (inSight || !ally.definition->IsRanged())) {
                        Face(ally, foe->position);
                        if (ally.recoverySeconds <= 0.0f) StartSwing(ally);
                        else ally.animator.Play(clips_.idle.id, true);
                        continue;
                    }
                    WalkToward(ally, world, foe->position, dt, ally.definition->attackRange * 0.9f);
                    continue;
                }

                // No foe: back to the hero (at once when left far behind)
                const float away = Distance(ally.position, hero);
                if (away > CatchUpDistance) {
                    PlaceAround(world, hero, false);
                    continue;
                }
                if (away > FollowDistance) WalkToward(ally, world, hero, dt, FollowDistance * 0.8f);
                else ally.animator.Play(clips_.idle.id, true);
            }
            return events;
        }

        /// <summary>
        /// The allies as enemies see them (their targets, and what their arrows can hit)
        /// </summary>
        [[nodiscard]] std::vector<PlayerTarget> Targets() const {
            std::vector<PlayerTarget> targets;
            for (const auto& ally : allies_) targets.push_back({ ally.position, !ally.IsDown(), ally.Defense(), Radius, ally.id });
            return targets;
        }

        [[nodiscard]] const std::vector<Ally>& Allies() const noexcept { return allies_; }
        [[nodiscard]] const Ally* Find(std::uint32_t id) const {
            const auto it = std::ranges::find(allies_, id, &Ally::id);
            return it != allies_.end() ? &*it : nullptr;
        }

        [[nodiscard]] static std::int32_t MaxHitPointsAt(const Content::AllyDefinition& definition, std::int32_t level) noexcept {
            return definition.hitPoints + definition.hitPointsPerLevel * (std::max(1, level) - 1);
        }

        static constexpr float HitFlashSeconds = 0.2f;

    private:
        Ally* Mutable(std::uint32_t id) {
            const auto it = std::ranges::find(allies_, id, &Ally::id);
            return it != allies_.end() ? &*it : nullptr;
        }

        static bool Awake(const Enemy& enemy) noexcept {
            return enemy.IsTargetable() && (enemy.state == EnemyState::Chasing || enemy.state == EnemyState::Attacking || enemy.state == EnemyState::Staggered);
        }

        static const Enemy* NearestFoe(const Encounter& encounter, const Ally& ally, WorldPoint hero) {
            const Enemy* best = nullptr;
            float bestDistance = std::numeric_limits<float>::infinity();
            for (const auto& enemy : encounter.GetEnemies()) {
                if (!Awake(enemy) || Distance(enemy.position, hero) > GuardRadius) continue;
                const float distance = Distance(enemy.position, ally.position);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = &enemy;
                }
            }
            return best;
        }

        void GetUp(Ally& ally, float fraction) {
            ally.state = AllyState::Following;
            ally.hitPoints = std::max(ally.hitPoints, static_cast<std::int32_t>(std::lround(static_cast<float>(ally.maxHitPoints) * fraction)));
            ally.quietSeconds = 0.0f;
            ally.animator.Play(clips_.idle.id, true, 1.0f, true, 0.2f);
        }

        static void Face(Ally& ally, WorldPoint at) {
            const float dx = at.x - ally.position.x;
            const float dy = at.y - ally.position.y;
            if (dx != 0.0f || dy != 0.0f) ally.facingYaw = std::atan2(dx, dy);
        }

        void StartSwing(Ally& ally) {
            ally.swinging = true;
            ally.impactDone = false;
            ally.swingSeconds = 0.0f;
            const auto& clip = ally.definition->IsRanged() ? clips_.shoot : clips_.slash;
            ally.animator.Play(clip.id, false, clip.seconds / ally.definition->attackTimeSeconds, true, 0.1f);
        }

        // The blow lands (or the arrow leaves) halfway; then a pause before the next
        void Swing(Ally& ally, float dt, Encounter& encounter, std::vector<CombatEvent>& events) {
            const auto& type = *ally.definition;
            ally.swingSeconds += dt;
            if (!ally.impactDone && ally.swingSeconds >= type.attackTimeSeconds * BlowFraction) {
                ally.impactDone = true;
                if (const auto* foe = ally.target ? encounter.FindEnemy(*ally.target) : nullptr; foe && foe->IsTargetable()) {
                    Face(ally, foe->position);
                    if (type.IsRanged()) {
                        const WorldPoint direction{ foe->position.x - ally.position.x, foe->position.y - ally.position.y };
                        encounter.Shoot(ally.Attack(), ally.position, direction, type.projectileSpeed, type.attackRange * 1.15f, type.projectileModel, Radius);
                    }
                    else if (auto event = encounter.StrikeEnemy(foe->id, ally.Attack(), ally.position, type.attackRange)) {
                        events.push_back(*event);
                    }
                }
            }
            if (ally.swingSeconds >= type.attackTimeSeconds) {
                ally.swinging = false;
                ally.recoverySeconds = type.attackRecoverySeconds;
            }
        }

        // Straight there when in sight, else along a path around the walls; stops `stopAt` short
        void WalkToward(Ally& ally, const WorldMap& world, WorldPoint goal, float dt, float stopAt) {
            WorldPoint heading = goal;
            if (!world.LineOfSight(ally.position, goal)) {
                const auto goalCell = world.CellOf(goal);
                if (ally.routeGoal != goalCell || ally.routeNext >= ally.route.size()) {
                    ally.route = world.FindPath(ally.position, goal);
                    ally.routeNext = 0;
                    ally.routeGoal = goalCell;
                }
                while (ally.routeNext + 1 < ally.route.size() && Distance(ally.position, ally.route[ally.routeNext]) < 0.5f) ++ally.routeNext;
                if (ally.routeNext < ally.route.size()) heading = ally.route[ally.routeNext];
            }
            const float dx = heading.x - ally.position.x;
            const float dy = heading.y - ally.position.y;
            const float length = std::hypot(dx, dy);
            const float left = heading.x == goal.x && heading.y == goal.y ? length - stopAt : length;
            if (length < 1e-4f || left <= 0.0f) {
                ally.animator.Play(clips_.idle.id, true);
                return;
            }
            const float step = std::min(ally.definition->movementSpeed * dt, left);
            ally.position = world.Move(ally.position, { dx / length * step, dy / length * step }, Radius);
            ally.facingYaw = std::atan2(dx, dy);
            ally.animator.Play(clips_.run.id, true);
        }

        std::vector<Ally> allies_;
        AllyClips clips_{};
        std::uint32_t nextId_{1};
    };

} // namespace LB::Engine
