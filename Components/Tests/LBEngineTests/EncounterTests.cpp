// EncounterTests.cpp - LB.Engine tests: the animator and the enemy encounter (AI, waves, attacks)
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// Animator Tests
// ============================================================================

TEST(AnimatorTests, FirstClip_StartsWithoutFade) {
    Animator animator;

    animator.Play(5, true);

    EXPECT_EQ(animator.GetState().current.clip, 5u);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 1.0f);
    EXPECT_EQ(animator.GetState().previous.clip, LB::Systems::Rendering::InvalidAnimationId);
}

TEST(AnimatorTests, SwitchingClips_CrossFades) {
    Animator animator;
    animator.Play(5, true);
    animator.Update(0.5f);

    animator.Play(6, false);
    EXPECT_EQ(animator.GetState().previous.clip, 5u);
    EXPECT_FLOAT_EQ(animator.GetState().previous.timeSeconds, 0.5f);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 0.0f);
    EXPECT_FALSE(animator.GetState().current.loop);

    animator.Update(Animator::DefaultFadeSeconds * 0.5f);
    EXPECT_NEAR(animator.GetState().blend, 0.5f, 1e-5f);

    animator.Update(Animator::DefaultFadeSeconds);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 1.0f);
    EXPECT_EQ(animator.GetState().previous.clip, LB::Systems::Rendering::InvalidAnimationId);
}

TEST(AnimatorTests, SameClip_KeepsTimeUnlessRestarted) {
    Animator animator;
    animator.Play(5, true);
    animator.Update(0.4f);

    animator.Play(5, true, 2.0f);
    EXPECT_FLOAT_EQ(animator.CurrentTime(), 0.4f);
    animator.Update(0.1f);
    EXPECT_FLOAT_EQ(animator.CurrentTime(), 0.6f);   // twice as fast now

    animator.Play(5, true, 1.0f, true);
    EXPECT_FLOAT_EQ(animator.CurrentTime(), 0.0f);
}

TEST(AnimatorTests, NoClips_StaysInRestPose) {
    Animator animator;

    animator.Play(LB::Systems::Rendering::InvalidAnimationId, true);
    animator.Play(LB::Systems::Rendering::InvalidAnimationId, false, 1.0f, true);
    animator.Update(1.0f);

    EXPECT_EQ(animator.CurrentClip(), LB::Systems::Rendering::InvalidAnimationId);
    EXPECT_FLOAT_EQ(animator.GetState().blend, 1.0f);
}

// ============================================================================
// Encounter Tests
// ============================================================================

TEST(EncounterTests, WaveComposition_GrowsAndCaps) {
    EXPECT_EQ(Encounter::WaveComposition(1), std::make_pair(2, 0));
    EXPECT_EQ(Encounter::WaveComposition(2), std::make_pair(2, 1));
    EXPECT_EQ(Encounter::WaveComposition(3), std::make_pair(3, 1));
    EXPECT_EQ(Encounter::WaveComposition(4), std::make_pair(3, 2));
    EXPECT_EQ(Encounter::WaveComposition(40), std::make_pair(4, 2));
}

TEST(EncounterTests, StartWave_RisesAwayFromThePlayer) {
    EncounterContent content;
    ASSERT_NE(content.minion, nullptr);
    ASSERT_NE(content.warrior, nullptr);
    auto encounter = MakeEncounter(content);

    encounter.StartWave(2, { 1.0f, 1.0f });

    EXPECT_EQ(encounter.GetWave(), 2);
    ASSERT_EQ(encounter.GetEnemies().size(), 3u);
    EXPECT_EQ(std::ranges::count(encounter.GetEnemies(), content.warrior, &Enemy::definition), 1);
    for (const auto& enemy : encounter.GetEnemies()) {
        EXPECT_EQ(enemy.state, EnemyState::Spawning);
        EXPECT_GE(Encounter::Distance(enemy.position, { 1.0f, 1.0f }), Encounter::MinimumSpawnDistance);
        EXPECT_LE(std::abs(enemy.position.x), GameEngine::ArenaHalfWidth);
        EXPECT_LE(std::abs(enemy.position.y), GameEngine::ArenaHalfHeight);
        EXPECT_EQ(enemy.hitPoints, enemy.maxHitPoints);
        EXPECT_GT(enemy.maxHitPoints, enemy.definition->hitPoints);   // wave 2 is tougher
    }
}

TEST(EncounterTests, Spawning_UntargetableUntilRisen) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f);

    EXPECT_FALSE(encounter.FindTarget({ 0.0f, 0.0f }, 2.0f).has_value());
    EXPECT_FALSE(encounter.StrikeEnemy(id, FixedHit(3), { 0.0f, 0.0f }, 2.0f).has_value());

    RunEncounter(encounter, PlayerAt(0.0f, 0.0f), Encounter::SpawnSeconds + Step);
    EXPECT_EQ(encounter.FindTarget({ 0.0f, 0.0f }, 2.0f), std::optional<std::uint32_t>(id));
}

