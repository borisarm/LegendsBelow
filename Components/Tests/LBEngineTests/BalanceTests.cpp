// BalanceTests.cpp - LB.Engine tests: a simple bot plays each class headless through the first
// tier (three floors and its boss) and must reach the resting place below; it reports deaths,
// potions, and time for balancing. A run is minutes of game time, so these are built only in
// optimized configurations (Release; seconds there, minutes in Debug).
#include "pch.h"
#include "EngineTestHelpers.h"

#ifdef NDEBUG

namespace {
    using LB::Character::CharacterClass;

    /// <summary>
    /// How a bot's run went
    /// </summary>
    struct BotReport {
        std::int32_t depth{1};
        std::int32_t deaths{0};
        std::int32_t potions{0};
        std::int32_t level{1};
        float minutes{0.0f};
    };

    // Rooms the bot must clear (their enemies keep the way down shut)
    bool IsFightingRoom(LB::Dungeon::RoomType type) {
        using LB::Dungeon::RoomType;
        return type == RoomType::Combat || type == RoomType::Elite || type == RoomType::Exit;
    }

    /// <summary>
    /// Fights whatever is awake or near (abilities when ready, a potion when low), else walks to
    /// the nearest room still to clear, and last to the exit's trapdoor
    /// </summary>
    class Bot {
    public:
        explicit Bot(GameEngine& engine) : engine_(engine), world_(engine.GetWorld()) {
            // A bow in the kit (the Rogue's): shoot while the arrows last, the melee weapon after
            if (engine_.GetPlayerWeapon()) melee_ = engine_.GetPlayerWeapon()->key;
            if (engine_.CountItem(GameEngine::StartingBow) > 0) engine_.EquipItem(GameEngine::StartingBow);
        }

        void Step() {
            ++steps_;
            potionWait_ = std::max(0, potionWait_ - 1);
            if (engine_.GetDepth() != depth_) {   // a new floor: a new map
                depth_ = engine_.GetDepth();
                world_ = engine_.GetWorld();
                goalSet_ = false;
            }
            if (!engine_.IsPlayerAlive()) {
                Release();
                return;
            }
            if (const auto* weapon = engine_.GetPlayerWeapon(); weapon && !weapon->ammoType.empty() && engine_.AmmoCount(weapon->ammoType) == 0 && !melee_.empty()) {
                engine_.EquipItem(melee_);
            }
            const auto hp = PlayerHitPoints(engine_);
            if (hp * 10 < PlayerMaxHitPoints(engine_) * 4 && potionWait_ == 0 && engine_.CountItem(GameEngine::HealingPotion) > 0) {
                engine_.UseItem(GameEngine::HealingPotion);
                ++potions_;
                potionWait_ = 90;
            }

            // Moving cancels a cast: stand still until it leaves
            if (engine_.IsPlayerCasting()) {
                Release();
                return;
            }
            if (const auto* foe = Foe()) {
                const auto at = engine_.GetPlayerPosition();
                const float distance = Encounter::Distance(at, foe->position);
                // A spell waits for the swing to end (no casting mid-attack)
                if (!pending_ && steps_ % 15 == 0) pending_ = SpellToCast(distance, hp);
                if (pending_) {
                    Release();
                    if (!engine_.IsPlayerAttacking()) {
                        TapKey(engine_, Keys::FromChar(static_cast<char>('1' + *pending_)));
                        pending_.reset();
                    }
                    return;
                }
                const auto* weapon = engine_.GetPlayerWeapon();
                const bool ranged = weapon && weapon->IsRanged() && (weapon->ammoType.empty() || engine_.AmmoCount(weapon->ammoType) > 0);
                const float reach = ranged ? weapon->range * 0.8f : 1.2f;
                // A shooter backs away from one closing in (for a while: cornered, it stands and fights)
                if (distance >= 3.5f) kiteSteps_ = 0;
                if (ranged && distance < 2.5f && kiteSteps_ < 120) {
                    ++kiteSteps_;
                    Release();
                    SteerToward(engine_, { at.x + (at.x - foe->position.x) / std::max(distance, 0.01f) * 3.0f,
                                           at.y + (at.y - foe->position.y) / std::max(distance, 0.01f) * 3.0f });
                    return;
                }
                if (distance <= reach && engine_.GetWorld().LineOfSight(at, foe->position)) {
                    engine_.OnGamepadState(0, GamepadState{ .connected = true });
                    if (!attacking_) engine_.OnKeyDown(Keys::Space);
                    attacking_ = true;
                    return;
                }
                if (attacking_) engine_.OnKeyUp(Keys::Space);
                attacking_ = false;
                GoTo(foe->position);
                return;
            }
            Release();
            GoTo(NextPlace());
        }

        void Release() {
            if (attacking_) engine_.OnKeyUp(Keys::Space);
            attacking_ = false;
            engine_.OnGamepadState(0, GamepadState{ .connected = true });
        }

        [[nodiscard]] std::int32_t Potions() const noexcept { return potions_; }

    private:
        // The nearest enemy awake, or close and in sight
        const Enemy* Foe() const {
            const Enemy* best = nullptr;
            float bestDistance = 1e9f;
            const auto at = engine_.GetPlayerPosition();
            for (const auto& enemy : engine_.GetEncounter().GetEnemies()) {
                if (!enemy.IsTargetable()) continue;
                const float distance = Encounter::Distance(at, enemy.position);
                const bool awake = enemy.state == EnemyState::Chasing || enemy.state == EnemyState::Attacking;
                if (!awake && (distance > 7.0f || !engine_.GetWorld().LineOfSight(at, enemy.position))) continue;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = &enemy;
                }
            }
            return best;
        }

