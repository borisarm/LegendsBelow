#include "gtest/gtest.h"


import LB.CombatService;
import LB.Domain;

using namespace LB::Services::Combat;
using namespace LB::Domain;

// ============================================================================
// Attack Roll Tests
// ============================================================================

TEST(CombatServiceTests, RollAttack_HitsWithHighRoll_Success) {
    CombatService service;

    auto result = service.RollAttack(5, 15, false, false);

    // With +5 bonus, need to roll 10+ on d20 to hit AC 15
    // Can't guarantee outcome due to randomness, but structure should be valid
    EXPECT_GE(result.attackRoll, 1);
    EXPECT_LE(result.attackRoll, 20);
    EXPECT_EQ(result.attackBonus, 5);
    EXPECT_EQ(result.targetArmorClass, 15);
}

TEST(CombatServiceTests, RollAttack_LowAttackBonusHighAC_LikelyMisses) {
    CombatService service;
    int hits = 0;
    int attempts = 100;

    for (int i = 0; i < attempts; ++i) {
        auto result = service.RollAttack(0, 20, false, false);
        if (result.hit) hits++;
    }

    // With +0 vs AC 20, should only hit ~5% of the time (nat 20)
    EXPECT_LE(hits, 15); // Should be around 5, allow some variance
}

TEST(CombatServiceTests, RollAttack_HighAttackBonusLowAC_LikelyHits) {
    CombatService service;
    int hits = 0;
    int attempts = 100;

    for (int i = 0; i < attempts; ++i) {
        auto result = service.RollAttack(10, 5, false, false);
        if (result.hit && !result.criticalMiss) hits++;
    }

    // With +10 vs AC 5, should hit almost always except nat 1
    EXPECT_GE(hits, 90); // Should hit 95% of time
}

TEST(CombatServiceTests, RollAttack_WithAdvantage_IncreasesHitChance) {
    CombatService service;
    int normalHits = 0;
    int advantageHits = 0;
    int attempts = 100;

    for (int i = 0; i < attempts; ++i) {
        auto normal = service.RollAttack(2, 15, false, false);
        auto advantage = service.RollAttack(2, 15, true, false);

        if (normal.hit) normalHits++;
        if (advantage.hit) advantageHits++;
    }

    // Advantage should result in more hits
    EXPECT_GT(advantageHits, normalHits);
}

TEST(CombatServiceTests, RollAttack_WithDisadvantage_DecreasesHitChance) {
    CombatService service;
    int normalHits = 0;
    int disadvantageHits = 0;
    int attempts = 100;

    for (int i = 0; i < attempts; ++i) {
        auto normal = service.RollAttack(5, 12, false, false);
        auto disadvantage = service.RollAttack(5, 12, false, true);

        if (normal.hit) normalHits++;
        if (disadvantage.hit) disadvantageHits++;
    }

    // Disadvantage should result in fewer hits
    EXPECT_LT(disadvantageHits, normalHits);
}

// ============================================================================
// Damage Calculation Tests
// ============================================================================

TEST(CombatServiceTests, CalculateDamage_BasicDamage_CorrectTotal) {
    CombatService service;

    auto result = service.CalculateDamage(10, 3, DamageType::Slashing, false, 0.0f);

    EXPECT_EQ(result.baseDamage, 10);
    EXPECT_EQ(result.bonusDamage, 3);
    EXPECT_EQ(result.totalDamage, 13);
    EXPECT_EQ(result.damageType, DamageType::Slashing);
    EXPECT_FALSE(result.wasCritical);
}

TEST(CombatServiceTests, CalculateDamage_CriticalHit_DoublesDice) {
    CombatService service;

    auto result = service.CalculateDamage(10, 3, DamageType::Slashing, true, 0.0f);

    EXPECT_EQ(result.baseDamage, 10);
    EXPECT_EQ(result.bonusDamage, 3);
    EXPECT_EQ(result.totalDamage, 23); // (10 * 2) + 3 = 23
    EXPECT_TRUE(result.wasCritical);
}

TEST(CombatServiceTests, CalculateDamage_WithReduction_ReducesDamage) {
    CombatService service;

    auto result = service.CalculateDamage(20, 5, DamageType::Slashing, false, 0.5f);

    EXPECT_EQ(result.totalDamage, 12); // 25 * 0.5 = 12.5, rounds to 12
    EXPECT_EQ(result.mitigatedDamage, 13); // 25 * 0.5 = 12.5, mitigated 13
}

