// DecorTests.cpp - LB.Engine tests: the tiers' looks, props that bodies walk around, and spike traps
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    void InitializeAt(GameEngine& engine, std::int32_t depth) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
            .randomSeed = CombatSeed, .dungeon = true })) << engine.GetInitializationError();
        engine.EnterFloor(depth);
        engine.Tick(OneSixtieth);
    }
}

TEST(DecorTests, Themes_ChangeWithEachTier) {
    EXPECT_EQ(ThemeForDepth(1).name, "Crypts");
    EXPECT_EQ(ThemeForDepth(3).name, "Crypts");
    EXPECT_EQ(ThemeForDepth(4).name, "Caves");
    EXPECT_EQ(ThemeForDepth(7).name, "Ruins");
    EXPECT_EQ(ThemeForDepth(10).name, "Archive");
    EXPECT_EQ(ThemeForDepth(13).name, "Archive");   // the last tier's resting place and the final floor
    EXPECT_EQ(ThemeForDepth(GameEngine::FinalDepth).name, "Archive");
}

TEST(DecorTests, Props_StandInRoomsAndBlockBodies) {
    GameEngine engine;
    InitializeAt(engine, 2);
    const auto& props = engine.GetDecor().Props();
    ASSERT_FALSE(props.empty());
    EXPECT_EQ(engine.GetWorld().ObstacleCount(), props.size());

    for (const auto& prop : props) {
        EXPECT_GE(engine.GetWorld().RoomAt(prop.position), 0);                      // in a room
        EXPECT_FALSE(engine.GetWorld().CircleFits(prop.position, 0.1f));            // solid
    }
    // A body walking into one stops at its edge
    const auto& prop = props.front();
    const WorldPoint start{ prop.position.x - 2.0f, prop.position.y };
    if (engine.GetWorld().CircleFits(start, 0.3f)) {
        const auto end = engine.GetWorld().Move(start, { 2.0f, 0.0f }, 0.3f);
        EXPECT_GE(Encounter::Distance(end, prop.position), prop.radius + 0.3f - 1e-3f);
    }
}

TEST(DecorTests, SameFloor_SameDecor) {
    GameEngine first;
    InitializeAt(first, 3);
    GameEngine second;
    InitializeAt(second, 3);

    ASSERT_EQ(first.GetDecor().Props().size(), second.GetDecor().Props().size());
    for (std::size_t i = 0; i < first.GetDecor().Props().size(); ++i) {
        EXPECT_EQ(first.GetDecor().Props()[i].model, second.GetDecor().Props()[i].model);
        EXPECT_FLOAT_EQ(first.GetDecor().Props()[i].position.x, second.GetDecor().Props()[i].position.x);
    }
}

TEST(DecorTests, Traps_InCorridorsFromTheSecondFloor_NoneOnRestFloors) {
    GameEngine engine;
    InitializeAt(engine, 1);
    EXPECT_TRUE(engine.GetDecor().Traps().empty());

    engine.EnterFloor(3);
    ASSERT_FALSE(engine.GetDecor().Traps().empty());
    for (const auto& trap : engine.GetDecor().Traps()) EXPECT_LT(engine.GetWorld().RoomAt(trap.center), 0);   // a corridor

    engine.EnterFloor(4);   // a resting place
    EXPECT_TRUE(engine.GetDecor().Traps().empty());
}

TEST(DecorTests, Spikes_StrikeOnceACycle) {
    SpikeTrap trap{ 0, 0, { 1.0f, 1.0f }, 0.0f };
    FloorDecor decor;
    const float upAt = FloorDecor::TrapPeriodSeconds - FloorDecor::TrapUpSeconds;
    EXPECT_FLOAT_EQ(FloorDecor::Raise(trap, 0.0), 0.0f);
    EXPECT_GT(FloorDecor::Raise(trap, upAt - 0.1), 0.0f);    // stirring: a warning
    EXPECT_LT(FloorDecor::Raise(trap, upAt - 0.1), 1.0f);
    EXPECT_FLOAT_EQ(FloorDecor::Raise(trap, upAt + 0.1), 1.0f);
}

TEST(DecorTests, Spikes_HurtThePlayerStandingOnThem) {
    GameEngine engine;
    InitializeAt(engine, 3);
    ASSERT_FALSE(engine.GetDecor().Traps().empty());
    const auto trap = engine.GetDecor().Traps().front();
    const auto before = PlayerHitPoints(engine);

    // Stand on the trap for a full cycle (no enemies are awake in a corridor)
    bool hurt = false;
    for (int i = 0; i < static_cast<int>(FloorDecor::TrapPeriodSeconds * 60.0f) + 10 && !hurt; ++i) {
        engine.EnterFloorPosition(trap.center);
        engine.Tick(OneSixtieth);
        hurt = PlayerHitPoints(engine) < before;
    }
    EXPECT_TRUE(hurt);
}
