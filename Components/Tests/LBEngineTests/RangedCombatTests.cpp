// RangedCombatTests.cpp - LB.Engine tests: arrows and bolts in flight, the skeleton crossbowmen,
// and the player's bows and crossbows
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// Projectile Tests
// ============================================================================

TEST(ProjectileTests, Flies_StopsInAWall_ThenIsGone) {
    const auto world = WorldMap::Arena(GameEngine::ArenaHalfWidth, GameEngine::ArenaHalfHeight);
    ProjectileSet set;
    set.Launch(TestShot({ 0.0f, 0.0f }, { 1.0f, 0.0f }, 10.0f, 30.0f));

    std::vector<ProjectileImpact> impacts;
    for (int i = 0; i < 120 && impacts.empty(); ++i) impacts = set.Update(Step, &world, {});

    ASSERT_EQ(impacts.size(), 1u);
    EXPECT_FALSE(impacts[0].target.has_value());
    EXPECT_NEAR(impacts[0].at.x, GameEngine::ArenaHalfWidth, ProjectileSet::SweepTiles + 1e-3f);   // at the wall's face
    ASSERT_EQ(set.Get().size(), 1u);
    EXPECT_FALSE(set.Get()[0].InFlight());   // stuck, for a moment
    EXPECT_FLOAT_EQ(ProjectileSet::Opacity(set.Get()[0]), 1.0f);

    for (int i = 0; i < static_cast<int>(ProjectileSet::StuckSeconds / Step) + 2; ++i) set.Update(Step, &world, {});
    EXPECT_TRUE(set.Get().empty());
}

TEST(ProjectileTests, HitsOnlyTheOtherSide_EvenWhenFast) {
    ProjectileSet set;
    set.Launch(TestShot({ 0.0f, 0.0f }, { 1.0f, 0.0f }, 120.0f, 20.0f, ProjectileSide::Player));   // two tiles a step
    const ProjectileTarget targets[] = {
        { 0, ProjectileSide::Player, { 3.0f, 0.0f }, 0.3f },    // the shooter's side: passed through
        { 9, ProjectileSide::Enemies, { 5.0f, 0.0f }, 0.3f },
    };

    std::vector<ProjectileImpact> impacts;
    for (int i = 0; i < 10 && impacts.empty(); ++i) impacts = set.Update(Step, nullptr, targets);

    ASSERT_EQ(impacts.size(), 1u);
    EXPECT_EQ(impacts[0].target, std::optional<std::uint32_t>{ 9u });
    EXPECT_NEAR(impacts[0].at.x, 5.0f - 0.3f - ProjectileSet::Radius, ProjectileSet::SweepTiles + 1e-3f);
    EXPECT_TRUE(set.Get().empty());   // gone into the body
}

TEST(ProjectileTests, Spent_LeavesWithoutImpact) {
    ProjectileSet set;
    set.Launch(TestShot({ 0.0f, 0.0f }, { 0.0f, 3.0f }, 10.0f, 2.0f));   // the direction is normalized

    std::vector<ProjectileImpact> impacts;
    for (int i = 0; i < 60; ++i) {
        for (const auto& impact : set.Update(Step, nullptr, {})) impacts.push_back(impact);
    }

    EXPECT_TRUE(impacts.empty());
    EXPECT_TRUE(set.Get().empty());
}

// ============================================================================
// Crossbowman Tests (Encounter)
// ============================================================================

TEST(CrossbowmanTests, ShootsAPlayerInSight_TheBoltArrives) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.crossbowman, { 5.0f, 0.0f }, FacingWest, false);
    const auto player = PlayerAt(0.0f, 0.0f);

    bool boltInFlight = false;
    std::vector<CombatEvent> events;
    for (int i = 0; i < 60 * 4 && events.empty(); ++i) {
        events = encounter.Update(Step, player);
        boltInFlight = boltInFlight || !encounter.GetProjectiles().empty();
    }

    ASSERT_EQ(events.size(), 1u);
    EXPECT_TRUE(boltInFlight);   // it flew before it arrived
    EXPECT_EQ(events[0].kind, CombatEventKind::EnemyAttack);
    EXPECT_EQ(events[0].enemyId, id);
    EXPECT_NEAR(encounter.FindEnemy(id)->position.x, 5.0f, 1e-3f);   // in range already: it did not close in
}

