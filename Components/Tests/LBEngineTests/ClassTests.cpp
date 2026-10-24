// ClassTests.cpp - LB.Engine tests: the Rogue (sneak attacks, evasion, shadowstep) and the Cleric
// (smite, healing, cures, turn undead), and choosing them on New Game
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    void InitializeAs(GameEngine& engine, LB::Character::CharacterClass characterClass) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false, .randomSeed = CombatSeed, .playerClass = characterClass }))
            << engine.GetInitializationError();
    }

    void Cast(GameEngine& engine, char key) {
        TapKey(engine, Keys::FromChar(key));
        engine.Tick(OneSixtieth);
    }
}

// ============================================================================
// The Rogue
// ============================================================================

TEST(RogueTests, Kit_DaggerBowAndFourAbilities) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Rogue);

    EXPECT_EQ(engine.GetPlayerWeapon()->category, LB::Combat::WeaponType::Dagger);
    EXPECT_EQ(engine.CountItem(GameEngine::StartingBow), 1);
    ASSERT_EQ(engine.GetSpellBar().size(), 4u);   // and Fan of Knives (3), Smoke Bomb (5)
    EXPECT_TRUE(engine.GetSpellBar()[0]->IsDash());
    EXPECT_TRUE(engine.GetSpellBar()[1]->IsShadowstep());
}

TEST(RogueTests, ShadowstepThenABlow_IsASneakAttack) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Rogue);
    engine.SpawnEnemy(GameEngine::LightEnemy, { 5.0f, 0.0f });

    Cast(engine, '2');
    RunSteps(engine, static_cast<int>(engine.GetSpellBar()[1]->castTimeSeconds * 60.0f) + 1);
    // Swing through the sneak window: a miss does not spend it
    engine.OnKeyDown(Keys::Space);
    bool sneak = false;
    for (int i = 0; i < static_cast<int>(GameEngine::SneakWindowSeconds * 60.0f) && !sneak; ++i) {
        engine.Tick(OneSixtieth);
        for (const auto& event : engine.GetCombatEvents()) sneak = sneak || event.result.extraDamage > 0;
    }
    engine.OnKeyUp(Keys::Space);

    EXPECT_TRUE(sneak);
    EXPECT_TRUE(std::ranges::any_of(engine.GetFloatingTexts(), [](const auto& t) { return t.text == "SNEAK ATTACK"; }));
}

TEST(RogueTests, Evasion_RollsAndDodges) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Rogue);
    const auto start = engine.GetPlayerPosition();

    Cast(engine, '1');

    const auto& evasion = *engine.GetSpellBar()[0];
    EXPECT_NEAR(Encounter::Distance(engine.GetPlayerPosition(), start), evasion.dashTiles, 0.1f);
    EXPECT_GT(engine.GetSpellCooldown(0), 0.0f);
}

TEST(RogueTests, Shadowstep_AppearsBehindTheTarget) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Rogue);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 5.0f, 0.0f });

    Cast(engine, '2');
    RunSteps(engine, 15);

    const auto* enemy = engine.GetEncounter().FindEnemy(id);
    EXPECT_LT(Encounter::Distance(engine.GetPlayerPosition(), enemy->position), 1.2f);
}

TEST(RogueTests, Shadowstep_NeedsATarget) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Rogue);

    Cast(engine, '2');

    EXPECT_FALSE(engine.IsPlayerCasting());
    EXPECT_TRUE(std::ranges::any_of(engine.GetFloatingTexts(), [](const auto& t) { return t.text == "NO TARGET"; }));
}

// ============================================================================
// The Cleric
// ============================================================================

TEST(ClericTests, Kit_MaceShieldAndHolySpells) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Cleric);

    EXPECT_EQ(engine.GetPlayerWeapon()->category, LB::Combat::WeaponType::Mace);
    EXPECT_EQ(engine.CountItem("armor_shield_wooden_shield_t1"), 1);
    ASSERT_EQ(engine.GetSpellBar().size(), 4u);
    EXPECT_TRUE(engine.GetSpellBar()[0]->IsTargetedStrike());   // Smite
    EXPECT_TRUE(engine.GetSpellBar()[1]->IsHeal());             // Divine Light
}

