// TacticsTests.cpp - LB.Engine tests: group tactics (fighters behind their tank, shooters behind
// the front line), telegraphed boss attacks, and a boss for each tier
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    constexpr std::uint32_t Group = 5;

    const LB::Content::EnemyDefinition& Type(const GameEngine& engine, std::string_view key) {
        return *engine.GetContent()->FindEnemy(key);
    }

    float DistanceToOrigin(const Encounter& encounter, std::uint32_t id) {
        return Encounter::Distance(encounter.FindEnemy(id)->position, { 0.0f, 0.0f });
    }

    // Step until the enemy starts its telegraphed attack (at most a few seconds)
    bool AwaitWindup(GameEngine& engine, std::uint32_t id) {
        for (int i = 0; i < 60 * 4; ++i) {
            if (engine.GetEncounter().FindEnemy(id)->slamSeconds > 0.0f) return true;
            engine.Tick(OneSixtieth);
        }
        return false;
    }
}

TEST(TacticsTests, Fighters_WaitBehindTheirTank) {
    GameEngine engine;
    InitializeForCombat(engine);
    Encounter encounter(CombatSeed);
    const auto tank = encounter.Spawn(Type(engine, "enemy_skeleton_warrior"), { 7.0f, 0.0f }, 0.0f, false, 1, Group);
    const auto minion = encounter.Spawn(Type(engine, "enemy_skeleton_minion"), { 5.0f, 0.5f }, 0.0f, false, 1, Group);
    const auto alone = encounter.Spawn(Type(engine, "enemy_skeleton_minion"), { -5.0f, 0.0f }, 0.0f, false, 1);   // no group

    for (int i = 0; i < 60; ++i) encounter.Update(1.0f / 60.0f, PlayerAt(0.0f, 0.0f));

    EXPECT_LT(DistanceToOrigin(encounter, tank), 6.0f);       // the tank closes in
    EXPECT_GT(DistanceToOrigin(encounter, minion), 4.8f);     // its fighter waits for it
    EXPECT_LT(DistanceToOrigin(encounter, alone), 3.5f);      // one on its own does not

    // Once the tank is on the player, the fighter joins in
    for (int i = 0; i < 60 * 5; ++i) encounter.Update(1.0f / 60.0f, PlayerAt(0.0f, 0.0f));
    EXPECT_LT(DistanceToOrigin(encounter, minion), 2.0f);
}

TEST(TacticsTests, Shooters_KeepBehindTheFrontLine) {
    GameEngine engine;
    InitializeForCombat(engine);
    Encounter encounter(CombatSeed);
    const auto& shooterType = Type(engine, "enemy_skeleton_rogue");
    const auto front = encounter.Spawn(Type(engine, "enemy_skeleton_warrior"), { 8.0f, 0.0f }, 0.0f, false, 1, Group);
    const auto shooter = encounter.Spawn(shooterType, { 7.5f, 1.0f }, 0.0f, false, 1, Group);

    for (int i = 0; i < 60 * 3; ++i) {
        encounter.Update(1.0f / 60.0f, PlayerAt(0.0f, 0.0f));
        const float allowed = std::min(DistanceToOrigin(encounter, front) + EnemyBehavior::RankGap * 2.0f, shooterType.attackRange * 0.9f);
        ASSERT_GE(DistanceToOrigin(encounter, shooter), allowed - 0.1f) << "step " << i;
    }
}

TEST(TacticsTests, Bosses_OnePerTier) {
    EXPECT_EQ(GameEngine::BossForDepth(3), "enemy_necromancer");
    EXPECT_EQ(GameEngine::BossForDepth(6), "enemy_golem_warlord");
    EXPECT_EQ(GameEngine::BossForDepth(9), "enemy_frost_golem");
    EXPECT_EQ(GameEngine::BossForDepth(12), "enemy_abomination");
    EXPECT_EQ(GameEngine::BossForDepth(GameEngine::FinalDepth), "enemy_first_archivist");   // the final floor's
}

TEST(TacticsTests, Slam_ShowsItsRingThenStrikesThoseInside) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy("enemy_golem_warlord", { 1.8f, 0.0f });
    ASSERT_TRUE(AwaitWindup(engine, id));

    engine.Tick(OneSixtieth);
    EXPECT_GE(engine.GetRendering().GetQueuedParticles().size(), 28u);   // the ring

    bool struck = false;
    for (int i = 0; i < 60 * 2 && !struck; ++i) {
        const bool landing = engine.GetEncounter().FindEnemy(id)->slamSeconds > 0.0f;
        engine.Tick(OneSixtieth);
        if (landing && engine.GetEncounter().FindEnemy(id)->slamSeconds <= 0.0f) {
            struck = std::ranges::any_of(engine.GetCombatEvents(), [&](const CombatEvent& event) {
                return event.kind == CombatEventKind::EnemyAttack && event.enemyId == id;
            });
            break;
        }
    }
    EXPECT_TRUE(struck);
}

