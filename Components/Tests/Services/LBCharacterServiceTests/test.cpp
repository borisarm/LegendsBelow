#include "pch.h"

import LB.CharacterService;

using namespace LB::Services::Character;

// ============================================================================
// Character Creation Tests
// ============================================================================

TEST(CharacterServiceTests, CreateCharacter_ValidWarrior_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    CharacterCreationRequest request("TestWarrior", CharacterClass::Warrior, baseAttribs);

    auto characterId = service.CreateCharacter(request);

    ASSERT_TRUE(characterId.has_value());
    EXPECT_GT(characterId.value(), 0);

    auto status = service.GetCharacterStatus(characterId.value());
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->name, "TestWarrior");
    EXPECT_EQ(status->characterClass, CharacterClass::Warrior);
    EXPECT_EQ(status->level, 1);
    EXPECT_TRUE(status->isAlive);
}

TEST(CharacterServiceTests, CreateCharacter_ValidMage_Success) {
    CharacterService service;
    Attributes baseAttribs{8, 10, 8, 14, 12, 10};
    CharacterCreationRequest request("TestMage", CharacterClass::Mage, baseAttribs);

    auto characterId = service.CreateCharacter(request);

    ASSERT_TRUE(characterId.has_value());
    auto status = service.GetCharacterStatus(characterId.value());
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->characterClass, CharacterClass::Mage);
    EXPECT_GT(status->maxManaPoints, 0); // Mages should have mana
}

TEST(CharacterServiceTests, CreateCharacter_ValidRogue_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 14, 10, 10, 10, 12};
    CharacterCreationRequest request("TestRogue", CharacterClass::Rogue, baseAttribs);

    auto characterId = service.CreateCharacter(request);

    ASSERT_TRUE(characterId.has_value());
    auto status = service.GetCharacterStatus(characterId.value());
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->characterClass, CharacterClass::Rogue);
}

TEST(CharacterServiceTests, CreateCharacter_ValidCleric_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 12, 10, 14, 10};
    CharacterCreationRequest request("TestCleric", CharacterClass::Cleric, baseAttribs);

    auto characterId = service.CreateCharacter(request);

    ASSERT_TRUE(characterId.has_value());
    auto status = service.GetCharacterStatus(characterId.value());
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->characterClass, CharacterClass::Cleric);
    EXPECT_GT(status->maxManaPoints, 0); // Clerics should have mana
}

TEST(CharacterServiceTests, CreateCharacter_EmptyName_Fails) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    CharacterCreationRequest request("", CharacterClass::Warrior, baseAttribs);

    auto characterId = service.CreateCharacter(request);

    EXPECT_FALSE(characterId.has_value());
}

TEST(CharacterServiceTests, CreateMultipleCharacters_UniqueIds) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto id1 = service.CreateCharacter(CharacterCreationRequest("Char1", CharacterClass::Warrior, baseAttribs));
    auto id2 = service.CreateCharacter(CharacterCreationRequest("Char2", CharacterClass::Mage, baseAttribs));
    auto id3 = service.CreateCharacter(CharacterCreationRequest("Char3", CharacterClass::Rogue, baseAttribs));

    ASSERT_TRUE(id1.has_value());
    ASSERT_TRUE(id2.has_value());
    ASSERT_TRUE(id3.has_value());
    EXPECT_NE(id1.value(), id2.value());
    EXPECT_NE(id2.value(), id3.value());
    EXPECT_NE(id1.value(), id3.value());
}

// ============================================================================
// Experience and Progression Tests
// ============================================================================

TEST(CharacterServiceTests, AwardExperience_NoLevelUp_ExperienceIncreases) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto result = service.AwardExperience(characterId.value(), 50);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->levelIncreased);

    auto status = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(status->currentExperience, 50);
    EXPECT_EQ(status->level, 1);
}

