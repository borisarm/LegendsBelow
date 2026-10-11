// DungeonTests.cpp - LB.Engine tests: the world map, dungeon floors, torches, and the minimap
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// World Map Tests
// ============================================================================

TEST(WorldMapTests, Arena_CenteredOnTheOrigin) {
    const auto world = WorldMap::Arena(10.0f, 7.0f);

    EXPECT_TRUE(world.IsOpen({ 0.0f, 0.0f }));
    EXPECT_TRUE(world.IsOpen({ 9.9f, 6.9f }));
    EXPECT_FALSE(world.IsOpen({ 10.1f, 0.0f }));
    EXPECT_FALSE(world.IsOpen({ 0.0f, -7.1f }));
    EXPECT_EQ(world.RoomAt({ 0.0f, 0.0f }), 0);
    EXPECT_FLOAT_EQ(world.RoomCenter(0).x, 0.0f);
    EXPECT_FLOAT_EQ(world.RoomCenter(0).y, 0.0f);
}

TEST(WorldMapTests, Move_StopsExactlyAtTheWall) {
    const auto world = WorldMap::Arena(10.0f, 7.0f);

    const auto stopped = world.Move({ 8.0f, 0.0f }, { 5.0f, 0.0f }, 0.375f);

    EXPECT_FLOAT_EQ(stopped.x, 10.0f - 0.375f);
    EXPECT_FLOAT_EQ(stopped.y, 0.0f);
}

TEST(WorldMapTests, Move_SlidesAlongTheWall) {
    const auto world = WorldMap::Arena(10.0f, 7.0f);

    const auto slid = world.Move({ 9.5f, 0.0f }, { 1.0f, 1.0f }, 0.375f);

    EXPECT_FLOAT_EQ(slid.x, 10.0f - 0.375f);
    EXPECT_FLOAT_EQ(slid.y, 1.0f);
}

TEST(WorldMapTests, Move_NeverThroughAWall) {
    const auto world = LShapedWorld();

    // From room A straight toward room B (through solid cells): only up to A's wall
    const auto moved = world.Move(world.RoomCenter(0), { 20.0f, 20.0f }, 0.3f);

    EXPECT_EQ(world.RoomAt(moved), 0);
    EXPECT_TRUE(world.CircleFits(moved, 0.3f));
}

TEST(WorldMapTests, LineOfSight_BlockedByWalls) {
    const auto world = LShapedWorld();

    EXPECT_TRUE(world.LineOfSight({ 3.0f, 3.0f }, { 7.0f, 7.0f }));     // across room A
    EXPECT_FALSE(world.LineOfSight(world.RoomCenter(0), world.RoomCenter(1)));
}

TEST(WorldMapTests, FlowField_LeadsAroundTheCorner) {
    const auto world = LShapedWorld();
    world.BuildFlowField(world.RoomCenter(1));

    // Follow waypoints from room A: down the corridor, then along it into room B
    auto at = world.RoomCenter(0);
    for (int i = 0; i < 40; ++i) {
        const auto next = world.NextWaypoint(at);
        if (!next) break;
        at = *next;
    }

    EXPECT_EQ(world.RoomAt(at), 1);
}

TEST(WorldMapTests, FlowField_UnreachableTarget_NoWaypoint) {
    const auto world = LShapedWorld();
    world.BuildFlowField({ -50.0f, -50.0f });

    EXPECT_FALSE(world.NextWaypoint(world.RoomCenter(0)).has_value());
}

// ============================================================================
// GameEngine Dungeon Tests
// ============================================================================

TEST(GameEngineDungeonTests, Start_PlayerAtTheEntranceOfDepthOne) {
    GameEngine engine;
    InitializeDungeon(engine);

    const auto& layout = engine.GetWorld().Layout();
    EXPECT_EQ(engine.GetDepth(), 1);
    EXPECT_GE(layout.rooms.size(), 5u);
    EXPECT_EQ(engine.GetWorld().RoomAt(engine.GetPlayerPosition()), layout.entranceRoom);
    EXPECT_TRUE(engine.GetRoomStates()[static_cast<std::size_t>(layout.entranceRoom)].cleared);
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());   // nothing until a room is entered
    EXPECT_EQ(engine.GetEncounter().GetWave(), 0);
}

