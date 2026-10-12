// BestiaryTests.cpp - LB.Engine tests: each tier's inhabitants (the caves' and ruins' creatures),
// the werewolf's lunge, the vampire's life drain, and creatures that do not rise from the ground
#include "pch.h"
#include "EngineTestHelpers.h"

TEST(BestiaryTests, EachTier_ItsOwnInhabitants) {
    EXPECT_EQ(GameEngine::RosterForDepth(2).light, "enemy_skeleton_minion");
    EXPECT_EQ(GameEngine::RosterForDepth(5).light, "enemy_werewolf");
    EXPECT_EQ(GameEngine::RosterForDepth(5).brute, "enemy_orc_brute");
    EXPECT_EQ(GameEngine::RosterForDepth(8).caster, "enemy_tiefling");
    EXPECT_EQ(GameEngine::RosterForDepth(11).caster, "enemy_lorekeeper");      // the archive
    EXPECT_EQ(GameEngine::RosterForDepth(14).light, "enemy_skeleton_minion");   // and around again

    // Every name of every roster is in the shipped content
    GameEngine engine;
    InitializeForCombat(engine);
    for (const auto& roster : GameEngine::Rosters) {
        for (const auto key : { roster.light, roster.heavy, roster.ranged, roster.caster, roster.brute }) EXPECT_NE(engine.GetContent()->FindEnemy(key), nullptr) << key;
    }
    for (const auto key : GameEngine::BossEnemies) EXPECT_TRUE(engine.GetContent()->FindEnemy(key)->boss) << key;
}

TEST(BestiaryTests, ACavesRoom_RaisesTheCavesCreatures) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = CombatSeed, .dungeon = true }));
    engine.EnterFloor(5);
    engine.Tick(OneSixtieth);
    const auto room = FindRoom(engine, LB::Dungeon::RoomType::Combat);
    ASSERT_GE(room, 0);
    engine.EnterFloorPosition(engine.GetWorld().RoomCenter(room));
    engine.Tick(OneSixtieth);

    ASSERT_FALSE(engine.GetEncounter().GetEnemies().empty());
    EXPECT_TRUE(std::ranges::any_of(engine.GetEncounter().GetEnemies(), [](const Enemy& enemy) { return enemy.definition->key == "enemy_werewolf"; }));
    EXPECT_FALSE(std::ranges::any_of(engine.GetEncounter().GetEnemies(), [](const Enemy& enemy) { return enemy.definition->key == "enemy_skeleton_minion"; }));
}

TEST(BestiaryTests, AWerewolf_LungesFromAfar) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy("enemy_werewolf", { 4.0f, 0.0f });
    const auto start = Encounter::Distance(engine.GetEncounter().FindEnemy(id)->position, engine.GetPlayerPosition());

    RunSteps(engine, 24);   // 0.4 seconds

    const auto* wolf = engine.GetEncounter().FindEnemy(id);
    const auto covered = start - Encounter::Distance(wolf->position, engine.GetPlayerPosition());
    EXPECT_GT(covered, 2.0f);   // walking, it would have covered about 1.2
    EXPECT_GT(wolf->lungeCooldown, 0.0f);
}

TEST(BestiaryTests, AVampiresBlows_HealIt) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy("enemy_vampire", { 1.0f, 0.0f });
    // Wound it first
    engine.OnKeyDown(Keys::Space);
    StepUntil(engine, [&] { const auto* v = engine.GetEncounter().FindEnemy(id); return v->hitPoints < v->maxHitPoints; }, 10.0f);
    engine.OnKeyUp(Keys::Space);
    RunSteps(engine, 30);
    const auto* vampire = engine.GetEncounter().FindEnemy(id);
    ASSERT_NE(vampire, nullptr);
    ASSERT_FALSE(vampire->IsDead());
    const auto wounded = vampire->hitPoints;
    ASSERT_LT(wounded, vampire->maxHitPoints);

    // Its next blow that lands on the hero heals it
    const auto hp = PlayerHitPoints(engine);
    for (int i = 0; i < 60 * 15 && PlayerHitPoints(engine) == hp; ++i) engine.Tick(OneSixtieth);
    ASSERT_LT(PlayerHitPoints(engine), hp);
    EXPECT_GT(engine.GetEncounter().FindEnemy(id)->hitPoints, wounded);
}

TEST(BestiaryTests, TheLiving_DoNotRise) {
    GameEngine engine;
    InitializeForCombat(engine);
    EXPECT_FALSE(engine.GetContent()->FindEnemy("enemy_orc_raider")->rises);
    EXPECT_TRUE(engine.GetContent()->FindEnemy("enemy_skeleton_minion")->rises);
}
