// SpellTests.cpp - LB.Engine tests: statuses, spells in the encounter (missiles, bursts, casters),
// and the Mage: the class choice, the spell bar, mana, casting, and saving the class
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// Status Tests
// ============================================================================

TEST(StatusTests, Apply_HindersUntilItExpires) {
    LB::Content::StatusDefinition slow{};
    slow.key = "status_slow";
    slow.durationSeconds = 1.0f;
    slow.movementMultiplier = 0.5f;
    StatusSet statuses;

    statuses.Apply(slow);
    EXPECT_FLOAT_EQ(statuses.MovementMultiplier(), 0.5f);
    EXPECT_TRUE(statuses.Has("status_slow"));
    statuses.Update(0.6f);
    statuses.Apply(slow);                 // again: starts over
    statuses.Update(0.6f);
    EXPECT_TRUE(statuses.Has("status_slow"));
    statuses.Update(0.5f);

    EXPECT_FALSE(statuses.Has("status_slow"));
    EXPECT_FLOAT_EQ(statuses.MovementMultiplier(), 1.0f);
}

TEST(StatusTests, DamageOverTime_TicksEachInterval) {
    LB::Content::StatusDefinition burning{};
    burning.key = "status_burning";
    burning.durationSeconds = 3.0f;
    burning.tickIntervalSeconds = 1.0f;
    burning.damagePerTick = 2;
    burning.damageType = LB::Combat::DamageType::Fire;
    StatusSet statuses;
    statuses.Apply(burning);

    std::int32_t damage = 0;
    for (int i = 0; i < 60 * 4; ++i) {
        for (const auto& tick : statuses.Update(Step)) damage += tick.damage;
    }

    EXPECT_EQ(damage, 6);   // three ticks of 2
    EXPECT_TRUE(statuses.Active().empty());
}

TEST(StatusTests, Frozen_PreventsActionsAndRoots) {
    LB::Content::StatusDefinition frozen{};
    frozen.key = "status_frozen";
    frozen.durationSeconds = 2.0f;
    frozen.movementMultiplier = 0.0f;
    frozen.preventsActions = true;
    StatusSet statuses;
    statuses.Apply(frozen);

    EXPECT_TRUE(statuses.PreventsActions());
    EXPECT_FLOAT_EQ(statuses.MovementMultiplier(), 0.0f);
}

// ============================================================================
// Spells in the Encounter
// ============================================================================

TEST(SpellEncounterTests, FrostNova_HitsAndRootsEveryoneInItsRadius) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.SetContent(content.engine.GetContent().get());
    const auto near = encounter.Spawn(*content.warrior, { 2.0f, 0.0f }, FacingWest, false);
    const auto far = encounter.Spawn(*content.warrior, { 8.0f, 0.0f }, FacingWest, false);
    const auto* nova = content.engine.GetContent()->FindSpell("spell_frost_frost_nova_l2");

    const auto events = encounter.CastBurst(SpellPayload{ nova, 5 }, { 0.0f, 0.0f });

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].enemyId, near);
    EXPECT_EQ(events[0].spell, nova);
    EXPECT_TRUE(encounter.FindEnemy(near)->statuses.Has("status_root_basic"));
    EXPECT_FALSE(encounter.FindEnemy(far)->statuses.Has("status_root_basic"));
    ASSERT_EQ(encounter.GetBursts().size(), 1u);
    EXPECT_FLOAT_EQ(encounter.GetBursts()[0].radius, nova->radius);
}

TEST(SpellEncounterTests, Rooted_StaysPutButStillAttacks) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.SetContent(content.engine.GetContent().get());
    const auto id = encounter.Spawn(*content.minion, { 3.0f, 0.0f }, FacingWest, false);
    encounter.CastBurst(SpellPayload{ content.engine.GetContent()->FindSpell("spell_frost_frost_nova_l2"), 1 }, { 3.0f, 0.0f });

    RunEncounter(encounter, PlayerAt(0.0f, 0.0f), 1.0f);

    EXPECT_NEAR(encounter.FindEnemy(id)->position.x, 3.0f, 1e-3f);   // would have closed in
}

