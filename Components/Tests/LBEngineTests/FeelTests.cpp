// FeelTests.cpp - LB.Engine tests: shadows under characters, hit-stop and camera shake on heavy
// blows, knockback, the death screen and the run's summary, and each tier's ambience, footsteps,
// and air (dust, mist, embers)
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    EngineOptions ArenaOptions() {
        return EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false, .randomSeed = CombatSeed };
    }

    // A boss beside the hero, stepping until its first heavy blow shakes the camera
    bool AwaitAHeavyBlow(GameEngine& engine) {
        engine.SpawnEnemy("enemy_golem_warlord", { 1.6f, 0.0f });
        for (int i = 0; i < 60 * 10; ++i) {
            engine.Tick(OneSixtieth);
            if (engine.GetShake() > 0.0f) return true;
        }
        return false;
    }
}

TEST(ShadowTests, EveryCharacter_CastsOne) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    engine.Tick(OneSixtieth);

    const auto& shadows = engine.GetRendering().GetQueuedShadows();
    ASSERT_GE(shadows.size(), 2u);
    const auto at = engine.GetPlayerPosition();
    EXPECT_TRUE(std::ranges::any_of(shadows, [&](const auto& shadow) {
        return std::abs(shadow.position.x - at.x) < 1e-3f && std::abs(shadow.position.z - at.y) < 1e-3f && shadow.opacity == GameEngine::ShadowOpacity;
    }));
}

TEST(ShadowTests, NearATorch_StretchedAwayFromTheFlame) {
    GameEngine engine;
    InitializeDungeon(engine);
    engine.Tick(OneSixtieth);
    ASSERT_FALSE(engine.GetTorches().empty());
    // A step from a torch, toward the middle of the room it lights
    const auto& torch = engine.GetTorches().front();
    const auto flame = TorchSet::FlameOf(torch);
    const auto center = engine.GetWorld().RoomCenter(engine.GetWorld().RoomAt({ torch.position.x + std::sin(torch.yaw), torch.position.y + std::cos(torch.yaw) }));
    const float dx = center.x - flame.x, dy = center.y - flame.z;
    const float length = std::hypot(dx, dy);
    const WorldPoint spot{ flame.x + dx / length * 1.5f, flame.z + dy / length * 1.5f };
    engine.EnterFloorPosition(spot);
    ASSERT_LT(Encounter::Distance(engine.GetPlayerPosition(), spot), 1e-3f);
    engine.Tick(OneSixtieth);

    std::vector<LB::Systems::Rendering::ShadowRenderData> mine;
    for (const auto& shadow : engine.GetRendering().GetQueuedShadows()) {
        if (Encounter::Distance({ shadow.position.x, shadow.position.z }, spot) < 1.5f) mine.push_back(shadow);
    }
    ASSERT_EQ(mine.size(), 3u);   // the feet's, and two fainter ones trailing away
    const auto fromFlame = [&](const auto& shadow) { return Encounter::Distance({ shadow.position.x, shadow.position.z }, { flame.x, flame.z }); };
    EXPECT_LT(fromFlame(mine[0]), fromFlame(mine[1]));
    EXPECT_LT(fromFlame(mine[1]), fromFlame(mine[2]));
    EXPECT_GT(mine[0].opacity, mine[1].opacity);
}

TEST(FeelTests, HeavyBlows_ShakeTheCamera_AndFreezeTheActionWhenEnabled) {
    GameEngine engine;
    auto options = ArenaOptions();
    options.hitStop = true;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
    ASSERT_TRUE(AwaitAHeavyBlow(engine));

    ASSERT_GT(engine.GetHitStopSeconds(), 0.0f);
    const auto time = engine.GetGameTimeSeconds();
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.GetGameTimeSeconds(), time);   // frozen
    RunSteps(engine, 10);
    EXPECT_GT(engine.GetGameTimeSeconds(), time);   // and on again
}

TEST(FeelTests, ABossBlow_ThrowsTheHeroBack) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, ArenaOptions()));
    const auto start = engine.GetPlayerPosition();
    ASSERT_TRUE(AwaitAHeavyBlow(engine));   // the warlord at +x

    RunSteps(engine, 20);

    EXPECT_LT(engine.GetPlayerPosition().x, start.x - 0.5f);   // thrown away from it
}