TEST(CombatServiceTests, CalculateDamage_MaxReduction_MinimalDamage) {
    CombatService service;

    auto result = service.CalculateDamage(10, 0, DamageType::Slashing, false, 0.99f);

    EXPECT_GT(result.totalDamage, 0); // Should still do some damage
    EXPECT_LT(result.totalDamage, 2); // But very little
}

TEST(CombatServiceTests, CalculateDamage_ZeroDamage_ReturnsZero) {
    CombatService service;

    auto result = service.CalculateDamage(0, 0, DamageType::Slashing, false, 0.0f);

    EXPECT_EQ(result.totalDamage, 0);
}

// ============================================================================
// Damage Reduction Tests
// ============================================================================

TEST(CombatServiceTests, CalculateDamageReduction_PhysicalDamageHighArmor_HighReduction) {
    CombatService service;

    float reduction = service.CalculateDamageReduction(20, DamageType::Slashing);

    EXPECT_GT(reduction, 0.2f);
    EXPECT_LT(reduction, 0.6f);
}

TEST(CombatServiceTests, CalculateDamageReduction_PiercingDamage_LessReduction) {
    CombatService service;

    float slashingReduction = service.CalculateDamageReduction(20, DamageType::Slashing);
    float piercingReduction = service.CalculateDamageReduction(20, DamageType::Piercing);

    EXPECT_LT(piercingReduction, slashingReduction); // Piercing ignores some armor
}

TEST(CombatServiceTests, CalculateDamageReduction_ElementalDamage_MinimalReduction) {
    CombatService service;

    float fireReduction = service.CalculateDamageReduction(20, DamageType::Fire);
    float slashingReduction = service.CalculateDamageReduction(20, DamageType::Slashing);

    EXPECT_LT(fireReduction, slashingReduction); // Elemental ignores most armor
}

TEST(CombatServiceTests, CalculateDamageReduction_SpiritualDamage_NoReduction) {
    CombatService service;

    float holyReduction = service.CalculateDamageReduction(30, DamageType::Holy);
    float shadowReduction = service.CalculateDamageReduction(30, DamageType::Shadow);

    EXPECT_EQ(holyReduction, 0.0f); // Spiritual ignores physical armor
    EXPECT_EQ(shadowReduction, 0.0f);
}

TEST(CombatServiceTests, CalculateDamageReduction_LowArmor_LowReduction) {
    CombatService service;

    float reduction = service.CalculateDamageReduction(10, DamageType::Slashing);

    EXPECT_EQ(reduction, 0.0f); // AC 10 is baseline, no reduction
}

// ============================================================================
// Weapon Damage Tests
// ============================================================================

TEST(CombatServiceTests, CalculateWeaponDamage_WithStrength_AddsModifier) {
    CombatService service;
    Attributes attribs{16, 10, 10, 10, 10, 10}; // STR 16 = +3

    int32_t damage = service.CalculateWeaponDamage(8, attribs, true);

    EXPECT_EQ(damage, 11); // 8 + 3 = 11
}

TEST(CombatServiceTests, CalculateWeaponDamage_WithDexterity_AddsModifier) {
    CombatService service;
    Attributes attribs{10, 16, 10, 10, 10, 10}; // DEX 16 = +3

    int32_t damage = service.CalculateWeaponDamage(6, attribs, false);

    EXPECT_EQ(damage, 9); // 6 + 3 = 9
}

TEST(CombatServiceTests, CalculateWeaponDamage_LowAttribute_MinimumOne) {
    CombatService service;
    Attributes attribs{6, 10, 10, 10, 10, 10}; // STR 6 = -2

    int32_t damage = service.CalculateWeaponDamage(1, attribs, true);

    EXPECT_EQ(damage, 1); // 1 + (-2) = -1, but minimum is 1
}

// ============================================================================
// Spell Damage Tests
// ============================================================================

TEST(CombatServiceTests, CalculateSpellDamage_MageWithIntelligence_AddsBonus) {
    CombatService service;
    Attributes attribs{10, 10, 10, 18, 10, 10}; // INT 18 = +4

    int32_t damage = service.CalculateSpellDamage(12, attribs, CharacterClass::Mage);

    EXPECT_EQ(damage, 16); // 12 + 4 = 16
}