TEST(SpellEncounterTests, Fireball_BurstsWhereItLands_AndBurns) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.SetContent(content.engine.GetContent().get());
    const auto first = encounter.Spawn(*content.warrior, { 5.0f, 0.0f }, FacingWest, false);
    const auto second = encounter.Spawn(*content.warrior, { 5.0f, 1.5f }, FacingWest, false);
    const auto* fireball = content.engine.GetContent()->FindSpell("spell_evocation_fireball_l2");
    encounter.CastMissile(SpellPayload{ fireball, 3 }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, 0.375f);

    std::vector<CombatEvent> hits;
    for (int i = 0; i < 60 && hits.empty(); ++i) hits = encounter.Update(Step, PlayerAt(0.0f, 0.0f, false));

    ASSERT_EQ(hits.size(), 2u);   // both in the burst
    EXPECT_TRUE(std::ranges::any_of(hits, [&](const CombatEvent& e) { return e.enemyId == first; }));
    EXPECT_TRUE(std::ranges::any_of(hits, [&](const CombatEvent& e) { return e.enemyId == second; }));
    EXPECT_TRUE(encounter.FindEnemy(first)->statuses.HasDamageOverTime(LB::Combat::DamageType::Fire));

    // Burning: damage ticks counted as the player's
    const auto ticks = RunEncounter(encounter, PlayerAt(0.0f, 0.0f, false), 3.5f);
    EXPECT_GE(std::ranges::count_if(ticks, [&](const CombatEvent& e) { return e.enemyId == first && e.kind == CombatEventKind::PlayerAttack; }), 1);
}

TEST(SpellEncounterTests, SkeletonMage_CastsFrostBolts) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.SetContent(content.engine.GetContent().get());
    const auto* mage = content.engine.GetContent()->FindEnemy(GameEngine::CasterEnemy);
    encounter.Spawn(*mage, { 6.0f, 0.0f }, FacingWest, false);

    bool spellInFlight = false;
    std::vector<CombatEvent> events;
    for (int i = 0; i < 60 * 5 && events.empty(); ++i) {
        events = encounter.Update(Step, PlayerAt(0.0f, 0.0f));
        for (const auto& projectile : encounter.GetProjectiles()) spellInFlight = spellInFlight || projectile.spell.has_value();
    }

    EXPECT_TRUE(spellInFlight);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, CombatEventKind::EnemyAttack);
    ASSERT_NE(events[0].spell, nullptr);
    EXPECT_EQ(events[0].spell->key, "spell_frost_frost_bolt_l1");
    EXPECT_TRUE(events[0].result.hit);   // spells do not roll to hit
}

TEST(SpellEncounterTests, Waves_CastersFromTheThirdWave) {
    EXPECT_EQ(Encounter::WaveCasters(2), 0);
    EXPECT_EQ(Encounter::WaveCasters(3), 1);
    EXPECT_EQ(Encounter::WaveCasters(6), 2);
}

// ============================================================================
// The Mage (GameEngine)
// ============================================================================

TEST(GameEngineMageTests, Mage_StartsWithWandStaffSpellsAndFullMana) {
    GameEngine engine;
    InitializeMage(engine);

    EXPECT_EQ(engine.GetPlayerClass(), LB::Character::CharacterClass::Mage);
    ASSERT_NE(engine.GetPlayerWeapon(), nullptr);
    EXPECT_EQ(engine.GetPlayerWeapon()->category, LB::Combat::WeaponType::Wand);
    EXPECT_EQ(engine.CountItem("weapon_staff_apprentices_staff_t1"), 1);
    EXPECT_EQ(engine.GetSpellBar().size(), 4u);
    EXPECT_GT(PlayerMaxMana(engine), 0);
    EXPECT_EQ(PlayerMana(engine), PlayerMaxMana(engine));
    EXPECT_GT(engine.GetPlayerSpellPower(), 0);
}