TEST(EncounterTests, Idle_UntilThePlayerComesWithinAggroRange) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 9.0f, 0.0f }, 0.0f, false);

    RunEncounter(encounter, PlayerAt(-9.0f, 0.0f), 2.0f);   // 18 tiles away
    EXPECT_EQ(encounter.GetEnemies()[0].state, EnemyState::Idle);
    EXPECT_FLOAT_EQ(encounter.GetEnemies()[0].position.x, 9.0f);

    RunEncounter(encounter, PlayerAt(4.0f, 0.0f), 0.5f);
    EXPECT_EQ(encounter.GetEnemies()[0].state, EnemyState::Chasing);
    EXPECT_LT(encounter.GetEnemies()[0].position.x, 9.0f);
}

TEST(EncounterTests, ClosesInAndAttacksThePlayer) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 4.0f, 0.0f }, 0.0f, false);

    const auto events = RunEncounter(encounter, PlayerAt(0.0f, 0.0f), 4.0f);

    EXPECT_LE(encounter.FindEnemy(id)->position.x, content.minion->attackRange + 1e-3f);
    EXPECT_GE(encounter.FindEnemy(id)->position.x, Encounter::CharacterRadius * 2.0f - 1e-3f);
    ASSERT_FALSE(events.empty());
    for (const auto& event : events) {
        EXPECT_EQ(event.kind, CombatEventKind::EnemyAttack);
        EXPECT_EQ(event.enemyId, id);
    }
}

TEST(EncounterTests, Attack_LandsPartWayThroughItsTime) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 1.0f, 0.0f }, -std::numbers::pi_v<float> * 0.5f, false);   // facing the player
    const auto player = PlayerAt(0.0f, 0.0f);

    int stepsToAttack = 0;
    while (encounter.GetEnemies()[0].state != EnemyState::Attacking && stepsToAttack < 600) {
        encounter.Update(Step, player);
        ++stepsToAttack;
    }
    ASSERT_EQ(encounter.GetEnemies()[0].state, EnemyState::Attacking);

    int steps = 0;
    while (encounter.Update(Step, player).empty() && steps < 600) ++steps;
    const float impact = content.minion->attackTimeSeconds * content.minion->attackImpactFraction;
    EXPECT_NEAR((steps + 1) * Step, impact, Step * 1.5f);
}

TEST(EncounterTests, PlayerSteppingAway_AttackWhiffs) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 1.0f, 0.0f }, -std::numbers::pi_v<float> * 0.5f, false);
    for (int i = 0; i < 600 && encounter.GetEnemies()[0].state != EnemyState::Attacking; ++i) {
        encounter.Update(Step, PlayerAt(0.0f, 0.0f));
    }
    ASSERT_EQ(encounter.GetEnemies()[0].state, EnemyState::Attacking);

    const auto events = RunEncounter(encounter, PlayerAt(-4.0f, 0.0f), content.minion->attackTimeSeconds);

    EXPECT_TRUE(events.empty());
}

TEST(EncounterTests, PlayerDown_EnemiesStandDown) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f, false);

    const auto events = RunEncounter(encounter, PlayerAt(0.0f, 0.0f, false), 3.0f);

    EXPECT_TRUE(events.empty());
    EXPECT_EQ(encounter.GetEnemies()[0].state, EnemyState::Idle);
}

TEST(EncounterTests, StrikeEnemy_DamagesStaggersAndKills) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f, false);
    const auto hp = content.minion->hitPoints;

    const auto first = StrikeUntilHit(encounter, id, 3);
    EXPECT_EQ(first.kind, CombatEventKind::PlayerAttack);
    EXPECT_EQ(first.result.finalDamage, 3);
    EXPECT_FALSE(first.killed);
    EXPECT_EQ(encounter.FindEnemy(id)->hitPoints, hp - 3);
    EXPECT_EQ(encounter.FindEnemy(id)->state, EnemyState::Staggered);
    EXPECT_GT(encounter.FindEnemy(id)->hitFlashSeconds, 0.0f);

    const auto last = StrikeUntilHit(encounter, id, hp);
    EXPECT_TRUE(last.killed);
    EXPECT_EQ(encounter.FindEnemy(id)->hitPoints, 0);
    EXPECT_TRUE(encounter.FindEnemy(id)->IsDead());
    EXPECT_EQ(encounter.LivingEnemyCount(), 0u);
    EXPECT_FALSE(encounter.StrikeEnemy(id, FixedHit(1), { 1.0f, 0.0f }, 1.0f).has_value());
}

