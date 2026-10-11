// CombatTests.cpp - LB.Engine tests: the player's combat and the combat texts
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// GameEngine Combat Tests
// ============================================================================

TEST(GameEngineCombatTests, Initialize_FirstWaveRises) {
    GameEngine engine;
    InitializeForCombat(engine, true);

    EXPECT_EQ(engine.GetEncounter().GetWave(), 1);
    ASSERT_EQ(engine.GetEncounter().GetEnemies().size(), 2u);
    for (const auto& enemy : engine.GetEncounter().GetEnemies()) {
        EXPECT_EQ(enemy.state, EnemyState::Spawning);
        EXPECT_GE(Encounter::Distance(enemy.position, engine.GetPlayerPosition()), Encounter::MinimumSpawnDistance);
    }
}

TEST(GameEngineCombatTests, SpawnEnemiesOff_EmptyArena) {
    GameEngine engine;
    InitializeForCombat(engine);
    RunSteps(engine, 60);

    EXPECT_EQ(engine.GetEncounter().GetWave(), 0);
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());
    EXPECT_FALSE(engine.SpawnEnemy("enemy_unknown", { 1.0f, 0.0f }).has_value());
}

TEST(GameEngineCombatTests, NoContent_NoCombat) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600));
    TapKey(engine, Keys::Space);
    RunSteps(engine, 30);

    EXPECT_FALSE(engine.IsPlayerAlive());
    EXPECT_FALSE(engine.IsPlayerAttacking());
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());
}

TEST(GameEngineCombatTests, PlayerAttack_FromStartingWeaponAndClass) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto* sword = engine.GetContent()->FindWeapon(GameEngine::StartingWeapon);
    ASSERT_NE(sword, nullptr);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    const auto strength = player.GetAttributes().GetStrengthModifier();

    const auto attack = engine.GetPlayerAttackProfile();

    EXPECT_EQ(attack.attackBonus, *engine.GetCharacters().GetAttackBonus(*engine.GetPlayerId(), sword->category));
    EXPECT_EQ(attack.damageDice.sides, sword->damageDice.sides);
    EXPECT_EQ(attack.damageBonus, sword->damageBonus + strength);
    EXPECT_EQ(attack.damageType, LB::Combat::DamageType::Slashing);
    EXPECT_EQ(attack.critRangeMin, sword->critRangeMin);
    EXPECT_FLOAT_EQ(attack.critMultiplier, sword->critMultiplier);
}

TEST(GameEngineCombatTests, PlayerDefense_TenPlusDexterityAndArmour) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();

    EXPECT_EQ(engine.GetPlayerDefense().armorClass, 10 + player.GetAttributes().GetDexterityModifier());
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, 0);

    ASSERT_TRUE(engine.EquipPlayerArmor("armor_chest_rusted_breastplate_t1"));
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, engine.GetContent()->FindArmor("armor_chest_rusted_breastplate_t1")->physicalAR);
}

TEST(GameEngineCombatTests, Attack_TurnsTowardAndStrikesTheEnemyInReach) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });
    ASSERT_TRUE(id.has_value());

    TapKey(engine, Keys::Space);
    engine.Tick(OneSixtieth);
    EXPECT_TRUE(engine.IsPlayerAttacking());

    const auto events = StepUntil(engine, [&] { return !engine.IsPlayerAttacking(); }, 2.0f);
    ASSERT_EQ(CountKind(events, CombatEventKind::PlayerAttack), 1u);
    const auto strike = *std::ranges::find(events, CombatEventKind::PlayerAttack, &CombatEvent::kind);
    EXPECT_EQ(strike.enemyId, *id);

    // Turned east (+X) toward the enemy: a quarter turn from facing +Z
    EXPECT_NEAR(engine.GetPlayerFacingYaw(), std::numbers::pi_v<float> * 0.5f, 1e-4f);
    if (strike.result.hit) {
        EXPECT_LT(engine.GetEncounter().FindEnemy(*id)->hitPoints, engine.GetContent()->FindEnemy(GameEngine::LightEnemy)->hitPoints);
    }
}

TEST(GameEngineCombatTests, Swing_RootsThePlayerUntilTheStrike) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto* sword = engine.GetContent()->FindWeapon(GameEngine::StartingWeapon);

    engine.OnKeyDown(Keys::FromChar('D'));
    TapKey(engine, Keys::Space);
    const int windUpSteps = static_cast<int>(sword->attackTimeSeconds * Encounter::AttackImpactFraction * 60.0f) - 1;
    RunSteps(engine, windUpSteps);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);

    RunSteps(engine, 10);   // past the strike: walking again
    engine.OnKeyUp(Keys::FromChar('D'));
    EXPECT_NE(engine.GetPlayerPosition().x, 0.0f);
}