TEST(GameEngineDungeonTests, SameSeed_SameFloor) {
    GameEngine a;
    GameEngine b;
    InitializeDungeon(a, 5);
    InitializeDungeon(b, 5);

    EXPECT_EQ(a.GetWorld().Layout().cells, b.GetWorld().Layout().cells);
}

TEST(GameEngineDungeonTests, EnteringACombatRoom_RaisesItsEnemies) {
    GameEngine engine;
    InitializeDungeon(engine);
    const auto room = FindRoom(engine, LB::Dungeon::RoomType::Combat);
    ASSERT_GE(room, 0);

    WalkTo(engine, engine.GetWorld().RoomCenter(room), 60 * 60);

    const auto& state = engine.GetRoomStates()[static_cast<std::size_t>(room)];
    EXPECT_TRUE(state.triggered);
    EXPECT_GE(state.enemies.size(), 2u);
    for (const auto id : state.enemies) {
        const auto* enemy = engine.GetEncounter().FindEnemy(id);
        ASSERT_NE(enemy, nullptr);
        EXPECT_EQ(engine.GetWorld().RoomAt(enemy->position), room);
    }
}

TEST(GameEngineDungeonTests, Walls_FollowTheFloor) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .randomSeed = CombatSeed, .dungeon = true }))
        << engine.GetInitializationError();

    // One wall segment per edge between a floor cell and a solid one
    const auto& layout = engine.GetWorld().Layout();
    std::size_t edges = 0;
    for (std::int32_t y = 0; y < layout.height; ++y) {
        for (std::int32_t x = 0; x < layout.width; ++x) {
            if (!layout.IsFloor(x, y)) continue;
            edges += !layout.IsFloor(x + 1, y) + !layout.IsFloor(x - 1, y) + !layout.IsFloor(x, y + 1) + !layout.IsFloor(x, y - 1);
        }
    }
    engine.Tick(OneSixtieth);
    EXPECT_EQ(DrawsOf(engine, "wall").size(), edges);
    EXPECT_GT(DrawsOf(engine, "pillar").size(), 4u);
    EXPECT_EQ(DrawsOf(engine, "trapdoor").size(), 1u);
}

TEST(GameEngineDungeonTests, Descend_NextFloorHealedAtTheEntrance) {
    GameEngine engine;
    InitializeDungeon(engine);
    const auto firstFloor = engine.GetWorld().Layout().cells;
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { engine.GetPlayerPosition().x + 1.0f, engine.GetPlayerPosition().y });
    StepUntil(engine, [&] { return PlayerHitPoints(engine) < PlayerMaxHitPoints(engine); }, 20.0f);

    engine.Descend();

    EXPECT_EQ(engine.GetDepth(), 2);
    EXPECT_NE(engine.GetWorld().Layout().cells, firstFloor);
    EXPECT_EQ(engine.GetWorld().RoomAt(engine.GetPlayerPosition()), engine.GetWorld().Layout().entranceRoom);
    EXPECT_EQ(PlayerHitPoints(engine), PlayerMaxHitPoints(engine));
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());
}

TEST(GameEngineDungeonTests, Chest_OpensAndDropsLoot) {
    // A floor with a treasure room (seeds are tried until one has one)
    std::optional<std::uint32_t> seed;
    for (std::uint32_t candidate = 1; candidate < 40 && !seed; ++candidate) {
        GameEngine probe;
        InitializeDungeon(probe, candidate);
        if (FindRoom(probe, LB::Dungeon::RoomType::Treasure) >= 0) seed = candidate;
    }
    ASSERT_TRUE(seed.has_value());
    GameEngine engine;
    InitializeDungeon(engine, *seed);
    const auto room = FindRoom(engine, LB::Dungeon::RoomType::Treasure);
    const auto gold = engine.GetGold();

    WalkTo(engine, engine.GetWorld().RoomCenter(room), 60 * 90, true);
    RunSteps(engine, 30);

    EXPECT_TRUE(engine.GetRoomStates()[static_cast<std::size_t>(room)].chestOpened);
    EXPECT_GT(engine.GetGold(), gold);   // the chest always holds gold, picked up on the spot
}

