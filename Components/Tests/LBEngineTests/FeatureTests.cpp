// FeatureTests.cpp - LB.Engine tests: cells closed off in the world, a treasure room behind a
// locked gate (and its key) or a lever, a secret room behind a cracked wall, candle puzzles, greater
// chests, and waystones
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    // A floor of `depth` with what `wanted` looks for (seeds tried in turn)
    template<typename Wanted>
    std::uint32_t FindFloor(std::int32_t depth, Wanted wanted) {
        for (std::uint32_t seed = 1; seed < 400; ++seed) {
            GameEngine engine;
            if (!engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = seed, .dungeon = true })) return 0;
            engine.EnterFloor(depth);
            if (wanted(engine)) return seed;
        }
        return 0;
    }

    // The touches that light every candle of the floor's first puzzle (each touch flips three in
    // the ring: try every set)
    void Solve(GameEngine& engine) {
        const auto puzzle = engine.GetFeatures().Puzzles().front();
        std::optional<std::uint32_t> solution;
        for (std::uint32_t mask = 0; mask < (1u << puzzle.candles.size()) && !solution; ++mask) {
            auto trial = puzzle;
            for (std::size_t c = 0; c < trial.candles.size(); ++c) {
                if (mask & (1u << c)) trial.Touch(c);
            }
            if (trial.solved) solution = mask;
        }
        ASSERT_TRUE(solution.has_value());
        for (std::size_t c = 0; c < puzzle.candles.size(); ++c) {
            if (*solution & (1u << c)) engine.TouchCandle(0, c);
        }
        ASSERT_TRUE(engine.GetFeatures().Puzzles().front().solved);
    }

    void Open(GameEngine& engine, std::uint32_t seed, std::int32_t depth) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = seed, .dungeon = true }));
        engine.EnterFloor(depth);
        engine.Tick(OneSixtieth);
    }

    const Gate* GateOf(const GameEngine& engine, GateKind kind) {
        for (const auto& gate : engine.GetFeatures().Gates()) {
            if (gate.kind == kind) return &gate;
        }
        return nullptr;
    }

    // Where the hero stands at a gate, on the far side from its room: up against it, as walking
    // up to it leaves them (a little over a tile from its middle)
    WorldPoint Outside(const GameEngine& engine, const Gate& gate) {
        const auto& world = engine.GetWorld();
        for (const auto& [dx, dy] : { std::pair{ 1, 0 }, std::pair{ -1, 0 }, std::pair{ 0, 1 }, std::pair{ 0, -1 } }) {
            const auto x = gate.cellX + dx, y = gate.cellY + dy;
            if (!world.Layout().IsFloor(x, y) || world.Layout().RoomAt(x, y) == gate.room) continue;
            const auto next = world.CellCenter(x, y);
            return { gate.center.x + (next.x - gate.center.x) * 0.7f, gate.center.y + (next.y - gate.center.y) * 0.7f };
        }
        return gate.center;
    }
}

TEST(FeatureTests, BlockedCells_StopBodiesSightAndPaths) {
    auto world = LShapedWorld();
    const auto from = world.CellCenter(1, 1);
    const auto to = world.CellCenter(6, 6);
    ASSERT_FALSE(world.FindPath(from, to).empty());

    world.SetBlocked(2, 5, true);   // the corridor
    EXPECT_TRUE(world.FindPath(from, to).empty());
    EXPECT_FALSE(world.IsOpen(world.CellCenter(2, 5)));
    EXPECT_FALSE(world.CircleFits(world.CellCenter(2, 5), 0.3f));
    EXPECT_FALSE(world.LineOfSight(world.CellCenter(2, 4), world.CellCenter(2, 6)));

    world.SetBlocked(2, 5, false);
    EXPECT_FALSE(world.FindPath(from, to).empty());
}

