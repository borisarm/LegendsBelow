// AllyTests.cpp - LB.Engine tests: companions hired from the merchant, fighting beside the hero,
// targeted by enemies, going down and getting up, healed by the Cleric, and kept in saves
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    constexpr std::string_view Barbarian = "ally_barbarian";

    void InitializeAs(GameEngine& engine, LB::Character::CharacterClass characterClass) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false, .randomSeed = CombatSeed, .playerClass = characterClass }))
            << engine.GetInitializationError();
    }

    const Ally& OnlyAlly(const GameEngine& engine) {
        return engine.GetAllies().Allies().front();
    }
}

TEST(AllyTests, Hire_JoinsBesideTheHero_OneAtATime) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.GrantExperience(400);   // level 3
    engine.Tick(OneSixtieth);

    const auto id = engine.HireAlly(Barbarian);
    ASSERT_TRUE(id.has_value());
    const auto& ally = OnlyAlly(engine);
    EXPECT_LT(Encounter::Distance(ally.position, engine.GetPlayerPosition()), 2.0f);
    EXPECT_EQ(ally.level, PlayerLevel(engine));
    EXPECT_EQ(ally.maxHitPoints, ally.definition->hitPoints + ally.definition->hitPointsPerLevel * (PlayerLevel(engine) - 1));   // grown with the hero
    EXPECT_FALSE(engine.HireAlly("ally_ranger").has_value());   // the party is full
    EXPECT_FALSE(engine.HireAlly("ally_nobody").has_value());
}

TEST(AllyTests, AnAlly_FightsWhatIsAwakeNearTheHero) {
    GameEngine engine;
    InitializeForCombat(engine);
    ASSERT_TRUE(engine.HireAlly(Barbarian).has_value());
    const auto enemy = *engine.SpawnEnemy(GameEngine::LightEnemy, { 3.5f, 0.0f });

    const auto events = StepUntil(engine, [&] { return !engine.GetEncounter().FindEnemy(enemy) || engine.GetEncounter().FindEnemy(enemy)->IsDead(); }, 20.0f);

    const auto* left = engine.GetEncounter().FindEnemy(enemy);
    EXPECT_TRUE(!left || left->IsDead());   // the barbarian cut it down (the hero never swung)
    EXPECT_GT(CountKind(events, CombatEventKind::PlayerAttack), 0u);
    engine.Tick(OneSixtieth);
    EXPECT_GT(engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetExperience(), 0);   // the hero's experience
}

TEST(AllyTests, Enemies_GoAfterTheNearestOfTheParty) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.HireAlly(Barbarian);
    const auto at = OnlyAlly(engine).position;
    const auto hero = engine.GetPlayerPosition();
    // Beyond the ally, away from the hero
    const WorldPoint away{ at.x + (at.x - hero.x) * 1.5f, at.y + (at.y - hero.y) * 1.5f };
    engine.SpawnEnemy(GameEngine::HeavyEnemy, away);

    const auto events = StepUntil(engine, [&] { return false; }, 6.0f);

    EXPECT_TRUE(std::ranges::any_of(events, [&](const CombatEvent& event) { return event.kind == CombatEventKind::EnemyAttack && event.ally == id; }));
}

TEST(AllyTests, ADownedAlly_GetsUpOnceTheFightIsOver) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.HireAlly(Barbarian);

    EXPECT_TRUE(engine.DamageAlly(id, 1000));
    EXPECT_TRUE(OnlyAlly(engine).IsDown());
    RunSteps(engine, static_cast<int>(AllyParty::ReviveQuietSeconds * 60.0f) + 5);

    EXPECT_FALSE(OnlyAlly(engine).IsDown());
    EXPECT_EQ(OnlyAlly(engine).hitPoints, static_cast<std::int32_t>(std::lround(OnlyAlly(engine).maxHitPoints * AllyParty::ReviveFraction)));
}

TEST(AllyTests, DivineLight_GoesToWhoeverIsWorstOff) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Cleric);
    const auto id = *engine.HireAlly(Barbarian);
    engine.DamageAlly(id, OnlyAlly(engine).maxHitPoints / 2);
    const auto before = OnlyAlly(engine).hitPoints;

    TapKey(engine, Keys::FromChar('2'));   // Divine Light
    RunSteps(engine, 90);

    EXPECT_GT(OnlyAlly(engine).hitPoints, before);
}

