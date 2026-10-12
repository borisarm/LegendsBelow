#include "pch.h"

import LB.PersistenceSystem;
import std;

using namespace LB::Systems::Persistence;

// Each test gets its own empty save directory, removed afterwards, so tests never share
// files with each other or write into the working directory.
class PersistenceSystemTests : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path() / "LegendsBelowTests" /
            (std::string(info->name()) + "_" + std::to_string(std::random_device{}()));
        std::filesystem::remove_all(dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }

    // UTF-8 path string, as the system expects
    std::string SaveDir() const {
        const auto text = dir_.u8string();
        return std::string(text.begin(), text.end());
    }

    std::filesystem::path dir_;
};

// ============================================================================
// PersistenceSystem Initialization Tests
// ============================================================================

TEST_F(PersistenceSystemTests, Initialize_ValidPath_Success) {
    PersistenceSystem system;

    bool result = system.Initialize(SaveDir());

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsInitialized());
}

TEST_F(PersistenceSystemTests, Shutdown_ClearsState) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    system.Shutdown();

    EXPECT_FALSE(system.IsInitialized());
}

TEST_F(PersistenceSystemTests, GetMaxSaveSlots_ReturnsExpected) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    int32_t slots = system.GetMaxSaveSlots();

    EXPECT_EQ(slots, 10);
}

// ============================================================================
// Save Game Tests
// ============================================================================

TEST_F(PersistenceSystemTests, SaveGame_ValidSlot_Success) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto result = system.SaveGame(0, "Test Save");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(system.SaveSlotExists(0));
}

TEST_F(PersistenceSystemTests, SaveGame_BeforeInitialize_Fails) {
    PersistenceSystem system;

    auto result = system.SaveGame(0, "Test Save");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PersistenceSystemTests, SaveGame_InvalidSlotNegative_Fails) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto result = system.SaveGame(-1, "Test Save");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid save slot index");
}

TEST_F(PersistenceSystemTests, SaveGame_InvalidSlotTooHigh_Fails) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto result = system.SaveGame(20, "Test Save");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid save slot index");
}

TEST_F(PersistenceSystemTests, SaveGame_MultipleSlots_AllSaved) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    system.SaveGame(0, "Save 1");
    system.SaveGame(1, "Save 2");
    system.SaveGame(2, "Save 3");

    EXPECT_TRUE(system.SaveSlotExists(0));
    EXPECT_TRUE(system.SaveSlotExists(1));
    EXPECT_TRUE(system.SaveSlotExists(2));
}

TEST_F(PersistenceSystemTests, SaveGame_OverwriteExisting_Success) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    system.SaveGame(0, "First Save");

    auto result = system.SaveGame(0, "Overwritten Save");

    EXPECT_TRUE(result.success);
    auto info = system.GetSaveSlotInfo(0);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->saveName, "Overwritten Save");
}

// ============================================================================
// Load Game Tests
// ============================================================================

TEST_F(PersistenceSystemTests, LoadGame_ExistingSlot_Success) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    system.SaveGame(0, "Test Save");

    auto result = system.LoadGame(0);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.slotInfo.slotIndex, 0);
}

TEST_F(PersistenceSystemTests, LoadGame_NonExistentSlot_Fails) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto result = system.LoadGame(5);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Save slot does not exist");
}

TEST_F(PersistenceSystemTests, LoadGame_BeforeInitialize_Fails) {
    PersistenceSystem system;

    auto result = system.LoadGame(0);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PersistenceSystemTests, LoadGame_InvalidSlot_Fails) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto result = system.LoadGame(-1);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid save slot index");
}

// ============================================================================
// Delete Save Tests
// ============================================================================

TEST_F(PersistenceSystemTests, DeleteSave_ExistingSlot_Success) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    system.SaveGame(0, "Test Save");

    bool result = system.DeleteSave(0);

    EXPECT_TRUE(result);
    EXPECT_FALSE(system.SaveSlotExists(0));
}

TEST_F(PersistenceSystemTests, DeleteSave_NonExistentSlot_Fails) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    bool result = system.DeleteSave(5);

    EXPECT_FALSE(result);
}

TEST_F(PersistenceSystemTests, DeleteSave_BeforeInitialize_Fails) {
    PersistenceSystem system;

    bool result = system.DeleteSave(0);

    EXPECT_FALSE(result);
}

// ============================================================================
// Save Slot Info Tests
// ============================================================================

TEST_F(PersistenceSystemTests, GetSaveSlotInfo_ExistingSlot_ReturnsInfo) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    system.SaveGame(0, "My Save");

    auto info = system.GetSaveSlotInfo(0);

    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->slotIndex, 0);
    EXPECT_EQ(info->saveName, "My Save");
    EXPECT_TRUE(info->exists);
}

TEST_F(PersistenceSystemTests, GetSaveSlotInfo_NonExistentSlot_ReturnsNullopt) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto info = system.GetSaveSlotInfo(5);

    EXPECT_FALSE(info.has_value());
}