TEST(FeatureTests, PuzzleRooms_FromTheSecondFloor) {
    const auto seed = FindFloor(2, [](const GameEngine& engine) {
        return std::ranges::any_of(engine.GetWorld().Layout().rooms, [](const auto& room) { return room.type == LB::Dungeon::RoomType::Puzzle; });
    });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    Open(engine, seed, 2);
    ASSERT_FALSE(engine.GetFeatures().Puzzles().empty());
    const auto& puzzle = engine.GetFeatures().Puzzles().front();
    EXPECT_EQ(puzzle.candles.size(), FloorFeatures::PuzzleCandles);
    EXPECT_FALSE(puzzle.solved);
    EXPECT_FALSE(std::ranges::all_of(puzzle.candles, &PuzzleCandle::lit));   // scrambled
}

TEST(FeatureTests, ACandlePuzzle_Solved_OpensAGreaterChest) {
    const auto seed = FindFloor(2, [](const GameEngine& engine) { return !engine.GetFeatures().Puzzles().empty(); });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    Open(engine, seed, 2);
    const auto puzzle = engine.GetFeatures().Puzzles().front();
    const auto chest = engine.GetWorld().RoomCenter(puzzle.room);

    // Not before it is solved
    engine.EnterFloorPosition(chest);
    RunSteps(engine, 5);
    EXPECT_FALSE(engine.GetRoomStates()[static_cast<std::size_t>(puzzle.room)].chestOpened);

    Solve(engine);

    const auto gold = engine.GetGold();
    const auto bag = engine.GetInventory().size();
    RunSteps(engine, 3);   // it opens, and what falls at the hero's feet is picked up at once
    EXPECT_TRUE(engine.GetRoomStates()[static_cast<std::size_t>(puzzle.room)].chestOpened);
    EXPECT_GT(engine.GetGold(), gold);                // every roll brings gold
    EXPECT_GE(engine.GetInventory().size(), bag + 1);  // and finds
}

TEST(FeatureTests, AGreaterChestsLid_SwingsOpen) {
    const auto seed = FindFloor(2, [](const GameEngine& engine) { return !engine.GetFeatures().Puzzles().empty(); });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(),
        .randomSeed = seed, .dungeon = true }));
    engine.EnterFloor(2);
    engine.Tick(OneSixtieth);
    const auto chest = engine.GetWorld().RoomCenter(engine.GetFeatures().Puzzles().front().room);
    engine.EnterFloorPosition(chest);
    Solve(engine);
    // The lid's angle (the cosine in its pose) as the opened chest is drawn (the floor may hold
    // another greater chest, closed, behind a gate)
    const auto lid = [&]() -> std::optional<float> {
        for (const auto& draw : DrawsOf(engine, std::filesystem::path(GameEngine::GreaterChestModel).stem().string())) {
            if (draw.partPoses.size() == 1) return draw.partPoses[0].transform.m[5];
        }
        return std::nullopt;
    };

    RunSteps(engine, 6);
    const auto room = static_cast<std::size_t>(engine.GetFeatures().Puzzles().front().room);
    ASSERT_TRUE(engine.GetRoomStates()[room].chestOpened);
    ASSERT_TRUE(lid().has_value());   // still the greater chest, its lid posed
    const float early = *lid();
    RunSteps(engine, 40);
    EXPECT_LT(*lid(), early);         // swung further back (past upright: the cosine goes negative)
    EXPECT_LT(*lid(), 0.0f);
}

TEST(FeatureTests, ALockedGate_OpensForAKey) {
    const auto seed = FindFloor(2, [](const GameEngine& engine) { return GateOf(engine, GateKind::Locked) != nullptr; });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    Open(engine, seed, 2);
    const auto gate = *GateOf(engine, GateKind::Locked);
    EXPECT_TRUE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    EXPECT_EQ(engine.GetWorld().Layout().rooms[static_cast<std::size_t>(gate.room)].type, LB::Dungeon::RoomType::Treasure);
    const auto keyRoom = engine.GetFeatures().KeyRoom();
    ASSERT_GE(keyRoom, 0);
    EXPECT_TRUE(engine.GetWorld().Layout().rooms[static_cast<std::size_t>(keyRoom)].onCriticalPath);   // on the way: no soft-lock

    // Without a key
    engine.EnterFloorPosition(Outside(engine, gate));
    RunSteps(engine, 5);
    EXPECT_FALSE(engine.GetFeatures().Gates().front().open && engine.GetFeatures().Gates().front().kind == GateKind::Locked);
    EXPECT_TRUE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));

    // With one: it opens, and the key is spent
    engine.GiveItem(GameEngine::KeyItem);
    RunSteps(engine, 5);
    EXPECT_FALSE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    EXPECT_EQ(engine.CountItem(GameEngine::KeyItem), 0);
    EXPECT_FALSE(engine.UseItem(GameEngine::KeyItem));   // keys are not used from the bag
}

