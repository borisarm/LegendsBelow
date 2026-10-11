// SessionAndComponentTests.cpp - LB.Engine tests: run state across restarts and the engine parts on their own
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// GameEngine Session Tests
// ============================================================================

TEST(GameEngineSessionTests, ReinitializeAsArena_NothingLeftFromTheDungeon) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .randomSeed = CombatSeed, .dungeon = true }));
    TapKey(engine, Keys::FromChar('M'));
    RunSteps(engine, 30);
    ASSERT_FALSE(engine.GetTorches().empty());
    ASSERT_TRUE(engine.IsMapExpanded());

    engine.Shutdown();
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .spawnEnemies = false, .randomSeed = CombatSeed }));

    EXPECT_TRUE(engine.GetTorches().empty());
    EXPECT_TRUE(engine.GetParticles().GetParticles().empty());
    EXPECT_FALSE(engine.IsMapExpanded());
    EXPECT_EQ(engine.GetDepth(), 0);
    EXPECT_TRUE(engine.GetRoomStates().empty());
    EXPECT_EQ(engine.GetGameTimeSeconds(), 0.0);
    RunSteps(engine, 30);   // walls and torches of the old floor are not touched
    EXPECT_TRUE(engine.GetRendering().GetQueuedLights().empty());
}

TEST(GameEngineSessionTests, InitializeTwice_StartsAFreshRun) {
    GameEngine engine;
    InitializeDungeon(engine);
    RunSteps(engine, 30);
    engine.GiveItem("armor_head_leather_cap_t1");

    InitializeDungeon(engine);

    EXPECT_EQ(engine.CountItem("armor_head_leather_cap_t1"), 0);
    EXPECT_EQ(engine.GetCharacters().GetAllCharacterIds().size(), 1u);
    EXPECT_EQ(engine.GetGameTimeSeconds(), 0.0);
}

TEST(GameEngineSessionTests, FailedInitialize_LeavesNothingBehind) {
    GameEngine engine;
    InitializeDungeon(engine);
    RunSteps(engine, 30);

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = "Z:/no/such/content" }));

    EXPECT_FALSE(engine.IsInitialized());
    EXPECT_FALSE(engine.GetPlayerId().has_value());
    EXPECT_TRUE(engine.GetTorches().empty());
    EXPECT_TRUE(engine.GetGroundItems().empty());
}

// ============================================================================
// Engine Component Tests (each part on its own, without a GameEngine)
// ============================================================================

TEST(EffectsSystemTests, Emit_TagsAndCountsParticles) {
    EffectsSystem effects;

    effects.Emit(ParticleEffect::HitSparks, { 0.0f, 1.0f, 0.0f }, 12);
    effects.Emit(ParticleEffect::BoneDust, { 0.0f, 0.0f, 0.0f }, 5);

    EXPECT_EQ(effects.Particles().Count(ParticleEffect::HitSparks), 12u);
    EXPECT_EQ(effects.Particles().Count(ParticleEffect::BoneDust), 5u);
    for (const auto& p : effects.Particles().GetParticles()) {
        EXPECT_EQ(p.glowing, p.effect == ParticleEffect::HitSparks);   // sparks glow, dust does not
    }
}

TEST(EffectsSystemTests, Stream_AveragesItsRate) {
    EffectsSystem effects;

    for (int i = 0; i < 600; ++i) effects.Stream(ParticleEffect::LootSparkle, {}, 3.0f, 1.0f / 60.0f);

    // 3 a second for 10 seconds; streams are random, so allow some spread
    EXPECT_NEAR(static_cast<double>(effects.Particles().Count(ParticleEffect::LootSparkle)), 30.0, 12.0);
}

TEST(EffectsSystemTests, SameSeed_SameParticles) {
    EffectsSystem a{ 7u };
    EffectsSystem b{ 7u };

    a.Emit(ParticleEffect::ChestBurst, {}, 10);
    b.Emit(ParticleEffect::ChestBurst, {}, 10);

    for (std::size_t i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(a.Particles().GetParticles()[i].velocity.y, b.Particles().GetParticles()[i].velocity.y);
    }
}

TEST(TorchSetTests, ArenaRoom_TorchesOnEverySideFacingIn) {
    const auto world = WorldMap::Arena(10.0f, 7.0f);
    TorchSet torches;

    torches.Build(world, WallSet{});

    ASSERT_GE(torches.Torches().size(), 4u);
    for (const auto& torch : torches.Torches()) {
        EXPECT_FALSE(torch.wall.has_value());   // no wall pieces without models
        const WorldPoint ahead{ torch.position.x + std::sin(torch.yaw) * 0.5f, torch.position.y + std::cos(torch.yaw) * 0.5f };
        EXPECT_TRUE(world.IsOpen(ahead));
    }
}

