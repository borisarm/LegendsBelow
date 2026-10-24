// BalanceTests.cpp - LB.Engine tests: a simple bot plays each class headless through the first
// tier (three floors and its boss), the second (the caves' creatures and the Golem Warlord), the
// third (the ruins' creatures and the Frost Golem), and the fourth (the archive's and the
// Abomination) and must reach the resting place below each; then through the final floor to the
// victory, and down again through New Game+'s first tier; it reports deaths, potions, and time
// for balancing. It grows as a player would (attribute points, a path, better finds, a companion
// and potions from the merchant, its spares sold there or dropped for room, the locked gate's
// treasure). A run is minutes of game time, so these are built only in optimized configurations
// (Release; seconds there, minutes in Debug).
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
        std::int32_t gates{0};      // locked gates opened
        std::int32_t allies{0};     // companions at the end
        float minutes{0.0f};
    };

    // The attribute a class's points go into
    LB::AttributeType MainAttribute(CharacterClass characterClass) {
        switch (characterClass) {
        case CharacterClass::Rogue: return LB::AttributeType::Dexterity;
        case CharacterClass::Mage: return LB::AttributeType::Intelligence;
        case CharacterClass::Cleric: return LB::AttributeType::Wisdom;
        default: return LB::AttributeType::Strength;
        }
    }

    // The run's seed: CombatSeed, or LB_BALANCE_SEED to play other floors (checking a change over
    // several seeds without editing the tests)
    std::uint32_t BalanceSeed() {
        char* text = nullptr;
        std::size_t length = 0;
        std::uint32_t seed = CombatSeed;
        if (_dupenv_s(&text, &length, "LB_BALANCE_SEED") == 0 && text) {
            std::from_chars(text, text + std::strlen(text), seed);
            std::free(text);
        }
        return seed;
    }

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
        explicit Bot(GameEngine& engine, CharacterClass characterClass) : engine_(engine), world_(engine.GetWorld()), class_(characterClass) {
            // A bow in the kit (the Rogue's): shoot while the arrows last, the melee weapon after
            if (engine_.GetPlayerWeapon()) melee_ = engine_.GetPlayerWeapon()->key;
            if (engine_.CountItem(GameEngine::StartingBow) > 0) engine_.EquipItem(GameEngine::StartingBow);
        }

        void Step() {
            ++steps_;
            potionWait_ = std::max(0, potionWait_ - 1);
            if (engine_.GetDepth() != depth_ || engine_.GetCycle() != cycle_) {   // a new floor: a new map
                depth_ = engine_.GetDepth();
                cycle_ = engine_.GetCycle();
                world_ = engine_.GetWorld();
                goalSet_ = false;
                openGates_ = 0;
                deathsIn_.clear();
                avoided_.clear();
                givenUp_.clear();
            }
            // A gate opened: the map has a new way through
            const auto open = static_cast<std::int32_t>(std::ranges::count_if(engine_.GetFeatures().Gates(), [](const Gate& gate) { return gate.open; }));
            if (open != openGates_) {
                gates_ += std::max(0, open - openGates_);
                openGates_ = open;
                world_ = engine_.GetWorld();
                goalSet_ = false;
            }
            if (steps_ % 60 == 0) Grow();
            if (!engine_.IsPlayerAlive()) {
                // A room that keeps killing the hero is left alone (all but the exit's are optional)
                if (!down_) {
                    const auto room = engine_.GetWorld().RoomAt(engine_.GetPlayerPosition());
                    if (room >= 0 && ++deathsIn_[room] >= 2) avoided_.insert(room);
                }
                down_ = true;
                Release();
                return;
            }
            down_ = false;
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
                const bool keepsAway = ranged;
                if (distance >= 3.5f) kiteSteps_ = 0;
                if (keepsAway && distance < 2.5f && kiteSteps_ < 120) {
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
            if (const auto shop = Shopping()) {
                GoTo(*shop);
                return;
            }
            // An optional goal (a gate, a chest) not reached in 40 seconds is given up
            const auto [place, optional] = NextPlace();
            if (!optional || Encounter::Distance(place, optionalGoal_) > 0.5f) {
                optionalGoal_ = place;
                optionalSteps_ = 0;
            }
            if (optional && ++optionalSteps_ > 40 * 60) {
                givenUp_.push_back(place);
                optionalSteps_ = 0;
            }
            GoTo(place);
        }

        void Release() {
            if (attacking_) engine_.OnKeyUp(Keys::Space);
            attacking_ = false;
            engine_.OnGamepadState(0, GamepadState{ .connected = true });
        }

        [[nodiscard]] std::int32_t Potions() const noexcept { return potions_; }
        [[nodiscard]] std::int32_t GatesOpened() const noexcept { return gates_; }

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

        // The ability bar: heals when hurt, the rest at a foe in range; the big ones (with a
        // cooldown) first when ready, the later slots first, then the cheap ones in order
        std::optional<std::size_t> SpellToCast(float distance, std::int32_t hp) const {
            const auto bar = engine_.GetSpellBar();
            std::vector<std::size_t> order;
            for (std::size_t slot = bar.size(); slot-- > 0;) {
                if (bar[slot]->cooldownSeconds > 0.0f) order.push_back(slot);
            }
            for (std::size_t slot = 0; slot < bar.size(); ++slot) {
                if (bar[slot]->cooldownSeconds <= 0.0f) order.push_back(slot);
            }
            for (const auto slot : order) {
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

        // The nearest room still to clear; then the locked gate (with its key), the treasure rooms
        // open to it (these two optional), and last the exit's trapdoor
        std::pair<WorldPoint, bool> NextPlace() const {
            const auto givenUp = [&](WorldPoint place) {
                return std::ranges::any_of(givenUp_, [&](WorldPoint other) { return Encounter::Distance(place, other) < 0.5f; });
            };
            const auto& layout = engine_.GetWorld().Layout();
            const auto& rooms = engine_.GetRoomStates();
            const auto& features = engine_.GetFeatures();
            const auto at = engine_.GetPlayerPosition();
            // Nearest by the walk there, not as the crow flies: two rooms nearly as near in a line,
            // with ways leaving in opposite directions, swapped at every step and held the hero in
            // place (seed 1's depth 9). Along the walk, each step toward the nearest brings it a cell
            // closer and no other room closer faster, so the choice holds (ties: the first room).
            const auto nearest = [&](auto wanted) {
                std::optional<WorldPoint> best;
                std::size_t bestSteps = std::numeric_limits<std::size_t>::max();
                for (std::size_t i = 0; i < layout.rooms.size() && i < rooms.size(); ++i) {
                    if (static_cast<std::int32_t>(i) == layout.exitRoom || !wanted(i)) continue;
                    const auto center = engine_.GetWorld().RoomCenter(static_cast<std::int32_t>(i));
                    const bool there = world_.CellOf(at) == world_.CellOf(center);
                    const auto path = there ? std::vector<WorldPoint>{} : world_.FindPath(at, center);
                    if (!there && path.empty()) continue;   // no way there (yet)
                    if (path.size() < bestSteps) {
                        bestSteps = path.size();
                        best = center;
                    }
                }
                return best;
            };
            if (const auto fight = nearest([&](std::size_t i) {
                    return !rooms[i].cleared && IsFightingRoom(layout.rooms[i].type) && !avoided_.contains(static_cast<std::int32_t>(i));
                })) {
                return { *fight, false };
            }
            if (engine_.CountItem(GameEngine::KeyItem) > 0) {
                for (const auto& gate : features.Gates()) {
                    if (gate.kind != GateKind::Locked || gate.open) continue;
                    // Before it, on the corridor's side
                    const auto room = engine_.GetWorld().RoomCenter(gate.room);
                    const float dx = gate.center.x - room.x;
                    const float dy = gate.center.y - room.y;
                    const float length = std::max(std::hypot(dx, dy), 0.01f);
                    const WorldPoint before{ gate.center.x + dx / length * 1.4f, gate.center.y + dy / length * 1.4f };
                    if (!givenUp(before)) return { before, true };
                }
            }
            const auto shut = [&](std::size_t i) {
                return std::ranges::any_of(features.Gates(), [&](const Gate& gate) { return gate.room == static_cast<std::int32_t>(i) && !gate.open; });
            };
            if (const auto chest = nearest([&](std::size_t i) {
                    return layout.rooms[i].type == LB::Dungeon::RoomType::Treasure && !rooms[i].chestOpened && !shut(i)
                        && features.ChestReady(static_cast<std::int32_t>(i)) && !givenUp(engine_.GetWorld().RoomCenter(static_cast<std::int32_t>(i)));
                })) {
                return { *chest, true };
            }
            return { engine_.GetWorld().RoomCenter(layout.exitRoom), false };
        }

        // As a player grows: points into the class's attribute, the first path when it opens, and
        // the better finds in the bag (a weapon of the same reach, armour worth more than what is worn)
        void Grow() {
            while (engine_.SpendAttributePoint(MainAttribute(class_))) {}
            if (!engine_.GetPlayerPath()) {
                for (const auto& classData : engine_.GetContent()->GetClasses()) {
                    if (classData.characterClass == class_ && !classData.specializations.empty()) engine_.ChoosePath(classData.specializations.front().key);
                }
            }
            const auto bag = engine_.GetInventory();
            for (const auto& stack : bag) {
                const auto item = ItemRef::Unpack(stack.itemId);
                const auto base = engine_.BaseItem(item);
                if (base.kind == ItemKind::Weapon) {
                    const auto* found = engine_.GetContent()->FindWeapon(base.id);
                    const auto* wielded = engine_.GetPlayerWeapon();
                    if (!found || !wielded || found->IsRanged() != wielded->IsRanged()) continue;
                    if (engine_.PriceOf(item) > engine_.PriceOf(engine_.GetPlayerWeaponItem()) && engine_.EquipItemRef(item) && !found->IsRanged()) melee_ = found->key;
                }
                else if (base.kind == ItemKind::Armor) {
                    const auto* piece = engine_.GetContent()->FindArmor(base.id);
                    if (!piece) continue;
                    const auto worn = engine_.EquippedItem(piece->slot);
                    if (!worn.IsValid() || engine_.PriceOf(item) > engine_.PriceOf(worn)) engine_.EquipItemRef(item);
                }
            }
            // Room for the next finds: the cheapest spare goes on the floor when one stack is left
            // (before, the bag filled by the third tier and the bot met the last boss in its gear)
            while (engine_.GetInventory().size() + 1 >= static_cast<std::size_t>(GameEngine::BagCapacity)) {
                const auto spares = Spares();
                if (spares.empty() || !engine_.DropItem(*std::ranges::min_element(spares, {}, [&](ItemRef item) { return engine_.PriceOf(item); }))) break;
            }
        }

        // The weapons and armour in the bag (the better ones are worn already), but for the melee
        // weapon a shooter falls back on
        std::vector<ItemRef> Spares() const {
            std::vector<ItemRef> spares;
            const auto* wielded = engine_.GetPlayerWeapon();
            for (const auto& stack : engine_.GetInventory()) {
                const auto item = ItemRef::Unpack(stack.itemId);
                const auto base = engine_.BaseItem(item);
                if (base.kind != ItemKind::Weapon && base.kind != ItemKind::Armor) continue;
                if (base.kind == ItemKind::Weapon && wielded && wielded->IsRanged() && item == ItemRef::FromKey(*engine_.GetContent(), melee_)) continue;
                spares.push_back(item);
            }
            return spares;
        }

        // At a merchant (the resting places): a companion when the party has room, then potions up
        // to six; the merchant's place until done
        std::optional<WorldPoint> Shopping() {
            const auto merchant = engine_.GetMerchantPosition();
            if (!merchant || shopped_ == Floor()) return std::nullopt;
            if (Encounter::Distance(engine_.GetPlayerPosition(), *merchant) > 1.2f) return *merchant;
            const auto buy = [&](auto wanted) {
                for (bool bought = true; bought;) {
                    bought = false;
                    const auto& stock = engine_.GetMerchantStock();
                    for (std::size_t i = 0; i < stock.size() && !bought; ++i) {
                        bought = wanted(stock[i]) && engine_.GetGold() >= stock[i].price && engine_.BuyFromMerchant(i);
                    }
                }
            };
            for (const auto spare : Spares()) engine_.SellToMerchant(spare);   // the gold buys the rest
            buy([&](const GameEngine::StockEntry& entry) { return entry.ally && engine_.GetAllies().Allies().empty(); });
            buy([&](const GameEngine::StockEntry& entry) {
                return entry.item == engine_.MakeItem(GameEngine::HealingPotion) && engine_.CountItem(GameEngine::HealingPotion) < 6;
            });
            shopped_ = Floor();
            return std::nullopt;
        }

        // This floor, told apart from the same depth in another cycle
        [[nodiscard]] std::int32_t Floor() const noexcept { return cycle_ * 100 + depth_; }

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
        CharacterClass class_;
        std::int32_t depth_{1};
        std::int32_t cycle_{0};
        std::int32_t shopped_{0};
        std::int32_t openGates_{0};
        std::int32_t gates_{0};
        bool down_{false};
        std::map<std::int32_t, std::int32_t> deathsIn_;   // deaths per room on this floor
        std::set<std::int32_t> avoided_;
        std::vector<WorldPoint> givenUp_;                  // optional goals not reached in time
        WorldPoint optionalGoal_{};
        std::int64_t optionalSteps_{0};
        std::int32_t potionWait_{0};
        std::int32_t kiteSteps_{0};
        std::int32_t potions_{0};
        std::int64_t steps_{0};
    };

    // Play a class from the first floor until it stands on `depth` (or the time runs out)
    BotReport Play(CharacterClass characterClass, std::int32_t depth, float budgetMinutes) {
        GameEngine engine;
        EXPECT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = BalanceSeed(),
            .dungeon = true, .playerClass = characterClass })) << engine.GetInitializationError();
        Bot bot(engine, characterClass);
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
        report.gates = bot.GatesOpened();
        report.allies = static_cast<std::int32_t>(engine.GetAllies().Allies().size());
        report.minutes = static_cast<float>(step) / 3600.0f;
        std::printf("[ BALANCE  ] %-8s seed %u, depth %d, level %d, %d deaths, %d potions, %d gates, %d allies, %.1f minutes\n",
                    LB::Character::ToString(characterClass), BalanceSeed(), report.depth, report.level, report.deaths, report.potions, report.gates, report.allies, report.minutes);
        return report;
    }

    // Every class clears the first tier, its boss included, in time. The bot charges in and sets up
    // no sneak attacks, so it dies more than a player would: the cap catches a class that got much
    // harder (at the last pass: Warrior 0, Mage 2, Rogue 29, Cleric 8 deaths, level 5 or 6; 5 to
    // 15 minutes; over six seeds the Warrior, Mage, and Cleric died 0 to 13 times)
    constexpr std::int32_t RestingPlace = GameEngine::BossFloorInterval + 1;
    constexpr float BudgetMinutes = 20.0f;
    constexpr std::int32_t MostDeaths = 15;
    // The bot's Rogue sets up no sneak attacks (the class's strength): it dies most, and varies most
    // with the floors' layouts (9 to 29 deaths over six seeds)
    constexpr std::int32_t MostRogueDeaths = 35;
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
    EXPECT_LE(report.deaths, MostRogueDeaths);
}