TEST(GameEngineCombatTests, NoEnemyInReach_SwingWhiffs) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 6.0f, 0.0f });

    TapKey(engine, Keys::Space);
    const auto events = StepUntil(engine, [&] { return false; }, 0.8f);

    EXPECT_EQ(CountKind(events, CombatEventKind::PlayerAttack), 0u);
}

TEST(GameEngineCombatTests, HoldingAttack_KillsAMinion) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    engine.OnKeyDown(Keys::Space);
    const auto events = StepUntil(engine, [&] { return engine.GetEncounter().FindEnemy(id)->IsDead(); }, 15.0f);
    engine.OnKeyUp(Keys::Space);

    EXPECT_TRUE(engine.GetEncounter().FindEnemy(id)->IsDead());
    EXPECT_GE(CountKind(events, CombatEventKind::PlayerAttack), 2u);
    EXPECT_TRUE(std::ranges::any_of(events, &CombatEvent::killed));
}

TEST(GameEngineCombatTests, EnemyHits_ReducePlayerHealth) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f });
    const auto before = PlayerHitPoints(engine);

    const auto events = StepUntil(engine, [&] { return PlayerHitPoints(engine) < before; }, 20.0f);

    std::int32_t dealt = 0;
    for (const auto& event : events) {
        if (event.kind == CombatEventKind::EnemyAttack && event.result.hit) dealt += event.result.finalDamage;
    }
    EXPECT_GT(dealt, 0);
    EXPECT_EQ(PlayerHitPoints(engine), before - dealt);
}

TEST(GameEngineCombatTests, PlayerDown_GetsBackUpWithFullHealth) {
    GameEngine engine;
    InitializeForCombat(engine);
    for (float y : { -1.0f, 1.0f }) {
        for (float x : { -1.0f, 1.0f }) engine.SpawnEnemy(GameEngine::HeavyEnemy, { x, y });
    }

    StepUntil(engine, [&] { return !engine.IsPlayerAlive(); }, 60.0f);
    ASSERT_FALSE(engine.IsPlayerAlive());

    // Down: no moving, no swinging
    engine.OnKeyDown(Keys::FromChar('D'));
    TapKey(engine, Keys::Space);
    RunSteps(engine, 30);
    engine.OnKeyUp(Keys::FromChar('D'));
    EXPECT_FALSE(engine.IsPlayerAttacking());

    RunSteps(engine, static_cast<int>(GameEngine::PlayerRespawnSeconds * 60.0f));
    EXPECT_TRUE(engine.IsPlayerAlive());
    EXPECT_EQ(PlayerHitPoints(engine), PlayerMaxHitPoints(engine));
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);
    EXPECT_TRUE(engine.GetEncounter().GetEnemies().empty());   // waves off: the arena is cleared
}

TEST(GameEngineCombatTests, PlayerDown_WavesStartOver) {
    GameEngine engine;
    InitializeForCombat(engine, true);
    for (float y : { -1.0f, 1.0f }) {
        for (float x : { -1.0f, 1.0f }) engine.SpawnEnemy(GameEngine::HeavyEnemy, { x, y });
    }
    StepUntil(engine, [&] { return !engine.IsPlayerAlive(); }, 60.0f);
    ASSERT_FALSE(engine.IsPlayerAlive());

    RunSteps(engine, static_cast<int>(GameEngine::PlayerRespawnSeconds * 60.0f) + 1);

    EXPECT_EQ(engine.GetEncounter().GetWave(), 1);
    EXPECT_EQ(engine.GetEncounter().LivingEnemyCount(), 2u);
}

TEST(GameEngineCombatTests, Pause_FreezesEnemies) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 5.0f, 0.0f });
    RunSteps(engine, 10);
    TapKey(engine, Keys::Escape);
    RunSteps(engine, 1);
    const auto frozen = engine.GetEncounter().GetEnemies()[0].position;

    RunSteps(engine, 60);

    EXPECT_FLOAT_EQ(engine.GetEncounter().GetEnemies()[0].position.x, frozen.x);
    EXPECT_FLOAT_EQ(engine.GetEncounter().GetEnemies()[0].position.y, frozen.y);
}