TEST(FeelTests, Knock_ThrowsAnEnemyBack_ButNotABoss) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, ArenaOptions()));
    const auto minion = *engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    const auto boss = *engine.SpawnEnemy("enemy_golem_warlord", { -3.0f, 0.0f });
    RunSteps(engine, 60);   // up from the ground
    const auto before = engine.GetEncounter().FindEnemy(minion)->position.x;

    EXPECT_TRUE(engine.KnockEnemy(minion, engine.GetPlayerPosition(), 1.0f));
    EXPECT_FALSE(engine.KnockEnemy(boss, engine.GetPlayerPosition(), 1.0f));
    RunSteps(engine, 11);   // the throw's time (it keeps walking at the hero meanwhile)

    EXPECT_GT(engine.GetEncounter().FindEnemy(minion)->position.x, before + 0.4f);
}

TEST(FeelTests, ShieldBash_ThrowsItsTargetBack) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, ArenaOptions()));
    engine.GrantExperience(400);   // level 3: the bash
    engine.Tick(OneSixtieth);
    const auto id = *engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.2f, 0.0f });
    RunSteps(engine, 60);   // up from the ground
    const auto before = engine.GetEncounter().FindEnemy(id)->position.x;

    TapKey(engine, Keys::FromChar('2'));
    RunSteps(engine, 40);

    EXPECT_GT(engine.GetEncounter().FindEnemy(id)->position.x, before + 1.0f);
}

TEST(FeelTests, WithoutHitStop_TheShakeAlone) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, ArenaOptions()));
    ASSERT_TRUE(AwaitAHeavyBlow(engine));

    EXPECT_EQ(engine.GetHitStopSeconds(), 0.0f);
    const float shake = engine.GetShake();
    RunSteps(engine, 5);
    EXPECT_LT(engine.GetShake(), shake);   // dying away
}

TEST(DeathScreenTests, AFallenHero_SeesTheRun_AndRisesAgain) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
        .randomSeed = CombatSeed, .dungeon = true, .deathScreen = true }));
    engine.Tick(OneSixtieth);
    engine.SpawnEnemy("enemy_golem_warlord", { engine.GetPlayerPosition().x + 1.6f, engine.GetPlayerPosition().y });
    for (int i = 0; i < 60 * 60 && !engine.GetMenu().IsOpen(); ++i) engine.Tick(OneSixtieth);

    ASSERT_EQ(engine.GetMenu().Page(), MenuPage::Fallen);
    EXPECT_FALSE(engine.IsPlayerAlive());
    EXPECT_EQ(engine.GetRunStats().falls, 1);
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "YOU HAVE FALLEN"; }));
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "Falls: 1"; }));
    TapKey(engine, Keys::Escape);   // no way back: the choice is the player's
    engine.Tick(OneSixtieth);
    ASSERT_EQ(engine.GetMenu().Page(), MenuPage::Fallen);

    TapKey(engine, Keys::Enter);    // Rise Again
    engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_TRUE(engine.IsPlayerAlive());
}

TEST(DeathScreenTests, TheRunsStats_Count) {
    GameEngine engine;
    InitializeForCombat(engine);
    KillWithSword(engine, *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f }));
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.GetRunStats().kills, 1);
}

TEST(AmbienceTests, EachTier_ItsOwnLoop_AndFootsteps) {
    GameEngine engine;
    InitializeWithSounds(engine, false, true);
    engine.Tick(OneSixtieth);
    auto played = engine.GetAudio().TakePlayedClips();
    EXPECT_NE(std::ranges::find(played, "ambience_crypts"), played.end());

    engine.EnterFloor(5);   // the caves
    engine.Tick(OneSixtieth);
    played = engine.GetAudio().TakePlayedClips();
    EXPECT_NE(std::ranges::find(played, "ambience_caves"), played.end());

    HoldKeysFor(engine, { Keys::FromChar('W') }, 90);
    played = engine.GetAudio().TakePlayedClips();
    EXPECT_TRUE(Played(played, GameSound::FootstepDirt));
    EXPECT_FALSE(Played(played, GameSound::Footstep));
}

TEST(AmbienceTests, EachTier_ItsOwnAir) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
        .randomSeed = CombatSeed, .dungeon = true }));
    const auto drifting = [&](ParticleEffect effect) { return engine.GetParticles().Count(effect); };
    RunSteps(engine, 60);
    EXPECT_GT(drifting(ParticleEffect::CryptDust), 5u);
    EXPECT_EQ(drifting(ParticleEffect::CaveMist), 0u);

    engine.EnterFloor(5);   // the caves
    RunSteps(engine, 60);
    EXPECT_GT(drifting(ParticleEffect::CaveMist), 1u);
    engine.EnterFloor(8);   // the ruins
    RunSteps(engine, 60);
    EXPECT_GT(drifting(ParticleEffect::RuinEmbers), 2u);
}
