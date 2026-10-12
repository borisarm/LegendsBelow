// AutoSaveTests.cpp - LB.Engine tests: the run saves itself (a new game, each floor down,
// quitting, closing the game) so Continue picks it up; the newest save is the one loaded
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    EngineOptions Options(const TempSaveDir& dir, bool autoSave = true) {
        return EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(), .titleScreen = true,
            .randomSeed = 7u, .dungeon = true, .autoSave = autoSave };
    }

    void Press(GameEngine& engine, std::int32_t key) {
        TapKey(engine, key);
        engine.Tick(OneSixtieth);
    }

    void Choose(GameEngine& engine, std::string_view text) {
        for (int i = 0; i < 10; ++i) {
            const auto* selected = engine.GetMenu().Selected();
            if (selected && selected->text == text) break;
            Press(engine, Keys::Down);
        }
        ASSERT_TRUE(engine.GetMenu().Selected() && engine.GetMenu().Selected()->text == text) << "no item " << text;
        Press(engine, Keys::Enter);
    }

    void NewGame(GameEngine& engine) {
        engine.Tick(OneSixtieth);
        Choose(engine, "New Game");
        Choose(engine, "Warrior");
    }
}

TEST(AutoSaveTests, ANewGame_IsSavedAtOnce) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.LatestSaveSlot().has_value());   // the run behind the title is not saved

    NewGame(engine);

    ASSERT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_EQ(engine.LatestSaveSlot(), GameEngine::AutoSaveSlot);
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "Saved"; }));
}

TEST(AutoSaveTests, QuitToTitle_ThenContinue_PicksUpTheRun) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    NewGame(engine);
    engine.GrantExperience(400);
    engine.Tick(OneSixtieth);
    const auto level = PlayerLevel(engine);
    ASSERT_GT(level, 1);

    Press(engine, Keys::Escape);   // the pause menu
    Choose(engine, "Quit to Title");
    ASSERT_EQ(engine.GetMenu().Page(), MenuPage::Title);
    EXPECT_EQ(PlayerLevel(engine), 1);   // a fresh run behind the title

    Choose(engine, "Continue");
    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_EQ(PlayerLevel(engine), level);
}

TEST(AutoSaveTests, ClosingTheGame_KeepsTheRun) {
    TempSaveDir dir;
    std::int32_t level = 0;
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
        NewGame(engine);
        engine.GrantExperience(400);
        engine.Tick(OneSixtieth);
        level = PlayerLevel(engine);
    }   // closed: saved on the way out
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetMenu().Items().size() > 1 && engine.GetMenu().Items()[1].enabled);   // Continue

    Choose(engine, "Continue");
    EXPECT_EQ(PlayerLevel(engine), level);
}

TEST(AutoSaveTests, TheNewestSave_IsTheOneLoaded) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir)));
    NewGame(engine);
    ASSERT_EQ(engine.LatestSaveSlot(), GameEngine::AutoSaveSlot);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    Press(engine, Keys::F5);   // a quick save, after the autosave
    EXPECT_EQ(engine.LatestSaveSlot(), GameEngine::QuickSaveSlot);
}

TEST(AutoSaveTests, Off_NothingSavesOnItsOwn) {
    TempSaveDir dir;
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir, false)));
        NewGame(engine);
        Press(engine, Keys::Escape);
        Choose(engine, "Quit to Title");
        EXPECT_FALSE(engine.LatestSaveSlot().has_value());
    }
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, Options(dir, false)));
    EXPECT_FALSE(engine.LatestSaveSlot().has_value());
}
