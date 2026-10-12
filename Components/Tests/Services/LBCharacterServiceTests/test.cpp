#include "pch.h"

import LB.CharacterService;

using namespace LB::Services::Character;

namespace {
    // Fixed class rules for tests (not the shipped classes.json): level 1 base values, then a
    // constant gain per level. Shapes follow design-character-system.md section 3.
    ClassRules MakeRules(float speed, int hp1, int mp1, int melee1, int ranged1, int hpGain, int mpGain, int swordAdjustment) {
        ClassRules rules{};
        rules.baseMovementSpeed = speed;
        for (int level = 1; level <= ClassProgression::MaxLevel; ++level) {
            LB::Character::ClassLevelGains row{};
            row.level = level;
            row.hpGain = level == 1 ? hp1 : hpGain;
            row.mpGain = level == 1 ? mp1 : mpGain;
            row.meleeAttackBonus = level == 1 ? melee1 : (level % 2 == 1 ? 1 : 0);
            row.rangedAttackBonus = level == 1 ? ranged1 : (level % 4 == 0 ? 1 : 0);
            rules.progression.levels.push_back(row);
        }
        rules.weaponAdjustments[WeaponType::Sword] = swordAdjustment;
        rules.weaponAdjustments[WeaponType::Bow] = 1;
        return rules;
    }

    // Primary attributes, 1 automatic + 1 free point per level, caps 25/18, and the approved
    // armour scales (movement, casting): 0.5 signature, 1 trained, 1.5 untrained, 2 restricted
    ClassRules WithGrowthAndArmor(ClassRules rules, AttributeType first, AttributeType second,
                                  std::initializer_list<std::pair<ArmorCategory, ArmorAdjustment>> armour) {
        rules.primaryAttributes = { first, second };
        rules.attributeGrowth = AttributeGrowth{ 1, 1, 25, 18 };
        for (const auto& [category, adjustment] : armour) rules.armorAdjustments[category] = adjustment;
        return rules;
    }

    CharacterService MakeTestService() {
        CharacterService service;
        service.SetClassRules(CharacterClass::Warrior, WithGrowthAndArmor(MakeRules(4.0f, 12, 3, 2, 0, 8, 2, +3),
            AttributeType::Strength, AttributeType::Constitution,
            { {ArmorCategory::Heavy, {0.5f, 1.0f}}, {ArmorCategory::Shield, {0.5f, 1.0f}} }));
        service.SetClassRules(CharacterClass::Rogue, WithGrowthAndArmor(MakeRules(5.0f, 8, 6, 1, 2, 6, 4, +1),
            AttributeType::Dexterity, AttributeType::Intelligence,
            { {ArmorCategory::Light, {0.5f, 1.0f}}, {ArmorCategory::Heavy, {2.0f, 1.0f}} }));
        service.SetClassRules(CharacterClass::Mage, WithGrowthAndArmor(MakeRules(4.0f, 6, 10, 0, 2, 4, 8, -1),
            AttributeType::Intelligence, AttributeType::Wisdom,
            { {ArmorCategory::Light, {1.5f, 1.5f}}, {ArmorCategory::Heavy, {2.0f, 2.0f}} }));
        service.SetClassRules(CharacterClass::Cleric, WithGrowthAndArmor(MakeRules(4.0f, 10, 8, 1, 1, 7, 6, -5),
            AttributeType::Wisdom, AttributeType::Constitution,
            { {ArmorCategory::Medium, {1.0f, 0.5f}}, {ArmorCategory::Heavy, {1.0f, 1.0f}} }));
        return service;
    }
}

// ============================================================================
// Character Creation Tests
// ============================================================================

TEST(CharacterServiceTests, CreateCharacter_ValidWarrior_Success) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 14, 10, 10, 10, 12};
    CharacterCreationRequest request("TestRogue", CharacterClass::Rogue, baseAttribs);

    auto characterId = service.CreateCharacter(request);

    ASSERT_TRUE(characterId.has_value());
    auto status = service.GetCharacterStatus(characterId.value());
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->characterClass, CharacterClass::Rogue);
}

TEST(CharacterServiceTests, CreateCharacter_ValidCleric_Success) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    CharacterCreationRequest request("", CharacterClass::Warrior, baseAttribs);

    auto characterId = service.CreateCharacter(request);

    EXPECT_FALSE(characterId.has_value());
}