TEST(FeatureTests, TheKey_FallsWhenItsRoomIsClear) {
    const auto seed = FindFloor(2, [](const GameEngine& engine) { return GateOf(engine, GateKind::Locked) != nullptr; });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    Open(engine, seed, 2);
    engine.GrantExperience(20000);   // a hero who wins this fight
    engine.Tick(OneSixtieth);
    const auto room = engine.GetFeatures().KeyRoom();
    const auto center = engine.GetWorld().RoomCenter(room);
    engine.EnterFloorPosition(center);

    engine.OnKeyDown(Keys::Space);
    for (int i = 0; i < 60 * 90 && !engine.GetRoomStates()[static_cast<std::size_t>(room)].cleared; ++i) {
        auto goal = center;
        float nearest = std::numeric_limits<float>::max();
        for (const auto& enemy : engine.GetEncounter().GetEnemies()) {
            const float distance = Encounter::Distance(enemy.position, engine.GetPlayerPosition());
            if (enemy.IsTargetable() && distance < nearest) {
                nearest = distance;
                goal = enemy.position;
            }
        }
        SteerToward(engine, goal);
        engine.Tick(OneSixtieth);
    }
    engine.OnKeyUp(Keys::Space);
    ASSERT_TRUE(engine.GetRoomStates()[static_cast<std::size_t>(room)].cleared);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.GetFeatures().KeyDropped());
    const auto key = ItemRef::FromKey(*engine.GetContent(), GameEngine::KeyItem);
    EXPECT_TRUE(std::ranges::any_of(engine.GetGroundItems(), [&](const GroundItem& ground) { return ground.item == key; })
                || engine.CountItem(GameEngine::KeyItem) > 0);   // lying there, or picked up already
}

TEST(FeatureTests, ASecretWall_BreaksUnderABlow) {
    const auto seed = FindFloor(3, [](const GameEngine& engine) { return GateOf(engine, GateKind::Secret) != nullptr; });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    Open(engine, seed, 3);
    const auto gate = *GateOf(engine, GateKind::Secret);
    ASSERT_TRUE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    const auto outside = Outside(engine, gate);
    engine.EnterFloorPosition(outside);
    // Face it (a step toward it), then swing
    for (int i = 0; i < 20; ++i) {
        SteerToward(engine, gate.center);
        engine.Tick(OneSixtieth);
    }
    engine.OnGamepadState(0, GamepadState{ .connected = true });
    engine.OnKeyDown(Keys::Space);
    RunSteps(engine, 40);
    engine.OnKeyUp(Keys::Space);

    EXPECT_FALSE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    EXPECT_TRUE(std::ranges::any_of(engine.GetFeatures().Gates(), [](const Gate& g) { return g.kind == GateKind::Secret && g.open; }));
}

TEST(FeatureTests, OpenedGatesAndCandles_KeptInSaves) {
    const auto seed = FindFloor(2, [](const GameEngine& engine) {
        return GateOf(engine, GateKind::Locked) != nullptr && !engine.GetFeatures().Puzzles().empty();
    });
    ASSERT_NE(seed, 0u);
    TempSaveDir dir;
    const EngineOptions options{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(), .randomSeed = seed, .dungeon = true };
    Gate gate;
    std::vector<bool> lit;
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
        engine.EnterFloor(2);
        engine.Tick(OneSixtieth);
        gate = *GateOf(engine, GateKind::Locked);
        engine.GiveItem(GameEngine::KeyItem);
        engine.EnterFloorPosition(Outside(engine, gate));
        RunSteps(engine, 5);
        ASSERT_FALSE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
        engine.TouchCandle(0, 1);
        for (const auto& candle : engine.GetFeatures().Puzzles().front().candles) lit.push_back(candle.lit);
        ASSERT_TRUE(engine.SaveGame(1));
    }
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
    ASSERT_TRUE(engine.LoadGame(1));
    EXPECT_FALSE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    const auto& candles = engine.GetFeatures().Puzzles().front().candles;
    for (std::size_t c = 0; c < candles.size(); ++c) EXPECT_EQ(candles[c].lit, lit[c]) << c;
}