TEST(GameEngineDungeonTests, SaveAndLoad_SameFloorAndClearedRooms) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(), .randomSeed = CombatSeed, .dungeon = true }));
    engine.Descend();
    const auto floor = engine.GetWorld().Layout().cells;
    ASSERT_TRUE(engine.SaveGame(1));

    engine.Descend();
    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_EQ(engine.GetDepth(), 2);
    EXPECT_EQ(engine.GetWorld().Layout().cells, floor);
    EXPECT_TRUE(engine.GetRoomStates()[static_cast<std::size_t>(engine.GetWorld().Layout().entranceRoom)].cleared);
}

TEST(GameEngineDungeonTests, Playthrough_FightThroughToTheSecondFloor) {
    GameEngine engine;
    InitializeDungeon(engine);
    const auto& layout = engine.GetWorld().Layout();

    // Clear the critical path room by room (as a careful player would), going after the nearest
    // enemy (crossbowmen back away) and swinging at whatever is in reach, drinking a potion when
    // low; after a fall, set out again from the entrance
    std::string trace;
    const auto fightUntilCleared = [&](std::int32_t room) {
        engine.OnKeyDown(Keys::Space);
        for (int i = 0; i < 60 * 60 && engine.IsPlayerAlive() && !engine.GetRoomStates()[static_cast<std::size_t>(room)].cleared; ++i) {
            auto goal = engine.GetWorld().RoomCenter(room);
            float nearest = std::numeric_limits<float>::max();
            for (const auto& enemy : engine.GetEncounter().GetEnemies()) {
                const float distance = Encounter::Distance(enemy.position, engine.GetPlayerPosition());
                if (enemy.IsTargetable() && distance < nearest) {
                    nearest = distance;
                    goal = enemy.position;
                }
            }
            SteerToward(engine, goal);
            if (PlayerHitPoints(engine) * 10 < PlayerMaxHitPoints(engine) * 4) TapKey(engine, Keys::FromChar('R'));
            engine.Tick(OneSixtieth);
        }
        engine.OnKeyUp(Keys::Space);
    };
    for (int attempt = 0; attempt < 6 && engine.GetDepth() == 1; ++attempt) {
        for (std::int32_t room = 1; room <= layout.exitRoom && engine.IsPlayerAlive() && engine.GetDepth() == 1; ++room) {
            WalkTo(engine, engine.GetWorld().RoomCenter(room), 60 * 60, true);
            fightUntilCleared(room);
        }
        if (engine.IsPlayerAlive() && engine.GetDepth() == 1) {
            WalkTo(engine, engine.GetWorld().RoomCenter(layout.exitRoom), 60 * 10);   // onto the trapdoor
            engine.Tick(OneSixtieth);
        }
        trace += std::format("attempt {}: room {} alive {} level {} enemies {}\n", attempt,
            engine.GetWorld().RoomAt(engine.GetPlayerPosition()), engine.IsPlayerAlive(),
            engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetLevel(), engine.GetEncounter().LivingEnemyCount());
        if (!engine.IsPlayerAlive()) RunSteps(engine, static_cast<int>(GameEngine::PlayerRespawnSeconds * 60.0f) + 2);
    }

    EXPECT_EQ(engine.GetDepth(), 2) << trace;
}

// ============================================================================
// GameEngine Torch and Minimap Tests
// ============================================================================

TEST(GameEngineTorchTests, Torches_HangOnRoomWallsFacingIn) {
    GameEngine engine;
    InitializeDungeon(engine);
    const auto& world = engine.GetWorld();

    ASSERT_GE(engine.GetTorches().size(), engine.GetWorld().Layout().rooms.size());
    for (const auto& torch : engine.GetTorches()) {
        const WorldPoint ahead{ torch.position.x + std::sin(torch.yaw) * 0.5f, torch.position.y + std::cos(torch.yaw) * 0.5f };
        const WorldPoint behind{ torch.position.x - std::sin(torch.yaw) * 0.3f, torch.position.y - std::cos(torch.yaw) * 0.3f };
        EXPECT_TRUE(world.IsOpen(ahead));      // the room in front
        EXPECT_FALSE(world.IsOpen(behind));    // the wall behind
        EXPECT_GE(world.RoomAt(ahead), 0);     // in rooms, not corridors
    }
}