TEST(AllyTests, HealingCircle_HealsEveryoneAround) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Cleric);
    engine.GrantExperience(400);   // level 3: the circle opens
    engine.Tick(OneSixtieth);
    ASSERT_EQ(engine.GetSpellBar()[2]->name, "Healing Circle");
    const auto id = *engine.HireAlly(Barbarian);
    engine.DamageAlly(id, 6);
    const auto before = OnlyAlly(engine).hitPoints;

    TapKey(engine, Keys::FromChar('3'));
    RunSteps(engine, 90);

    EXPECT_GT(OnlyAlly(engine).hitPoints, before);
}

TEST(AllyTests, Allies_FollowTheHeroDown_AndAreKeptInSaves) {
    TempSaveDir dir;
    const EngineOptions options{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
        .randomSeed = CombatSeed, .dungeon = true };
    std::int32_t hitPoints = 0;
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
        const auto id = *engine.HireAlly("ally_ranger");
        engine.DamageAlly(id, 3);
        engine.EnterFloor(2);
        EXPECT_LT(Encounter::Distance(OnlyAlly(engine).position, engine.GetPlayerPosition()), 2.0f);   // came along
        hitPoints = OnlyAlly(engine).hitPoints;
        ASSERT_TRUE(engine.SaveGame(1));
    }
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
    ASSERT_TRUE(engine.LoadGame(1));
    ASSERT_EQ(engine.GetAllies().Allies().size(), 1u);
    EXPECT_EQ(OnlyAlly(engine).definition->key, "ally_ranger");
    EXPECT_EQ(OnlyAlly(engine).hitPoints, hitPoints);
}

TEST(AllyTests, TheMerchant_HiresThemForGold) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = CombatSeed, .dungeon = true }))
        << engine.GetInitializationError();
    engine.EnterFloor(4);
    engine.Tick(OneSixtieth);
    const auto& stock = engine.GetMerchantStock();
    const auto offer = std::ranges::find_if(stock, [](const auto& entry) { return entry.ally != nullptr; });
    ASSERT_NE(offer, stock.end());
    const auto index = static_cast<std::size_t>(offer - stock.begin());
    const auto price = offer->price;
    for (int i = 0; i < 40 && engine.GetGold() < price; ++i) {
        engine.DropLoot("loot_necromancer", engine.GetPlayerPosition());
        RunSteps(engine, 2);
    }
    ASSERT_GE(engine.GetGold(), price);
    WalkTo(engine, *engine.GetMerchantPosition(), 60 * 8);
    const auto gold = engine.GetGold();

    ASSERT_TRUE(engine.BuyFromMerchant(index));
    EXPECT_EQ(engine.GetGold(), gold - price);
    EXPECT_EQ(engine.GetAllies().Allies().size(), 1u);
}

TEST(AllyPartyTests, Damage_Heal_AndTheCap) {
    LB::Content::AllyDefinition definition{};
    definition.key = "ally_test";
    definition.hitPoints = 20;
    definition.hitPointsPerLevel = 5;
    AllyParty party;
    const auto id = *party.Add(definition, { 0.0f, 0.0f }, 3);
    EXPECT_EQ(party.Find(id)->maxHitPoints, 30);
    EXPECT_FALSE(party.Add(definition, { 0.0f, 0.0f }, 1).has_value());   // MaxAllies

    EXPECT_FALSE(party.Damage(id, 12));
    EXPECT_EQ(party.Heal(id, 100), 12);   // up to the most
    party.SetLevel(5);
    EXPECT_EQ(party.Find(id)->maxHitPoints, 40);
    EXPECT_EQ(party.Find(id)->hitPoints, 40);   // the hit points gained come healed
    EXPECT_TRUE(party.Damage(id, 40));
    EXPECT_EQ(party.Heal(id, 10), 0);           // not while down
    party.RestoreAll();
    EXPECT_EQ(party.Find(id)->hitPoints, 40);
}
