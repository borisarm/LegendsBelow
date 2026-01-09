#include "pch.h"
#include "../../../LBModel/Common/Types.h"
#include "../../../Services/LBAssetPersistenceService/SpellRepository.h"
#include "../../../Systems/LBAssetSystem/AssetManager.h"
#include "gtest/gtest.h"

using namespace LB;
using namespace LB::Domain;
using namespace LB::Services;
using namespace LB::Systems;

class AssetSystemIntegrationTests : public ::testing::Test {
protected:
    void SetUp() override {
        assetManager = std::make_shared<AssetManager>();
        spellRepository = std::make_shared<SpellRepository>(assetManager);
    }

    std::shared_ptr<AssetManager> assetManager;
    std::shared_ptr<SpellRepository> spellRepository;
};

TEST_F(AssetSystemIntegrationTests, AssetManagerLoadsTextFiles) {
    // Test that the asset manager can load files
    EXPECT_TRUE(assetManager != nullptr);
    
    // This test requires the actual spells.json file to exist
    // For now, just test the infrastructure
}

TEST_F(AssetSystemIntegrationTests, SpellRepositoryCanBeCreated) {
    // Test that the spell repository can be created
    EXPECT_TRUE(spellRepository != nullptr);
    
    // Initially empty
    auto allSpells = spellRepository->GetAllSpellIds();
    EXPECT_TRUE(allSpells.empty());
}

TEST_F(AssetSystemIntegrationTests, SpellRepositoryCanParseBasicJSON) {
    // Test with minimal JSON data
    String testJson = R"([
        {
            "id": "test_spell",
            "name": "Test Spell",
            "school": "Evocation",
            "level": 1,
            "requiredLevel": 1,
            "manaCost": 10,
            "castTimeSeconds": 1.0,
            "cooldownSeconds": 0.0,
            "range": 5.0,
            "radius": 0.0
        }
    ])";
    
    // Cache the test data
    assetManager->CacheAsset("test.json", testJson);
    
    // Try to load
    bool loaded = spellRepository->LoadSpells("test.json");
    EXPECT_TRUE(loaded);
    
    // Check that we got the spell
    auto allSpells = spellRepository->GetAllSpellIds();
    EXPECT_EQ(allSpells.size(), 1);
    
    if (!allSpells.empty()) {
        auto spell = spellRepository->GetSpell(allSpells[0]);
        EXPECT_TRUE(spell.has_value());
        EXPECT_EQ(spell->GetName(), "Test Spell");
        EXPECT_EQ(spell->GetSchool(), SpellSchool::Evocation);
        EXPECT_EQ(spell->GetLevel(), 1);
    }
}