// PerformanceTests.cpp - LB.Engine tests: a frame-time budget for one step (simulation and the
// render queues, headless) on a crowded deep floor. Built only in optimized configurations
// (Release), like the balance bot; each prints a [ PERF ] line.
#include "pch.h"
#include "EngineTestHelpers.h"

#ifdef NDEBUG

namespace {
    // A step's share of a 60 Hz frame: the rest is the GPU's and the shell's. Generous, for
    // shared build machines; a step here takes a fraction of it.
    constexpr double StepBudgetMilliseconds = 4.0;
    constexpr double SlowestStepMilliseconds = 25.0;
}

TEST(PerformanceTests, ACrowdedFloor_StepsWithinItsBudget) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(),
        .randomSeed = CombatSeed, .dungeon = true }));
    engine.GrantExperience(30000);   // a hero who lasts the run
    engine.EnterFloor(8);           // the ruins: the largest rooms and rosters
    engine.Tick(OneSixtieth);
    engine.HireAlly("ally_barbarian");
    engine.HireAlly("ally_acolyte");

    // The biggest combat room, woken, and more enemies besides
    const auto& layout = engine.GetWorld().Layout();
    std::int32_t room = -1;
    for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
        if (layout.rooms[i].type == LB::Dungeon::RoomType::Combat) room = static_cast<std::int32_t>(i);
    }
    ASSERT_GE(room, 0);
    const auto center = engine.GetWorld().RoomCenter(room);
    engine.EnterFloorPosition(center);
    engine.Tick(OneSixtieth);
    for (int i = 0; i < 12; ++i) engine.SpawnEnemy(GameEngine::RosterForDepth(8).light, { center.x + static_cast<float>(i % 4) - 1.5f, center.y + 2.5f });

    using Clock = std::chrono::steady_clock;
    constexpr int Steps = 60 * 10;
    double total = 0.0;
    double slowest = 0.0;
    for (int i = 0; i < Steps; ++i) {
        const auto start = Clock::now();
        engine.Tick(OneSixtieth);
        const double milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        total += milliseconds;
        slowest = std::max(slowest, milliseconds);
    }
    const double average = total / Steps;
    std::printf("[ PERF     ] %.3f ms a step on average, %.3f ms the slowest (%zu enemies, %zu models queued)\n", average, slowest,
                engine.GetEncounter().GetEnemies().size(), engine.GetRendering().GetQueuedModels().size());
    EXPECT_LT(average, StepBudgetMilliseconds);
    EXPECT_LT(slowest, SlowestStepMilliseconds);
}

#endif // NDEBUG