// A lever gate (design-dungeon-and-world.md 6.3): bars no key opens; a lever in the room on the
// way does, once, and says so on the run's bus
TEST(FeatureTests, ALever_OpensItsGate) {
    const auto seed = FindFloor(5, [](const GameEngine& engine) { return GateOf(engine, GateKind::Lever) != nullptr; });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    Open(engine, seed, 5);
    const auto gate = *GateOf(engine, GateKind::Lever);
    ASSERT_EQ(engine.GetFeatures().Levers().size(), 1u);
    const auto lever = engine.GetFeatures().Levers().front();
    EXPECT_EQ(&engine.GetFeatures().Gates()[lever.gate], GateOf(engine, GateKind::Lever));
    EXPECT_TRUE(engine.GetWorld().Layout().rooms[static_cast<std::size_t>(lever.room)].onCriticalPath);   // on the way: no soft-lock
    EXPECT_EQ(engine.GetFeatures().KeyRoom(), -1);   // no key on this floor
    EXPECT_TRUE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    std::vector<FeatureKind> activated;
    engine.GetEvents().Subscribe<FeatureActivatedEvent>([&](const FeatureActivatedEvent& event) { activated.push_back(event.kind); });

    // A key does not open it
    engine.GiveItem(GameEngine::KeyItem);
    engine.EnterFloorPosition(Outside(engine, gate));
    RunSteps(engine, 5);
    EXPECT_TRUE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    EXPECT_EQ(engine.CountItem(GameEngine::KeyItem), 1);

    // Interact by the lever does
    engine.EnterFloorPosition({ lever.position.x + 0.5f, lever.position.y + 0.5f });
    RunSteps(engine, 2);
    TapKey(engine, Keys::FromChar('F'));
    RunSteps(engine, 2);
    EXPECT_TRUE(engine.GetFeatures().Levers().front().pulled);
    EXPECT_FALSE(engine.GetWorld().IsBlocked(gate.cellX, gate.cellY));
    EXPECT_EQ(activated, std::vector<FeatureKind>{ FeatureKind::Lever });
}

// Waystones (design-dungeon-and-world.md 6.3, teleporters): the far one awakens when reached; then
// either carries the hero to the other, unless enemies are awake nearby; kept in saves
TEST(FeatureTests, Waystones_CarryTheHeroBackAndForth) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .randomSeed = CombatSeed, .dungeon = true }));
    engine.EnterFloor(2);
    engine.Tick(OneSixtieth);
    const auto stones = engine.GetFeatures().Waystones();
    ASSERT_EQ(stones.size(), 2u);
    const auto& layout = engine.GetWorld().Layout();
    EXPECT_EQ(stones[0].room, layout.entranceRoom);
    EXPECT_TRUE(std::ranges::find(layout.rooms[static_cast<std::size_t>(layout.exitRoom)].connections, stones[1].room)
                != layout.rooms[static_cast<std::size_t>(layout.exitRoom)].connections.end());   // the room before the way down

    // Asleep: no travel yet
    engine.EnterFloorPosition({ stones[0].position.x + 0.3f, stones[0].position.y });
    RunSteps(engine, 2);
    EXPECT_FALSE(engine.TravelByWaystone(0));

    // Reached: it awakens, and carries the hero back to the entrance
    engine.EnterFloorPosition(stones[1].position);
    RunSteps(engine, 2);
    ASSERT_TRUE(engine.GetFeatures().WaystonesAwake());
    const auto awake = std::ranges::any_of(engine.GetEncounter().GetEnemies(), [](const Enemy& e) { return e.state == EnemyState::Chasing || e.state == EnemyState::Attacking; });
    if (awake) {
        EXPECT_FALSE(engine.TravelByWaystone(1));   // not with enemies awake nearby
        for (const auto& enemy : engine.GetEncounter().GetEnemies()) engine.DefeatEnemy(enemy.id);
        RunSteps(engine, 2);
    }
    ASSERT_TRUE(engine.TravelByWaystone(1));
    EXPECT_LT(Encounter::Distance(engine.GetPlayerPosition(), stones[0].position), 1.5f);
    EXPECT_EQ(engine.GetWorld().RoomAt(engine.GetPlayerPosition()), layout.entranceRoom);

    // Kept in a save
    ASSERT_TRUE(engine.SaveGame(GameEngine::SaveSlots[0]));
    GameEngine loaded;
    ASSERT_TRUE(loaded.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .randomSeed = CombatSeed, .dungeon = true }));
    ASSERT_TRUE(loaded.LoadGame(GameEngine::SaveSlots[0]));
    EXPECT_TRUE(loaded.GetFeatures().WaystonesAwake());
    EXPECT_FALSE(loaded.GetFeatures().Waystones().empty());
}