TEST(GameEngineCombatTests, Hud_HealthBarsFollowHitPoints) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    const auto fills = [&] {
        std::vector<LB::Systems::Rendering::RectangleRenderData> bars;
        for (const auto& rect : engine.GetRendering().GetQueuedRectangles()) {
            if (rect.layerDepth == 12) bars.push_back(rect);
        }
        return bars;
    };

    engine.Tick(OneSixtieth);
    auto bars = fills();
    ASSERT_EQ(bars.size(), 1u);   // the player's; unhurt enemies show no bar
    EXPECT_EQ(bars[0].rect.width, 240);

    engine.OnKeyDown(Keys::Space);
    StepUntil(engine, [&] {
        const auto* enemy = engine.GetEncounter().FindEnemy(id);
        return enemy->hitPoints < enemy->definition->hitPoints;
    }, 10.0f);
    engine.OnKeyUp(Keys::Space);
    engine.Tick(OneSixtieth);

    bars = fills();
    const auto* enemy = engine.GetEncounter().FindEnemy(id);
    if (!enemy->IsDead()) {
        ASSERT_EQ(bars.size(), 2u);
        EXPECT_LT(bars[1].rect.width, 44);
    }
}

TEST(GameEngineCombatTests, PrimitiveEnemies_WithoutModels) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.Tick(OneSixtieth);
    const auto before = engine.GetRendering().GetQueuedMeshes().size();

    engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetRendering().GetQueuedMeshes().size(), before + 2);   // body and skull
}

TEST(GameEngineModelTests, Enemies_AnimatedAndHoldingTheirWeapons) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .spawnEnemies = false }))
        << engine.GetInitializationError();
    engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { -3.0f, 0.0f });

    RunSteps(engine, 30);

    const auto minions = DrawsOf(engine, "Skeleton_Minion");
    ASSERT_EQ(minions.size(), 1u);
    const auto walk = engine.GetRendering().FindAnimation("Walking_A");
    ASSERT_TRUE(walk.has_value());
    EXPECT_EQ(minions[0].animation.current.clip, *walk);   // closing in on the player
    EXPECT_FLOAT_EQ(minions[0].scale, engine.GetContent()->FindEnemy(GameEngine::LightEnemy)->modelScale);

    for (std::string_view weapon : { "Skeleton_Blade", "Skeleton_Axe", "Skeleton_Shield_Large_A" }) {
        const auto held = DrawsOf(engine, weapon);
        ASSERT_EQ(held.size(), 1u) << weapon;
        EXPECT_TRUE(held[0].transform.has_value()) << weapon;
    }
    EXPECT_EQ(DrawsOf(engine, "Skeleton_Warrior").size(), 1u);
}

TEST(GameEngineModelTests, Player_SwingsTheSword) {
    GameEngine engine;
    InitializeWithModels(engine);
    RunSteps(engine, 2);
    const auto restingSword = DrawsOf(engine, "sword_1handed");
    ASSERT_EQ(restingSword.size(), 1u);
    ASSERT_TRUE(restingSword[0].transform.has_value());   // in the knight's right hand slot
    const auto& rendering = engine.GetRendering();
    EXPECT_EQ(DrawsOf(engine, "Knight")[0].animation.current.clip, *rendering.FindAnimation("Idle_A"));

    TapKey(engine, Keys::Space);
    RunSteps(engine, 15);

    EXPECT_EQ(DrawsOf(engine, "Knight")[0].animation.current.clip, *rendering.FindAnimation("Melee_1H_Attack_Slice_Diagonal"));
    const auto swinging = DrawsOf(engine, "sword_1handed");
    ASSERT_EQ(swinging.size(), 1u);
    const auto rest = restingSword[0].transform->Translation();
    const auto now = swinging[0].transform->Translation();
    EXPECT_GT(std::abs(rest.x - now.x) + std::abs(rest.y - now.y) + std::abs(rest.z - now.z), 0.05f);
}

TEST(GameEngineModelTests, Enemies_AttackWithTheirMeleeClips) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .spawnEnemies = false }))
        << engine.GetInitializationError();
    const auto& rendering = engine.GetRendering();
    for (const auto& type : engine.GetContent()->GetEnemies()) {
        const auto name = (type.rig == "Large" ? std::string(GameEngine::LargeClipPrefix) : std::string{}) + type.attackAnimation;
        ASSERT_TRUE(rendering.FindAnimation(name).has_value()) << name;
    }
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    for (int i = 0; i < 300 && engine.GetEncounter().FindEnemy(id)->state != EnemyState::Attacking; ++i) engine.Tick(OneSixtieth);
    ASSERT_EQ(engine.GetEncounter().FindEnemy(id)->state, EnemyState::Attacking);
    engine.Tick(OneSixtieth);

    const auto minions = DrawsOf(engine, "Skeleton_Minion");
    ASSERT_EQ(minions.size(), 1u);
    const auto* minion = engine.GetContent()->FindEnemy(GameEngine::LightEnemy);
    EXPECT_EQ(minions[0].animation.current.clip, *rendering.FindAnimation(minion->attackAnimation));
}