TEST(CombatServiceTests, CalculateSpellDamage_ClericWithWisdom_AddsBonus) {
    CombatService service;
    Attributes attribs{10, 10, 10, 10, 16, 10}; // WIS 16 = +3

    int32_t damage = service.CalculateSpellDamage(10, attribs, CharacterClass::Cleric);

    EXPECT_EQ(damage, 13); // 10 + 3 = 13
}

TEST(CombatServiceTests, CalculateSpellDamage_WarriorNoCasting_NoBonus) {
    CombatService service;
    Attributes attribs{18, 10, 10, 10, 10, 10};

    int32_t damage = service.CalculateSpellDamage(8, attribs, CharacterClass::Warrior);

    EXPECT_EQ(damage, 8); // No spell bonus for warrior
}

TEST(CombatServiceTests, CalculateSpellDamage_RogueLimitedCasting_HalfBonus) {
    CombatService service;
    Attributes attribs{10, 10, 10, 14, 10, 10}; // INT 14 = +2

    int32_t damage = service.CalculateSpellDamage(10, attribs, CharacterClass::Rogue);

    EXPECT_EQ(damage, 11); // 10 + (2/2) = 11
}

// ============================================================================
// Spell Attack and Save Tests
// ============================================================================

TEST(CombatServiceTests, SpellAttackHits_HighBonus_Hits) {
    CombatService service;
    int hits = 0;

    for (int i = 0; i < 100; ++i) {
        if (service.SpellAttackHits(10, 5)) {
            hits++;
        }
    }

    EXPECT_GT(hits, 90); // Should hit most of the time
}

TEST(CombatServiceTests, SpellAttackHits_LowBonus_Misses) {
    CombatService service;
    int hits = 0;

    for (int i = 0; i < 100; ++i) {
        if (service.SpellAttackHits(0, 20)) {
            hits++;
        }
    }

    EXPECT_LT(hits, 10); // Should rarely hit
}

TEST(CombatServiceTests, MakesSavingThrow_HighBonus_Succeeds) {
    CombatService service;
    int successes = 0;

    for (int i = 0; i < 100; ++i) {
        if (service.MakesSavingThrow(12, 8, false)) {
            successes++;
        }
    }

    EXPECT_GT(successes, 70); // Should succeed most of the time
}

TEST(CombatServiceTests, MakesSavingThrow_WithAdvantage_MoreSuccesses) {
    CombatService service;
    int normalSuccesses = 0;
    int advantageSuccesses = 0;

    for (int i = 0; i < 100; ++i) {
        if (service.MakesSavingThrow(15, 2, false)) normalSuccesses++;
        if (service.MakesSavingThrow(15, 2, true)) advantageSuccesses++;
    }

    EXPECT_GT(advantageSuccesses, normalSuccesses);
}

TEST(CombatServiceTests, CalculateSpellSaveDC_HighLevelHighStat_HighDC) {
    CombatService service;
    Attributes attribs{10, 10, 10, 20, 10, 10}; // INT 20 = +5

    int32_t dc = service.CalculateSpellSaveDC(10, attribs, CharacterClass::Mage);

    EXPECT_GE(dc, 18); // 8 + 5(level/2) + 5(INT) = 18
}

TEST(CombatServiceTests, CalculateSpellSaveDC_LowLevelLowStat_LowDC) {
    CombatService service;
    Attributes attribs{10, 10, 10, 10, 10, 10}; // INT 10 = +0

    int32_t dc = service.CalculateSpellSaveDC(1, attribs, CharacterClass::Mage);

    EXPECT_EQ(dc, 8); // 8 + 0 + 0 = 8
}

// ============================================================================
// Area Damage Tests
// ============================================================================

TEST(CombatServiceTests, CalculateAreaDamage_MultipleTargets_AllReceiveDamage) {
    CombatService service;
    std::vector<int32_t> targetACs{12, 15, 18};

    auto results = service.CalculateAreaDamage(20, 5, DamageType::Fire, targetACs);

    ASSERT_EQ(results.size(), size_t(3));
    for (const auto& result : results) {
        EXPECT_GT(result.totalDamage, 0);
        EXPECT_EQ(result.damageType, DamageType::Fire);
        EXPECT_FALSE(result.wasCritical); // AoE doesn't crit
    }
}