TEST(ClericTests, Smite_StrikesTheTargetAtOnce) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Cleric);
    const auto id = *engine.SpawnEnemy(GameEngine::HeavyEnemy, { 4.0f, 0.0f });
    const auto before = engine.GetEncounter().FindEnemy(id)->hitPoints;

    Cast(engine, '1');
    RunSteps(engine, static_cast<int>(engine.GetSpellBar()[0]->castTimeSeconds * 60.0f) + 1);

    EXPECT_TRUE(engine.GetEncounter().GetProjectiles().empty());   // no missile: it strikes
    EXPECT_LT(engine.GetEncounter().FindEnemy(id)->hitPoints, before);
}

TEST(ClericTests, DivineLight_HealsTheCaster) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Cleric);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 0.9f, 0.0f });
    for (int i = 0; i < 60 * 10 && PlayerHitPoints(engine) >= PlayerMaxHitPoints(engine); ++i) engine.Tick(OneSixtieth);
    ASSERT_LT(PlayerHitPoints(engine), PlayerMaxHitPoints(engine));
    ASSERT_TRUE(engine.IsPlayerAlive());
    const auto hurt = PlayerHitPoints(engine);

    Cast(engine, '2');
    RunSteps(engine, static_cast<int>(engine.GetSpellBar()[1]->castTimeSeconds * 60.0f) + 1);

    EXPECT_GT(PlayerHitPoints(engine), hurt - 3);   // healed (the warrior may land a blow meanwhile)
    EXPECT_TRUE(std::ranges::any_of(engine.GetFloatingTexts(), [](const auto& t) { return t.text.starts_with("+"); }));
}

TEST(ClericTests, HealingCircle_LiftsAHindrance) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Cleric);
    engine.GrantExperience(400);   // level 3: the circle opens
    engine.Tick(OneSixtieth);
    engine.SpawnEnemy(GameEngine::CasterEnemy, { 6.0f, 0.0f });
    for (int i = 0; i < 60 * 6 && !engine.GetPlayerStatuses().Has("status_slow_basic"); ++i) engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetPlayerStatuses().Has("status_slow_basic"));

    Cast(engine, '3');
    RunSteps(engine, static_cast<int>(engine.GetSpellBar()[2]->castTimeSeconds * 60.0f) + 1);

    EXPECT_FALSE(engine.GetPlayerStatuses().Has("status_slow_basic"));
}

// ============================================================================
// Choosing them
// ============================================================================

TEST(ClassChoiceTests, AllFourClasses_AreOnTheNewGamePage) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .titleScreen = true, .randomSeed = 7u }));
    engine.Tick(OneSixtieth);
    TapKey(engine, Keys::Enter);   // New Game
    engine.Tick(OneSixtieth);

    std::vector<std::string> classes;
    for (const auto& item : engine.GetMenu().Items()) classes.push_back(item.text);
    EXPECT_EQ(classes, (std::vector<std::string>{ "Warrior", "Mage", "Rogue", "Cleric", "Necromancer", "Back" }));
}

// Progression by the data: the engine's ProgressionService has each class's spells by level
TEST(ClassTests, Progression_OpensTheClassSpellsByLevel) {
    GameEngine engine;
    InitializeMage(engine);
    const auto& progression = engine.GetProgression();
    EXPECT_EQ(progression.GetExperienceForLevel(5), engine.GetCharacters().GetExperienceForLevel(5));   // one curve
    const auto opened = progression.GetAbilitiesAtLevel(LB::Character::CharacterClass::Mage, engine.GetContent()->FindSpell("spell_utility_blink_l2")->requiredLevel);
    EXPECT_TRUE(std::ranges::find(opened, std::string("Blink")) != opened.end());
    EXPECT_TRUE(progression.GetAbilitiesForLevelRange(LB::Character::CharacterClass::Mage, 1, 20).size() >= engine.Spellbook().size());
}