TEST(BalanceTests, Cleric_ClearsTheFirstTier) {
    const auto report = Play(CharacterClass::Cleric, RestingPlace, BudgetMinutes);
    EXPECT_EQ(report.depth, RestingPlace);
    EXPECT_LE(report.deaths, MostDeaths);
}

// The second tier: the caves' creatures and the Golem Warlord, with what the first tier gave
// (a companion and potions from the resting place's merchant)
namespace {
    constexpr std::int32_t SecondRestingPlace = 2 * GameEngine::BossFloorInterval + 1;
    constexpr float SecondBudgetMinutes = 30.0f;
    // At the last pass: Warrior 0, Mage 8, Rogue 30, Cleric 8 deaths, level 9 or 10, 10 to 23
    // minutes; over six seeds the Warrior, Mage, and Cleric died 0 to 14 times, the Rogue 9 to 30
    constexpr std::int32_t MostSecondTierDeaths = 30;
    constexpr std::int32_t MostRogueSecondTierDeaths = 40;
}

TEST(BalanceTests, Warrior_ClearsTheSecondTier) {
    const auto report = Play(CharacterClass::Warrior, SecondRestingPlace, SecondBudgetMinutes);
    EXPECT_EQ(report.depth, SecondRestingPlace);
    EXPECT_LE(report.deaths, MostSecondTierDeaths);
}

