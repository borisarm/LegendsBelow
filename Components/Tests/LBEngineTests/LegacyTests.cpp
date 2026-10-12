// LegacyTests.cpp - LB.Engine tests: save slots (saving into one, loading any, their summaries),
// and the legacy: records kept across runs and the boons they unlock for new heroes
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    EngineOptions Options(const TempSaveDir& dir) {
        return EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(), .titleScreen = true, .randomSeed = 7u, .dungeon = true };
    }

    void Press(GameEngine& engine, std::int32_t key) {
        TapKey(engine, key);
        engine.Tick(OneSixtieth);
    }

    void Choose(GameEngine& engine, std::string_view text) {
        for (int i = 0; i < 12; ++i) {
            const auto* selected = engine.GetMenu().Selected();
            if (selected && selected->text == text) break;
            Press(engine, Keys::Down);
        }
        ASSERT_TRUE(engine.GetMenu().Selected() && engine.GetMenu().Selected()->text == text) << "no item " << text;
        Press(engine, Keys::Enter);
    }

    void NewGame(GameEngine& engine, std::string_view hero = "Warrior") {
        engine.Tick(OneSixtieth);
        Choose(engine, "New Game");
        Choose(engine, hero);
    }

    bool Drawn(const GameEngine& engine, std::string_view text) {
        return std::ranges::any_of(engine.GetRendering().GetQueuedText(), [&](const auto& t) { return t.text.find(text) != std::string::npos; });
    }
}

TEST(SaveSlotTests, SavedIntoASlot_LoadedFromTheTitle) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    NewGame(engine);
    engine.GrantExperience(400);
    engine.Tick(OneSixtieth);
    const auto level = PlayerLevel(engine);

    Press(engine, Keys::Escape);
    Choose(engine, "Save Game");
    Choose(engine, "Slot 2");
    EXPECT_EQ(engine.GetMenu().Message(), "Game saved");
    EXPECT_TRUE(engine.SlotSummary(GameEngine::SaveSlots[1]).starts_with(std::format("Warrior, level {}, depth 1", level))) << engine.SlotSummary(GameEngine::SaveSlots[1]);
    EXPECT_TRUE(engine.SlotSummary(GameEngine::SaveSlots[0]).empty());
    Press(engine, Keys::Escape);
    Choose(engine, "Quit to Title");

    Choose(engine, "Load Game");
    ASSERT_EQ(engine.GetMenu().Page(), MenuPage::LoadSlots);
    EXPECT_TRUE(Drawn(engine, "Warrior, level"));
    EXPECT_TRUE(std::ranges::any_of(engine.GetMenu().Items(), [](const MenuItem& item) { return item.text == "Slot 1" && !item.enabled; }));   // empty
    Choose(engine, "Slot 2");
    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_EQ(PlayerLevel(engine), level);
}

TEST(LegacyTests, Records_CarryOverAndUnlockBoons) {
    TempSaveDir dir;
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
        engine.Tick(OneSixtieth);
        EXPECT_EQ(engine.GetLegacy().deepest, 0);   // the run behind the title counts for nothing
        NewGame(engine);
        EXPECT_EQ(engine.GetLegacy().runs, 1);
        EXPECT_EQ(engine.GetLegacy().deepest, 1);
        EXPECT_FALSE(engine.GetLegacy().Has(Boon::Provisions));

        engine.EnterFloor(3);   // the third floor reached: Provisions
        engine.Tick(OneSixtieth);
        EXPECT_EQ(engine.GetLegacy().deepest, 3);
        EXPECT_TRUE(engine.GetLegacy().Has(Boon::Provisions));
        ASSERT_NE(engine.GetFanfare().Current(), nullptr);
        EXPECT_EQ(engine.GetFanfare().Current()->title, "LEGACY: PROVISIONS");
    }
    // A new game, later: the record stands, and the boon is given
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    EXPECT_EQ(engine.GetLegacy().deepest, 3);
    NewGame(engine);
    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), GameEngine::StartingPotions + 1);
    EXPECT_EQ(engine.GetLegacy().runs, 2);
}

TEST(LegacyTests, TheLegacyPage_ListsRecordsAndBoons) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    engine.Tick(OneSixtieth);
    Choose(engine, "Legacy");

    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Legacy);
    EXPECT_TRUE(Drawn(engine, "Deepest floor reached: 0"));
    EXPECT_TRUE(Drawn(engine, "Heirloom (locked): reach depth 7"));
}

TEST(LegacyTests, Boons_FromTheRecords) {
    Legacy legacy;
    legacy.deepest = 7;
    legacy.bosses = 1;
    EXPECT_TRUE(legacy.Has(Boon::Provisions));
    EXPECT_TRUE(legacy.Has(Boon::OldCoin));
    EXPECT_TRUE(legacy.Has(Boon::Heirloom));
    EXPECT_FALSE(legacy.Has(Boon::Veteran));

    Legacy before;
    before.deepest = 3;
    EXPECT_EQ(legacy.NewSince(before).size(), 2u);   // Old Coin and Heirloom are new
    EXPECT_EQ(Legacy::FromProfile(legacy.ToProfile()).bosses, 1);
}

TEST(LegacyTests, AnHeirloom_ForAVeteranOfTheDepths) {
    TempSaveDir dir;
    GameEngine first;
    ASSERT_TRUE(first.Initialize(nullptr, 800, 600, Options(dir)));
    NewGame(first);
    first.EnterFloor(7);
    first.Tick(OneSixtieth);
    ASSERT_TRUE(first.GetLegacy().Has(Boon::Heirloom));

    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    NewGame(engine);
    EXPECT_EQ(engine.GetPlayerWeaponItem().kind, ItemKind::Rolled);   // the class's weapon, uncommon
    EXPECT_EQ(engine.GetPlayerWeapon()->key, GameEngine::StartingWeapon);
}