TEST(CharacterServiceTests, AwardExperience_LevelUp_StatsIncrease) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto statusBefore = service.GetCharacterStatus(characterId.value());
    auto hpBefore = statusBefore->maxHitPoints;

    auto result = service.AwardExperience(characterId.value(), 150); // Enough to reach level 2

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->levelIncreased);
    EXPECT_EQ(result->newLevel, 2);
    EXPECT_GT(result->hitPointIncrease, 0);

    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(statusAfter->level, 2);
    EXPECT_GT(statusAfter->maxHitPoints, hpBefore);
    EXPECT_EQ(statusAfter->currentHitPoints, statusAfter->maxHitPoints); // Level up restores health
}

TEST(CharacterServiceTests, AwardExperience_MultipleLevel_ProgressionWorks) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto result = service.AwardExperience(characterId.value(), 500); // Multiple levels

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->levelIncreased);
    EXPECT_GT(result->newLevel, 2);
}

TEST(CharacterServiceTests, AwardExperience_InvalidCharacter_Fails) {
    CharacterService service;

    auto result = service.AwardExperience(999, 100);

    EXPECT_FALSE(result.has_value());
}

TEST(CharacterServiceTests, AwardExperience_NegativeExperience_Fails) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto result = service.AwardExperience(characterId.value(), -50);

    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Damage and Healing Tests
// ============================================================================

TEST(CharacterServiceTests, DamageCharacter_ValidDamage_HealthDecreases) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto statusBefore = service.GetCharacterStatus(characterId.value());
    auto hpBefore = statusBefore->currentHitPoints;

    bool success = service.DamageCharacter(characterId.value(), 5);

    EXPECT_TRUE(success);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_LT(statusAfter->currentHitPoints, hpBefore);
}

TEST(CharacterServiceTests, DamageCharacter_WithReduction_ReducedDamage) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto statusBefore = service.GetCharacterStatus(characterId.value());
    auto hpBefore = statusBefore->currentHitPoints;

    bool success = service.DamageCharacter(characterId.value(), 10, 0.5f); // 50% reduction

    EXPECT_TRUE(success);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    auto actualDamage = hpBefore - statusAfter->currentHitPoints;
    EXPECT_LE(actualDamage, 5); // Damage should be reduced
}

TEST(CharacterServiceTests, DamageCharacter_Lethal_CharacterDies) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto status = service.GetCharacterStatus(characterId.value());
    service.DamageCharacter(characterId.value(), status->maxHitPoints + 10);

    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_FALSE(statusAfter->isAlive);
}

TEST(CharacterServiceTests, HealCharacter_ValidHeal_HealthIncreases) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    service.DamageCharacter(characterId.value(), 5);
    auto statusBefore = service.GetCharacterStatus(characterId.value());
    auto hpBefore = statusBefore->currentHitPoints;

    int32_t healed = service.HealCharacter(characterId.value(), 3);

    EXPECT_EQ(healed, 3);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(statusAfter->currentHitPoints, hpBefore + 3);
}

TEST(CharacterServiceTests, HealCharacter_OverMax_CapsAtMaxHP) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    service.DamageCharacter(characterId.value(), 5);
    auto status = service.GetCharacterStatus(characterId.value());
    int32_t healed = service.HealCharacter(characterId.value(), 100);

    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(statusAfter->currentHitPoints, statusAfter->maxHitPoints);
    EXPECT_LE(healed, 5); // Can only heal the damage taken
}

// ============================================================================
// Movement Tests
// ============================================================================

TEST(CharacterServiceTests, MoveCharacter_ValidDistance_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    Position newPos{2, 2};
    bool success = service.MoveCharacter(characterId.value(), newPos);

    EXPECT_TRUE(success);
    auto status = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(status->position.x, 2);
    EXPECT_EQ(status->position.y, 2);
}

TEST(CharacterServiceTests, MoveCharacter_TooFar_Fails) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    Position newPos{100, 100};
    bool success = service.MoveCharacter(characterId.value(), newPos);

    EXPECT_FALSE(success);
    auto status = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(status->position.x, 0); // Should remain at starting position
    EXPECT_EQ(status->position.y, 0);
}