TEST(CharacterServiceTests, CreateMultipleCharacters_UniqueIds) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto result = service.AwardExperience(characterId.value(), 500); // Multiple levels

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->levelIncreased);
    EXPECT_GT(result->newLevel, 2);
}

TEST(CharacterServiceTests, AwardExperience_InvalidCharacter_Fails) {
    CharacterService service = MakeTestService();

    auto result = service.AwardExperience(999, 100);

    EXPECT_FALSE(result.has_value());
}

TEST(CharacterServiceTests, AwardExperience_NegativeExperience_Fails) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto status = service.GetCharacterStatus(characterId.value());
    service.DamageCharacter(characterId.value(), status->maxHitPoints + 10);

    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_FALSE(statusAfter->isAlive);
}

TEST(CharacterServiceTests, HealCharacter_ValidHeal_HealthIncreases) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 14, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    // Leave 1 HP so a half-max short rest cannot fully heal (Mage level 1 has few HP)
    const auto maxHitPoints = service.GetCharacterStatus(characterId.value())->maxHitPoints;
    service.DamageCharacter(characterId.value(), maxHitPoints - 1);
    auto statusBefore = service.GetCharacterStatus(characterId.value());

    bool success = service.RestCharacter(characterId.value(), false);

    EXPECT_TRUE(success);
    auto statusAfter = service.GetCharacterStatus(characterId.value());
    EXPECT_GE(statusAfter->currentHitPoints, statusBefore->currentHitPoints);
    EXPECT_LT(statusAfter->currentHitPoints, statusAfter->maxHitPoints); // Not fully restored
}

TEST(CharacterServiceTests, ConsumeMana_SufficientMana_Success) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 14, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Mage, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    auto status = service.GetCharacterStatus(characterId.value());
    bool success = service.ConsumeMana(characterId.value(), status->maxManaPoints + 10);

    EXPECT_FALSE(success);
}

TEST(CharacterServiceTests, RestoreMana_ValidAmount_Success) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    service.CreateCharacter(CharacterCreationRequest("Char1", CharacterClass::Warrior, baseAttribs));
    service.CreateCharacter(CharacterCreationRequest("Char2", CharacterClass::Mage, baseAttribs));
    service.CreateCharacter(CharacterCreationRequest("Char3", CharacterClass::Rogue, baseAttribs));

    auto ids = service.GetAllCharacterIds();

    EXPECT_EQ(ids.size(), size_t(3));
}

TEST(CharacterServiceTests, GetCharactersByClass_FiltersByClass) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto id1 = service.CreateCharacter(CharacterCreationRequest("Char1", CharacterClass::Warrior, baseAttribs));
    auto id2 = service.CreateCharacter(CharacterCreationRequest("Char2", CharacterClass::Mage, baseAttribs));

    auto status = service.GetCharacterStatus(id1.value());
    service.DamageCharacter(id1.value(), status->maxHitPoints + 10); // Kill character

    auto aliveChars = service.GetAliveCharacters();

    EXPECT_EQ(aliveChars.size(), size_t(1));
}

TEST(CharacterServiceTests, HasCharacter_ExistingCharacter_ReturnsTrue) {
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));

    EXPECT_TRUE(service.HasCharacter(characterId.value()));
    EXPECT_FALSE(service.HasCharacter(999));
}

TEST(CharacterServiceTests, RemoveCharacter_ExistingCharacter_Success) {
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    bool removed = service.RemoveCharacter(characterId.value());

    EXPECT_TRUE(removed);
    EXPECT_FALSE(service.HasCharacter(characterId.value()));
}

TEST(CharacterServiceTests, GetCharacterCount_TracksCount) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    bool learned = service.LearnAbility(characterId.value(), "Power Attack");

    EXPECT_TRUE(learned);
}

TEST(CharacterServiceTests, ReviveCharacter_DeadCharacter_BecomesAlive) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestChar", CharacterClass::Warrior, baseAttribs));
    ASSERT_TRUE(characterId.has_value());

    bool revived = service.ReviveCharacter(characterId.value(), 0.5f);

    EXPECT_FALSE(revived);
}