// Mage Hand pulls a lever from afar
TEST(FeatureTests, MageHand_PullsALeverFromAfar) {
    const auto seed = FindFloor(5, [](const GameEngine& engine) { return GateOf(engine, GateKind::Lever) != nullptr; });
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = seed, .dungeon = true,
        .playerClass = LB::Character::CharacterClass::Mage }));
    engine.EnterFloor(5);
    engine.Tick(OneSixtieth);
    ASSERT_EQ(engine.GetFeatures().Levers().size(), 1u);
    const auto lever = engine.GetFeatures().Levers().front();
    const auto room = engine.GetWorld().RoomCenter(lever.room);
    engine.EnterFloorPosition(room);   // a few tiles off, in sight
    RunSteps(engine, 2);
    for (const auto& enemy : engine.GetEncounter().GetEnemies()) engine.DefeatEnemy(enemy.id);
    RunSteps(engine, 2);
    ASSERT_GT(Encounter::Distance(engine.GetPlayerPosition(), lever.position), GameEngine::LeverReach);
    ASSERT_TRUE(engine.SetSpell(0, "spell_utility_mage_hand_l1"));
    TapKey(engine, Keys::FromChar('1'));
    RunSteps(engine, 20);
    EXPECT_TRUE(engine.GetFeatures().Levers().front().pulled);
    EXPECT_TRUE(engine.GetFeatures().Gates()[lever.gate].open);
}

namespace {
    // A room's enemies rise from the ground before they can be struck
    void WaitUntilRisen(GameEngine& engine) {
        for (int i = 0; i < 600; ++i) {
            const auto& enemies = engine.GetEncounter().GetEnemies();
            if (!enemies.empty() && std::ranges::none_of(enemies, [](const Enemy& e) { return e.state == EnemyState::Spawning; })) return;
            engine.Tick(OneSixtieth);
        }
    }
}