// ============================================================================
// Rest and Mana Tests
// ============================================================================

TEST(CharacterServiceTests, RestCharacter_FullRest_RestoresAll) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 14, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    service.DamageCharacter(characterId.value(), 5);
    service.ConsumeMana(characterId.value(), 5);

    bool success = service.RestCharacter(characterId.value(), true);

    EXPECT_TRUE(success);
    auto status = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(status->currentHitPoints, status->maxHitPoints);
    EXPECT_EQ(status->currentManaPoints, status->maxManaPoints);
}

TEST(CharacterServiceTests, RestCharacter_ShortRest_PartialRestore) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 14, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    service.DamageCharacter(characterId.value(), 6);
    auto statusBefore = service.GetCharacterStatus(characterId.value());

    bool success = service.RestCharacter(characterId.value(), false);

    EXPECT_TRUE(success);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_GE(statusAfter->currentHitPoints, statusBefore->currentHitPoints);
    EXPECT_LT(statusAfter->currentHitPoints, statusAfter->maxHitPoints); // Not fully restored
}

TEST(CharacterServiceTests, ConsumeMana_SufficientMana_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 14, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto statusBefore = service.GetCharacterStatus(characterId.value());
    auto manaBefore = statusBefore->currentManaPoints;

    bool success = service.ConsumeMana(characterId.value(), 5);

    EXPECT_TRUE(success);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(statusAfter->currentManaPoints, manaBefore - 5);
}

TEST(CharacterServiceTests, ConsumeMana_InsufficientMana_Fails) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 14, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto status = service.GetCharacterStatus(characterId.value());
    bool success = service.ConsumeMana(characterId.value(), status->maxManaPoints + 10);

    EXPECT_FALSE(success);
}

TEST(CharacterServiceTests, RestoreMana_ValidAmount_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 14, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    service.ConsumeMana(characterId.value(), 10);
    auto statusBefore = service.GetCharacterStatus(characterId.value());

    int32_t restored = service.RestoreMana(characterId.value(), 5);

    EXPECT_EQ(restored, 5);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_EQ(statusAfter->currentManaPoints, statusBefore->currentManaPoints + 5);
}

// ============================================================================
// Character Query Tests
// ============================================================================

TEST(CharacterServiceTests, GetAllCharacterIds_MultipleCharacters_ReturnsAll) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    service.CreateCharacter(CharacterCreationRequest("Char1", CharacterClass::Warrior, baseAttribs));
    service.CreateCharacter(CharacterCreationRequest("Char2", CharacterClass::Mage, baseAttribs));
    service.CreateCharacter(CharacterCreationRequest("Char3", CharacterClass::Rogue, baseAttribs));

    auto ids = service.GetAllCharacterIds();

    EXPECT_EQ(ids.size(), size_t(3));
}

TEST(CharacterServiceTests, GetCharactersByClass_FiltersByClass) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    service.CreateCharacter(CharacterCreationRequest("Warrior1", CharacterClass::Warrior, baseAttribs));
    service.CreateCharacter(CharacterCreationRequest("Mage1", CharacterClass::Mage, baseAttribs));
    service.CreateCharacter(CharacterCreationRequest("Warrior2", CharacterClass::Warrior, baseAttribs));

    auto warriors = service.GetCharactersByClass(CharacterClass::Warrior);
    auto mages = service.GetCharactersByClass(CharacterClass::Mage);

    EXPECT_EQ(warriors.size(), size_t(2));
    EXPECT_EQ(mages.size(), size_t(1));
}

