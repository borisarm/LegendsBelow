#include "pch.h"

import LB.PersistenceSystem;

using namespace LB::Systems::Persistence;

// ============================================================================
// PersistenceSystem Initialization Tests
// ============================================================================

TEST(PersistenceSystemTests, Initialize_ValidPath_Success) {
    PersistenceSystem system;

    bool result = system.Initialize("./SaveData");

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsInitialized());
}

TEST(PersistenceSystemTests, Shutdown_ClearsState) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    system.Shutdown();

    EXPECT_FALSE(system.IsInitialized());
}

TEST(PersistenceSystemTests, GetMaxSaveSlots_ReturnsExpected) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    int32_t slots = system.GetMaxSaveSlots();

    EXPECT_EQ(slots, 10);
}

// ============================================================================
// Save Game Tests
// ============================================================================

TEST(PersistenceSystemTests, SaveGame_ValidSlot_Success) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto result = system.SaveGame(0, "Test Save");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(system.SaveSlotExists(0));
}

TEST(PersistenceSystemTests, SaveGame_BeforeInitialize_Fails) {
    PersistenceSystem system;

    auto result = system.SaveGame(0, "Test Save");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST(PersistenceSystemTests, SaveGame_InvalidSlotNegative_Fails) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto result = system.SaveGame(-1, "Test Save");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid save slot index");
}

TEST(PersistenceSystemTests, SaveGame_InvalidSlotTooHigh_Fails) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto result = system.SaveGame(20, "Test Save");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid save slot index");
}

TEST(PersistenceSystemTests, SaveGame_MultipleSlots_AllSaved) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    system.SaveGame(0, "Save 1");
    system.SaveGame(1, "Save 2");
    system.SaveGame(2, "Save 3");

    EXPECT_TRUE(system.SaveSlotExists(0));
    EXPECT_TRUE(system.SaveSlotExists(1));
    EXPECT_TRUE(system.SaveSlotExists(2));
}

TEST(PersistenceSystemTests, SaveGame_OverwriteExisting_Success) {
    PersistenceSystem system;
    system.Initialize("./SaveData");
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

TEST(PersistenceSystemTests, LoadGame_ExistingSlot_Success) {
    PersistenceSystem system;
    system.Initialize("./SaveData");
    system.SaveGame(0, "Test Save");

    auto result = system.LoadGame(0);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.slotInfo.slotIndex, 0);
}

TEST(PersistenceSystemTests, LoadGame_NonExistentSlot_Fails) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto result = system.LoadGame(5);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Save slot does not exist");
}

TEST(PersistenceSystemTests, LoadGame_BeforeInitialize_Fails) {
    PersistenceSystem system;

    auto result = system.LoadGame(0);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST(PersistenceSystemTests, LoadGame_InvalidSlot_Fails) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto result = system.LoadGame(-1);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Invalid save slot index");
}

// ============================================================================
// Delete Save Tests
// ============================================================================

TEST(PersistenceSystemTests, DeleteSave_ExistingSlot_Success) {
    PersistenceSystem system;
    system.Initialize("./SaveData");
    system.SaveGame(0, "Test Save");

    bool result = system.DeleteSave(0);

    EXPECT_TRUE(result);
    EXPECT_FALSE(system.SaveSlotExists(0));
}

TEST(PersistenceSystemTests, DeleteSave_NonExistentSlot_Fails) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    bool result = system.DeleteSave(5);

    EXPECT_FALSE(result);
}

TEST(PersistenceSystemTests, DeleteSave_BeforeInitialize_Fails) {
    PersistenceSystem system;

    bool result = system.DeleteSave(0);

    EXPECT_FALSE(result);
}

// ============================================================================
// Save Slot Info Tests
// ============================================================================

TEST(PersistenceSystemTests, GetSaveSlotInfo_ExistingSlot_ReturnsInfo) {
    PersistenceSystem system;
    system.Initialize("./SaveData");
    system.SaveGame(0, "My Save");

    auto info = system.GetSaveSlotInfo(0);

    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->slotIndex, 0);
    EXPECT_EQ(info->saveName, "My Save");
    EXPECT_TRUE(info->exists);
}

TEST(PersistenceSystemTests, GetSaveSlotInfo_NonExistentSlot_ReturnsNullopt) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto info = system.GetSaveSlotInfo(5);

    EXPECT_FALSE(info.has_value());
}

TEST(PersistenceSystemTests, GetAllSaveSlots_MultipleSaves_ReturnsAll) {
    PersistenceSystem system;
    system.Initialize("./SaveData");
    system.SaveGame(0, "Save 1");
    system.SaveGame(2, "Save 2");
    system.SaveGame(5, "Save 3");

    auto slots = system.GetAllSaveSlots();

    EXPECT_EQ(slots.size(), 3);
}

TEST(PersistenceSystemTests, GetAllSaveSlots_NoSaves_ReturnsEmpty) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto slots = system.GetAllSaveSlots();

    EXPECT_TRUE(slots.empty());
}

// ============================================================================
// Player Preferences Tests
// ============================================================================

TEST(PersistenceSystemTests, SavePreferences_ValidPrefs_Success) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    PlayerPreferences prefs{};
    prefs.masterVolume = 0.8f;
    prefs.musicVolume = 0.6f;
    prefs.fullscreen = false;

    bool result = system.SavePreferences(prefs);

    EXPECT_TRUE(result);
}

TEST(PersistenceSystemTests, SavePreferences_BeforeInitialize_Fails) {
    PersistenceSystem system;

    PlayerPreferences prefs{};
    bool result = system.SavePreferences(prefs);

    EXPECT_FALSE(result);
}

TEST(PersistenceSystemTests, LoadPreferences_AfterSave_ReturnsSavedValues) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

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

TEST(PersistenceSystemTests, LoadPreferences_NoSavedPrefs_ReturnsDefaults) {
    PersistenceSystem system;
    system.Initialize("./SaveData");

    auto prefs = system.LoadPreferences();

    EXPECT_EQ(prefs.masterVolume, 1.0f);
    EXPECT_EQ(prefs.musicVolume, 0.7f);
    EXPECT_TRUE(prefs.fullscreen);
}

TEST(PersistenceSystemTests, LoadPreferences_BeforeInitialize_ReturnsDefaults) {
    PersistenceSystem system;

    auto prefs = system.LoadPreferences();

    EXPECT_EQ(prefs.masterVolume, 1.0f);
}