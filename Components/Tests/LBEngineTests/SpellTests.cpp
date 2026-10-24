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
    EXPECT_EQ(PlayerHitPoints(engine), 10);   // level 1 hpGain 10, CON 10 (no modifier)
}

TEST(GameEngineMageTests, Warrior_HasStaminaAbilities) {
    GameEngine engine;
    InitializeForCombat(engine);

    ASSERT_EQ(engine.GetSpellBar().size(), 3u);   // Cleave, Shield Bash (3), Whirlwind (5)
    EXPECT_EQ(engine.GetSpellBar()[0]->name, "Cleave");
    EXPECT_EQ(engine.SpellBlocker(0), "");
    EXPECT_EQ(engine.SpellBlocker(1), "level 3");
    EXPECT_EQ(engine.SpellBlocker(3), "empty");
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

// Armour slows a cast (design-armour-system.md 4.3): a cleric's breastplate, castPenalty 0.05
TEST(GameEngineMageTests, Armour_SlowsTheCast) {
    const auto stepsToCast = [](bool breastplate) {
        GameEngine engine;
        EXPECT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
            .randomSeed = CombatSeed, .playerClass = LB::Character::CharacterClass::Cleric }));
        engine.GrantExperience(1600);   // level 5, the breastplate's
        engine.Tick(OneSixtieth);
        if (breastplate) {
            EXPECT_EQ(engine.GiveItem("armor_chest_rusted_breastplate_t1"), 1);
            EXPECT_TRUE(engine.EquipItem("armor_chest_rusted_breastplate_t1"));
        }
        TapKey(engine, Keys::FromChar('2'));   // Divine Light, 0.8 seconds
        int steps = 0;
        for (engine.Tick(OneSixtieth); engine.IsPlayerCasting() && steps < 600; ++steps) engine.Tick(OneSixtieth);
        return steps;
    };
    const auto bare = stepsToCast(false);
    const auto armoured = stepsToCast(true);
    EXPECT_NEAR(bare, 48, 1);
    EXPECT_NEAR(armoured, 50, 1);   // 5% longer
    EXPECT_GT(armoured, bare);
}

// A heavy blow breaks a cast for free (design-spells-system.md 4.3); a light one does not
TEST(GameEngineMageTests, AHeavyBlow_InterruptsTheCast) {
    GameEngine engine;
    InitializeMage(engine);
    const auto full = PlayerMana(engine);
    const auto most = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetMaxHitPoints();

    TapKey(engine, Keys::FromChar('1'));
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.IsPlayerCasting());
    if (static_cast<float>(most) * GameEngine::CastInterruptFraction > 1.0f) {
        engine.DamagePlayer(0);
        EXPECT_TRUE(engine.IsPlayerCasting());
    }
    engine.DamagePlayer(static_cast<std::int32_t>(std::ceil(static_cast<float>(most) * GameEngine::CastInterruptFraction)));

    EXPECT_FALSE(engine.IsPlayerCasting());
    RunSteps(engine, 60);
    EXPECT_TRUE(engine.GetEncounter().GetProjectiles().empty());
    EXPECT_EQ(PlayerMana(engine), full);
}

// A critical cast: its hits are critical, its damage multiplied once (design-spells-system.md 5.1)
TEST(SpellEncounterTests, ACriticalCast_HitsCritically) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.SetContent(content.engine.GetContent().get());
    const auto id = encounter.Spawn(*content.warrior, { 2.0f, 0.0f }, FacingWest, false);
    const auto* bolt = content.engine.GetContent()->FindSpell("spell_evocation_arcane_bolt_l1");
    ASSERT_TRUE(bolt->canCrit);

    const auto plain = encounter.StrikeWithSpell(SpellPayload{ bolt, 6 }, id);
    const auto critical = encounter.StrikeWithSpell(SpellPayload{ bolt, 9, true }, id);
    ASSERT_TRUE(plain && critical);
    EXPECT_FALSE(plain->result.critical);
    EXPECT_TRUE(critical->result.critical);
    EXPECT_EQ(critical->result.rawDamage, 9);
}