TEST(EncounterTests, StrikeEnemy_DefensesApply) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.warrior, { 1.0f, 0.0f }, 0.0f, false);
    auto piercing = FixedHit(9);
    piercing.damageType = LB::Combat::DamageType::Piercing;

    std::optional<CombatEvent> hit;
    for (int i = 0; i < 20 && !(hit && hit->result.hit); ++i) hit = encounter.StrikeEnemy(id, piercing, { 0.0f, 0.0f }, 1.5f);

    ASSERT_TRUE(hit && hit->result.hit);
    // Skeleton warrior: physical armour first, then its piercing resistance
    const auto resisted = 1.0f - content.warrior->GetResistance(LB::Combat::DamageType::Piercing);
    ASSERT_LT(resisted, 1.0f);
    const auto expected = static_cast<std::int32_t>(std::lround(static_cast<float>(9 - content.warrior->physicalArmor) * resisted));
    EXPECT_EQ(hit->result.finalDamage, expected);
}

TEST(EncounterTests, StrikeEnemy_OutOfReach_Whiffs) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 5.0f, 0.0f }, 0.0f, false);

    EXPECT_FALSE(encounter.StrikeEnemy(id, FixedHit(3), { 0.0f, 0.0f }, 1.4f).has_value());
    EXPECT_FALSE(encounter.FindTarget({ 0.0f, 0.0f }, 1.4f).has_value());
}

TEST(EncounterTests, HitWhileAttacking_NoStagger) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.warrior, { 1.0f, 0.0f }, -std::numbers::pi_v<float> * 0.5f, false);
    for (int i = 0; i < 600 && encounter.FindEnemy(id)->state != EnemyState::Attacking; ++i) {
        encounter.Update(Step, PlayerAt(0.0f, 0.0f));
    }
    ASSERT_EQ(encounter.FindEnemy(id)->state, EnemyState::Attacking);

    StrikeUntilHit(encounter, id, 1);

    EXPECT_EQ(encounter.FindEnemy(id)->state, EnemyState::Attacking);
}

TEST(EncounterTests, Corpse_FadesThenDisappears) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    const auto id = encounter.Spawn(*content.minion, { 1.0f, 0.0f }, 0.0f, false);
    StrikeUntilHit(encounter, id, 100);
    const auto player = PlayerAt(-5.0f, 0.0f);

    RunEncounter(encounter, player, Encounter::CorpseSeconds - Encounter::CorpseFadeSeconds * 0.5f);
    ASSERT_NE(encounter.FindEnemy(id), nullptr);
    EXPECT_NEAR(Encounter::Opacity(*encounter.FindEnemy(id)), 0.5f, 0.05f);

    RunEncounter(encounter, player, Encounter::CorpseFadeSeconds);
    EXPECT_EQ(encounter.FindEnemy(id), nullptr);
}

TEST(EncounterTests, WaveCleared_NextWaveAfterDelay) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.StartWave(1, { 0.0f, 0.0f });
    const auto player = PlayerAt(0.0f, 0.0f);
    RunEncounter(encounter, player, Encounter::SpawnSeconds + Step);

    for (const auto& enemy : std::vector<Enemy>(encounter.GetEnemies())) StrikeUntilHit(encounter, enemy.id, 100);
    ASSERT_EQ(encounter.LivingEnemyCount(), 0u);

    RunEncounter(encounter, player, Encounter::WaveDelaySeconds - 0.1f);
    EXPECT_EQ(encounter.GetWave(), 1);

    RunEncounter(encounter, player, 0.2f);
    EXPECT_EQ(encounter.GetWave(), 2);
    const auto [light, heavy] = Encounter::WaveComposition(2);
    EXPECT_EQ(encounter.LivingEnemyCount(), static_cast<std::size_t>(light + heavy));
}

TEST(EncounterTests, Bodies_DoNotOverlap) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    for (int i = 0; i < 3; ++i) encounter.Spawn(*content.minion, { 5.0f, 0.0f }, 0.0f, false);

    RunEncounter(encounter, PlayerAt(0.0f, 0.0f), 2.0f);

    const auto& enemies = encounter.GetEnemies();
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        EXPECT_GE(Encounter::Distance(enemies[i].position, { 0.0f, 0.0f }), Encounter::CharacterRadius * 2.0f - 1e-3f);
        for (std::size_t j = i + 1; j < enemies.size(); ++j) {
            EXPECT_GE(Encounter::Distance(enemies[i].position, enemies[j].position), Encounter::CharacterRadius * 2.0f - 0.05f);
        }
    }
}

TEST(EncounterTests, SameSeed_SameFight) {
    EncounterContent content;
    auto a = MakeEncounter(content);
    auto b = MakeEncounter(content);
    a.StartWave(3, { 0.0f, 0.0f });
    b.StartWave(3, { 0.0f, 0.0f });

    const auto ea = RunEncounter(a, PlayerAt(0.0f, 0.0f), 6.0f);
    const auto eb = RunEncounter(b, PlayerAt(0.0f, 0.0f), 6.0f);

    ASSERT_EQ(ea.size(), eb.size());
    for (std::size_t i = 0; i < ea.size(); ++i) {
        EXPECT_EQ(ea[i].enemyId, eb[i].enemyId);
        EXPECT_EQ(ea[i].result.finalDamage, eb[i].result.finalDamage);
    }
}