TEST(CharacterServiceTests, ClearAll_RemovesAllCharacters) {
    CharacterService service = MakeTestService();
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
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto warriorId = service.CreateCharacter(CharacterCreationRequest("Warrior", CharacterClass::Warrior, baseAttribs));
    auto mageId = service.CreateCharacter(CharacterCreationRequest("Mage", CharacterClass::Mage, baseAttribs));

    auto warriorStatus = service.GetCharacterStatus(warriorId.value());
    auto mageStatus = service.GetCharacterStatus(mageId.value());

    EXPECT_GT(warriorStatus->maxHitPoints, mageStatus->maxHitPoints);
}

TEST(CharacterServiceTests, MageClass_HasHighMana) {
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto warriorId = service.CreateCharacter(CharacterCreationRequest("Warrior", CharacterClass::Warrior, baseAttribs));
    auto mageId = service.CreateCharacter(CharacterCreationRequest("Mage", CharacterClass::Mage, baseAttribs));

    auto warriorStatus = service.GetCharacterStatus(warriorId.value());
    auto mageStatus = service.GetCharacterStatus(mageId.value());

    EXPECT_GT(mageStatus->maxManaPoints, warriorStatus->maxManaPoints);
}

TEST(CharacterServiceTests, RogueClass_LevelUpRaisesDexterity) {
    CharacterService service = MakeTestService();
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};
    auto characterId = service.CreateCharacter(CharacterCreationRequest("TestRogue", CharacterClass::Rogue, baseAttribs));

    service.AwardExperience(characterId.value(), 100);  // level 2: the automatic point goes to DEX

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
    CharacterService service = MakeTestService();

    EXPECT_FALSE(service.GetMovementSpeed(999).has_value());
}

TEST(CharacterServiceTests, GetMovementSpeed_Warrior_UsesBaseSpeedAndDex) {
    CharacterService service = MakeTestService();
    auto id = CreateTestCharacter(service, CharacterClass::Warrior, 14);

    auto speed = service.GetMovementSpeed(id);

    ASSERT_TRUE(speed.has_value());
    EXPECT_FLOAT_EQ(*speed, ExpectedSpeed(service, id, 4.0f));
}

TEST(CharacterServiceTests, GetMovementSpeed_HigherDex_Faster) {
    CharacterService service = MakeTestService();
    auto slow = CreateTestCharacter(service, CharacterClass::Warrior, 8);
    auto fast = CreateTestCharacter(service, CharacterClass::Warrior, 18);

    EXPECT_GT(*service.GetMovementSpeed(fast), *service.GetMovementSpeed(slow));
}

TEST(CharacterServiceTests, GetMovementSpeed_Rogue_FasterBaseThanWarrior) {
    CharacterService service = MakeTestService();
    auto rogue = CreateTestCharacter(service, CharacterClass::Rogue, 12);

    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(rogue), ExpectedSpeed(service, rogue, 5.0f));
}

TEST(CharacterServiceTests, GetMovementSpeed_NeverBelowMinimum) {
    CharacterService service = MakeTestService();
    auto id = CreateTestCharacter(service, CharacterClass::Mage, 1);

    EXPECT_GE(*service.GetMovementSpeed(id), 1.0f);
}

// ============================================================================
// Placement Tests
// ============================================================================

TEST(CharacterServiceTests, PlaceCharacter_FarPosition_BypassesMoveValidation) {
    CharacterService service = MakeTestService();
    auto id = CreateTestCharacter(service, CharacterClass::Warrior, 10);
    const Position far{50, -40};

    EXPECT_FALSE(service.MoveCharacter(id, far));  // too far for one move
    EXPECT_TRUE(service.PlaceCharacter(id, far));

    EXPECT_EQ(service.GetCharacter(id)->get().GetPosition(), far);
}

TEST(CharacterServiceTests, PlaceCharacter_UnknownCharacter_Fails) {
    CharacterService service = MakeTestService();

    EXPECT_FALSE(service.PlaceCharacter(999, Position{1, 1}));
}

// ============================================================================
// Class Rules Tests (progression tables, weapon adjustments)
// ============================================================================

namespace {
    const Character& Get(const CharacterService& service, CharacterId id) {
        return service.GetCharacter(id)->get();
    }
}