TEST(CombatServiceTests, CalculateAreaDamage_DifferentArmor_DifferentReduction) {
    CombatService service;
    std::vector<int32_t> targetACs{10, 20, 30};

    auto results = service.CalculateAreaDamage(30, 0, DamageType::Slashing, targetACs);

    ASSERT_EQ(results.size(), size_t(3));
    // Higher AC should take less damage
    EXPECT_GE(results[0].totalDamage, results[1].totalDamage);
    EXPECT_GE(results[1].totalDamage, results[2].totalDamage);
}

// ============================================================================
// Healing Tests
// ============================================================================

TEST(CombatServiceTests, CalculateHealing_WithWisdom_AddsBonus) {
    CombatService service;
    Attributes attribs{10, 10, 10, 10, 16, 10}; // WIS 16 = +3

    int32_t healing = service.CalculateHealing(10, attribs);

    EXPECT_EQ(healing, 13); // 10 + 3 = 13
}

TEST(CombatServiceTests, CalculateHealing_LowWisdom_MinimumOne) {
    CombatService service;
    Attributes attribs{10, 10, 10, 10, 6, 10}; // WIS 6 = -2

    int32_t healing = service.CalculateHealing(1, attribs);

    EXPECT_EQ(healing, 1); // Minimum 1 healing
}

// ============================================================================
// Combat Condition Tests
// ============================================================================

TEST(CombatServiceTests, HasAdvantage_HiddenAttacker_ReturnsTrue) {
    CombatService service;

    bool advantage = service.HasAdvantage(true, false, false);

    EXPECT_TRUE(advantage);
}

TEST(CombatServiceTests, HasAdvantage_ProneTarget_ReturnsTrue) {
    CombatService service;

    bool advantage = service.HasAdvantage(false, true, false);

    EXPECT_TRUE(advantage);
}

TEST(CombatServiceTests, HasAdvantage_NoConditions_ReturnsFalse) {
    CombatService service;

    bool advantage = service.HasAdvantage(false, false, false);

    EXPECT_FALSE(advantage);
}

TEST(CombatServiceTests, HasDisadvantage_BlindedAttacker_ReturnsTrue) {
    CombatService service;

    bool disadvantage = service.HasDisadvantage(true, false, false);

    EXPECT_TRUE(disadvantage);
}

TEST(CombatServiceTests, HasDisadvantage_ProneAttacker_ReturnsTrue) {
    CombatService service;

    bool disadvantage = service.HasDisadvantage(false, true, false);

    EXPECT_TRUE(disadvantage);
}

// ============================================================================
// Critical Hit Tests
// ============================================================================

TEST(CombatServiceTests, GetCriticalMultiplier_Rogue_HighestMultiplier) {
    CombatService service;

    float rogueMultiplier = service.GetCriticalMultiplier(CharacterClass::Rogue);
    float warriorMultiplier = service.GetCriticalMultiplier(CharacterClass::Warrior);
    float mageMultiplier = service.GetCriticalMultiplier(CharacterClass::Mage);

    EXPECT_GT(rogueMultiplier, warriorMultiplier);
    EXPECT_GT(warriorMultiplier, mageMultiplier);
}

TEST(CombatServiceTests, GetCriticalHitThreshold_Rogue_LowerThreshold) {
    CombatService service;

    int32_t rogueThreshold = service.GetCriticalHitThreshold(CharacterClass::Rogue);
    int32_t mageThreshold = service.GetCriticalHitThreshold(CharacterClass::Mage);

    EXPECT_LT(rogueThreshold, mageThreshold); // Lower threshold = more crits
}

TEST(CombatServiceTests, GetCriticalHitThreshold_Warrior_EnhancedCrits) {
    CombatService service;

    int32_t threshold = service.GetCriticalHitThreshold(CharacterClass::Warrior);

    EXPECT_EQ(threshold, 19); // Warriors crit on 19-20
}

TEST(CombatServiceTests, GetCriticalHitThreshold_StandardClass_NormalThreshold) {
    CombatService service;

    int32_t threshold = service.GetCriticalHitThreshold(CharacterClass::Mage);

    EXPECT_EQ(threshold, 20); // Standard classes crit only on natural 20
}