TEST(TacticsTests, Slam_MissesWhoeverStepsOut) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy("enemy_golem_warlord", { 1.8f, 0.0f });
    ASSERT_TRUE(AwaitWindup(engine, id));
    const auto before = PlayerHitPoints(engine);

    // Run out of the ring before it lands
    engine.OnKeyDown(Keys::FromChar('A'));
    for (int i = 0; i < 60 * 2 && engine.GetEncounter().FindEnemy(id)->slamSeconds > 0.0f; ++i) engine.Tick(OneSixtieth);
    engine.Tick(OneSixtieth);
    engine.OnKeyUp(Keys::FromChar('A'));

    EXPECT_EQ(PlayerHitPoints(engine), before);
}

// Opportunity attacks (design-combat-system.md 3.2): walking out of an awake melee enemy's reach
// draws one blow, at most once a second; stepping about within it, or away from one asleep, does not
TEST(TacticsTests, WalkingAway_DrawsAnOpportunityAttack) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.warrior, { 1.0f, 0.0f }, FacingWest, false);
    const auto attacks = [&](const std::vector<CombatEvent>& events) {
        return std::ranges::count_if(events, [&](const CombatEvent& event) { return event.kind == CombatEventKind::EnemyAttack && event.enemyId == id; });
    };
    // The hero ends each step out of its reach (it chases, not swings); the walks checked start inside it
    const auto player = PlayerAt(-1.5f, 0.0f);
    const WorldPoint inside{ 0.2f, 0.0f };
    encounter.CheckOpportunity(inside, player.position, player.defense);
    EXPECT_EQ(attacks(encounter.Update(1.0f / 60.0f, player)), 0);   // it had not noticed the hero
    ASSERT_EQ(encounter.FindEnemy(id)->state, EnemyState::Chasing);

    encounter.CheckOpportunity(inside, { 0.4f, 0.3f }, player.defense);   // still in reach: nothing
    encounter.CheckOpportunity(inside, player.position, player.defense);  // out of it: one blow
    EXPECT_EQ(attacks(encounter.Update(1.0f / 60.0f, player)), 1);

    encounter.CheckOpportunity(inside, player.position, player.defense);  // too soon again
    EXPECT_EQ(attacks(encounter.Update(1.0f / 60.0f, player)), 0);
}

// Flanking (design-combat-system.md 7.1): a second enemy on the far side of the target
TEST(TacticsTests, EnemiesOnBothSides_FlankTheirTarget) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto east = encounter.Spawn(*content.warrior, { 1.0f, 0.0f }, FacingWest, false);
    const auto west = encounter.Spawn(*content.warrior, { -1.0f, 0.1f }, FacingWest, false);
    const auto north = encounter.Spawn(*content.warrior, { 0.0f, -1.0f }, FacingWest, false);
    encounter.Update(1.0f / 60.0f, PlayerAt(0.0f, 0.0f));   // they notice the hero

    EXPECT_TRUE(encounter.IsFlanking(east, { 0.0f, 0.0f }));
    EXPECT_TRUE(encounter.IsFlanking(west, { 0.0f, 0.0f }));
    // North has nobody to the south; east and west stand beside it, not opposite
    EXPECT_FALSE(encounter.IsFlanking(north, { 0.0f, 0.0f }));
    EXPECT_FALSE(encounter.IsFlanking(east, { 5.0f, 5.0f }));   // nobody threatens that spot
}

// Focus fire: a shooter aims at the most wounded within its range; a fighter at the nearest
TEST(TacticsTests, Shooters_FocusTheMostWounded) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto shooter = encounter.Spawn(*content.crossbowman, { 4.0f, 0.0f }, FacingWest, false);
    const auto fighter = encounter.Spawn(*content.warrior, { 1.0f, 0.0f }, FacingWest, false);
    auto player = PlayerAt(0.0f, 0.0f);
    PlayerTarget ally = PlayerAt(0.0f, 1.0f);
    ally.ally = 7;
    ally.health = 0.3f;
    const std::array allies{ ally };

    EXPECT_EQ(encounter.TargetOf(shooter, player, allies), 7u);   // the wounded ally
    EXPECT_EQ(encounter.TargetOf(fighter, player, allies), 0u);   // the nearer hero
    player.health = 0.2f;
    EXPECT_EQ(encounter.TargetOf(shooter, player, allies), 0u);
}
