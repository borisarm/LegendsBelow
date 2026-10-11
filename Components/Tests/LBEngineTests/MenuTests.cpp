// MenuTests.cpp - LB.Engine tests: the menu model and the title, pause, options, and credits pages
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    // A key pressed and one step run
    void Press(GameEngine& engine, std::int32_t key) {
        TapKey(engine, key);
        engine.Tick(OneSixtieth);
    }

    // The game as the player starts it: the title screen first
    void InitializeWithTitle(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .titleScreen = true, .randomSeed = 7u, .dungeon = true }))
            << engine.GetInitializationError();
        engine.Tick(OneSixtieth);
    }

    // Put the cursor on an item by its text (Down until it is selected)
    void Select(GameEngine& engine, std::string_view text) {
        for (int i = 0; i < 10; ++i) {
            const auto* selected = engine.GetMenu().Selected();
            if (selected && selected->text == text) return;
            Press(engine, Keys::Down);
        }
        FAIL() << "no item " << text;
    }

    void Choose(GameEngine& engine, std::string_view text) {
        Select(engine, text);
        Press(engine, Keys::Enter);
    }

    bool Drawn(const GameEngine& engine, std::string_view text) {
        return std::ranges::any_of(engine.GetRendering().GetQueuedText(), [&](const auto& t) { return t.text == text; });
    }

    bool HasItem(const GameEngine& engine, std::string_view text, bool enabled) {
        return std::ranges::any_of(engine.GetMenu().Items(), [&](const MenuItem& item) { return item.text == text && item.enabled == enabled; });
    }
}

// ============================================================================
// Menu Model Tests
// ============================================================================

TEST(GameMenuTests, Step_SkipsDisabledItems_AndWraps) {
    GameMenu menu;
    menu.Open(MenuPage::Title);
    menu.SetItems({ { "A", MenuCommand::NewGame }, { "B", MenuCommand::Continue, false }, { "C", MenuCommand::Quit } });

    EXPECT_EQ(menu.Selected()->text, "A");
    menu.Step(+1);
    EXPECT_EQ(menu.Selected()->text, "C");   // B is disabled
    menu.Step(+1);
    EXPECT_EQ(menu.Selected()->text, "A");   // wrapped
    menu.Step(-1);
    EXPECT_EQ(menu.Selected()->text, "C");
}

TEST(GameMenuTests, Push_AndBack_ReturnToThePageAndItsCursor) {
    GameMenu menu;
    menu.Open(MenuPage::Pause);
    menu.SetItems({ { "Resume", MenuCommand::Resume }, { "Options", MenuCommand::Options } });
    menu.Step(+1);
    menu.Push(MenuPage::Options);
    EXPECT_EQ(menu.Page(), MenuPage::Options);

    EXPECT_TRUE(menu.Back());
    EXPECT_EQ(menu.Page(), MenuPage::Pause);
    EXPECT_EQ(menu.Cursor(), 1u);

    EXPECT_FALSE(menu.Back());   // the pause menu was opened on its own: Back closes
    EXPECT_FALSE(menu.IsOpen());
}

TEST(GameMenuTests, CreditsScroll_StaysWithinTheLines) {
    GameMenu menu;
    menu.Open(MenuPage::Credits);

    menu.Scroll(-5, 30, 16);
    EXPECT_EQ(menu.ScrollPosition(), 0u);
    menu.Scroll(100, 30, 16);
    EXPECT_EQ(menu.ScrollPosition(), 14u);
}

// ============================================================================
// GameEngine Menu Tests
// ============================================================================

TEST(GameEngineMenuTests, TitleScreen_FirstThing_GamePausedAndHudHidden) {
    GameEngine engine;
    InitializeWithTitle(engine);

    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Title);
    EXPECT_TRUE(engine.IsPaused());
    EXPECT_TRUE(Drawn(engine, "LEGENDS BELOW"));
    EXPECT_TRUE(Drawn(engine, "New Game"));
    EXPECT_FALSE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text.starts_with("HP "); }));
    EXPECT_TRUE(HasItem(engine, "Continue", false));   // nothing saved yet
}