TEST(GameEngineTorchTests, EachTorch_LightsTheFrameAndFlickers) {
    GameEngine engine;
    InitializeDungeon(engine);

    engine.Tick(OneSixtieth);
    const auto first = engine.GetRendering().GetQueuedLights();
    RunSteps(engine, 7);
    const auto later = engine.GetRendering().GetQueuedLights();

    ASSERT_EQ(first.size(), engine.GetTorches().size());
    EXPECT_FLOAT_EQ(first[0].radius, GameEngine::TorchLightRadius);
    EXPECT_GT(first[0].position.y, 1.0f);   // up at the flame
    EXPECT_NE(first[0].intensity, later[0].intensity);
}

TEST(GameEngineTorchTests, Dungeon_DarkWithoutTorches_ArenaKeepsDefaultLight) {
    GameEngine dungeon;
    InitializeDungeon(dungeon);
    GameEngine arena;
    InitializeForCombat(arena);

    dungeon.Tick(OneSixtieth);
    arena.Tick(OneSixtieth);

    EXPECT_FLOAT_EQ(dungeon.GetRendering().GetLighting().directional, GameEngine::DungeonDirectionalLight);
    EXPECT_FLOAT_EQ(arena.GetRendering().GetLighting().directional, 1.0f);
    EXPECT_TRUE(arena.GetTorches().empty());
    EXPECT_TRUE(arena.GetRendering().GetQueuedLights().empty());
}

TEST(GameEngineTorchTests, Torches_DrawnWithModelsOnTheirWalls) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .randomSeed = CombatSeed, .dungeon = true }))
        << engine.GetInitializationError();

    engine.Tick(OneSixtieth);

    EXPECT_EQ(DrawsOf(engine, "torchWall").size(), engine.GetTorches().size());
    for (const auto& torch : engine.GetTorches()) EXPECT_TRUE(torch.wall.has_value());
}

TEST(GameEngineMinimapTests, Start_EntranceExploredFarRoomsNot) {
    GameEngine engine;
    InitializeDungeon(engine);
    const auto& layout = engine.GetWorld().Layout();
    const auto& entrance = layout.rooms[static_cast<std::size_t>(layout.entranceRoom)].area;
    const auto& exit = layout.rooms[static_cast<std::size_t>(layout.exitRoom)].area;

    for (std::int32_t y = entrance.y; y < entrance.y + entrance.height; ++y) {
        for (std::int32_t x = entrance.x; x < entrance.x + entrance.width; ++x) EXPECT_TRUE(engine.IsCellExplored(x, y));
    }
    EXPECT_FALSE(engine.IsCellExplored(exit.CenterX(), exit.CenterY()));
    EXPECT_FALSE(engine.IsCellExplored(0, 0));        // solid
    EXPECT_FALSE(engine.IsCellExplored(-1, 3));       // outside
}

TEST(GameEngineMinimapTests, Walking_RevealsTheWay) {
    GameEngine engine;
    InitializeDungeon(engine);
    const auto room = FindRoom(engine, LB::Dungeon::RoomType::Combat);
    const auto& area = engine.GetWorld().Layout().rooms[static_cast<std::size_t>(room)].area;
    ASSERT_FALSE(engine.IsCellExplored(area.CenterX(), area.CenterY()));

    WalkTo(engine, engine.GetWorld().RoomCenter(room), 60 * 60);

    EXPECT_TRUE(engine.IsCellExplored(area.CenterX(), area.CenterY()));
    EXPECT_TRUE(engine.IsCellExplored(area.x, area.y));   // the whole room on entering
}

TEST(GameEngineMinimapTests, KeyM_TogglesTheLargeMap) {
    GameEngine engine;
    InitializeDungeon(engine);
    engine.Tick(OneSixtieth);
    const auto small = engine.GetRendering().GetQueuedRectangles().size();

    TapKey(engine, Keys::FromChar('M'));
    engine.Tick(OneSixtieth);
    EXPECT_TRUE(engine.IsMapExpanded());

    TapKey(engine, Keys::FromChar('M'));
    engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.IsMapExpanded());
    EXPECT_GT(small, 0u);
}

TEST(GameEngineMinimapTests, Arena_NoMinimap) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.Tick(OneSixtieth);

    for (const auto& rect : engine.GetRendering().GetQueuedRectangles()) EXPECT_LT(rect.layerDepth, 14);
}
