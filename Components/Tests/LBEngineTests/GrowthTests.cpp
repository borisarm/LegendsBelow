// GrowthTests.cpp - LB.Engine tests: abilities opening with levels (the Warrior's stamina
// abilities, the Rogue's), and the path chosen at level 5
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    void InitializeAs(GameEngine& engine, LB::Character::CharacterClass characterClass) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false, .randomSeed = CombatSeed, .playerClass = characterClass }))
            << engine.GetInitializationError();
    }

    // Experience until a level (as kills would award it)
    void ReachLevel(GameEngine& engine, std::int32_t level) {
        for (int i = 0; i < 200 && PlayerLevel(engine) < level; ++i) engine.GrantExperience(50);
        engine.Tick(OneSixtieth);
    }

    bool Hurt(const GameEngine& engine, std::uint32_t id) {
        const auto* enemy = engine.GetEncounter().FindEnemy(id);
        return !enemy || enemy->IsDead() || enemy->hitPoints < enemy->definition->hitPoints;
    }
}

TEST(GrowthTests, Warrior_CleaveHitsEveryoneAround) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    const auto left = *engine.SpawnEnemy(GameEngine::LightEnemy, { -1.0f, 0.0f });
    const auto right = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });
    const auto stamina = PlayerMana(engine);

    TapKey(engine, Keys::FromChar('1'));
    StepUntil(engine, [&] { return Hurt(engine, left) && Hurt(engine, right); }, 2.0f);

    EXPECT_TRUE(Hurt(engine, left));
    EXPECT_TRUE(Hurt(engine, right));
    EXPECT_LT(PlayerMana(engine), stamina);   // paid in stamina
}

TEST(GrowthTests, Warrior_ShieldBashOpensAtLevelThreeAndStuns) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    EXPECT_EQ(engine.SpellBlocker(1), "level 3");

    ReachLevel(engine, 3);
    ASSERT_EQ(PlayerLevel(engine), 3);
    EXPECT_EQ(engine.SpellBlocker(1), "");
    const auto id = *engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.2f, 0.0f });
    engine.Tick(OneSixtieth);

    TapKey(engine, Keys::FromChar('2'));
    StepUntil(engine, [&] { return Hurt(engine, id); }, 2.0f);
    const auto* enemy = engine.GetEncounter().FindEnemy(id);
    ASSERT_NE(enemy, nullptr);
    EXPECT_TRUE(enemy->statuses.Has("status_stun_basic"));
}

TEST(GrowthTests, LevelUp_AnnouncesTheNewAbility) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Rogue);
    ReachLevel(engine, 3);

    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "NEW: Fan of Knives"; }));
}

TEST(GrowthTests, Path_ChosenAtLevelFiveAddsItsBonuses) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    EXPECT_FALSE(engine.ChoosePath("path_warrior_guardian"));   // too early

    ReachLevel(engine, 5);
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "A PATH TO CHOOSE (I)"; }));
    const auto armorClass = engine.GetPlayerDefense().armorClass;
    EXPECT_FALSE(engine.ChoosePath("path_mage_archmage"));      // another class's
    ASSERT_TRUE(engine.ChoosePath("path_warrior_guardian"));

    ASSERT_NE(engine.GetPlayerPath(), nullptr);
    EXPECT_EQ(engine.GetPlayerPath()->name, "Guardian");
    EXPECT_EQ(engine.GetPlayerDefense().armorClass, armorClass + 1);
    EXPECT_FALSE(engine.ChoosePath("path_warrior_berserker"));  // one path a hero
}

TEST(GrowthTests, Path_ChosenOnTheCharacterScreen) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Mage);
    ReachLevel(engine, 5);
    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetCharacterScreen().IsOpen());

    const auto& rows = engine.GetCharacterScreen().GetRows();
    const auto choice = std::ranges::find_if(rows, [](const auto& row) { return row.action == ScreenAction::ChoosePath; });
    ASSERT_NE(choice, rows.end());
    const auto index = static_cast<std::size_t>(choice - rows.begin());
    for (int i = 0; i < 100 && engine.GetCharacterScreen().GetCursor() != index; ++i) {
        TapKey(engine, Keys::Down);
        engine.Tick(OneSixtieth);
    }
    TapKey(engine, Keys::Enter);
    engine.Tick(OneSixtieth);

    ASSERT_NE(engine.GetPlayerPath(), nullptr);
    EXPECT_EQ(engine.GetPlayerPath()->name, "Pyromancer");
}

TEST(GrowthTests, Path_KeptBySaves) {
    TempSaveDir dir;
    const EngineOptions options{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
        .randomSeed = CombatSeed, .playerClass = LB::Character::CharacterClass::Rogue };
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
        ReachLevel(engine, 5);
        ASSERT_TRUE(engine.ChoosePath("path_rogue_shadow"));
        ASSERT_TRUE(engine.SaveGame(1));
    }
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, options));
    ASSERT_TRUE(engine.LoadGame(1));

    ASSERT_NE(engine.GetPlayerPath(), nullptr);
    EXPECT_EQ(engine.GetPlayerPath()->key, "path_rogue_shadow");
}
