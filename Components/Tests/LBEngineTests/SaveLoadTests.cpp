// SaveLoadTests.cpp - LB.Engine tests: saving and loading games and preferences
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// GameEngine Save/Load Tests
// ============================================================================

TEST(GameEngineSaveTests, Headless_NoDirectory_MemoryOnly) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.GetPersistence().IsPersistentStorage());
}

TEST(GameEngineSaveTests, SaveThenLoad_RestoresGameTime) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    RunSteps(engine, 30);
    const double savedTime = engine.GetGameTimeSeconds();
    ASSERT_TRUE(engine.SaveGame(1));

    RunSteps(engine, 30);
    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_DOUBLE_EQ(engine.GetGameTimeSeconds(), savedTime);
}

TEST(GameEngineSaveTests, LoadGame_MissingSlot_FailsAndKeepsState) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    RunSteps(engine, 10);
    const double before = engine.GetGameTimeSeconds();

    EXPECT_FALSE(engine.LoadGame(5));
    EXPECT_DOUBLE_EQ(engine.GetGameTimeSeconds(), before);
}

TEST(GameEngineSaveTests, SaveGame_BeforeInitialize_Fails) {
    GameEngine engine;

    EXPECT_FALSE(engine.SaveGame(0));
}

TEST(GameEngineSaveTests, QuickSaveAndQuickLoad_Keys) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    RunSteps(engine, 20);
    TapKey(engine, Keys::F5);
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetPersistence().SaveSlotExists(GameEngine::QuickSaveSlot));
    EXPECT_EQ(engine.GetPersistence().GetSaveSlotInfo(GameEngine::QuickSaveSlot)->saveName, "Quick Save");

    RunSteps(engine, 120);
    TapKey(engine, Keys::F9);
    engine.Tick(OneSixtieth);

    // Save and load both run at the start of a step, before that step's time is simulated:
    // the save captured 20 steps, and the step that loaded it adds one more
    const double stepSeconds = StepTimer::TicksToSeconds(engine.GetTimer().GetTargetElapsedTicks());
    EXPECT_NEAR(engine.GetGameTimeSeconds(), 21.0 * stepSeconds, 1e-9);
}

TEST(GameEngineSaveTests, SaveDirectory_SavesSurviveRestart) {
    TempSaveDir dir;
    double savedTime = 0.0;
    {
        GameEngine engine;
        engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });
        ASSERT_TRUE(engine.GetPersistence().IsPersistentStorage());
        RunSteps(engine, 45);
        savedTime = engine.GetGameTimeSeconds();
        ASSERT_TRUE(engine.SaveGame(2));
    }

    GameEngine restarted;
    restarted.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });

    ASSERT_TRUE(restarted.LoadGame(2));
    EXPECT_DOUBLE_EQ(restarted.GetGameTimeSeconds(), savedTime);
}

TEST(GameEngineSaveTests, VolumePreferences_SurviveRestart) {
    TempSaveDir dir;
    {
        GameEngine engine;
        engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });
        engine.GetAudio().SetMasterVolume(0.35f);
        engine.GetAudio().SetChannelVolume(AudioChannel::Music, 0.2f);
        engine.Shutdown();
    }

    GameEngine restarted;
    restarted.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8() });

    EXPECT_FLOAT_EQ(restarted.GetAudio().GetMasterVolume(), 0.35f);
    EXPECT_FLOAT_EQ(restarted.GetAudio().GetChannelVolume(AudioChannel::Music), 0.2f);
}

TEST(GameEngineSaveTests, UnusableSaveDirectory_FallsBackToMemory) {
    TempSaveDir dir;
    std::filesystem::create_directories(dir.path);
    std::ofstream(dir.path / "file") << "x";
    const auto filePath = (dir.path / "file").u8string();

    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = std::string(filePath.begin(), filePath.end()) }));

    EXPECT_FALSE(engine.GetPersistence().IsPersistentStorage());
    EXPECT_TRUE(engine.SaveGame(0));
}