TEST(CharacterServiceRulesTests, CreateCharacter_ClassWithoutRules_Fails) {
    CharacterService unconfigured;
    CharacterCreationRequest request("Nobody", CharacterClass::Warrior, Attributes{10, 10, 10, 10, 10, 10});

    EXPECT_FALSE(unconfigured.CreateCharacter(request).has_value());
}

TEST(CharacterServiceRulesTests, GetClassRules_ConfiguredAndMissing) {
    CharacterService service = MakeTestService();
    CharacterService unconfigured;

    EXPECT_NE(service.GetClassRules(CharacterClass::Mage), nullptr);
    EXPECT_EQ(unconfigured.GetClassRules(CharacterClass::Mage), nullptr);
}

TEST(CharacterServiceRulesTests, StartingHitPoints_TablePlusConModifier) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("Tank", CharacterClass::Warrior, Attributes{10, 10, 14, 10, 10, 10}));

    const auto& warrior = Get(service, id);
    // MaxHP = table HP at level 1 (12) + CON_mod * 1
    EXPECT_EQ(warrior.GetMaxHitPoints(), 12 + warrior.GetAttributes().GetConstitutionModifier());
    EXPECT_EQ(warrior.GetCurrentHitPoints(), warrior.GetMaxHitPoints());
}

TEST(CharacterServiceRulesTests, StartingMana_CasterModifierOnlyForCasters) {
    CharacterService service = MakeTestService();
    auto mageId = *service.CreateCharacter(CharacterCreationRequest("Caster", CharacterClass::Mage, Attributes{10, 10, 10, 16, 10, 10}));
    auto clericId = *service.CreateCharacter(CharacterCreationRequest("Healer", CharacterClass::Cleric, Attributes{10, 10, 10, 10, 16, 10}));
    auto warriorId = *service.CreateCharacter(CharacterCreationRequest("Brute", CharacterClass::Warrior, Attributes{10, 10, 10, 16, 10, 10}));

    const auto& mage = Get(service, mageId);
    const auto& cleric = Get(service, clericId);
    EXPECT_EQ(mage.GetMaxManaPoints(), 10 + mage.GetAttributes().GetIntelligenceModifier());   // INT for Mages
    EXPECT_EQ(cleric.GetMaxManaPoints(), 8 + cleric.GetAttributes().GetWisdomModifier());      // WIS for Clerics
    EXPECT_EQ(Get(service, warriorId).GetMaxManaPoints(), 3);                                  // no caster modifier
}

TEST(CharacterServiceRulesTests, LevelUp_MaxHitPointsFromTable) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("Tank", CharacterClass::Warrior, Attributes{10, 10, 12, 10, 10, 10}));
    const int32_t before = Get(service, id).GetMaxHitPoints();

    auto result = service.AwardExperience(id, 100);  // level 2

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->newLevel, 2);
    const auto& warrior = Get(service, id);
    // MaxHP = (12 + 8) + CON_mod * 2, with the level-up attribute gains applied
    EXPECT_EQ(warrior.GetMaxHitPoints(), 20 + warrior.GetAttributes().GetConstitutionModifier() * 2);
    EXPECT_EQ(result->hitPointIncrease, warrior.GetMaxHitPoints() - before);
    EXPECT_EQ(warrior.GetMaxManaPoints(), 3 + 2);  // Warrior: table MP only
}

TEST(CharacterServiceRulesTests, BeyondMaxLevel_NoMoreTableGains) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("Veteran", CharacterClass::Mage, Attributes{10, 10, 10, 10, 10, 10}));

    service.AwardExperience(id, 1'000'000);

    const auto& mage = Get(service, id);
    ASSERT_GT(mage.GetLevel(), ClassProgression::MaxLevel);
    const auto& rules = *service.GetClassRules(CharacterClass::Mage);
    EXPECT_EQ(mage.GetMaxHitPoints(),
              rules.progression.TotalsAt(ClassProgression::MaxLevel).maxHitPoints + mage.GetAttributes().GetConstitutionModifier() * mage.GetLevel());
}