// ============================================================================
// GameEngine Combat Text Tests
// ============================================================================

TEST(GameEngineCombatTextTests, PlayerStrike_FloatsTheDamageOverTheEnemy) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    TapKey(engine, Keys::Space);
    const auto events = StepUntil(engine, [&] { return !engine.GetFloatingTexts().empty(); }, 2.0f);

    ASSERT_EQ(engine.GetFloatingTexts().size(), 1u);
    const auto& text = engine.GetFloatingTexts()[0];
    const auto strike = std::ranges::find(events, CombatEventKind::PlayerAttack, &CombatEvent::kind);
    ASSERT_NE(strike, events.end());
    if (!strike->result.hit) {
        EXPECT_EQ(text.text, "MISS");
    }
    else {
        EXPECT_EQ(text.text, std::format("{}{}", strike->result.finalDamage, strike->result.critical ? "!" : ""));
    }
    EXPECT_NEAR(text.position.z, engine.GetEncounter().FindEnemy(id)->position.y, 1e-4f);
    EXPECT_GT(text.position.y, 1.0f);   // over the head
}

TEST(GameEngineCombatTextTests, EnemyHits_FloatRedOverThePlayer) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f });
    const auto before = PlayerHitPoints(engine);

    StepUntil(engine, [&] { return PlayerHitPoints(engine) < before; }, 20.0f);

    const auto red = std::ranges::find_if(engine.GetFloatingTexts(), [](const FloatingText& t) { return t.text != "MISS" && t.color.g < 0.5f; });
    ASSERT_NE(red, engine.GetFloatingTexts().end());
    EXPECT_NEAR(red->position.z, engine.GetPlayerPosition().y, 1e-4f);
}

TEST(GameEngineCombatTextTests, Texts_FadeAwayAndFreezeWhilePaused) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });
    TapKey(engine, Keys::Space);
    StepUntil(engine, [&] { return !engine.GetFloatingTexts().empty(); }, 2.0f);
    ASSERT_FALSE(engine.GetFloatingTexts().empty());

    TapKey(engine, Keys::Escape);
    RunSteps(engine, 120);
    EXPECT_FALSE(engine.GetFloatingTexts().empty());   // paused: still there

    TapKey(engine, Keys::Escape);
    RunSteps(engine, static_cast<int>(GameEngine::FloatingTextSeconds * 60.0f) + 2);
    for (const auto& text : engine.GetFloatingTexts()) EXPECT_LT(text.ageSeconds, GameEngine::FloatingTextSeconds);
}

TEST(GameEngineCombatTextTests, Hud_LabelsHitPointsAndWave) {
    GameEngine engine;
    InitializeForCombat(engine, true);

    engine.Tick(OneSixtieth);

    const auto& texts = engine.GetRendering().GetQueuedText();
    const auto has = [&](std::string_view label) {
        return std::ranges::any_of(texts, [&](const auto& t) { return t.text == label; });
    };
    const auto max = PlayerMaxHitPoints(engine);
    EXPECT_TRUE(has(std::format("HP {}/{}", max, max)));
    EXPECT_TRUE(has("WAVE 1"));
}

TEST(GameEngineCombatTextTests, FontDirectory_LoadsTheUiFont) {
    const auto models = ShippedModelDirectory();
    const auto fonts = std::filesystem::path(std::u8string(models.begin(), models.end())).parent_path() / "Fonts";
    const auto text = fonts.u8string();
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .fontDirectory = std::string(text.begin(), text.end()), .spawnEnemies = false }))
        << engine.GetInitializationError();

    EXPECT_TRUE(engine.GetRendering().FindFont("MedievalSharp-Regular").has_value());
}

TEST(GameEngineCombatTextTests, MissingFont_StopsStartup) {
    GameEngine engine;
    const auto empty = std::filesystem::temp_directory_path() / "lb_no_fonts_here";
    std::filesystem::create_directories(empty);

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .fontDirectory = empty.string() }));
    EXPECT_NE(engine.GetInitializationError().find("MedievalSharp-Regular.ttf: file not found"), std::string::npos) << engine.GetInitializationError();
    std::filesystem::remove_all(empty);
}