TEST(CrossbowmanTests, SteppingAside_DodgesTheBolt) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.crossbowman, { 6.0f, 0.0f }, FacingWest, false);
    auto player = PlayerAt(0.0f, 0.0f);
    for (int i = 0; i < 60 * 4 && encounter.GetShots().empty(); ++i) encounter.Update(Step, player);
    ASSERT_EQ(encounter.GetShots().size(), 1u);

    player.position = { 0.0f, 2.0f };   // out of the bolt's way (the next shot is seconds off)
    const auto events = RunEncounter(encounter, player, 1.5f);

    EXPECT_TRUE(events.empty());
}

TEST(CrossbowmanTests, WallBetween_NoShot) {
    EncounterContent content;
    const auto world = WalledArena();
    Encounter encounter(CombatSeed);
    encounter.Configure(content.minion, content.warrior, world);
    encounter.Spawn(*content.crossbowman, { 5.0f, 3.0f }, FacingWest, false);
    const auto player = PlayerAt(-5.0f, 3.0f);
    ASSERT_FALSE(world.LineOfSight({ 5.0f, 3.0f }, player.position));

    for (int i = 0; i < 60 * 2; ++i) {
        encounter.Update(Step, player);
        ASSERT_TRUE(encounter.GetShots().empty()) << "shot through the wall at step " << i;
    }
}

TEST(CrossbowmanTests, TooClose_BacksAwayWhileReloading) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.crossbowman, { 2.0f, 0.0f }, FacingWest, false);
    const auto player = PlayerAt(0.0f, 0.0f);
    for (int i = 0; i < 60 * 4 && encounter.GetShots().empty(); ++i) encounter.Update(Step, player);
    ASSERT_EQ(encounter.GetShots().size(), 1u);   // it shoots first, even this close
    const float before = Encounter::Distance(encounter.FindEnemy(id)->position, player.position);

    RunEncounter(encounter, player, 1.5f);

    EXPECT_GT(Encounter::Distance(encounter.FindEnemy(id)->position, player.position), before + 0.5f);
    EXPECT_LE(Encounter::Distance(encounter.FindEnemy(id)->position, player.position), content.crossbowman->keepAwayRange + 0.1f);
}

TEST(CrossbowmanTests, PlayersArrow_HitsTheFirstEnemyInItsWay) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto near = encounter.Spawn(*content.minion, { 3.0f, 0.0f }, FacingWest, false);
    const auto far = encounter.Spawn(*content.minion, { 6.0f, 0.0f }, FacingWest, false);
    encounter.Shoot(FixedHit(100), { 0.0f, 0.0f }, { 1.0f, 0.0f }, 20.0f, 10.0f, "arrow_bow.gltf", 0.375f);

    std::vector<CombatEvent> events;
    for (int i = 0; i < 30 && events.empty(); ++i) {
        for (const auto& event : encounter.Update(Step, PlayerAt(0.0f, 0.0f, false))) {
            if (event.kind == CombatEventKind::PlayerAttack) events.push_back(event);
        }
    }

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].enemyId, near);
    EXPECT_TRUE(events[0].killed);
    EXPECT_FALSE(encounter.FindEnemy(far)->IsDead());
}

TEST(CrossbowmanTests, Waves_ShootersFromTheSecondWave) {
    EXPECT_EQ(Encounter::WaveShooters(1), 0);
    EXPECT_EQ(Encounter::WaveShooters(2), 1);
    EXPECT_EQ(Encounter::WaveShooters(4), 1);
    EXPECT_EQ(Encounter::WaveShooters(5), 2);
    EXPECT_EQ(Encounter::WaveShooters(40), 2);
}

// ============================================================================
// GameEngine Ranged Combat Tests
// ============================================================================

TEST(GameEngineRangedTests, StartingKit_HasABow) {
    GameEngine engine;
    InitializeForCombat(engine);

    EXPECT_EQ(engine.CountItem(GameEngine::StartingBow), 1);
    EXPECT_FALSE(engine.GetPlayerWeapon()->IsRanged());   // the sword in hand
}