// The Necromancer (design: the Shadow school's caster)
namespace {
    void InitializeNecromancer(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
            .randomSeed = CombatSeed, .playerClass = LB::Character::CharacterClass::Necromancer })) << engine.GetInitializationError();
    }
}

TEST(NecromancerTests, StartsWithTheShadowBarAndAWand) {
    GameEngine engine;
    InitializeNecromancer(engine);
    const auto bar = engine.GetSpellBar();
    ASSERT_EQ(bar.size(), 4u);
    EXPECT_EQ(bar[0]->key, "spell_shadow_bone_shard_l1");
    EXPECT_EQ(bar[1]->key, "spell_shadow_life_drain_l2");
    EXPECT_EQ(bar[3]->key, "spell_shadow_devour_essence_l4");   // locked until level 13
    EXPECT_TRUE(engine.GetPlayerWeapon()->IsRanged());
    // Mana: the table's and INT's modifier (classes.json manaAttribute)
    const auto& hero = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(hero.GetMaxManaPoints(), engine.GetContent()->FindClass(LB::Character::CharacterClass::Necromancer)->progression.TotalsAt(1).maxManaPoints
                                       + hero.GetAttributes().GetIntelligenceModifier());
    EXPECT_GT(engine.GetPlayerSpellPower(), 0);
}

TEST(NecromancerTests, AWeakeningCurse_LandsOnItsTarget) {
    GameEngine engine;
    InitializeNecromancer(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::HeavyEnemy, { 4.0f, 0.0f });
    ASSERT_TRUE(engine.SetSpell(1, "spell_shadow_weakening_curse_l1"));   // from the spellbook
    TapKey(engine, Keys::FromChar('2'));
    RunSteps(engine, 90);
    ASSERT_NE(engine.GetEncounter().FindEnemy(id), nullptr);
    EXPECT_TRUE(engine.GetEncounter().FindEnemy(id)->statuses.Has("status_weakened"));

    StatusSet weakened;
    weakened.Apply(*engine.GetContent()->FindStatus("status_weakened"));
    EXPECT_EQ(weakened.Dealt(10), 9);   // a tenth less
    EXPECT_EQ(weakened.Dealt(1), 1);
}

TEST(NecromancerTests, LifeDrain_HealsTheCaster) {
    GameEngine engine;
    InitializeNecromancer(engine);
    engine.GrantExperience(900);   // level 4: Life Drain
    engine.Tick(OneSixtieth);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 3.0f, 0.0f });
    engine.DamagePlayer(8);
    const auto hurt = PlayerHitPoints(engine);
    TapKey(engine, Keys::FromChar('2'));   // Life Drain
    for (int i = 0; i < 120 && PlayerHitPoints(engine) <= hurt; ++i) engine.Tick(OneSixtieth);
    EXPECT_GT(PlayerHitPoints(engine), hurt);
}

// A curse of frailty: its holder takes a fifth more from each blow
TEST(NecromancerTests, Frailty_RaisesTheDamageTaken) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.SetContent(content.engine.GetContent().get());
    const auto plain = encounter.Spawn(*content.warrior, { 2.0f, 0.0f }, FacingWest, false);
    const auto frail = encounter.Spawn(*content.warrior, { -2.0f, 0.0f }, FacingWest, false);
    encounter.ApplyStatus(frail, "status_frail");
    const auto* bolt = content.engine.GetContent()->FindSpell("spell_evocation_arcane_bolt_l1");
    const auto a = encounter.StrikeWithSpell(SpellPayload{ bolt, 20 }, plain);
    const auto b = encounter.StrikeWithSpell(SpellPayload{ bolt, 20 }, frail);
    ASSERT_TRUE(a && b);
    EXPECT_EQ(b->result.finalDamage, static_cast<std::int32_t>(std::lround(a->result.finalDamage * 1.2f)));
}