TEST(CharacterServiceTests, GetAliveCharacters_ExcludesDead) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto id1 = service.CreateCharacter(CharacterCreationRequest("Char1", CharacterClass::Warrior, baseAttribs));
    auto id2 = service.CreateCharacter(CharacterCreationRequest("Char2", CharacterClass::Mage, baseAttribs));

    auto status = service.GetCharacterStatus(id1.value());
    service.DamageCharacter(id1.value(), status->maxHitPoints + 10); // Kill character

    auto aliveChars = service.GetAliveCharacters();

    EXPECT_EQ(aliveChars.size(), size_t(1));
}

TEST(CharacterServiceTests, HasCharacter_ExistingCharacter_ReturnsTrue) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));

    EXPECT_TRUE(service.HasCharacter(characterId.value()));
    EXPECT_FALSE(service.HasCharacter(999));
}

TEST(CharacterServiceTests, RemoveCharacter_ExistingCharacter_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    bool removed = service.RemoveCharacter(characterId.value());

    EXPECT_TRUE(removed);
    EXPECT_FALSE(service.HasCharacter(characterId.value()));
}

TEST(CharacterServiceTests, GetCharacterCount_TracksCount) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    EXPECT_EQ(service.GetCharacterCount(), size_t(0));

    auto id1 = service.CreateCharacter(CharacterCreationRequest("Char1", CharacterClass::Warrior, baseAttribs));
    EXPECT_EQ(service.GetCharacterCount(), size_t(1));

    auto id2 = service.CreateCharacter(CharacterCreationRequest("Char2", CharacterClass::Mage, baseAttribs));
    EXPECT_EQ(service.GetCharacterCount(), size_t(2));

    service.RemoveCharacter(id1.value());
    EXPECT_EQ(service.GetCharacterCount(), size_t(1));
}

// ============================================================================
// Ability and Revival Tests
// ============================================================================

TEST(CharacterServiceTests, LearnAbility_ValidAbility_Success) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    bool learned = service.LearnAbility(characterId.value(), "Power Attack");

    EXPECT_TRUE(learned);
}

TEST(CharacterServiceTests, ReviveCharacter_DeadCharacter_BecomesAlive) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto status = service.GetCharacterStatus(characterId.value());
    service.DamageCharacter(characterId.value(), status->maxHitPoints + 10);

    bool revived = service.ReviveCharacter(characterId.value(), 0.5f);

    EXPECT_TRUE(revived);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_TRUE(statusAfter->isAlive);
    EXPECT_GT(statusAfter->currentHitPoints, 0);
}

TEST(CharacterServiceTests, ReviveCharacter_AliveCharacter_Fails) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    bool revived = service.ReviveCharacter(characterId.value(), 0.5f);

    EXPECT_FALSE(revived);
}

TEST(CharacterServiceTests, ClearAll_RemovesAllCharacters) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    service.CreateCharacter(CharacterCreationRequest("Char1", CharacterClass::Warrior, baseAttribs));
    service.CreateCharacter(CharacterCreationRequest("Char2", CharacterClass::Mage, baseAttribs));
    EXPECT_EQ(service.GetCharacterCount(), size_t(2));

    service.ClearAll();

    EXPECT_EQ(service.GetCharacterCount(), size_t(0));
}

// ============================================================================
// Class-Specific Tests
// ============================================================================

TEST(CharacterServiceTests, WarriorClass_HasHighHP) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto warriorId = service.CreateCharacter(CharacterCreationRequest("Warrior", CharacterClass::Warrior, baseAttribs));
    auto mageId = service.CreateCharacter(CharacterCreationRequest("Mage", CharacterClass::Mage, baseAttribs));

    auto warriorStatus = service.GetCharacterStatus(warriorId.value());
    auto mageStatus = service.GetCharacterStatus(mageId.value());

    EXPECT_GT(warriorStatus->maxHitPoints, mageStatus->maxHitPoints);
}

