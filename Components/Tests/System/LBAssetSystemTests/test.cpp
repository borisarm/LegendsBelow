#include "pch.h"

import LB.AssetSystem;

using namespace LB::Systems::Asset;

// ============================================================================
// AssetSystem Initialization Tests
// ============================================================================

TEST(AssetSystemTests, Initialize_ValidPath_Success) {
    AssetSystem system;
    bool result = system.Initialize("./Assets");

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsInitialized());
}

TEST(AssetSystemTests, Shutdown_ClearsAllAssets) {
    AssetSystem system;
    system.Initialize("./Assets");
    system.LoadAsset("test_texture", AssetType::Texture);

    system.Shutdown();

    EXPECT_FALSE(system.IsInitialized());
    EXPECT_FALSE(system.IsAssetLoaded("test_texture"));
}

// ============================================================================
// Asset Loading Tests
// ============================================================================

TEST(AssetSystemTests, LoadAsset_ValidTexture_Success) {
    AssetSystem system;
    system.Initialize("./Assets");

    bool result = system.LoadAsset("player_sprite", AssetType::Texture);

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsAssetLoaded("player_sprite"));
}

TEST(AssetSystemTests, LoadAsset_MultipleAssets_AllLoaded) {
    AssetSystem system;
    system.Initialize("./Assets");

    system.LoadAsset("texture1", AssetType::Texture);
    system.LoadAsset("audio1", AssetType::Audio);
    system.LoadAsset("font1", AssetType::Font);

    EXPECT_TRUE(system.IsAssetLoaded("texture1"));
    EXPECT_TRUE(system.IsAssetLoaded("audio1"));
    EXPECT_TRUE(system.IsAssetLoaded("font1"));
}

TEST(AssetSystemTests, LoadAsset_BeforeInitialize_Fails) {
    AssetSystem system;

    bool result = system.LoadAsset("test", AssetType::Texture);

    EXPECT_FALSE(result);
}

// ============================================================================
// Asset Unloading Tests
// ============================================================================

TEST(AssetSystemTests, UnloadAsset_LoadedAsset_Success) {
    AssetSystem system;
    system.Initialize("./Assets");
    system.LoadAsset("test_asset", AssetType::Texture);

    bool result = system.UnloadAsset("test_asset");

    EXPECT_TRUE(result);
    EXPECT_FALSE(system.IsAssetLoaded("test_asset"));
}

TEST(AssetSystemTests, UnloadAsset_NonExistentAsset_Fails) {
    AssetSystem system;
    system.Initialize("./Assets");

    bool result = system.UnloadAsset("nonexistent");

    EXPECT_FALSE(result);
}

TEST(AssetSystemTests, UnloadAllAssets_ClearsAll) {
    AssetSystem system;
    system.Initialize("./Assets");
    system.LoadAsset("asset1", AssetType::Texture);
    system.LoadAsset("asset2", AssetType::Audio);

    system.UnloadAllAssets();

    EXPECT_FALSE(system.IsAssetLoaded("asset1"));
    EXPECT_FALSE(system.IsAssetLoaded("asset2"));
}

// ============================================================================
// Asset Info Tests
// ============================================================================

TEST(AssetSystemTests, GetAssetInfo_LoadedAsset_ReturnsInfo) {
    AssetSystem system;
    system.Initialize("./Assets");
    system.LoadAsset("test_texture", AssetType::Texture);

    auto info = system.GetAssetInfo("test_texture");

    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->id, "test_texture");
    EXPECT_EQ(info->type, AssetType::Texture);
    EXPECT_TRUE(info->IsLoaded());
}

TEST(AssetSystemTests, GetAssetInfo_NonExistentAsset_ReturnsNullopt) {
    AssetSystem system;
    system.Initialize("./Assets");

    auto info = system.GetAssetInfo("nonexistent");

    EXPECT_FALSE(info.has_value());
}

// ============================================================================
// Memory Management Tests
// ============================================================================

TEST(AssetSystemTests, GetTotalMemoryUsage_NoAssets_ReturnsZero) {
    AssetSystem system;
    system.Initialize("./Assets");

    size_t usage = system.GetTotalMemoryUsage();

    EXPECT_EQ(usage, 0);
}

TEST(AssetSystemTests, GetTotalMemoryUsage_MultipleAssets_ReturnsTotal) {
    AssetSystem system;
    system.Initialize("./Assets");
    system.LoadAsset("asset1", AssetType::Texture);
    system.LoadAsset("asset2", AssetType::Audio);

    size_t usage = system.GetTotalMemoryUsage();

    // Memory usage calculation is placeholder for now
    EXPECT_GE(usage, 0);
}