TEST(GameEngineRangedTests, Bow_ShootsTheNearestEnemyUntilItFalls) {
    GameEngine engine;
    InitializeForCombat(engine);
    ASSERT_TRUE(engine.EquipItem(GameEngine::StartingBow));
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 5.0f, 0.0f });

    engine.OnKeyDown(Keys::Space);
    bool arrowSeen = false;
    bool killed = false;
    for (int i = 0; i < 60 * 30 && !killed; ++i) {
        engine.Tick(OneSixtieth);
        arrowSeen = arrowSeen || !engine.GetEncounter().GetProjectiles().empty();
        const auto* enemy = engine.GetEncounter().FindEnemy(id);
        killed = !enemy || enemy->IsDead();
    }
    engine.OnKeyUp(Keys::Space);

    EXPECT_TRUE(arrowSeen);
    EXPECT_TRUE(killed);
    EXPECT_GT(PlayerExperience(engine), 0);   // the kill counted, though the arrow landed in the enemies' step
}

TEST(GameEngineRangedTests, Bow_WithoutATarget_ShootsAheadIntoTheWall) {
    GameEngine engine;
    InitializeForCombat(engine);
    ASSERT_TRUE(engine.EquipItem(GameEngine::StartingBow));
    const float yaw = engine.GetPlayerFacingYaw();

    TapKey(engine, Keys::Space);
    for (int i = 0; i < 120 && engine.GetEncounter().GetProjectiles().empty(); ++i) engine.Tick(OneSixtieth);

    ASSERT_EQ(engine.GetEncounter().GetProjectiles().size(), 1u);
    const auto arrow = engine.GetEncounter().GetProjectiles()[0];
    EXPECT_EQ(arrow.side, ProjectileSide::Player);
    EXPECT_NEAR(arrow.direction.x, std::sin(yaw), 1e-4f);
    EXPECT_NEAR(arrow.direction.y, std::cos(yaw), 1e-4f);

    for (int i = 0; i < 120 && engine.GetEncounter().GetProjectiles()[0].InFlight(); ++i) engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.GetEncounter().GetProjectiles()[0].InFlight());   // in the arena's wall (range 8, wall 7 away)
}

TEST(GameEngineRangedTests, Bow_RootsThePlayerUntilTheArrowLeaves) {
    GameEngine engine;
    InitializeForCombat(engine);
    ASSERT_TRUE(engine.EquipItem(GameEngine::StartingBow));
    const auto start = engine.GetPlayerPosition();

    engine.OnKeyDown(Keys::FromChar('D'));
    TapKey(engine, Keys::Space);
    const int drawSteps = static_cast<int>(engine.GetPlayerWeapon()->attackTimeSeconds * GameEngine::ShotReleaseFraction * 60.0f) - 2;
    RunSteps(engine, drawSteps);
    EXPECT_FLOAT_EQ(Encounter::Distance(engine.GetPlayerPosition(), start), 0.0f);   // drawing
    RunSteps(engine, 10);
    engine.OnKeyUp(Keys::FromChar('D'));

    EXPECT_GT(Encounter::Distance(engine.GetPlayerPosition(), start), 0.1f);   // loosed: free again
}

TEST(GameEngineRangedTests, Models_TheBowInHandAndTheArrowInFlight) {
    GameEngine engine;
    InitializeWithModels(engine);
    ASSERT_TRUE(engine.EquipItem(GameEngine::StartingBow));
    engine.Tick(OneSixtieth);
    ASSERT_EQ(DrawsOf(engine, "bow_withString").size(), 1u);
    EXPECT_TRUE(DrawsOf(engine, "bow_withString")[0].transform.has_value());   // in the left hand slot
    EXPECT_TRUE(DrawsOf(engine, "sword_1handed").empty());

    TapKey(engine, Keys::Space);
    for (int i = 0; i < 120 && engine.GetEncounter().GetProjectiles().empty(); ++i) engine.Tick(OneSixtieth);
    engine.Tick(OneSixtieth);

    const auto arrows = DrawsOf(engine, "arrow_bow");
    ASSERT_EQ(arrows.size(), 1u);
    EXPECT_FLOAT_EQ(arrows[0].position.y, ProjectileSet::FlightHeight);
    EXPECT_NEAR(arrows[0].yawRadians, engine.GetEncounter().GetProjectiles()[0].Yaw(), 1e-5f);
}