TEST(BalanceTests, Mage_ClearsTheSecondTier) {
    const auto report = Play(CharacterClass::Mage, SecondRestingPlace, SecondBudgetMinutes);
    EXPECT_EQ(report.depth, SecondRestingPlace);
    EXPECT_LE(report.deaths, MostSecondTierDeaths);
}

TEST(BalanceTests, Rogue_ClearsTheSecondTier) {
    const auto report = Play(CharacterClass::Rogue, SecondRestingPlace, SecondBudgetMinutes);
    EXPECT_EQ(report.depth, SecondRestingPlace);
    EXPECT_LE(report.deaths, MostRogueSecondTierDeaths);
}

TEST(BalanceTests, Cleric_ClearsTheSecondTier) {
    const auto report = Play(CharacterClass::Cleric, SecondRestingPlace, SecondBudgetMinutes);
    EXPECT_EQ(report.depth, SecondRestingPlace);
    EXPECT_LE(report.deaths, MostSecondTierDeaths);
}

// The third tier: the ruins' creatures and the Frost Golem, with what the first two gave
namespace {
    constexpr std::int32_t ThirdRestingPlace = 3 * GameEngine::BossFloorInterval + 1;
    constexpr float ThirdBudgetMinutes = 45.0f;
    // At the last pass: Warrior 0, Mage 15, Rogue 33, Cleric 8 deaths, level 14 or 15 (the third
    // item tier's levels), 18 to 34 minutes; over six seeds 0 to 33 deaths
    constexpr std::int32_t MostThirdTierDeaths = 45;
}