TEST(CharacterServiceRulesTests, WeaponAdjustment_SwordByClass) {
    CharacterService service = MakeTestService();
    const Attributes attributes{10, 10, 10, 10, 10, 10};
    auto warrior = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, attributes));
    auto rogue = *service.CreateCharacter(CharacterCreationRequest("R", CharacterClass::Rogue, attributes));
    auto mage = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, attributes));
    auto cleric = *service.CreateCharacter(CharacterCreationRequest("C", CharacterClass::Cleric, attributes));

    EXPECT_EQ(*service.GetWeaponAdjustment(warrior, WeaponType::Sword), 3);
    EXPECT_EQ(*service.GetWeaponAdjustment(rogue, WeaponType::Sword), 1);
    EXPECT_EQ(*service.GetWeaponAdjustment(mage, WeaponType::Sword), -1);
    EXPECT_EQ(*service.GetWeaponAdjustment(cleric, WeaponType::Sword), -5);
}

TEST(CharacterServiceRulesTests, WeaponAdjustment_UnlistedCategoryIsZero) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{10, 10, 10, 10, 10, 10}));

    EXPECT_EQ(*service.GetWeaponAdjustment(id, WeaponType::Axe), 0);
    EXPECT_FALSE(service.GetWeaponAdjustment(999, WeaponType::Sword).has_value());
}

TEST(CharacterServiceRulesTests, AttackBonus_MeleeUsesStrengthAndAdjustment) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{14, 10, 10, 10, 10, 10}));
    const auto& warrior = Get(service, id);

    // melee table bonus at level 1 (2) + STR_mod + sword adjustment (+3)
    EXPECT_EQ(*service.GetAttackBonus(id, WeaponType::Sword), 2 + warrior.GetAttributes().GetStrengthModifier() + 3);
}

TEST(CharacterServiceRulesTests, AttackBonus_ClericSwordPenalty) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("C", CharacterClass::Cleric, Attributes{10, 10, 10, 10, 10, 10}));
    const auto& cleric = Get(service, id);

    EXPECT_EQ(*service.GetAttackBonus(id, WeaponType::Sword), 1 + cleric.GetAttributes().GetStrengthModifier() - 5);
}

TEST(CharacterServiceRulesTests, AttackBonus_RangedUsesDexterity) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("R", CharacterClass::Rogue, Attributes{8, 16, 10, 10, 10, 10}));
    const auto& rogue = Get(service, id);

    // ranged table bonus at level 1 (2) + DEX_mod + bow adjustment (+1)
    EXPECT_EQ(*service.GetAttackBonus(id, WeaponType::Bow), 2 + rogue.GetAttributes().GetDexterityModifier() + 1);
}

TEST(CharacterServiceRulesTests, AttackBonus_UnknownCharacter_Nullopt) {
    CharacterService service = MakeTestService();

    EXPECT_FALSE(service.GetAttackBonus(999, WeaponType::Sword).has_value());
}

TEST(CharacterServiceRulesTests, MovementSpeed_FromClassRules) {
    CharacterService service = MakeTestService();
    auto rogue = *service.CreateCharacter(CharacterCreationRequest("R", CharacterClass::Rogue, Attributes{10, 12, 10, 10, 10, 10}));

    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(rogue),
                    5.0f + static_cast<float>(Get(service, rogue).GetAttributes().GetDexterityModifier()) * 0.1f);
}

// ============================================================================
// Attribute Growth Tests
// ============================================================================

TEST(CharacterServiceGrowthTests, CreateCharacter_KeepsStartingAttributes) {
    CharacterService service = MakeTestService();
    const Attributes start{16, 12, 14, 8, 10, 10};

    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, start));

    const auto& attributes = Get(service, id).GetAttributes();
    EXPECT_EQ(attributes.strength, 16);  // no hidden class bonus on top of the class template
    EXPECT_EQ(attributes.constitution, 14);
    EXPECT_EQ(Get(service, id).GetUnspentAttributePoints(), 0);
}

TEST(CharacterServiceGrowthTests, CreateCharacter_ClampsToClassCaps) {
    CharacterService service = MakeTestService();

    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{28, 10, 10, 22, 10, 10}));

    EXPECT_EQ(Get(service, id).GetAttributes().strength, 25);      // primary cap
    EXPECT_EQ(Get(service, id).GetAttributes().intelligence, 18);  // other cap
}