TEST(GameEngineMageTests, NotEnoughMana_SaysSoAndDoesNotCast) {
    GameEngine engine;
    InitializeMage(engine);
    const auto& bolt = *engine.GetSpellBar()[0];
    for (int cast = 0; cast < 2; ++cast) {           // two bolts spend most of a new mage's mana
        TapKey(engine, Keys::FromChar('1'));
        engine.Tick(OneSixtieth);
        RunSteps(engine, static_cast<int>(bolt.castTimeSeconds * 60.0f));
    }
    ASSERT_LT(PlayerMana(engine), bolt.manaCost);

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

// The spellbook: the class's castable spells, any of them on the bar; saved
TEST(SpellbookTests, TheBar_TakesAnySpellOfTheClass) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .spawnEnemies = false, .randomSeed = CombatSeed, .playerClass = LB::Character::CharacterClass::Mage }));
    const auto book = engine.Spellbook();
    const auto has = [&](std::string_view key) { return std::ranges::any_of(book, [&](const auto* spell) { return spell->key == key; }); };
    EXPECT_TRUE(has("spell_utility_blink_l2"));
    EXPECT_TRUE(has("spell_frost_ice_wall_l2"));
    EXPECT_FALSE(has("spell_holy_bless_l2"));   // the Cleric's
    for (const auto* spell : book) EXPECT_TRUE(spell->requiredClass.has_value()) << spell->key;   // the Shadow spells are reserved

    const auto before = engine.GetSpellBar()[0]->key;
    ASSERT_TRUE(engine.CycleSpell(0));
    const auto bar = engine.GetSpellBar();
    EXPECT_NE(bar[0]->key, before);
    for (std::size_t i = 0; i < bar.size(); ++i) {
        for (std::size_t j = i + 1; j < bar.size(); ++j) EXPECT_NE(bar[i], bar[j]);
    }
    EXPECT_FALSE(engine.SetSpell(1, "spell_holy_bless_l2"));
    EXPECT_FALSE(engine.SetSpell(1, bar[0]->key));   // on the bar already
    ASSERT_TRUE(engine.SetSpell(3, "spell_utility_blink_l2"));
    EXPECT_EQ(engine.GetSpellBar()[3]->key, "spell_utility_blink_l2");

    ASSERT_TRUE(engine.SaveGame(GameEngine::SaveSlots[0]));
    GameEngine loaded;
    ASSERT_TRUE(loaded.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .spawnEnemies = false, .randomSeed = CombatSeed, .playerClass = LB::Character::CharacterClass::Mage }));
    ASSERT_TRUE(loaded.LoadGame(GameEngine::SaveSlots[0]));
    ASSERT_EQ(loaded.GetSpellBar().size(), engine.GetSpellBar().size());
    for (std::size_t i = 0; i < loaded.GetSpellBar().size(); ++i) EXPECT_EQ(loaded.GetSpellBar()[i]->key, engine.GetSpellBar()[i]->key);
}

// Blink: the hero appears a few tiles ahead at once
TEST(SpellbookTests, Blink_CarriesTheHeroAhead) {
    GameEngine engine;
    InitializeMage(engine);
    engine.GrantExperience(2500);   // level 6
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.SetSpell(0, "spell_utility_blink_l2"));
    HoldKeysFor(engine, { Keys::FromChar('D') }, 3);   // face screen-right
    const auto start = engine.GetPlayerPosition();
    TapKey(engine, Keys::FromChar('1'));
    RunSteps(engine, 3);
    EXPECT_GT(Encounter::Distance(engine.GetPlayerPosition(), start), 3.0f);
    EXPECT_LE(Encounter::Distance(engine.GetPlayerPosition(), start), 5.5f);
}

// Bless: to hit while it lasts; a ward takes a fifth of each blow
TEST(SpellbookTests, BlessAndSanctuary_HelpTheHero) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
        .randomSeed = CombatSeed, .playerClass = LB::Character::CharacterClass::Cleric }));
    engine.GrantExperience(2500);   // level 6
    engine.Tick(OneSixtieth);
    const auto plain = engine.GetPlayerAttackProfile().attackBonus;
    ASSERT_TRUE(engine.SetSpell(0, "spell_holy_bless_l2"));
    TapKey(engine, Keys::FromChar('1'));
    RunSteps(engine, 70);   // a second's cast
    EXPECT_TRUE(engine.GetPlayerStatuses().Has("status_blessed"));
    EXPECT_EQ(engine.GetPlayerAttackProfile().attackBonus, plain + engine.GetContent()->FindStatus("status_blessed")->attackBonus);

    StatusSet ward;
    ward.Apply(*engine.GetContent()->FindStatus("status_sanctuary"));
    EXPECT_EQ(ward.Taken(10), 8);
    EXPECT_EQ(ward.Taken(1), 1);   // a blow that hurt still hurts
}

// Ice Wall: cells ahead shut for its duration, then open again
TEST(SpellbookTests, IceWall_ShutsTheWayAheadForAWhile) {
    GameEngine engine;
    InitializeMage(engine);
    engine.GrantExperience(2500);   // level 6
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.SetSpell(0, "spell_frost_ice_wall_l2"));
    HoldKeysFor(engine, { Keys::FromChar('D') }, 3);
    TapKey(engine, Keys::FromChar('1'));
    RunSteps(engine, 80);   // its cast
    ASSERT_EQ(engine.GetIceWalls().size(), 1u);
    const auto cells = engine.GetIceWalls().front().cells;
    ASSERT_FALSE(cells.empty());
    for (const auto& [x, y] : cells) EXPECT_TRUE(engine.GetWorld().IsBlocked(x, y));
    RunSteps(engine, static_cast<int>(60 * engine.GetContent()->FindSpell("spell_frost_ice_wall_l2")->durationSeconds) + 5);
    EXPECT_TRUE(engine.GetIceWalls().empty());
    for (const auto& [x, y] : cells) EXPECT_FALSE(engine.GetWorld().IsBlocked(x, y));
}
