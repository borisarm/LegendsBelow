// EffectsTests.cpp - LB.Engine tests: particles and the effects gameplay emits
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// Particle Tests
// ============================================================================

TEST(ParticleFieldTests, Particles_RiseAndExpire) {
    ParticleField field;
    Particle p{};
    p.velocity = { 0.0f, 0.5f, 0.0f };
    p.buoyancy = 1.0f;
    p.lifetimeSeconds = 0.5f;
    field.Spawn(p);

    field.Update(0.25f);
    ASSERT_EQ(field.GetParticles().size(), 1u);
    EXPECT_GT(field.GetParticles()[0].position.y, 0.125f);   // faster than its start speed
    EXPECT_GT(field.GetParticles()[0].velocity.y, 0.5f);

    field.Update(0.3f);
    EXPECT_TRUE(field.GetParticles().empty());
}

TEST(ParticleFieldTests, ColorCoolsShrinksAndFades) {
    Particle p{};
    p.lifetimeSeconds = 1.0f;
    p.startSize = 0.2f;
    p.endSize = 0.0f;
    p.startColor = { 1.0f, 1.0f, 1.0f };
    p.middleColor = { 1.0f, 0.5f, 0.0f };
    p.endColor = { 0.0f, 0.0f, 0.0f };

    EXPECT_FLOAT_EQ(p.CurrentColor().g, 1.0f);
    EXPECT_FLOAT_EQ(p.CurrentColor().a, 1.0f);
    p.ageSeconds = 0.5f;
    EXPECT_FLOAT_EQ(p.CurrentColor().g, 0.5f);
    EXPECT_FLOAT_EQ(p.Size(), 0.1f);
    p.ageSeconds = 0.75f;
    EXPECT_FLOAT_EQ(p.CurrentColor().a, 0.5f);
    p.ageSeconds = 1.0f;
    EXPECT_FLOAT_EQ(p.CurrentColor().a, 0.0f);
}

TEST(ParticleFieldTests, Bounded_OldestMakeRoom) {
    ParticleField field;
    for (std::size_t i = 0; i < ParticleField::MaxParticles + 10; ++i) {
        Particle p{};
        p.startSize = static_cast<float>(i);
        field.Spawn(p);
    }

    ASSERT_EQ(field.GetParticles().size(), ParticleField::MaxParticles);
    EXPECT_FLOAT_EQ(field.GetParticles().front().startSize, 10.0f);
}

TEST(GameEngineTorchTests, NearbyTorches_BurnWithParticles) {
    GameEngine engine;
    InitializeDungeon(engine);

    RunSteps(engine, 60);

    const auto& particles = engine.GetParticles().GetParticles();
    ASSERT_FALSE(particles.empty());
    EXPECT_EQ(engine.GetRendering().GetQueuedParticles().size(), particles.size());
    // Every particle rises from a torch within range of the player (but the sparkles of loot lying
    // about, and the dust hanging in the air)
    const auto atLoot = [&](const auto& particle) {
        return particle.effect == ParticleEffect::CryptDust || std::ranges::any_of(engine.GetGroundItems(), [&](const GroundItem& ground) {
            return std::hypot(ground.position.x - particle.position.x, ground.position.y - particle.position.z) < 1.0f;
        });
    };
    for (const auto& particle : particles) {
        if (atLoot(particle)) continue;
        const auto nearest = std::ranges::min(engine.GetTorches() | std::views::transform([&](const GameEngine::Torch& torch) {
            return std::hypot(torch.position.x - particle.position.x, torch.position.y - particle.position.z);
        }));
        EXPECT_LT(nearest, 1.5f);
        EXPECT_GT(particle.position.y, 1.0f);
    }
    for (const auto& torch : engine.GetTorches()) {
        if (Encounter::Distance(torch.position, engine.GetPlayerPosition()) <= GameEngine::TorchFireRange) continue;
        for (const auto& particle : particles) {
            if (particle.effect == ParticleEffect::CryptDust) continue;
            EXPECT_GT(std::hypot(torch.position.x - particle.position.x, torch.position.y - particle.position.z), 0.5f);
        }
    }
}