TEST(CharacterServiceGrowthTests, LevelUp_AutoPointsAlternatePrimaries) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{10, 10, 10, 10, 10, 10}));

    auto toLevel2 = service.AwardExperience(id, 100);
    EXPECT_EQ(toLevel2->attributeGains.strength, 1);       // level 2 -> first primary
    EXPECT_EQ(toLevel2->attributeGains.constitution, 0);

    auto toLevel3 = service.AwardExperience(id, 300);      // 400 total: level 3
    ASSERT_EQ(toLevel3->newLevel, 3);
    EXPECT_EQ(toLevel3->attributeGains.constitution, 1);   // level 3 -> second primary

    const auto& attributes = Get(service, id).GetAttributes();
    EXPECT_EQ(attributes.strength, 11);
    EXPECT_EQ(attributes.constitution, 11);
    EXPECT_EQ(attributes.intelligence, 10);                // non-primaries only grow with free points
}

TEST(CharacterServiceGrowthTests, LevelUp_GrantsFreePoints) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{10, 10, 10, 10, 10, 10}));

    auto result = service.AwardExperience(id, 400);  // level 1 -> 3

    EXPECT_EQ(result->attributePointsGained, 2);
    EXPECT_EQ(Get(service, id).GetUnspentAttributePoints(), 2);
}

TEST(CharacterServiceGrowthTests, LevelUp_CappedPrimariesTurnIntoFreePoints) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{25, 10, 25, 10, 10, 10}));

    auto result = service.AwardExperience(id, 100);  // level 2

    EXPECT_EQ(result->attributeGains.strength, 0);
    EXPECT_EQ(result->attributePointsGained, 2);     // 1 free + 1 automatic that had nowhere to go
    EXPECT_EQ(Get(service, id).GetAttributes().strength, 25);
}

TEST(CharacterServiceGrowthTests, SpendAttributePoint_RaisesAttributeAndConsumesPoint) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("R", CharacterClass::Rogue, Attributes{10, 14, 10, 10, 10, 10}));
    service.AwardExperience(id, 100);
    const int32_t charismaBefore = Get(service, id).GetAttributes().charisma;

    ASSERT_TRUE(service.SpendAttributePoint(id, AttributeType::Charisma));  // a non-primary: classes can blur

    EXPECT_EQ(Get(service, id).GetAttributes().charisma, charismaBefore + 1);
    EXPECT_EQ(Get(service, id).GetUnspentAttributePoints(), 0);
}

TEST(CharacterServiceGrowthTests, SpendAttributePoint_WithoutPoints_Fails) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("R", CharacterClass::Rogue, Attributes{10, 10, 10, 10, 10, 10}));

    EXPECT_FALSE(service.SpendAttributePoint(id, AttributeType::Strength));
}

TEST(CharacterServiceGrowthTests, SpendAttributePoint_AtOtherCap_Fails) {
    CharacterService service = MakeTestService();
    // STR is not a Mage primary: capped at 18, so the barrier between classes holds
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{18, 10, 10, 10, 10, 10}));
    service.AwardExperience(id, 100);

    EXPECT_FALSE(service.SpendAttributePoint(id, AttributeType::Strength));
    EXPECT_EQ(*service.GetAttributeCap(id, AttributeType::Strength), 18);
    EXPECT_EQ(*service.GetAttributeCap(id, AttributeType::Intelligence), 25);
    EXPECT_TRUE(service.SpendAttributePoint(id, AttributeType::Intelligence));
}

TEST(CharacterServiceGrowthTests, SpendAttributePoint_ConstitutionRaisesHitPoints) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{10, 10, 11, 10, 10, 10}));
    service.AwardExperience(id, 100);  // level 2: the automatic point goes to STR, CON stays 11
    const auto& before = Get(service, id);
    const int32_t maxBefore = before.GetMaxHitPoints();
    const int32_t modifierBefore = before.GetAttributes().GetConstitutionModifier();

    ASSERT_TRUE(service.SpendAttributePoint(id, AttributeType::Constitution));  // 11 -> 12: modifier +1

    const auto& after = Get(service, id);
    EXPECT_EQ(after.GetAttributes().GetConstitutionModifier(), modifierBefore + 1);
    EXPECT_EQ(after.GetMaxHitPoints(), maxBefore + after.GetLevel());  // CON_mod x level
}

// ============================================================================
// Armour Penalty Tests
// ============================================================================

namespace {
    ArmorPiece HeavyChest() {
        return ArmorPiece{ 7, LB::Combat::EquipmentSlot::Chest, ArmorCategory::Heavy, 0.15f, 0.05f };
    }

