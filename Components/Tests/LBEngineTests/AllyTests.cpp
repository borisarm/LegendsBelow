// AllyTests.cpp - LB.Engine tests: companions hired from the merchant, fighting beside the hero,
// targeted by enemies, going down and getting up, healed by the Cleric, and kept in saves; the
// Acolyte's prayers, the party's orders (hold, attack), their gear, and their statuses
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
    EXPECT_FALSE(engine.HireAlly(Barbarian).has_value());        // one of each
    EXPECT_TRUE(engine.HireAlly("ally_ranger").has_value());
    EXPECT_FALSE(engine.HireAlly("ally_acolyte").has_value());   // the party is full (two)
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
        engine.GiveItem("armor_chest_leather_jerkin_t1");
        ASSERT_TRUE(engine.GiveToAlly(id, engine.MakeItem("armor_chest_leather_jerkin_t1")));
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
    EXPECT_EQ(engine.AllyItems(OnlyAlly(engine).id).second, engine.MakeItem("armor_chest_leather_jerkin_t1"));   // and what they carry
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
    EXPECT_FALSE(party.Add(definition, { 0.0f, 0.0f }, 1).has_value());   // one of each

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

TEST(AllyTests, TheAcolyte_MendsTheWorstOff) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    const auto barbarian = *engine.HireAlly(Barbarian);
    ASSERT_TRUE(engine.HireAlly("ally_acolyte").has_value());
    const auto& party = engine.GetAllies();
    engine.DamageAlly(barbarian, party.Find(barbarian)->maxHitPoints * 2 / 3);
    const auto before = party.Find(barbarian)->hitPoints;

    RunSteps(engine, 30);

    EXPECT_GT(party.Find(barbarian)->hitPoints, before);
    RunSteps(engine, 60);   // and not again before the prayer's cooldown
    const auto once = party.Find(barbarian)->hitPoints;
    RunSteps(engine, 120);
    EXPECT_EQ(party.Find(barbarian)->hitPoints, once);
}

TEST(AllyTests, Hold_KeepsThePartyWhereTheHeroStood) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    ASSERT_TRUE(engine.HireAlly(Barbarian).has_value());
    const auto spot = engine.GetPlayerPosition();
    TapKey(engine, Keys::FromChar('G'));
    engine.Tick(OneSixtieth);
    ASSERT_EQ(engine.GetAllies().Stance(), AllyStance::Hold);

    WalkTo(engine, { spot.x - 4.0f, spot.y }, 60 * 4);
    RunSteps(engine, 60);
    EXPECT_LT(Encounter::Distance(OnlyAlly(engine).position, spot), 1.5f);   // stayed

    TapKey(engine, Keys::FromChar('G'));   // follow again
    RunSteps(engine, 120);
    EXPECT_EQ(engine.GetAllies().Stance(), AllyStance::Follow);
    EXPECT_LT(Encounter::Distance(OnlyAlly(engine).position, engine.GetPlayerPosition()), 2.5f);
}

TEST(AllyTests, Attack_SendsThePartyAfterTheNearestEnemy) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    ASSERT_TRUE(engine.HireAlly(Barbarian).has_value());
    const auto near = *engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    engine.SpawnEnemy(GameEngine::LightEnemy, { -5.0f, 0.0f });
    RunSteps(engine, 60);   // up from the ground

    TapKey(engine, Keys::FromChar('T'));
    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetAllies().FocusTarget(), near);
    EXPECT_EQ(OnlyAlly(engine).target, near);
}

TEST(AllyTests, GearGiven_StrengthensThem_AndComesBack) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    const auto id = *engine.HireAlly(Barbarian);
    const auto damage = OnlyAlly(engine).Attack().damageBonus;
    const auto armour = OnlyAlly(engine).Defense().physicalArmor;
    ASSERT_EQ(engine.GiveItem("weapon_sword_rusted_shortsword_t1"), 1);
    ASSERT_EQ(engine.GiveItem("armor_chest_leather_jerkin_t1"), 1);
    const auto bag = engine.GetInventory().size();
    const auto sword = engine.MakeItem("weapon_sword_rusted_shortsword_t1");
    const auto jerkin = engine.MakeItem("armor_chest_leather_jerkin_t1");

    ASSERT_TRUE(engine.GiveToAlly(id, sword));
    ASSERT_TRUE(engine.GiveToAlly(id, jerkin));
    EXPECT_FALSE(engine.GiveToAlly(id, engine.MakeItem(GameEngine::HealingPotion)));   // not gear
    EXPECT_GE(OnlyAlly(engine).Attack().damageBonus, damage);
    EXPECT_GT(OnlyAlly(engine).Defense().physicalArmor, armour);
    EXPECT_EQ(engine.GetInventory().size(), bag - 2);
    EXPECT_EQ(engine.AllyItems(id).first, sword);

    ASSERT_TRUE(engine.TakeFromAlly(id));
    EXPECT_EQ(engine.GetInventory().size(), bag);
    EXPECT_EQ(OnlyAlly(engine).Defense().physicalArmor, armour);
}

TEST(AllyPartyTests, Statuses_SlowThemAndClearWhenDown) {
    LB::Content::AllyDefinition definition{};
    definition.key = "ally_test";
    definition.hitPoints = 20;
    LB::Content::StatusDefinition slow{};
    slow.key = "status_test_slow";
    slow.durationSeconds = 5.0f;
    slow.movementMultiplier = 0.5f;
    AllyParty party;
    const auto id = *party.Add(definition, { 0.0f, 0.0f }, 1);

    party.ApplyStatus(id, slow);
    EXPECT_FLOAT_EQ(party.Find(id)->statuses.MovementMultiplier(), 0.5f);
    party.Damage(id, 100);
    EXPECT_FLOAT_EQ(party.Find(id)->statuses.MovementMultiplier(), 1.0f);
}

// The raised dead (AllyParty::Raise) stand apart from the companions: two at most, the oldest
// crumbling to make room, and the companions' places left free
TEST(AllyPartyTests, Raised_ApartFromTheCompanions) {
    LB::Content::AllyDefinition skeleton{};
    skeleton.key = "ally_test_skeleton";
    skeleton.hitPoints = 10;
    skeleton.summoned = true;
    LB::Content::AllyDefinition first{};
    first.key = "ally_test_first";
    LB::Content::AllyDefinition second{};
    second.key = "ally_test_second";
    AllyParty party;
    const auto a = party.Raise(skeleton, { 0.0f, 0.0f }, 1, 10.0f);
    party.Raise(skeleton, { 1.0f, 0.0f }, 1, 20.0f);   // more than one of each
    EXPECT_EQ(party.Allies().size(), 2u);
    EXPECT_EQ(party.Companions(), 0u);
    ASSERT_TRUE(party.Add(first, { 0.0f, 0.0f }, 1).has_value());
    ASSERT_TRUE(party.Add(second, { 0.0f, 0.0f }, 1).has_value());   // the raised take no companion's place
    EXPECT_EQ(party.Companions(), AllyParty::MaxAllies);

    party.Raise(skeleton, { 2.0f, 0.0f }, 1, 30.0f);   // a third: the oldest (least time left) gives way
    EXPECT_EQ(party.Allies().size(), 4u);
    EXPECT_EQ(party.Find(a), nullptr);
    EXPECT_EQ(party.TakeCrumbled().size(), 1u);
    EXPECT_TRUE(party.TakeCrumbled().empty());
}