TEST(GameEngineMageTests, Warrior_HasNoSpellBar) {
    GameEngine engine;
    InitializeForCombat(engine);

    EXPECT_TRUE(engine.GetSpellBar().empty());
    EXPECT_EQ(engine.SpellBlocker(0), "empty");
}

TEST(GameEngineMageTests, Wand_ShootsAnArcaneOrb) {
    GameEngine engine;
    InitializeMage(engine);

    TapKey(engine, Keys::Space);
    for (int i = 0; i < 60 && engine.GetEncounter().GetProjectiles().empty(); ++i) engine.Tick(OneSixtieth);

    ASSERT_EQ(engine.GetEncounter().GetProjectiles().size(), 1u);
    const auto& orb = engine.GetEncounter().GetProjectiles()[0];
    EXPECT_TRUE(orb.model.empty());
    EXPECT_FALSE(orb.spell.has_value());   // a weapon's shot
    EXPECT_EQ(orb.Element(), LB::Combat::DamageType::Arcane);
}

TEST(GameEngineMageTests, ArcaneBolt_CastsThenFliesAndSpendsManaAsItLeaves) {
    GameEngine engine;
    InitializeMage(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 6.0f, 0.0f });
    const auto& bolt = *engine.GetSpellBar()[0];
    const auto full = PlayerMana(engine);

    TapKey(engine, Keys::FromChar('1'));
    engine.Tick(OneSixtieth);
    EXPECT_TRUE(engine.IsPlayerCasting());
    EXPECT_EQ(PlayerMana(engine), full);   // not yet
    RunSteps(engine, static_cast<int>(bolt.castTimeSeconds * 60.0f));

    EXPECT_FALSE(engine.IsPlayerCasting());
    EXPECT_EQ(PlayerMana(engine), full - bolt.manaCost);
    ASSERT_EQ(engine.GetEncounter().GetProjectiles().size(), 1u);
    EXPECT_EQ(engine.GetEncounter().GetProjectiles()[0].spell->spell, &bolt);

    const auto* enemy = engine.GetEncounter().FindEnemy(id);
    const auto before = enemy->hitPoints;
    for (int i = 0; i < 60 && !engine.GetEncounter().GetProjectiles().empty(); ++i) engine.Tick(OneSixtieth);
    enemy = engine.GetEncounter().FindEnemy(id);
    EXPECT_TRUE(!enemy || enemy->hitPoints < before || enemy->IsDead());
}

TEST(GameEngineMageTests, Moving_CancelsTheCastForFree) {
    GameEngine engine;
    InitializeMage(engine);
    const auto full = PlayerMana(engine);

    TapKey(engine, Keys::FromChar('1'));
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.IsPlayerCasting());
    HoldKeysFor(engine, { Keys::FromChar('D') }, 5);

    EXPECT_FALSE(engine.IsPlayerCasting());
    RunSteps(engine, 60);
    EXPECT_TRUE(engine.GetEncounter().GetProjectiles().empty());
    EXPECT_EQ(PlayerMana(engine), full);
}

TEST(GameEngineMageTests, NotEnoughMana_SaysSoAndDoesNotCast) {
    GameEngine engine;
    InitializeMage(engine);
    const auto& bolt = *engine.GetSpellBar()[0];
    TapKey(engine, Keys::FromChar('1'));
    engine.Tick(OneSixtieth);
    RunSteps(engine, static_cast<int>(bolt.castTimeSeconds * 60.0f));
    ASSERT_LT(PlayerMana(engine), bolt.manaCost);   // one bolt spends most of a new mage's mana

    TapKey(engine, Keys::FromChar('1'));
    engine.Tick(OneSixtieth);

    EXPECT_FALSE(engine.IsPlayerCasting());
    EXPECT_EQ(engine.SpellBlocker(0), "mana");
    EXPECT_TRUE(std::ranges::any_of(engine.GetFloatingTexts(), [](const auto& text) { return text.text == "NO MANA"; }));
}