TEST(BalanceTests, Warrior_ClearsTheThirdTier) {
    const auto report = Play(CharacterClass::Warrior, ThirdRestingPlace, ThirdBudgetMinutes);
    EXPECT_EQ(report.depth, ThirdRestingPlace);
    EXPECT_LE(report.deaths, MostThirdTierDeaths);
}

TEST(BalanceTests, Mage_ClearsTheThirdTier) {
    const auto report = Play(CharacterClass::Mage, ThirdRestingPlace, ThirdBudgetMinutes);
    EXPECT_EQ(report.depth, ThirdRestingPlace);
    EXPECT_LE(report.deaths, MostThirdTierDeaths);
}

TEST(BalanceTests, Rogue_ClearsTheThirdTier) {
    const auto report = Play(CharacterClass::Rogue, ThirdRestingPlace, ThirdBudgetMinutes);
    EXPECT_EQ(report.depth, ThirdRestingPlace);
    EXPECT_LE(report.deaths, MostThirdTierDeaths);
}

TEST(BalanceTests, Cleric_ClearsTheThirdTier) {
    const auto report = Play(CharacterClass::Cleric, ThirdRestingPlace, ThirdBudgetMinutes);
    EXPECT_EQ(report.depth, ThirdRestingPlace);
    EXPECT_LE(report.deaths, MostThirdTierDeaths);
}