TEST(GameEngineTorchTests, Fire_BurnsOnDuringThePause) {
    GameEngine engine;
    InitializeDungeon(engine);
    RunSteps(engine, 30);
    TapKey(engine, Keys::Escape);
    RunSteps(engine, 2);
    ASSERT_TRUE(engine.IsPaused());
    const auto before = engine.GetParticles().GetParticles();

    RunSteps(engine, 60);

    ASSERT_FALSE(engine.GetParticles().GetParticles().empty());
    EXPECT_NE(engine.GetParticles().GetParticles().front().ageSeconds, before.front().ageSeconds);
}

TEST(GameEngineTorchTests, NewFloor_FreshFire) {
    GameEngine engine;
    InitializeDungeon(engine);
    RunSteps(engine, 30);

    engine.Descend();

    EXPECT_TRUE(engine.GetParticles().GetParticles().empty());
}

// ============================================================================
// GameEngine Effect Tests
// ============================================================================

TEST(GameEngineEffectTests, RisingSkeleton_StirsDust) {
    GameEngine engine;
    InitializeForCombat(engine, true);   // the first wave rises

    RunSteps(engine, 20);

    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::RiseDust), 0u);
    for (const auto& p : engine.GetParticles().GetParticles()) {
        if (p.effect == ParticleEffect::RiseDust) EXPECT_FALSE(p.glowing);
    }
}

TEST(GameEngineEffectTests, Kill_SparksAndBoneDust) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    KillWithSword(engine, id);

    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::HitSparks), 0u);
    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::BoneDust), 0u);
}

TEST(GameEngineEffectTests, Potion_HealingSparkles) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f });
    const auto max = PlayerMaxHitPoints(engine);
    StepUntil(engine, [&] { return PlayerHitPoints(engine) < max; }, 20.0f);
    ASSERT_TRUE(engine.IsPlayerAlive());
    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::HitSparks), 0u);   // the blow that landed
    ASSERT_EQ(engine.GetParticles().Count(ParticleEffect::Healing), 0u);

    ASSERT_TRUE(engine.UseItem(GameEngine::HealingPotion));

    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::Healing), 20u);
}

TEST(GameEngineEffectTests, LevelUp_GoldenBurst) {
    GameEngine engine;
    InitializeForCombat(engine);

    LevelUp(engine);

    // The burst lives about a second and a half; it was emitted on the last kill
    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::LevelUp), 0u);
}

TEST(GameEngineEffectTests, GroundLoot_Sparkles_AlsoWhilePaused) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.DropLoot("loot_skeleton_warrior", { 4.0f, 4.0f });
    ASSERT_FALSE(engine.GetGroundItems().empty());
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.IsPaused());

    RunSteps(engine, 60);

    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::LootSparkle), 0u);
}

TEST(GameEngineEffectTests, Walking_KicksUpDust_StandingStillDoesNot) {
    GameEngine engine;
    InitializeForCombat(engine);
    RunSteps(engine, 60);
    EXPECT_EQ(engine.GetParticles().Count(ParticleEffect::Footstep), 0u);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::Footstep), 0u);
}

TEST(GameEngineEffectTests, Particles_DrawnDustAndGlow) {
    GameEngine engine;
    InitializeForCombat(engine, true);
    engine.DropLoot("loot_skeleton_warrior", { 4.0f, 4.0f });

    RunSteps(engine, 20);

    const auto& queued = engine.GetRendering().GetQueuedParticles();
    EXPECT_EQ(queued.size(), engine.GetParticles().GetParticles().size());
    EXPECT_TRUE(std::ranges::any_of(queued, &LB::Systems::Rendering::ParticleRenderData::glowing));
    EXPECT_TRUE(std::ranges::any_of(queued, [](const auto& p) { return !p.glowing; }));
}

TEST(GameEngineEffectTests, Chest_BurstsOpen) {
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

    WalkTo(engine, engine.GetWorld().RoomCenter(room), 60 * 90, true);

    ASSERT_TRUE(engine.GetRoomStates()[static_cast<std::size_t>(room)].chestOpened);
    EXPECT_GT(engine.GetParticles().Count(ParticleEffect::ChestBurst), 0u);
}
