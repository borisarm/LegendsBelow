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

TEST(GrowthTests, LevelUp_ABannerWithWhatWasGained) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Rogue);
    engine.GrantExperience(400);   // two levels in one award: one banner
    engine.Tick(OneSixtieth);
    ASSERT_EQ(PlayerLevel(engine), 3);

    const auto* card = engine.GetFanfare().Current();
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->title, "LEVEL 3");
    EXPECT_EQ(card->subtitle, "Rogue");
    EXPECT_TRUE(std::ranges::any_of(card->lines, [](const std::string& line) { return line.find("hit points") != std::string::npos; }));
    EXPECT_NE(std::ranges::find(card->lines, "New: Fan of Knives"), card->lines.end());
    RunSteps(engine, 120);   // drawn across the screen, its lines appearing one after another
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "LEVEL 3"; }));
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "New: Fan of Knives"; }));

    RunSteps(engine, static_cast<int>((Fanfare::InSeconds + Fanfare::HoldSeconds + Fanfare::OutSeconds) * 60.0f));
    EXPECT_EQ(engine.GetFanfare().Current(), nullptr);   // gone again
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
    TapKey(engine, Keys::Enter);   // the cards
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetPathChoice().IsOpen());
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "CHOOSE YOUR PATH"; }));
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "+3 spell power"; }));
    TapKey(engine, Keys::Right);   // the second card
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.GetPathChoice().Cursor(), 1u);
    TapKey(engine, Keys::Left);
    engine.Tick(OneSixtieth);
    TapKey(engine, Keys::Enter);   // take it
    engine.Tick(OneSixtieth);

    ASSERT_NE(engine.GetPlayerPath(), nullptr);
    EXPECT_EQ(engine.GetPlayerPath()->name, "Pyromancer");
    EXPECT_FALSE(engine.GetPathChoice().IsOpen());
    EXPECT_FALSE(engine.GetCharacterScreen().IsOpen());   // back to the game for the banner
    // The levels' banners first (one each), then the path's
    const auto* card = engine.GetFanfare().Current();
    for (int i = 0; i < 60 * 30 && card && card->title != "PATH OF THE PYROMANCER"; ++i) {
        engine.Tick(OneSixtieth);
        card = engine.GetFanfare().Current();
    }
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->title, "PATH OF THE PYROMANCER");
    EXPECT_NE(std::ranges::find(card->lines, "+3 spell power"), card->lines.end());
}

TEST(GrowthTests, PathCards_BackOutWithoutChoosing) {
    GameEngine engine;
    InitializeAs(engine, LB::Character::CharacterClass::Warrior);
    ReachLevel(engine, 5);
    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);
    const auto& rows = engine.GetCharacterScreen().GetRows();
    const auto index = static_cast<std::size_t>(std::ranges::find_if(rows, [](const auto& row) { return row.action == ScreenAction::ChoosePath; }) - rows.begin());
    for (int i = 0; i < 100 && engine.GetCharacterScreen().GetCursor() != index; ++i) {
        TapKey(engine, Keys::Down);
        engine.Tick(OneSixtieth);
    }
    TapKey(engine, Keys::Enter);
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetPathChoice().IsOpen());

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_FALSE(engine.GetPathChoice().IsOpen());
    EXPECT_TRUE(engine.GetCharacterScreen().IsOpen());   // back where the cards were opened
    EXPECT_EQ(engine.GetPlayerPath(), nullptr);
    const auto cards = engine.PathCards();
    ASSERT_EQ(cards.size(), 2u);
    EXPECT_EQ(cards[0].name, "Berserker");
    EXPECT_FALSE(cards[0].flavor.empty());
    EXPECT_NE(cards[0].color, cards[1].color);   // each in its own color
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