// The fourth tier (the archive's creatures and the Abomination, toward the level cap), the final
// floor (the First Archivist: the victory), and New Game+'s first tier, in one run: the bot takes
// Descend Again on the victory page (design-dungeon-and-world.md 7.4, design-loot-and-progression.md 7.3)
namespace {
    constexpr std::int32_t FourthRestingPlace = 4 * GameEngine::BossFloorInterval + 1;
    constexpr float FourthBudgetMinutes = 60.0f;
    // At the last pass: Warrior 0, Mage 32, Rogue 33, Cleric 8 deaths, level 19 or 20 (the cap),
    // 24 to 45 minutes; over six seeds 0 to 33 deaths, 21 to 49 minutes
    constexpr std::int32_t MostFourthTierDeaths = 60;
    // From the fourth resting place to the victory. The hall's boss starts over whenever the hero
    // falls, so it must be won in one life. At the last pass: Warrior 1.1, Mage 1.0, Rogue 1.5,
    // Cleric 1.2 minutes, without a death; over six seeds (LB_BALANCE_SEED 1 to 5 and this one)
    // every class won without a death in 1.0 to 2.1 minutes. Before the bot sold or dropped its
    // spares, its bag was full by the third tier and it met the Archivist in that tier's gear: the
    // Rogue's fight swung from 0 deaths to 77 in 58 minutes.
    constexpr float FinalFloorBudgetMinutes = 15.0f;
    constexpr std::int32_t MostFinalFloorDeaths = 15;
    // From the victory to New Game+1's first resting place. At the last pass: no deaths; 2.9 to
    // 4.4 minutes, the Cleric 10.4 (its blows land least on the veterans' armour); over six seeds
    // no deaths, 2.7 to 10.7 minutes
    constexpr float NewGamePlusBudgetMinutes = 30.0f;
    constexpr std::int32_t MostNewGamePlusDeaths = 30;

    /// <summary>
    /// Where a run stood at a milestone: game minutes and deaths so far, the hero's level
    /// </summary>
    struct Milestone {
        bool reached{false};
        float minutes{0.0f};
        std::int32_t deaths{0};
        std::int32_t level{1};
    };

    struct CampaignReport {
        Milestone fourthRest;    // the fourth tier's resting place
        Milestone victory;       // the victory page
        Milestone newGamePlus;   // New Game+1's first resting place
    };