TEST(GameEngineMenuTests, Title_EscapeDoesNotLeave) {
    GameEngine engine;
    InitializeWithTitle(engine);

    Press(engine, Keys::Escape);

    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Title);
    EXPECT_TRUE(engine.IsPaused());
}

TEST(GameEngineMenuTests, NewGame_AsAMage) {
    GameEngine engine;
    InitializeWithTitle(engine);

    Choose(engine, "New Game");
    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::ChooseClass);
    EXPECT_TRUE(Drawn(engine, "CHOOSE YOUR HERO"));
    Choose(engine, "Mage");

    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_EQ(engine.GetPlayerClass(), LB::Character::CharacterClass::Mage);
    EXPECT_EQ(engine.GetSpellBar().size(), 4u);
}

TEST(GameEngineMenuTests, NewGame_StartsPlaying) {
    GameEngine engine;
    InitializeWithTitle(engine);

    Choose(engine, "New Game");
    Choose(engine, "Warrior");

    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_FALSE(engine.IsPaused());
    const auto before = engine.GetPlayerPosition();
    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);
    EXPECT_NE(engine.GetPlayerPosition().x, before.x);
}

TEST(GameEngineMenuTests, Escape_OpensThePauseMenu_EscapeCloses) {
    GameEngine engine;
    InitializeDungeon(engine);

    Press(engine, Keys::Escape);
    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Pause);
    EXPECT_TRUE(engine.IsPaused());
    EXPECT_TRUE(Drawn(engine, "PAUSED"));

    Press(engine, Keys::Escape);
    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_FALSE(engine.IsPaused());
}

TEST(GameEngineMenuTests, Options_FromPause_BackReturnsToPause) {
    GameEngine engine;
    InitializeDungeon(engine);
    Press(engine, Keys::Escape);

    Choose(engine, "Options");
    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Options);
    Press(engine, Keys::Escape);

    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Pause);
    EXPECT_TRUE(engine.IsPaused());
}

TEST(GameEngineMenuTests, Options_VolumeLeftRight_SavedToPreferences) {
    GameEngine engine;
    InitializeDungeon(engine);
    Press(engine, Keys::Escape);
    Choose(engine, "Options");
    Select(engine, "Music volume");
    const float before = engine.GetAudio().GetChannelVolume(LB::Systems::Audio::AudioChannel::Music);

    Press(engine, Keys::Left);
    Press(engine, Keys::Left);

    const float after = engine.GetAudio().GetChannelVolume(LB::Systems::Audio::AudioChannel::Music);
    EXPECT_NEAR(after, before - 0.2f, 1e-4f);
    EXPECT_NEAR(engine.GetPersistence().LoadPreferences().musicVolume, after, 1e-4f);
    EXPECT_TRUE(std::ranges::any_of(engine.GetMenu().Items(), [&](const MenuItem& item) {
        return item.text == "Music volume" && item.value == std::format("{}%", static_cast<int>(std::lround(after * 100.0f)));
    }));
}

TEST(GameEngineMenuTests, Credits_ShowTheShippedCredits) {
    GameEngine engine;
    InitializeWithTitle(engine);

    Choose(engine, "Credits");
    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Credits);
    const auto& credits = engine.GetCredits();
    EXPECT_TRUE(std::ranges::any_of(credits, [](const std::string& line) { return line.find("Alexandr Zhelanov") != std::string::npos; }));
    EXPECT_TRUE(std::ranges::any_of(credits, [](const std::string& line) { return line.find("CC-BY 3.0") != std::string::npos; }));
    EXPECT_TRUE(Drawn(engine, "\"Mystical Theme\" by Alexandr Zhelanov"));
}

TEST(GameEngineMenuTests, SaveFromPause_ThenContinueFromTheTitle) {
    GameEngine engine;
    InitializeWithTitle(engine);
    Choose(engine, "New Game");
    Choose(engine, "Warrior");
    engine.GiveItem("armor_head_leather_cap_t1");
    Press(engine, Keys::Escape);

    Choose(engine, "Save Game");
    EXPECT_EQ(engine.GetMenu().Message(), "Game saved");
    EXPECT_TRUE(HasItem(engine, "Load Game", true));

    Choose(engine, "Quit to Title");
    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Title);
    EXPECT_EQ(engine.CountItem("armor_head_leather_cap_t1"), 0);   // a fresh run behind the title

    Choose(engine, "Continue");
    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_EQ(engine.CountItem("armor_head_leather_cap_t1"), 1);   // the saved game
}