TEST(TorchSetTests, Fire_OnlyNearThePlayer) {
    const auto world = WorldMap::Arena(10.0f, 7.0f);
    TorchSet torches;
    torches.Build(world, WallSet{});
    EffectsSystem effects;

    torches.UpdateFire(1.0f, { 1000.0f, 1000.0f }, WallSet{}, effects);
    EXPECT_TRUE(effects.Particles().GetParticles().empty());

    torches.UpdateFire(1.0f, { 0.0f, 0.0f }, WallSet{}, effects);
    EXPECT_EQ(effects.Particles().Count(ParticleEffect::TorchFlame), torches.Torches().size() * static_cast<std::size_t>(TorchSet::FlamesPerSecond));
}

TEST(TorchSetTests, Flicker_StaysInItsBand) {
    for (int i = 0; i < 200; ++i) {
        const float f = TorchSet::Flicker(i * 0.037, 1.3f);
        EXPECT_GE(f, 0.8f);
        EXPECT_LE(f, 1.0f);
    }
}

TEST(MinimapTests, Reveal_TheRoomAroundThePlayer) {
    const auto world = WorldMap::Arena(10.0f, 7.0f);
    Minimap map;
    map.Reset(world);
    const auto [x, y] = world.CellOf({ 0.0f, 0.0f });
    EXPECT_FALSE(map.IsExplored(x, y));

    map.Reveal(world, { 0.0f, 0.0f });

    EXPECT_TRUE(map.IsExplored(x, y));
    EXPECT_FALSE(map.IsExplored(-1, 0));
    EXPECT_FALSE(map.IsExplored(world.Layout().width, 0));
}

TEST(MinimapTests, Clear_ForgetsEverything) {
    const auto world = WorldMap::Arena(10.0f, 7.0f);
    Minimap map;
    map.Reset(world);
    map.Reveal(world, {});
    map.ToggleExpanded();

    map = Minimap{};

    const auto [x, y] = world.CellOf({});
    EXPECT_FALSE(map.IsExplored(x, y));
    EXPECT_FALSE(map.IsExpanded());
}

TEST(CombatTextLogTests, Attacks_DamageMissAndCritical) {
    CombatTextLog log;
    CombatEvent hit{};
    hit.result.hit = true;
    hit.result.finalDamage = 7;
    CombatEvent critical = hit;
    critical.result.critical = true;
    CombatEvent miss{};

    log.AddAttack(hit, {});
    log.AddAttack(critical, {});
    log.AddAttack(miss, {});

    ASSERT_EQ(log.Texts().size(), 3u);
    EXPECT_EQ(log.Texts()[0].text, "7");
    EXPECT_EQ(log.Texts()[1].text, "7!");
    EXPECT_GT(log.Texts()[1].scale, log.Texts()[0].scale);
    EXPECT_EQ(log.Texts()[2].text, "MISS");
    EXPECT_NE(log.Texts()[0].position.x, log.Texts()[1].position.x);   // side by side, not stacked
}

TEST(CombatTextLogTests, Update_DropsOldTexts) {
    CombatTextLog log;
    log.Add("+5", {}, {}, 3.0f);

    log.Update(CombatTextLog::Seconds * 0.5f);
    EXPECT_EQ(log.Texts().size(), 1u);
    log.Update(CombatTextLog::Seconds);
    EXPECT_TRUE(log.Texts().empty());
}

TEST(HudTests, Draw_ShowsWhatTheModelSays) {
    LB::Systems::Rendering::RenderingSystem renderer;
    renderer.Initialize(800, 600);
    renderer.BeginFrame();
    HudModel hud;
    hud.player = HudModel::Player{ .hitPoints = 9, .maxHitPoints = 14, .healthFraction = 9.0f / 14.0f, .level = 2, .gold = 31, .potions = 2 };
    hud.banner = "DEPTH 3";

    Hud::Draw(renderer, hud);

    const auto& texts = renderer.GetQueuedText();
    const auto shows = [&](std::string_view text) {
        return std::ranges::any_of(texts, [&](const auto& t) { return t.text == text; });
    };
    EXPECT_TRUE(shows("HP 9/14"));
    EXPECT_TRUE(shows("GOLD 31"));
    EXPECT_TRUE(shows("POTIONS 2"));
    EXPECT_TRUE(shows("DEPTH 3"));
    EXPECT_FALSE(shows("POINTS TO SPEND (I)"));
}

TEST(HudTests, NoPlayer_OnlyTheBanner) {
    LB::Systems::Rendering::RenderingSystem renderer;
    renderer.Initialize(800, 600);
    renderer.BeginFrame();
    HudModel hud;
    hud.banner = "WAVE 1";

    Hud::Draw(renderer, hud);

    ASSERT_EQ(renderer.GetQueuedText().size(), 1u);
    EXPECT_TRUE(renderer.GetQueuedRectangles().empty());
}