    ArmorPiece MediumHead() {
        return ArmorPiece{ 3, LB::Combat::EquipmentSlot::Head, ArmorCategory::Medium, 0.05f, 0.01f };
    }

    float BaseSpeed(const CharacterService& service, CharacterId id, float classSpeed) {
        return classSpeed + static_cast<float>(Get(service, id).GetAttributes().GetDexterityModifier()) * 0.1f;
    }
}

TEST(CharacterServiceArmorTests, NoArmor_NoPenalties) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{10, 10, 10, 10, 10, 10}));

    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(id), BaseSpeed(service, id, 4.0f));
    EXPECT_FLOAT_EQ(*service.GetCastTimeMultiplier(id), 1.0f);
    EXPECT_TRUE(service.GetEquippedArmor(id).empty());
}

TEST(CharacterServiceArmorTests, HeavyArmor_WarriorHalvesMovementPenalty) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{10, 10, 10, 10, 10, 10}));

    ASSERT_TRUE(service.EquipArmor(id, HeavyChest()));

    // 15% penalty x 0.5 (signature) = 7.5%
    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(id), BaseSpeed(service, id, 4.0f) * (1.0f - 0.075f));
    EXPECT_FLOAT_EQ(*service.GetCastTimeMultiplier(id), 1.0f + 0.05f);  // casting scale 1
}

TEST(CharacterServiceArmorTests, HeavyArmor_MageDoublesBothPenalties) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{10, 10, 10, 10, 10, 10}));

    service.EquipArmor(id, HeavyChest());

    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(id), BaseSpeed(service, id, 4.0f) * (1.0f - 0.30f));
    EXPECT_FLOAT_EQ(*service.GetCastTimeMultiplier(id), 1.0f + 0.10f);
}

TEST(CharacterServiceArmorTests, MediumArmor_ClericCastsWithHalfPenalty) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("C", CharacterClass::Cleric, Attributes{10, 10, 10, 10, 10, 10}));

    service.EquipArmor(id, MediumHead());

    EXPECT_FLOAT_EQ(*service.GetCastTimeMultiplier(id), 1.0f + 0.01f * 0.5f);
}

TEST(CharacterServiceArmorTests, PenaltiesAddUpAcrossPieces) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{10, 10, 10, 10, 10, 10}));

    service.EquipArmor(id, HeavyChest());
    service.EquipArmor(id, MediumHead());  // Medium has no entry in the test rules: scale 1

    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(id), BaseSpeed(service, id, 4.0f) * (1.0f - (0.30f + 0.05f)));
    EXPECT_EQ(service.GetEquippedArmor(id).size(), 2u);
}

TEST(CharacterServiceArmorTests, EquipSameSlot_Replaces) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{10, 10, 10, 10, 10, 10}));
    service.EquipArmor(id, HeavyChest());

    ArmorPiece robe{ 9, LB::Combat::EquipmentSlot::Chest, ArmorCategory::Cloth, 0.0f, 0.0f };
    service.EquipArmor(id, robe);

    ASSERT_EQ(service.GetEquippedArmor(id).size(), 1u);
    EXPECT_EQ(service.GetEquippedArmor(id)[0].armorId, 9);
    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(id), BaseSpeed(service, id, 4.0f));
}

TEST(CharacterServiceArmorTests, Unequip_RemovesPenalty) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{10, 10, 10, 10, 10, 10}));
    service.EquipArmor(id, HeavyChest());

    EXPECT_TRUE(service.UnequipArmor(id, LB::Combat::EquipmentSlot::Chest));
    EXPECT_FALSE(service.UnequipArmor(id, LB::Combat::EquipmentSlot::Chest));
    EXPECT_FLOAT_EQ(*service.GetCastTimeMultiplier(id), 1.0f);
}

TEST(CharacterServiceArmorTests, EquipArmor_InvalidTargets_Fail) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{10, 10, 10, 10, 10, 10}));

    EXPECT_FALSE(service.EquipArmor(999, HeavyChest()));
    ArmorPiece noSlot = HeavyChest();
    noSlot.slot = LB::Combat::EquipmentSlot::None;
    EXPECT_FALSE(service.EquipArmor(id, noSlot));
}

