// ObjectivesTests.cpp - LB.Engine tests: each floor's objective (a named champion, a relic, a
// captive, every room cleared), its reward, and the merchant's bounties
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    EngineOptions Options(std::uint32_t seed) {
        return EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false, .randomSeed = seed, .dungeon = true };
    }

    // A run seed whose second floor asks for `kind` (0: none in the first sixty)
    std::uint32_t SeedFor(ObjectiveKind kind) {
        for (std::uint32_t seed = 1; seed <= 60; ++seed) {
            GameEngine engine;
            if (!engine.Initialize(nullptr, 800, 600, Options(seed))) return 0;
            engine.EnterFloor(2);
            if (engine.GetObjective().kind == kind) return seed;
        }
        return 0;
    }

    void OpenFloor(GameEngine& engine, std::uint32_t seed) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(seed)));
        engine.EnterFloor(2);
        engine.Tick(OneSixtieth);
    }

    // Every enemy of a room slain outright (once they have risen)
    void ClearRoom(GameEngine& engine, std::int32_t room) {
        RunSteps(engine, 90);
        for (const auto id : engine.GetRoomStates()[static_cast<std::size_t>(room)].enemies) engine.DefeatEnemy(id);
        RunSteps(engine, 3);
    }
}

TEST(ObjectiveTests, EachFloor_ItsOwnObjective_ButTheRestingPlaces) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(CombatSeed)));
    EXPECT_NE(engine.GetObjective().kind, ObjectiveKind::None);
    EXPECT_FALSE(engine.GetObjective().Title().empty());
    const auto kind = engine.GetObjective().kind;
    const auto title = engine.GetObjective().Title();

    engine.EnterFloor(4);
    EXPECT_EQ(engine.GetObjective().kind, ObjectiveKind::None);

    GameEngine again;   // the same floor, the same objective
    ASSERT_TRUE(again.Initialize(nullptr, 800, 600, Options(CombatSeed)));
    EXPECT_EQ(again.GetObjective().kind, kind);
    EXPECT_EQ(again.GetObjective().Title(), title);
}

TEST(ObjectiveTests, AChampion_LeadsItsRoom_AndPaysWhenSlain) {
    const auto seed = SeedFor(ObjectiveKind::SlayChampion);
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    OpenFloor(engine, seed);
    const auto& objective = engine.GetObjective();
    EXPECT_TRUE(objective.Title().starts_with("Slay "));
    engine.EnterFloorPosition(engine.GetWorld().RoomCenter(objective.room));
    engine.Tick(OneSixtieth);

    ASSERT_TRUE(objective.champion.has_value());
    const auto* champion = engine.GetEncounter().FindEnemy(*objective.champion);
    ASSERT_NE(champion, nullptr);
    EXPECT_TRUE(champion->champion);
    EXPECT_EQ(champion->title, objective.name);
    EXPECT_GT(champion->maxHitPoints, champion->definition->hitPoints * 2);
    const auto gold = engine.GetGold();
    RunSteps(engine, 90);   // risen

    ASSERT_TRUE(engine.DefeatEnemy(*objective.champion));
    RunSteps(engine, 2);

    EXPECT_TRUE(engine.GetObjective().done);
    EXPECT_GE(engine.GetGold(), gold + engine.GetContent()->GetObjectives().rewardGoldPerDepth * 2);
    ASSERT_NE(engine.GetFanfare().Current(), nullptr);
    EXPECT_EQ(engine.GetFanfare().Current()->title, "OBJECTIVE COMPLETE");
}

TEST(ObjectiveTests, ARelic_IsFoundWhereItLies) {
    const auto seed = SeedFor(ObjectiveKind::Retrieve);
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    OpenFloor(engine, seed);
    EXPECT_TRUE(engine.GetObjective().Title().starts_with("Find the "));
    EXPECT_FALSE(engine.GetObjective().model.empty());

    engine.EnterFloorPosition(engine.GetObjective().at);
    RunSteps(engine, 2);

    EXPECT_TRUE(engine.GetObjective().done);
}

