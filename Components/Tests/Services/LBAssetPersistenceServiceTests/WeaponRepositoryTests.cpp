#include "pch.h"
#include "../../../LBModel/Common/Types.h"
#include "../../../Services/LBAssetPersistenceService/WeaponRepository.h"
#include "../../../Systems/LBAssetSystem/AssetManager.h"
#include "gtest/gtest.h"

using namespace LB;
using namespace LB::Domain;
using namespace LB::Services;
using namespace LB::Systems;

class WeaponRepositoryTests : public ::testing::Test {
protected:
    void SetUp() override {
        assetManager = std::make_shared<AssetManager>();
        weaponRepository = std::make_shared<WeaponRepository>(assetManager);
    }

    std::shared_ptr<AssetManager> assetManager;
    std::shared_ptr<WeaponRepository> weaponRepository;
};

TEST_F(WeaponRepositoryTests, WeaponRepositoryCanBeCreated) {
    EXPECT_TRUE(weaponRepository != nullptr);
    
    // Initially empty
    auto allWeapons = weaponRepository->GetAllWeaponIds();
    EXPECT_TRUE(allWeapons.empty());
}

TEST_F(WeaponRepositoryTests, WeaponRepositoryCanParseBasicJSON) {
    // Test with minimal JSON data matching your weapons.json structure
    String testJson = R"([
        {
            "id": "test_sword",
            "name": "Test Sword",
            "category": "Sword",
            "slot": "MainHand",
            "requiredLevel": 1,
            "allowedClasses": ["Warrior","Rogue"],
            "damageDice": "1d8",
            "damageBonus": 1,
            "damageType": ["Slashing"],
            "attackTimeSeconds": 1.0,
            "critRange": "20",
            "critMultiplier": 1.5,
            "attributeScale": "STR",
            "scaleFactor": 1.0,
            "isTwoHanded": false,
            "specialProperties": [],
            "affixSlots": 2
        }
    ])";
    
    // Cache the test data
    assetManager->CacheAsset("test_weapons.json", testJson);
    
    // Try to load
    bool loaded = weaponRepository->LoadWeapons("test_weapons.json");
    EXPECT_TRUE(loaded);
    
    // Check that we got the weapon
    auto allWeapons = weaponRepository->GetAllWeaponIds();
    EXPECT_EQ(allWeapons.size(), 1);
    
    if (!allWeapons.empty()) {
        auto weapon = weaponRepository->GetWeapon(allWeapons[0]);
        EXPECT_TRUE(weapon.has_value());
        EXPECT_EQ(weapon->GetName(), "Test Sword");
        EXPECT_EQ(weapon->GetCategory(), WeaponType::Sword);
        EXPECT_EQ(weapon->GetRequiredLevel(), 1);
        EXPECT_FALSE(weapon->IsTwoHanded());
        EXPECT_TRUE(weapon->CanBeUsedBy(CharacterClass::Warrior));
        EXPECT_TRUE(weapon->CanBeUsedBy(CharacterClass::Rogue));
        EXPECT_FALSE(weapon->CanBeUsedBy(CharacterClass::Mage));
    }
}

TEST_F(WeaponRepositoryTests, WeaponRepositoryCanFilterByType) {
    String testJson = R"([
        {
            "name": "Test Sword",
            "category": "Sword",
            "slot": "MainHand",
            "requiredLevel": 1,
            "allowedClasses": ["Warrior"],
            "damageDice": "1d8",
            "damageBonus": 1,
            "damageType": ["Slashing"],
            "attackTimeSeconds": 1.0,
            "attributeScale": "STR",
            "scaleFactor": 1.0,
            "isTwoHanded": false,
            "affixSlots": 1
        },
        {
            "name": "Test Bow",
            "category": "Bow",
            "slot": "MainHand",
            "requiredLevel": 1,
            "allowedClasses": ["Rogue"],
            "damageDice": "1d6",
            "damageBonus": 0,
            "damageType": ["Piercing"],
            "attackTimeSeconds": 1.2,
            "attributeScale": "DEX",
            "scaleFactor": 1.0,
            "isTwoHanded": false,
            "affixSlots": 1
        }
    ])";
    
    assetManager->CacheAsset("test_weapons.json", testJson);
    weaponRepository->LoadWeapons("test_weapons.json");
    
    // Filter by type
    auto swords = weaponRepository->GetWeaponsByType(WeaponType::Sword);
    auto bows = weaponRepository->GetWeaponsByType(WeaponType::Bow);
    
    EXPECT_EQ(swords.size(), 1);
    EXPECT_EQ(bows.size(), 1);
    
    // Filter by class
    auto warriorWeapons = weaponRepository->GetWeaponsByClass(CharacterClass::Warrior);
    auto rogueWeapons = weaponRepository->GetWeaponsByClass(CharacterClass::Rogue);
    
    EXPECT_EQ(warriorWeapons.size(), 1);
    EXPECT_EQ(rogueWeapons.size(), 1);
}