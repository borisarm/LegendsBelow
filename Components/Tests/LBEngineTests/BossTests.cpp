// BossTests.cpp - LB.Engine tests: the golem (a brute on the large rig), groups that notice the
// player together, and the Necromancer (a boss that raises the dead and has a second phase)
#include "pch.h"
#include "EngineTestHelpers.h"

TEST(BossTests, Golem_DoesNotReelFromHits) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto* golem = content.engine.GetContent()->FindEnemy(GameEngine::BruteEnemy);
    const auto id = encounter.Spawn(*golem, { 3.0f, 0.0f }, FacingWest, false);

    StrikeUntilHit(encounter, id, 5);

    EXPECT_NE(encounter.FindEnemy(id)->state, EnemyState::Staggered);
}

TEST(BossTests, Group_NoticesThePlayerTogether) {
    EncounterContent content;
    const auto world = WalledArena();
    Encounter encounter(CombatSeed);
    encounter.Configure(content.minion, content.warrior, world);
    const auto seen = encounter.Spawn(*content.minion, { -3.0f, 3.0f }, FacingWest, false, 1, 7);
    const auto hidden = encounter.Spawn(*content.minion, { 5.0f, 3.0f }, FacingWest, false, 1, 7);      // behind the wall
    const auto loner = encounter.Spawn(*content.minion, { 6.0f, 4.0f }, FacingWest, false, 1, 8);      // another group
    const auto player = PlayerAt(-5.0f, 3.0f);
    ASSERT_FALSE(world.LineOfSight({ 5.0f, 3.0f }, player.position));

    RunEncounter(encounter, player, 0.1f);

    EXPECT_NE(encounter.FindEnemy(seen)->state, EnemyState::Idle);
    EXPECT_NE(encounter.FindEnemy(hidden)->state, EnemyState::Idle);   // told by its group
    EXPECT_EQ(encounter.FindEnemy(loner)->state, EnemyState::Idle);
}

namespace {
    struct BossFight {
        EncounterContent content;
        Encounter encounter{ CombatSeed };
        const LB::Content::EnemyDefinition* necromancer{ content.engine.GetContent()->FindEnemy(GameEngine::BossEnemy) };
        std::uint32_t boss{0};

        BossFight() {
            encounter.Configure(content.minion, content.warrior, content.world);
            encounter.SetContent(content.engine.GetContent().get());
            boss = encounter.Spawn(*necromancer, { 5.0f, 0.0f }, FacingWest, false);
        }

        std::vector<const Enemy*> Summons() const {
            std::vector<const Enemy*> raised;
            for (const auto& enemy : encounter.GetEnemies()) {
                if (enemy.summoner == boss) raised.push_back(&enemy);
            }
            return raised;
        }
    };
}

TEST(BossTests, Necromancer_RaisesMinionsWhileFighting) {
    BossFight fight;

    RunEncounter(fight.encounter, PlayerAt(0.0f, 0.0f), fight.necromancer->summonCooldownSeconds);

    const auto raised = fight.Summons();
    ASSERT_GE(raised.size(), static_cast<std::size_t>(fight.necromancer->summonCount));
    EXPECT_LE(raised.size(), static_cast<std::size_t>(fight.necromancer->maxSummons));
    EXPECT_EQ(raised[0]->definition->key, fight.necromancer->summon);
}

TEST(BossTests, Necromancer_SecondPhaseBelowHalfHealth) {
    BossFight fight;
    const auto* boss = fight.encounter.FindEnemy(fight.boss);
    const auto damage = boss->hitPoints - static_cast<std::int32_t>(static_cast<float>(boss->maxHitPoints) * fight.necromancer->phaseTwoHealthFraction) + 1;

    StrikeUntilHit(fight.encounter, fight.boss, damage);
    fight.encounter.Update(Step, PlayerAt(0.0f, 0.0f));

    EXPECT_TRUE(fight.encounter.FindEnemy(fight.boss)->phaseTwo);
    ASSERT_EQ(fight.encounter.GetPhaseChanges().size(), 1u);
    EXPECT_FALSE(fight.Summons().empty());   // it raises at once
}

TEST(BossTests, Summons_CrumbleWithTheirMaster) {
    BossFight fight;
    RunEncounter(fight.encounter, PlayerAt(0.0f, 0.0f), fight.necromancer->summonCooldownSeconds);
    ASSERT_FALSE(fight.Summons().empty());

    StrikeUntilHit(fight.encounter, fight.boss, 1000);
    fight.encounter.Update(Step, PlayerAt(0.0f, 0.0f));

    for (const auto* summoned : fight.Summons()) EXPECT_TRUE(summoned->IsDead());
}

TEST(BossTests, Hud_ShowsTheBossAcrossTheTop) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::BossEnemy, { 5.0f, 0.0f });

    RunSteps(engine, 30);

    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text.starts_with("The Necromancer"); }));
}

TEST(BossTests, Golem_UsesTheLargeRigsClips) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.SpawnEnemy(GameEngine::BruteEnemy, { 6.0f, 0.0f });

    RunSteps(engine, 3);

    const auto golems = DrawsOf(engine, "Skeleton_Golem");
    ASSERT_EQ(golems.size(), 1u);
    const auto clip = golems[0].animation.current.clip;
    const auto& rendering = engine.GetRendering();
    EXPECT_TRUE(clip == *rendering.FindAnimation("Large/Idle_A") || clip == *rendering.FindAnimation("Large/Walking_A"));
}