TEST_F(PersistenceSystemTests, GetAllSaveSlots_MultipleSaves_ReturnsAll) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    system.SaveGame(0, "Save 1");
    system.SaveGame(2, "Save 2");
    system.SaveGame(5, "Save 3");

    auto slots = system.GetAllSaveSlots();

    EXPECT_EQ(slots.size(), 3u);
}

TEST_F(PersistenceSystemTests, GetAllSaveSlots_NoSaves_ReturnsEmpty) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto slots = system.GetAllSaveSlots();

    EXPECT_TRUE(slots.empty());
}

// ============================================================================
// Player Preferences Tests
// ============================================================================

TEST_F(PersistenceSystemTests, SavePreferences_ValidPrefs_Success) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    PlayerPreferences prefs{};
    prefs.masterVolume = 0.8f;
    prefs.musicVolume = 0.6f;
    prefs.fullscreen = false;

    bool result = system.SavePreferences(prefs);

    EXPECT_TRUE(result);
}

TEST_F(PersistenceSystemTests, SavePreferences_BeforeInitialize_Fails) {
    PersistenceSystem system;

    PlayerPreferences prefs{};
    bool result = system.SavePreferences(prefs);

    EXPECT_FALSE(result);
}

TEST_F(PersistenceSystemTests, LoadPreferences_AfterSave_ReturnsSavedValues) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    PlayerPreferences savedPrefs{};
    savedPrefs.masterVolume = 0.8f;
    savedPrefs.musicVolume = 0.6f;
    savedPrefs.fullscreen = false;
    system.SavePreferences(savedPrefs);

    auto loadedPrefs = system.LoadPreferences();

    EXPECT_EQ(loadedPrefs.masterVolume, 0.8f);
    EXPECT_EQ(loadedPrefs.musicVolume, 0.6f);
    EXPECT_FALSE(loadedPrefs.fullscreen);
}

TEST_F(PersistenceSystemTests, LoadPreferences_NoSavedPrefs_ReturnsDefaults) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto prefs = system.LoadPreferences();

    EXPECT_EQ(prefs.masterVolume, 1.0f);
    EXPECT_EQ(prefs.musicVolume, 0.7f);
    EXPECT_TRUE(prefs.fullscreen);
}

TEST_F(PersistenceSystemTests, LoadPreferences_BeforeInitialize_ReturnsDefaults) {
    PersistenceSystem system;

    auto prefs = system.LoadPreferences();

    EXPECT_EQ(prefs.masterVolume, 1.0f);
}
// ============================================================================
// Disk Storage Tests
// ============================================================================

TEST_F(PersistenceSystemTests, Initialize_CreatesSaveDirectory) {
    PersistenceSystem system;

    ASSERT_TRUE(system.Initialize(SaveDir()));

    EXPECT_TRUE(std::filesystem::is_directory(dir_));
    EXPECT_TRUE(system.IsPersistentStorage());
}

TEST_F(PersistenceSystemTests, Initialize_EmptyDirectory_MemoryOnly) {
    PersistenceSystem system;

    ASSERT_TRUE(system.Initialize(""));

    EXPECT_FALSE(system.IsPersistentStorage());
    EXPECT_TRUE(system.SaveGame(0, "In Memory").success);
    EXPECT_TRUE(system.LoadGame(0).success);
}

TEST_F(PersistenceSystemTests, Initialize_PathIsAFile_Fails) {
    std::filesystem::create_directories(dir_);
    const auto filePath = dir_ / "not_a_directory";
    std::ofstream(filePath) << "x";
    PersistenceSystem system;

    const auto text = filePath.u8string();
    EXPECT_FALSE(system.Initialize(std::string(text.begin(), text.end())));
}

TEST_F(PersistenceSystemTests, SaveGame_WritesFile) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    auto result = system.SaveGame(3, "Disk Save");

    ASSERT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(dir_ / "save_3.json"));
    EXPECT_FALSE(result.filePath.empty());
    EXPECT_FALSE(std::filesystem::exists(dir_ / "save_3.json.tmp"));
}

TEST_F(PersistenceSystemTests, SaveGame_SetsTimestamp) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    system.SaveGame(0, "Stamped");

    const auto info = system.GetSaveSlotInfo(0);
    ASSERT_TRUE(info.has_value());
    // e.g. 2026-10-09T19:42:07.125Z (milliseconds: saves a moment apart still sort)
    EXPECT_EQ(info->timestamp.size(), 24u);
    EXPECT_EQ(info->timestamp[19], '.');
    EXPECT_EQ(info->timestamp.back(), 'Z');
}

TEST_F(PersistenceSystemTests, LaterSaves_SortLater) {
    PersistenceSystem system;
    system.Initialize(SaveDir());

    system.SaveGame(0, "First");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    system.SaveGame(1, "Second");

    EXPECT_LT(system.GetSaveSlotInfo(0)->timestamp, system.GetSaveSlotInfo(1)->timestamp);
}