// The Portal (design-spells-system.md, Portal: dungeon to town): from below a resting place it
// carries the hero to the last one above; Interact by the portal there brings them back to the
// very spot, the floor as they left it; kept in a save
TEST(FeatureTests, Portal_ToTheRestingPlaceAndBack) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .randomSeed = CombatSeed, .dungeon = true, .playerClass = LB::Character::CharacterClass::Mage, .startDepth = 3, .startLevel = 14 }))
        << engine.GetInitializationError();
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.PortalBlocker(), "NO RESTING PLACE ABOVE");   // depth 3: none yet
    EXPECT_EQ(GameEngine::RestDepthAbove(9), 7);
    EXPECT_EQ(GameEngine::RestDepthAbove(5), 4);
    EXPECT_FALSE(GameEngine::RestDepthAbove(4).has_value());

    engine.EnterFloor(8);
    engine.Tick(OneSixtieth);
    // Somewhere off the entrance, a room cleared, gold on the ground
    const auto& layout = engine.GetWorld().Layout();
    const auto room = layout.rooms[static_cast<std::size_t>(layout.entranceRoom)].connections.front();
    engine.EnterFloorPosition(engine.GetWorld().RoomCenter(room));
    WaitUntilRisen(engine);
    for (const auto& enemy : engine.GetEncounter().GetEnemies()) engine.DefeatEnemy(enemy.id);
    RunSteps(engine, 2);
    ASSERT_TRUE(engine.DropItem(ItemRef::FromKey(*engine.GetContent(), GameEngine::HealingPotion)));
    const auto spot = engine.GetPlayerPosition();
    const auto lying = engine.GetGroundItems().size();
    ASSERT_GT(lying, 0u);

    // Cast: three seconds, then the resting place of depth 7
    ASSERT_TRUE(engine.SetSpell(0, "spell_utility_portal_l4"));
    EXPECT_EQ(engine.SpellBlocker(0), "");
    const auto mana = PlayerMana(engine);
    TapKey(engine, Keys::FromChar('1'));
    RunSteps(engine, 200);
    ASSERT_EQ(engine.GetDepth(), 7);
    EXPECT_TRUE(engine.IsRestFloor());
    EXPECT_LT(PlayerMana(engine), mana);
    ASSERT_TRUE(engine.GetPortal().has_value());
    EXPECT_EQ(engine.GetPortalDepth(), 8);
    EXPECT_EQ(engine.PortalBlocker(), "YOU ARE AT REST");

    // Kept in a save made at the resting place
    ASSERT_TRUE(engine.SaveGame(GameEngine::SaveSlots[0]));
    GameEngine loaded;
    ASSERT_TRUE(loaded.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .randomSeed = CombatSeed, .dungeon = true, .playerClass = LB::Character::CharacterClass::Mage }));
    ASSERT_TRUE(loaded.LoadGame(GameEngine::SaveSlots[0]));
    EXPECT_EQ(loaded.GetDepth(), 7);
    EXPECT_EQ(loaded.GetPortalDepth(), 8);

    // Interact by the portal: back on depth 8, on the spot, the room still clear, the gold still there
    engine.EnterFloorPosition(*engine.GetPortal());
    RunSteps(engine, 2);
    TapKey(engine, Keys::FromChar('F'));
    RunSteps(engine, 2);
    ASSERT_EQ(engine.GetDepth(), 8);
    EXPECT_FALSE(engine.GetPortal().has_value());
    EXPECT_LT(Encounter::Distance(engine.GetPlayerPosition(), spot), 0.2f);
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());   // the cleared room did not refill
    EXPECT_EQ(engine.GetGroundItems().size(), lying);

    // From the loaded save, the way down goes back too (not to the floors between)
    loaded.Descend();
    EXPECT_EQ(loaded.GetDepth(), 8);
    EXPECT_FALSE(loaded.GetPortal().has_value());
}

// Not with enemies awake nearby: the cast is refused, and one that wakes them midway costs nothing
TEST(FeatureTests, Portal_NotWithEnemiesNear) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(),
        .randomSeed = CombatSeed, .dungeon = true, .playerClass = LB::Character::CharacterClass::Mage, .startDepth = 5, .startLevel = 14 }));
    engine.Tick(OneSixtieth);
    const auto& layout = engine.GetWorld().Layout();
    const auto room = layout.rooms[static_cast<std::size_t>(layout.entranceRoom)].connections.front();
    engine.EnterFloorPosition(engine.GetWorld().RoomCenter(room));
    WaitUntilRisen(engine);   // its enemies rise and come
    RunSteps(engine, 10);
    ASSERT_FALSE(engine.GetEncounter().GetEnemies().empty()) << "the room next to the entrance is empty on this seed";
    ASSERT_TRUE(engine.SetSpell(0, "spell_utility_portal_l4"));
    EXPECT_EQ(engine.PortalBlocker(), "ENEMIES ARE NEAR");
    EXPECT_EQ(engine.SpellBlocker(0), "portal ENEMIES ARE NEAR");
    EXPECT_FALSE(engine.OpenPortal());
    EXPECT_EQ(engine.GetDepth(), 5);
}