TEST(CharacterServiceTests, MageClass_HasHighMana) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto warriorId = service.CreateCharacter(CharacterCreationRequest("Warrior", CharacterClass::Warrior, baseAttribs));
    auto mageId = service.CreateCharacter(CharacterCreationRequest("Mage", CharacterClass::Mage, baseAttribs));

    auto warriorStatus = service.GetCharacterStatus(warriorId.value());
    auto mageStatus = service.GetCharacterStatus(mageId.value());

    EXPECT_GT(mageStatus->maxManaPoints, warriorStatus->maxManaPoints);
}

TEST(CharacterServiceTests, RogueClass_HasHighDexterity) {
    CharacterService service;
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    CharacterCreationRequest request("TestRogue", CharacterClass::Rogue, baseAttribs);

    auto characterId = service.CreateCharacter(request);
    auto status = service.GetCharacterStatus(characterId.value());

    EXPECT_GT(status->currentAttributes.dexterity, baseAttribs.dexterity);
}
// ============================================================================
// Movement Speed Tests
// ============================================================================

namespace {
    CharacterId CreateTestCharacter(CharacterService& service, CharacterClass characterClass, int32_t dexterity) {
        Attributes attributes{10, dexterity, 10, 10, 10, 10};
        CharacterCreationRequest request("Mover", characterClass, attributes);
        return *service.CreateCharacter(request);
    }

    // Expected speed from the created character's actual (class-adjusted) DEX
    float ExpectedSpeed(const CharacterService& service, CharacterId id, float classBaseSpeed) {
        const auto& character = service.GetCharacter(id)->get();
        return classBaseSpeed + static_cast<float>(character.GetAttributes().GetDexterityModifier()) * 0.1f;
    }
}

TEST(CharacterServiceTests, GetMovementSpeed_UnknownCharacter_Nullopt) {
    CharacterService service;

    EXPECT_FALSE(service.GetMovementSpeed(999).has_value());
}

TEST(CharacterServiceTests, GetMovementSpeed_Warrior_UsesBaseSpeedAndDex) {
    CharacterService service;
    auto id = CreateTestCharacter(service, CharacterClass::Warrior, 14);

    auto speed = service.GetMovementSpeed(id);

    ASSERT_TRUE(speed.has_value());
    EXPECT_FLOAT_EQ(*speed, ExpectedSpeed(service, id, 4.0f));
}

TEST(CharacterServiceTests, GetMovementSpeed_HigherDex_Faster) {
    CharacterService service;
    auto slow = CreateTestCharacter(service, CharacterClass::Warrior, 8);
    auto fast = CreateTestCharacter(service, CharacterClass::Warrior, 18);

    EXPECT_GT(*service.GetMovementSpeed(fast), *service.GetMovementSpeed(slow));
}

TEST(CharacterServiceTests, GetMovementSpeed_Rogue_FasterBaseThanWarrior) {
    CharacterService service;
    auto rogue = CreateTestCharacter(service, CharacterClass::Rogue, 12);

    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(rogue), ExpectedSpeed(service, rogue, 5.0f));
}

TEST(CharacterServiceTests, GetMovementSpeed_NeverBelowMinimum) {
    CharacterService service;
    auto id = CreateTestCharacter(service, CharacterClass::Mage, 1);

    EXPECT_GE(*service.GetMovementSpeed(id), 1.0f);
}

// ============================================================================
// Placement Tests
// ============================================================================

TEST(CharacterServiceTests, PlaceCharacter_FarPosition_BypassesMoveValidation) {
    CharacterService service;
    auto id = CreateTestCharacter(service, CharacterClass::Warrior, 10);
    const Position far{50, -40};

    EXPECT_FALSE(service.MoveCharacter(id, far));  // too far for one move
    EXPECT_TRUE(service.PlaceCharacter(id, far));

    EXPECT_EQ(service.GetCharacter(id)->get().GetPosition(), far);
}

TEST(CharacterServiceTests, PlaceCharacter_UnknownCharacter_Fails) {
    CharacterService service;

    EXPECT_FALSE(service.PlaceCharacter(999, Position{1, 1}));
}