    // Play a class from the first floor to the victory and through New Game+'s first tier
    CampaignReport PlayToTheEndAndBeyond(CharacterClass characterClass) {
        GameEngine engine;
        EXPECT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = BalanceSeed(),
            .dungeon = true, .playerClass = characterClass })) << engine.GetInitializationError();
        Bot bot(engine, characterClass);
        CampaignReport report;
        std::int32_t deaths = 0;
        bool alive = true;
        const auto budget = static_cast<std::int64_t>((FourthBudgetMinutes + FinalFloorBudgetMinutes + NewGamePlusBudgetMinutes) * 60.0f * 60.0f);
        std::int64_t step = 0;
        const auto mark = [&](Milestone& milestone) {
            milestone = Milestone{ true, static_cast<float>(step) / 3600.0f, deaths, PlayerLevel(engine) };
        };
        for (; step < budget && !report.newGamePlus.reached; ++step) {
            if (engine.GetMenu().Page() == MenuPage::Victory) {
                if (!report.victory.reached) mark(report.victory);
                bot.Release();
                if (engine.GetMenu().Selected() && engine.GetMenu().Selected()->command == MenuCommand::DescendAgain) TapKey(engine, Keys::Enter);
            }
            else {
                bot.Step();
            }
            engine.Tick(OneSixtieth);
            if (alive && !engine.IsPlayerAlive()) ++deaths;
            alive = engine.IsPlayerAlive();
            if (!report.fourthRest.reached && engine.GetCycle() == 0 && engine.GetDepth() == FourthRestingPlace) mark(report.fourthRest);
            if (engine.GetCycle() == 1 && engine.GetDepth() == RestingPlace) mark(report.newGamePlus);
        }
        bot.Release();
        const auto print = [&](const char* what, const Milestone& milestone, const Milestone& before) {
            if (!milestone.reached) {
                std::printf("[ BALANCE  ] %-8s %s not reached (stopped on depth %d, cycle %d, level %d; %d deaths, %.1f minutes in all)\n",
                            LB::Character::ToString(characterClass), what, engine.GetDepth(), engine.GetCycle(), PlayerLevel(engine), deaths,
                            static_cast<float>(step) / 3600.0f);
                return;
            }
            std::printf("[ BALANCE  ] %-8s %s: level %d, %d deaths, %.1f minutes (%d deaths, %.1f minutes since the last)\n", LB::Character::ToString(characterClass),
                        what, milestone.level, milestone.deaths, milestone.minutes, milestone.deaths - before.deaths, milestone.minutes - before.minutes);
        };
        print("depth 13", report.fourthRest, Milestone{ true });
        print("victory", report.victory, report.fourthRest);
        print("NG+1 depth 4", report.newGamePlus, report.victory);
        return report;
    }

    void ExpectTheEndAndBeyond(const CampaignReport& report) {
        ASSERT_TRUE(report.fourthRest.reached);
        EXPECT_LE(report.fourthRest.minutes, FourthBudgetMinutes);
        EXPECT_LE(report.fourthRest.deaths, MostFourthTierDeaths);
        ASSERT_TRUE(report.victory.reached);
        EXPECT_LE(report.victory.minutes - report.fourthRest.minutes, FinalFloorBudgetMinutes);
        EXPECT_LE(report.victory.deaths - report.fourthRest.deaths, MostFinalFloorDeaths);
        ASSERT_TRUE(report.newGamePlus.reached);
        EXPECT_LE(report.newGamePlus.minutes - report.victory.minutes, NewGamePlusBudgetMinutes);
        EXPECT_LE(report.newGamePlus.deaths - report.victory.deaths, MostNewGamePlusDeaths);
    }
}

TEST(BalanceTests, Warrior_WinsTheRunAndDescendsAgain) {
    ExpectTheEndAndBeyond(PlayToTheEndAndBeyond(CharacterClass::Warrior));
}

TEST(BalanceTests, Mage_WinsTheRunAndDescendsAgain) {
    ExpectTheEndAndBeyond(PlayToTheEndAndBeyond(CharacterClass::Mage));
}

TEST(BalanceTests, Rogue_WinsTheRunAndDescendsAgain) {
    ExpectTheEndAndBeyond(PlayToTheEndAndBeyond(CharacterClass::Rogue));
}

TEST(BalanceTests, Cleric_WinsTheRunAndDescendsAgain) {
    ExpectTheEndAndBeyond(PlayToTheEndAndBeyond(CharacterClass::Cleric));
}

#endif // NDEBUG