TEST(GameEngineMageTests, Mana_ComesBackOverTime) {
    GameEngine engine;
    InitializeMage(engine);
    const auto& bolt = *engine.GetSpellBar()[0];
    TapKey(engine, Keys::FromChar('1'));
    engine.Tick(OneSixtieth);
    RunSteps(engine, static_cast<int>(bolt.castTimeSeconds * 60.0f));
    const auto spent = PlayerMana(engine);

    RunSteps(engine, 120);

    const auto regen = engine.GetContent()->FindClass(LB::Character::CharacterClass::Mage)->manaRegenPerSecond;
    EXPECT_NEAR(static_cast<float>(PlayerMana(engine) - spent), regen * 2.0f, 1.0f);
}

TEST(GameEngineMageTests, Fireball_LockedUntilLevelFive) {
    GameEngine engine;
    InitializeMage(engine);

    EXPECT_EQ(engine.SpellBlocker(2), "level 5");
    TapKey(engine, Keys::FromChar('3'));
    engine.Tick(OneSixtieth);

    EXPECT_FALSE(engine.IsPlayerCasting());
}

TEST(GameEngineMageTests, FrostBolt_SlowsItsTarget) {
    GameEngine engine;
    InitializeMage(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::HeavyEnemy, { 5.0f, 0.0f });

    TapKey(engine, Keys::FromChar('2'));
    bool slowed = false;
    for (int i = 0; i < 120 && !slowed; ++i) {
        engine.Tick(OneSixtieth);
        const auto* enemy = engine.GetEncounter().FindEnemy(id);
        slowed = enemy && enemy->statuses.Has("status_slow_basic");
    }

    EXPECT_TRUE(slowed);
}

TEST(GameEngineMageTests, SkeletonMagesFrostBolt_SlowsThePlayer) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::CasterEnemy, { 6.0f, 0.0f });

    bool slowed = false;
    for (int i = 0; i < 60 * 6 && !slowed; ++i) {
        engine.Tick(OneSixtieth);
        slowed = engine.GetPlayerStatuses().Has("status_slow_basic");
    }

    EXPECT_TRUE(slowed);
}

TEST(GameEngineMageTests, Hud_ShowsManaAndTheSpellBar) {
    GameEngine engine;
    InitializeMage(engine);
    engine.Tick(OneSixtieth);

    const auto& texts = engine.GetRendering().GetQueuedText();
    EXPECT_TRUE(std::ranges::any_of(texts, [](const auto& t) { return t.text.starts_with("MP "); }));
    EXPECT_TRUE(std::ranges::any_of(texts, [](const auto& t) { return t.text == "Arcane"; }));   // "Arcane Bolt" on two lines
    EXPECT_TRUE(std::ranges::any_of(texts, [](const auto& t) { return t.text == "L5"; }));       // Fireball and Frost Nova, locked
}

TEST(GameEngineMageTests, SaveAndLoad_KeepTheClass) {
    TempSaveDir dir;
    {
        GameEngine mage;
        ASSERT_TRUE(mage.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
            .spawnEnemies = false, .randomSeed = CombatSeed, .playerClass = LB::Character::CharacterClass::Mage }));
        ASSERT_TRUE(mage.SaveGame(1));
    }
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .spawnEnemies = false, .randomSeed = CombatSeed }));
    ASSERT_EQ(engine.GetPlayerClass(), LB::Character::CharacterClass::Warrior);

    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_EQ(engine.GetPlayerClass(), LB::Character::CharacterClass::Mage);
    EXPECT_EQ(engine.GetPlayerWeapon()->category, LB::Combat::WeaponType::Wand);
    EXPECT_EQ(engine.GetSpellBar().size(), 4u);
}