TEST(GameEngineMenuTests, Quit_AsksTheHostToClose) {
    GameEngine engine;
    InitializeWithTitle(engine);
    EXPECT_FALSE(engine.IsQuitRequested());

    Choose(engine, "Quit");

    EXPECT_TRUE(engine.IsQuitRequested());
}

// ============================================================================
// UI Scale Tests
// ============================================================================

TEST(GameEngineUiScaleTests, UiScaleFor_FitsTheDefaultLayout) {
    EXPECT_FLOAT_EQ(GameEngine::UiScaleFor(800, 600), 1.0f);
    EXPECT_FLOAT_EQ(GameEngine::UiScaleFor(1600, 1200), 2.0f);
    EXPECT_FLOAT_EQ(GameEngine::UiScaleFor(3840, 1080), 1.8f);   // a wide window: the height decides
    EXPECT_FLOAT_EQ(GameEngine::UiScaleFor(100, 100), 0.5f);     // never smaller than half
}

TEST(GameEngineUiScaleTests, LargerWindow_HudLaidOutInUiUnits) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 1600, 1200, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .dungeon = true }));

    engine.Tick(OneSixtieth);

    EXPECT_FLOAT_EQ(engine.GetRendering().GetUiScale(), 2.0f);
    const auto& texts = engine.GetRendering().GetQueuedText();
    const auto hp = std::ranges::find_if(texts, [](const auto& t) { return t.text.starts_with("HP "); });
    ASSERT_NE(hp, texts.end());
    EXPECT_LT(hp->position.y, 600);    // in UI units: within the 800 x 600 layout, drawn twice as large
    EXPECT_GT(hp->position.y, 500);
}

// ============================================================================
// Menu Frame Tests
// ============================================================================

namespace {
    std::string ShippedUiDirectory() {
        const auto content = ShippedContentDirectory();
        const auto ui = std::filesystem::path(std::u8string(content.begin(), content.end())).parent_path() / "UI";
        const auto text = ui.u8string();
        return std::string(text.begin(), text.end());
    }
}

TEST(GameEngineMenuFrameTests, Title_FramedPanelSelectionAndOrnaments) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .titleScreen = true, .uiDirectory = ShippedUiDirectory() }))
        << engine.GetInitializationError();

    engine.Tick(OneSixtieth);

    // The panel's frame, the selected item's frame, and the two ornaments beside the subtitle
    const auto& images = engine.GetRendering().GetQueuedImages();
    ASSERT_EQ(images.size(), 4u);
    EXPECT_EQ(std::ranges::count_if(images, [](const auto& image) { return image.nineSlice > 0.0f; }), 2);
    EXPECT_EQ(std::ranges::count_if(images, [](const auto& image) { return image.mirrored; }), 1);
}

TEST(GameEngineMenuFrameTests, CharacterScreen_Framed_HudHidden) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .uiDirectory = ShippedUiDirectory() }));

    Press(engine, Keys::FromChar('I'));

    EXPECT_FALSE(engine.GetRendering().GetQueuedImages().empty());
    // The HUD's potion count (the screen shows hit points itself)
    EXPECT_FALSE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text.starts_with("POTIONS "); }));
}

TEST(GameEngineMenuFrameTests, NoUiDirectory_PlainPanels) {
    GameEngine engine;
    InitializeWithTitle(engine);

    EXPECT_TRUE(engine.GetRendering().GetQueuedImages().empty());
    EXPECT_TRUE(Drawn(engine, "New Game"));
}

TEST(GameEngineMenuFrameTests, MissingUiImages_StopStartup) {
    GameEngine engine;

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .uiDirectory = "Z:/no/ui" }));
    EXPECT_NE(engine.GetInitializationError().find("UI images in"), std::string::npos);
}