TEST(ObjectiveTests, ACaptive_IsFreedOnceTheirRoomIsClear) {
    const auto seed = SeedFor(ObjectiveKind::Rescue);
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    OpenFloor(engine, seed);
    const auto room = engine.GetObjective().room;
    engine.EnterFloorPosition(engine.GetObjective().at);
    RunSteps(engine, 2);
    EXPECT_FALSE(engine.GetObjective().done);   // its guards first

    ClearRoom(engine, room);
    RunSteps(engine, 2);

    EXPECT_TRUE(engine.GetRoomStates()[static_cast<std::size_t>(room)].cleared);
    EXPECT_TRUE(engine.GetObjective().done);
}

TEST(ObjectiveTests, ClearingTheFloor_CountsTheRooms) {
    const auto seed = SeedFor(ObjectiveKind::ClearFloor);
    ASSERT_NE(seed, 0u);
    GameEngine engine;
    OpenFloor(engine, seed);
    const auto rooms = engine.GetObjective().rooms;
    ASSERT_GT(rooms, 1);
    EXPECT_EQ(engine.GetObjective().Title(), std::format("Clear the floor (0/{})", rooms));

    const auto& layout = engine.GetWorld().Layout();
    std::int32_t cleared = 0;
    for (std::size_t i = 0; i < layout.rooms.size() && cleared == 0; ++i) {
        if (layout.rooms[i].type != LB::Dungeon::RoomType::Combat) continue;
        engine.EnterFloorPosition(engine.GetWorld().RoomCenter(static_cast<std::int32_t>(i)));
        engine.Tick(OneSixtieth);
        ClearRoom(engine, static_cast<std::int32_t>(i));
        cleared = engine.GetObjective().cleared;
    }
    EXPECT_EQ(cleared, 1);
    EXPECT_FALSE(engine.GetObjective().done);
}

TEST(BountyTests, TheMerchant_PostsOne_AndPaysForIt) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(CombatSeed)));
    EXPECT_FALSE(engine.GetBounty().Active());
    engine.EnterFloor(4);   // the first resting place
    engine.Tick(OneSixtieth);
    const auto bounty = engine.GetBounty();
    ASSERT_TRUE(bounty.Active());
    const auto& roster = GameEngine::RosterForDepth(5);
    EXPECT_TRUE(bounty.enemyKey == roster.light || bounty.enemyKey == roster.heavy || bounty.enemyKey == roster.caster) << bounty.enemyKey;
    EXPECT_TRUE(engine.GetBounty().Title().starts_with("Bounty: "));

    engine.EnterFloor(5);
    engine.Tick(OneSixtieth);
    const auto gold = engine.GetGold();
    for (std::int32_t i = 0; i < bounty.needed; ++i) {
        const auto id = engine.SpawnEnemy(bounty.enemyKey, { engine.GetPlayerPosition().x + 3.0f, engine.GetPlayerPosition().y });
        ASSERT_TRUE(id.has_value());
        RunSteps(engine, 2);
        ASSERT_TRUE(engine.DefeatEnemy(*id));
        RunSteps(engine, 2);
    }

    EXPECT_FALSE(engine.GetBounty().Active());
    EXPECT_GE(engine.GetGold(), gold + bounty.gold);
}

TEST(BountyTests, TheObjectiveAndTheBounty_KeptInSaves) {
    TempSaveDir dir;
    auto options = Options(CombatSeed);
    options.saveDirectory = dir.Utf8();
    std::string key;
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
        engine.EnterFloor(4);
        engine.Tick(OneSixtieth);
        key = engine.GetBounty().enemyKey;
        engine.EnterFloor(5);
        engine.Tick(OneSixtieth);
        const auto id = *engine.SpawnEnemy(key, { engine.GetPlayerPosition().x + 3.0f, engine.GetPlayerPosition().y });
        RunSteps(engine, 2);
        engine.DefeatEnemy(id);
        RunSteps(engine, 2);
        ASSERT_EQ(engine.GetBounty().killed, 1);
        ASSERT_TRUE(engine.SaveGame(1));
    }
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
    ASSERT_TRUE(engine.LoadGame(1));
    EXPECT_EQ(engine.GetBounty().enemyKey, key);
    EXPECT_EQ(engine.GetBounty().killed, 1);
    EXPECT_EQ(engine.GetObjective().kind != ObjectiveKind::None, true);
}