        // The ability bar, first ready slot that suits: heals when hurt, the rest at a foe in range
        std::optional<std::size_t> SpellToCast(float distance, std::int32_t hp) const {
            const auto bar = engine_.GetSpellBar();
            for (std::size_t slot = 0; slot < bar.size(); ++slot) {
                const auto& spell = *bar[slot];
                if (!engine_.SpellBlocker(slot).empty()) continue;
                if (spell.IsHeal()) {
                    if (hp * 10 >= PlayerMaxHitPoints(engine_) * 6) continue;
                }
                else {
                    const float range = spell.range > 0.0f ? spell.range : spell.IsBurstAroundCaster() ? spell.radius : spell.IsDash() ? 0.0f : 7.0f;
                    if (distance > range) continue;
                }
                return slot;
            }
            return std::nullopt;
        }

        // The nearest room still to clear (the exit's last), or the trapdoor
        WorldPoint NextPlace() const {
            const auto& layout = engine_.GetWorld().Layout();
            const auto& rooms = engine_.GetRoomStates();
            const auto at = engine_.GetPlayerPosition();
            std::optional<WorldPoint> best;
            float bestDistance = 1e9f;
            for (std::size_t i = 0; i < layout.rooms.size() && i < rooms.size(); ++i) {
                if (rooms[i].cleared || !IsFightingRoom(layout.rooms[i].type) || static_cast<std::int32_t>(i) == layout.exitRoom) continue;
                const auto center = engine_.GetWorld().RoomCenter(static_cast<std::int32_t>(i));
                if (Encounter::Distance(at, center) < bestDistance) {
                    bestDistance = Encounter::Distance(at, center);
                    best = center;
                }
            }
            return best.value_or(engine_.GetWorld().RoomCenter(layout.exitRoom));
        }

        // Along the floor's flow field (built again when the goal moves)
        void GoTo(WorldPoint goal) {
            if (!goalSet_ || Encounter::Distance(goal, goal_) > 1.0f) {
                world_.BuildFlowField(goal);
                goal_ = goal;
                goalSet_ = true;
            }
            const auto at = engine_.GetPlayerPosition();
            const auto next = world_.NextWaypoint(at);
            const bool sameRoom = world_.RoomAt(at) >= 0 && world_.RoomAt(at) == world_.RoomAt(goal);
            SteerToward(engine_, next && !sameRoom ? *next : goal);
        }

        GameEngine& engine_;
        WorldMap world_;
        WorldPoint goal_{};
        bool goalSet_{false};
        bool attacking_{false};
        std::optional<std::size_t> pending_;
        std::string melee_;
        std::int32_t depth_{1};
        std::int32_t potionWait_{0};
        std::int32_t kiteSteps_{0};
        std::int32_t potions_{0};
        std::int64_t steps_{0};
    };

    // Play a class from the first floor until it stands on `depth` (or the time runs out)
    BotReport Play(CharacterClass characterClass, std::int32_t depth, float budgetMinutes) {
        GameEngine engine;
        EXPECT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = CombatSeed,
            .dungeon = true, .playerClass = characterClass })) << engine.GetInitializationError();
        Bot bot(engine);
        BotReport report;
        bool alive = true;
        const auto budget = static_cast<std::int64_t>(budgetMinutes * 60.0f * 60.0f);
        std::int64_t step = 0;
        for (; step < budget && engine.GetDepth() < depth; ++step) {
            bot.Step();
            engine.Tick(OneSixtieth);
            if (alive && !engine.IsPlayerAlive()) ++report.deaths;
            alive = engine.IsPlayerAlive();
        }
        bot.Release();
        report.depth = engine.GetDepth();
        report.potions = bot.Potions();
        report.level = PlayerLevel(engine);
        report.minutes = static_cast<float>(step) / 3600.0f;
        std::printf("[ BALANCE  ] %-8s depth %d, level %d, %d deaths, %d potions, %.1f minutes\n",
                    LB::Character::ToString(characterClass), report.depth, report.level, report.deaths, report.potions, report.minutes);
        return report;
    }

    // Every class clears the first tier, its boss included, in time. The bot charges in and sets up
    // no sneak attacks, so it dies more than a player would: the cap catches a class that got much
    // harder (at the last pass: Warrior 2, Mage 5, Rogue 12, Cleric 3 deaths; 5 to 9 minutes)
    constexpr std::int32_t RestingPlace = GameEngine::BossFloorInterval + 1;
    constexpr float BudgetMinutes = 14.0f;
    constexpr std::int32_t MostDeaths = 15;
}

TEST(BalanceTests, Warrior_ClearsTheFirstTier) {
    const auto report = Play(CharacterClass::Warrior, RestingPlace, BudgetMinutes);
    EXPECT_EQ(report.depth, RestingPlace);
    EXPECT_LE(report.deaths, MostDeaths);
}

TEST(BalanceTests, Mage_ClearsTheFirstTier) {
    const auto report = Play(CharacterClass::Mage, RestingPlace, BudgetMinutes);
    EXPECT_EQ(report.depth, RestingPlace);
    EXPECT_LE(report.deaths, MostDeaths);
}

TEST(BalanceTests, Rogue_ClearsTheFirstTier) {
    const auto report = Play(CharacterClass::Rogue, RestingPlace, BudgetMinutes);
    EXPECT_EQ(report.depth, RestingPlace);
    EXPECT_LE(report.deaths, MostDeaths);
}

TEST(BalanceTests, Cleric_ClearsTheFirstTier) {
    const auto report = Play(CharacterClass::Cleric, RestingPlace, BudgetMinutes);
    EXPECT_EQ(report.depth, RestingPlace);
    EXPECT_LE(report.deaths, MostDeaths);
}

#endif // NDEBUG