TEST(CharacterServiceArmorTests, HugePenalty_SpeedNeverBelowMinimum) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{10, 10, 10, 10, 10, 10}));
    service.EquipArmor(id, ArmorPiece{ 1, LB::Combat::EquipmentSlot::Chest, ArmorCategory::Heavy, 0.9f, 0.0f });  // x2 = 180%

    EXPECT_FLOAT_EQ(*service.GetMovementSpeed(id), 1.0f);
}

TEST(CharacterServiceArmorTests, RemoveCharacter_ForgetsArmor) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{10, 10, 10, 10, 10, 10}));
    service.EquipArmor(id, HeavyChest());

    service.RemoveCharacter(id);

    EXPECT_TRUE(service.GetEquippedArmor(id).empty());
}

// ============================================================================
// Saved Progress Tests
// ============================================================================

TEST(CharacterServiceProgressTests, RestoreProgress_SetsLevelAttributesAndHitPoints) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{16, 12, 14, 8, 10, 10}));
    CharacterService reference = MakeTestService();
    auto other = *reference.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{16, 12, 14, 8, 10, 10}));
    reference.AwardExperience(other, 400);   // level 3
    const auto& expected = Get(reference, other);

    ASSERT_TRUE(service.RestoreProgress(id, expected.GetLevel(), expected.GetExperience(), expected.GetAttributes(), 2));

    const auto& restored = Get(service, id);
    EXPECT_EQ(restored.GetLevel(), 3);
    EXPECT_EQ(restored.GetExperience(), 400);
    EXPECT_EQ(restored.GetAttributes().strength, expected.GetAttributes().strength);
    EXPECT_EQ(restored.GetUnspentAttributePoints(), 2);
    EXPECT_EQ(restored.GetMaxHitPoints(), expected.GetMaxHitPoints());
    EXPECT_EQ(restored.GetCurrentHitPoints(), restored.GetMaxHitPoints());
}

TEST(CharacterServiceProgressTests, RestoreProgress_KeepsTheHealthFraction) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{16, 12, 14, 8, 10, 10}));
    const auto max = Get(service, id).GetMaxHitPoints();
    service.DamageCharacter(id, max / 2);
    const double fraction = static_cast<double>(Get(service, id).GetCurrentHitPoints()) / max;

    ASSERT_TRUE(service.RestoreProgress(id, 5, 800, Get(service, id).GetAttributes(), 0));

    const auto& after = Get(service, id);
    EXPECT_NEAR(static_cast<double>(after.GetCurrentHitPoints()) / after.GetMaxHitPoints(), fraction, 0.1);
}

TEST(CharacterServiceProgressTests, RestoreProgress_RejectsValuesOutsideTheRules) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("M", CharacterClass::Mage, Attributes{10, 10, 10, 16, 10, 10}));
    const Attributes fine{10, 10, 10, 16, 10, 10};

    EXPECT_FALSE(service.RestoreProgress(id, 0, 0, fine, 0));
    EXPECT_FALSE(service.RestoreProgress(id, ClassProgression::MaxLevel + 1, 0, fine, 0));
    EXPECT_FALSE(service.RestoreProgress(id, 2, -5, fine, 0));
    EXPECT_FALSE(service.RestoreProgress(id, 2, 100, fine, -1));
    EXPECT_FALSE(service.RestoreProgress(id, 2, 100, Attributes{19, 10, 10, 16, 10, 10}, 0));   // STR above a Mage's cap
    EXPECT_FALSE(service.RestoreProgress(999, 2, 100, fine, 0));
    EXPECT_EQ(Get(service, id).GetLevel(), 1);
}

TEST(CharacterServiceProgressTests, ExperienceForLevel_MatchesLevelUps) {
    CharacterService service = MakeTestService();
    auto id = *service.CreateCharacter(CharacterCreationRequest("W", CharacterClass::Warrior, Attributes{16, 12, 14, 8, 10, 10}));

    EXPECT_EQ(service.GetExperienceForLevel(1), 0);
    service.AwardExperience(id, service.GetExperienceForLevel(2) - 1);
    EXPECT_EQ(Get(service, id).GetLevel(), 1);
    service.AwardExperience(id, 1);
    EXPECT_EQ(Get(service, id).GetLevel(), 2);
}