TEST_F(PersistenceSystemTests, SaveAndLoad_PayloadAndMetadata_RoundTrip) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    SaveSlotInfo metadata{};
    metadata.saveName = "Crypt Run";
    metadata.characterName = "Aria";
    metadata.characterLevel = 7;
    metadata.location = "Crypts - Floor 3";
    metadata.playtimeSeconds = 5400;
    SaveData data{ {"gameTimeSeconds", "123.5"}, {"player.x", "42"} };

    ASSERT_TRUE(system.SaveGame(2, metadata, data).success);
    auto result = system.LoadGame(2);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.slotInfo.slotIndex, 2);
    EXPECT_EQ(result.slotInfo.saveName, "Crypt Run");
    EXPECT_EQ(result.slotInfo.characterName, "Aria");
    EXPECT_EQ(result.slotInfo.characterLevel, 7);
    EXPECT_EQ(result.slotInfo.location, "Crypts - Floor 3");
    EXPECT_EQ(result.slotInfo.playtimeSeconds, 5400);
    EXPECT_EQ(result.data, data);
}

TEST_F(PersistenceSystemTests, Saves_SurviveRestart) {
    {
        PersistenceSystem first;
        first.Initialize(SaveDir());
        first.SaveGame(1, SaveSlotInfo{ .saveName = "Before Restart" }, SaveData{ {"key", "value"} });
    }

    PersistenceSystem second;
    second.Initialize(SaveDir());

    ASSERT_TRUE(second.SaveSlotExists(1));
    EXPECT_EQ(second.GetSaveSlotInfo(1)->saveName, "Before Restart");
    auto result = second.LoadGame(1);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.data.at("key"), "value");
}

TEST_F(PersistenceSystemTests, Preferences_SurviveRestart) {
    {
        PersistenceSystem first;
        first.Initialize(SaveDir());
        PlayerPreferences prefs{};
        prefs.masterVolume = 0.25f;
        prefs.language = "es-ES";
        first.SavePreferences(prefs);
    }

    PersistenceSystem second;
    second.Initialize(SaveDir());
    auto prefs = second.LoadPreferences();

    EXPECT_EQ(prefs.masterVolume, 0.25f);
    EXPECT_EQ(prefs.language, "es-ES");
}

TEST_F(PersistenceSystemTests, DeleteSave_RemovesFile) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    system.SaveGame(4, "Doomed");

    ASSERT_TRUE(system.DeleteSave(4));

    EXPECT_FALSE(std::filesystem::exists(dir_ / "save_4.json"));
    EXPECT_FALSE(system.SaveSlotExists(4));
}

TEST_F(PersistenceSystemTests, Initialize_CorruptSaveFile_Skipped) {
    std::filesystem::create_directories(dir_);
    std::ofstream(dir_ / "save_0.json") << "{ not json";
    PersistenceSystem system;

    ASSERT_TRUE(system.Initialize(SaveDir()));

    EXPECT_FALSE(system.SaveSlotExists(0));
}

TEST_F(PersistenceSystemTests, LoadGame_FileCorruptedAfterDiscovery_Fails) {
    PersistenceSystem system;
    system.Initialize(SaveDir());
    system.SaveGame(0, "Valid");
    std::ofstream(dir_ / "save_0.json", std::ios::trunc) << "garbage";

    auto result = system.LoadGame(0);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PersistenceSystemTests, Initialize_CorruptPreferences_UsesDefaults) {
    std::filesystem::create_directories(dir_);
    std::ofstream(dir_ / "preferences.json") << "[1, 2";
    PersistenceSystem system;

    ASSERT_TRUE(system.Initialize(SaveDir()));

    EXPECT_EQ(system.LoadPreferences().masterVolume, PlayerPreferences{}.masterVolume);
}

TEST_F(PersistenceSystemTests, Initialize_UnicodePath_Works) {
    PersistenceSystem system;
    const std::string unicodeDir = SaveDir() + "/Partida_\xC3\xB1";  // "Partida_" + n with tilde, as UTF-8

    ASSERT_TRUE(system.Initialize(unicodeDir));
    ASSERT_TRUE(system.SaveGame(0, "Unicode").success);

    std::string listing;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_)) {
        const auto text = entry.path().u8string();
        listing += std::string(text.begin(), text.end()) + "\n";
    }
    EXPECT_TRUE(std::filesystem::exists(dir_ / std::filesystem::path(std::u8string(u8"Partida_") + char8_t(0xC3) + char8_t(0xB1)) / "save_0.json")) << listing;
}

TEST_F(PersistenceSystemTests, Profile_KeptOnDisk) {
    {
        PersistenceSystem system;
        system.Initialize(SaveDir());
        EXPECT_TRUE(system.LoadProfile().empty());
        ASSERT_TRUE(system.SaveProfile({ { "legacy.deepest", "7" } }));
    }
    PersistenceSystem system;
    system.Initialize(SaveDir());
    EXPECT_EQ(system.LoadProfile().at("legacy.deepest"), "7");
}
