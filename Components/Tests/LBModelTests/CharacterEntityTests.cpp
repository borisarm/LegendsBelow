#include <gtest/gtest.h>

import std;
import LB.Domain;

using namespace LB::Domain;

class CharacterEntityTests : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test attributes
        testAttributes = Attributes{
            15, // strength
            14, // dexterity 
            13, // constitution
            12, // intelligence
            11, // wisdom
            10  // charisma
        };
        
        testPosition = Position(5, 10);
    }

    Attributes testAttributes;
    Position testPosition;
};

// ========================================
// CHARACTER CREATION TESTS
// ========================================

TEST_F(CharacterEntityTests, CanCreateWarriorCharacter) {
    Character warrior(1, "Thorin", CharacterClass::Warrior, testAttributes, 1, 0, testPosition);
    
    EXPECT_EQ(warrior.GetId(), 1);
    EXPECT_EQ(warrior.GetName(), "Thorin");
    EXPECT_EQ(warrior.GetCharacterClass(), CharacterClass::Warrior);
    EXPECT_EQ(warrior.GetLevel(), 1);
    EXPECT_EQ(warrior.GetExperience(), 0);
    EXPECT_EQ(warrior.GetPosition(), testPosition);
    EXPECT_TRUE(warrior.IsAlive());
    EXPECT_TRUE(warrior.IsConscious());
    EXPECT_FALSE(warrior.IsDefeated());
}

TEST_F(CharacterEntityTests, CanCreateRogueCharacter) {
    Character rogue(2, "Shadow", CharacterClass::Rogue, testAttributes);
    
    EXPECT_EQ(rogue.GetCharacterClass(), CharacterClass::Rogue);
    EXPECT_EQ(rogue.GetLevel(), 1);
    EXPECT_TRUE(rogue.CanUseStealthAttacks());
    EXPECT_FALSE(rogue.CanWearHeavyArmor());
}

TEST_F(CharacterEntityTests, CanCreateMageCharacter) {
    Character mage(3, "Gandalf", CharacterClass::Mage, testAttributes);
    
    EXPECT_EQ(mage.GetCharacterClass(), CharacterClass::Mage);
    EXPECT_TRUE(mage.CanCastSpells());
    EXPECT_GT(mage.GetMaxManaPoints(), 0); // Mages should have mana
}

TEST_F(CharacterEntityTests, CanCreateClericCharacter) {
    Character cleric(4, "Pelor", CharacterClass::Cleric, testAttributes);
    
    EXPECT_EQ(cleric.GetCharacterClass(), CharacterClass::Cleric);
    EXPECT_TRUE(cleric.CanCastSpells());
    EXPECT_TRUE(cleric.CanChannelDivineEnergy());
    EXPECT_GT(cleric.GetMaxManaPoints(), 0); // Clerics should have mana
}

TEST_F(CharacterEntityTests, CanCreateCharacterWithExplicitValues) {
    Character custom(5, "Custom", CharacterClass::Warrior, testAttributes, 
                     5, 1000, 100, 50, testPosition);
    
    EXPECT_EQ(custom.GetLevel(), 5);
    EXPECT_EQ(custom.GetExperience(), 1000);
    EXPECT_EQ(custom.GetMaxHitPoints(), 100);
    EXPECT_EQ(custom.GetMaxManaPoints(), 50);
    EXPECT_EQ(custom.GetCurrentHitPoints(), 100);
    EXPECT_EQ(custom.GetCurrentManaPoints(), 50);
}

// ========================================
// CHARACTER CLASS SPECIFIC TESTS
// ========================================

TEST_F(CharacterEntityTests, WarriorHasHighHealth) {
    Character warrior(1, "Tank", CharacterClass::Warrior, testAttributes);
    Character mage(2, "Wizard", CharacterClass::Mage, testAttributes);
    
    // Warriors should have more HP than mages
    EXPECT_GT(warrior.GetMaxHitPoints(), mage.GetMaxHitPoints());
    EXPECT_EQ(warrior.GetMaxManaPoints(), 0); // Warriors have no mana
}

TEST_F(CharacterEntityTests, MageHasHighMana) {
    Character mage(1, "Wizard", CharacterClass::Mage, testAttributes);
    Character warrior(2, "Tank", CharacterClass::Warrior, testAttributes);
    
    // Mages should have more MP than warriors
    EXPECT_GT(mage.GetMaxManaPoints(), warrior.GetMaxManaPoints());
    EXPECT_LT(mage.GetMaxHitPoints(), warrior.GetMaxHitPoints());
}

TEST_F(CharacterEntityTests, RogueIsBalanced) {
    Character rogue(1, "Sneaky", CharacterClass::Rogue, testAttributes);
    Character warrior(2, "Tank", CharacterClass::Warrior, testAttributes);
    Character mage(3, "Wizard", CharacterClass::Mage, testAttributes);
    
    // Rogues should be between warriors and mages for HP
    EXPECT_LT(rogue.GetMaxHitPoints(), warrior.GetMaxHitPoints());
    EXPECT_GT(rogue.GetMaxHitPoints(), mage.GetMaxHitPoints());
    
    // Rogues should have some mana but not as much as mages
    EXPECT_GT(rogue.GetMaxManaPoints(), 0);
    EXPECT_LT(rogue.GetMaxManaPoints(), mage.GetMaxManaPoints());
}

// ========================================
// HEALTH MANAGEMENT TESTS
// ========================================

TEST_F(CharacterEntityTests, CanTakeDamage) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    int32_t initialHP = character.GetCurrentHitPoints();
    
    character.TakeDamage(10);
    
    EXPECT_EQ(character.GetCurrentHitPoints(), initialHP - 10);
    EXPECT_TRUE(character.IsAlive());
    EXPECT_TRUE(character.IsConscious());
}

TEST_F(CharacterEntityTests, BecomesUnconsciousAtLowHealth) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    int32_t maxHP = character.GetMaxHitPoints();
    int32_t damageToUnconscious = maxHP - (maxHP / 5); // Leave 20% health
    
    character.TakeDamage(damageToUnconscious);
    
    EXPECT_TRUE(character.IsAlive());
    EXPECT_FALSE(character.IsConscious());
    EXPECT_TRUE(character.IsDefeated());
}

TEST_F(CharacterEntityTests, DiesFromExcessiveDamage) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    int32_t maxHP = character.GetMaxHitPoints();
    
    character.TakeDamage(maxHP + 10); // Overkill
    
    EXPECT_FALSE(character.IsAlive());
    EXPECT_FALSE(character.IsConscious());
    EXPECT_TRUE(character.IsDefeated());
    EXPECT_EQ(character.GetCurrentHitPoints(), 0);
}

TEST_F(CharacterEntityTests, CanHeal) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    
    character.TakeDamage(12);
    int32_t damagedHP = character.GetCurrentHitPoints();
    
    character.Heal(10);
    
    EXPECT_EQ(character.GetCurrentHitPoints(), damagedHP + 10);
    EXPECT_TRUE(character.IsAlive());
}

TEST_F(CharacterEntityTests, HealingRestoresConsciousness) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    int32_t maxHP = character.GetMaxHitPoints();
    
    // Damage to unconsciousness
    character.TakeDamage(maxHP - (maxHP / 5));
    EXPECT_FALSE(character.IsConscious());
    
    // Heal enough to regain consciousness
    character.Heal(maxHP / 2);
    EXPECT_TRUE(character.IsConscious());
}

TEST_F(CharacterEntityTests, HealingCannotExceedMaxHealth) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    int32_t maxHP = character.GetMaxHitPoints();
    
    character.TakeDamage(10);
    character.Heal(1000); // Massive heal
    
    EXPECT_EQ(character.GetCurrentHitPoints(), maxHP);
}

TEST_F(CharacterEntityTests, CanRestoreToMaxHealth) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    character.TakeDamage(12);
    
    character.RestoreToMaxHealth();
    
    EXPECT_EQ(character.GetCurrentHitPoints(), character.GetMaxHitPoints());
    EXPECT_TRUE(character.IsConscious());
}

// ========================================
// MANA MANAGEMENT TESTS
// ========================================

TEST_F(CharacterEntityTests, CanSpendMana) {
    Character mage(1, "Wizard", CharacterClass::Mage, testAttributes);
    int32_t initialMP = mage.GetCurrentManaPoints();
    
    bool success = mage.SpendMana(10);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(mage.GetCurrentManaPoints(), initialMP - 10);
}

TEST_F(CharacterEntityTests, CannotSpendMoreManaThanAvailable) {
    Character mage(1, "Wizard", CharacterClass::Mage, testAttributes);
    int32_t maxMP = mage.GetMaxManaPoints();
    
    bool success = mage.SpendMana(maxMP + 10);
    
    EXPECT_FALSE(success);
    EXPECT_EQ(mage.GetCurrentManaPoints(), maxMP); // Should be unchanged
}

TEST_F(CharacterEntityTests, CanRestoreMana) {
    Character mage(1, "Wizard", CharacterClass::Mage, testAttributes);
    mage.SpendMana(20);
    int32_t spentMP = mage.GetCurrentManaPoints();
    
    mage.RestoreMana(10);
    
    EXPECT_EQ(mage.GetCurrentManaPoints(), spentMP + 10);
}

TEST_F(CharacterEntityTests, ManaRestorationCannotExceedMax) {
    Character mage(1, "Wizard", CharacterClass::Mage, testAttributes);
    int32_t maxMP = mage.GetMaxManaPoints();
    
    mage.SpendMana(10);
    mage.RestoreMana(1000); // Massive restoration
    
    EXPECT_EQ(mage.GetCurrentManaPoints(), maxMP);
}

// ========================================
// ATTRIBUTE AND CALCULATED STATS TESTS
// ========================================

TEST_F(CharacterEntityTests, AttributesAffectCalculatedStats) {
    // High constitution character should have more HP
    Attributes highCon = testAttributes;
    highCon.constitution = 20;
    Character tankyCon(1, "Tank", CharacterClass::Warrior, highCon);
    
    Attributes lowCon = testAttributes;
    lowCon.constitution = 8;
    Character weakCon(2, "Weak", CharacterClass::Warrior, lowCon);
    
    EXPECT_GT(tankyCon.GetMaxHitPoints(), weakCon.GetMaxHitPoints());
}

TEST_F(CharacterEntityTests, IntelligenceAffectsManaForMages) {
    Attributes highInt = testAttributes;
    highInt.intelligence = 20;
    Character smartMage(1, "Genius", CharacterClass::Mage, highInt);
    
    Attributes lowInt = testAttributes;
    lowInt.intelligence = 8;
    Character dumbMage(2, "Simple", CharacterClass::Mage, lowInt);
    
    EXPECT_GT(smartMage.GetMaxManaPoints(), dumbMage.GetMaxManaPoints());
}

TEST_F(CharacterEntityTests, DexterityAffectsArmorClass) {
    Attributes highDex = testAttributes;
    highDex.dexterity = 20;
    Character nimble(1, "Fast", CharacterClass::Rogue, highDex);
    
    Attributes lowDex = testAttributes;
    lowDex.dexterity = 8;
    Character clumsy(2, "Slow", CharacterClass::Rogue, lowDex);
    
    EXPECT_GT(nimble.GetArmorClass(), clumsy.GetArmorClass());
}

// ========================================
// PROGRESSION TESTS
// ========================================

TEST_F(CharacterEntityTests, CanAwardExperience) {
    Character character(1, "Hero", CharacterClass::Warrior, testAttributes);
    
    character.AwardExperience(100);
    
    EXPECT_EQ(character.GetExperience(), 100);
}

TEST_F(CharacterEntityTests, CanLevelUp) {
    Character character(1, "Hero", CharacterClass::Warrior, testAttributes);
    int32_t initialLevel = character.GetLevel();
    int32_t initialHP = character.GetMaxHitPoints();
    
    character.LevelUp();
    
    EXPECT_EQ(character.GetLevel(), initialLevel + 1);
    EXPECT_GT(character.GetMaxHitPoints(), initialHP); // Should gain HP
    EXPECT_GT(character.GetSkillPoints(), 0); // Should gain skill points
}

TEST_F(CharacterEntityTests, LevelUpRecalculatesStats) {
    Character character(1, "Hero", CharacterClass::Mage, testAttributes);
    int32_t initialMP = character.GetMaxManaPoints();
    
    character.LevelUp();
    
    EXPECT_GT(character.GetMaxManaPoints(), initialMP); // Mages should gain MP
}

TEST_F(CharacterEntityTests, CanLearnAbilities) {
    Character character(1, "Hero", CharacterClass::Warrior, testAttributes);
    
    character.LearnAbility("Power Strike");
    
    EXPECT_TRUE(character.KnowsAbility("Power Strike"));
    EXPECT_FALSE(character.KnowsAbility("Unknown Ability"));
}

TEST_F(CharacterEntityTests, CannotLearnDuplicateAbilities) {
    Character character(1, "Hero", CharacterClass::Warrior, testAttributes);
    
    character.LearnAbility("Power Strike");
    size_t abilitiesBefore = character.GetKnownAbilities().size();
    
    character.LearnAbility("Power Strike"); // Try to learn again
    
    EXPECT_EQ(character.GetKnownAbilities().size(), abilitiesBefore);
}

TEST_F(CharacterEntityTests, CanSpendSkillPoints) {
    Character character(1, "Hero", CharacterClass::Warrior, testAttributes);
    character.AwardSkillPoints(10);
    
    bool success = character.SpendSkillPoints(5);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(character.GetSkillPoints(), 5);
}

TEST_F(CharacterEntityTests, CannotSpendMoreSkillPointsThanAvailable) {
    Character character(1, "Hero", CharacterClass::Warrior, testAttributes);
    character.AwardSkillPoints(5);
    
    bool success = character.SpendSkillPoints(10);
    
    EXPECT_FALSE(success);
    EXPECT_EQ(character.GetSkillPoints(), 5); // Should be unchanged
}

// ========================================
// STARTING ABILITIES TESTS
// ========================================

TEST_F(CharacterEntityTests, WarriorStartsWithWarriorAbilities) {
    Character warrior(1, "Fighter", CharacterClass::Warrior, testAttributes);
    
    EXPECT_TRUE(warrior.KnowsAbility("Weapon Proficiency"));
    EXPECT_TRUE(warrior.KnowsAbility("Heavy Armor Proficiency"));
}

TEST_F(CharacterEntityTests, RogueStartsWithRogueAbilities) {
    Character rogue(1, "Thief", CharacterClass::Rogue, testAttributes);
    
    EXPECT_TRUE(rogue.KnowsAbility("Stealth"));
    EXPECT_TRUE(rogue.KnowsAbility("Lock Picking"));
    EXPECT_TRUE(rogue.KnowsAbility("Trap Detection"));
}

TEST_F(CharacterEntityTests, MageStartsWithMageAbilities) {
    Character mage(1, "Wizard", CharacterClass::Mage, testAttributes);
    
    EXPECT_TRUE(mage.KnowsAbility("Arcane Magic"));
    EXPECT_TRUE(mage.KnowsAbility("Spell Focus"));
    EXPECT_TRUE(mage.KnowsAbility("Cantrips"));
}

TEST_F(CharacterEntityTests, ClericStartsWithClericAbilities) {
    Character cleric(1, "Priest", CharacterClass::Cleric, testAttributes);
    
    EXPECT_TRUE(cleric.KnowsAbility("Divine Magic"));
    EXPECT_TRUE(cleric.KnowsAbility("Channel Divinity"));
    EXPECT_TRUE(cleric.KnowsAbility("Healing Touch"));
}

// ========================================
// PERCENTAGE CALCULATION TESTS  
// ========================================

TEST_F(CharacterEntityTests, HealthPercentageIsAccurate) {
    Character character(1, "Test", CharacterClass::Warrior, testAttributes);
    int32_t maxHP = character.GetMaxHitPoints();
    
    // Test full health
    EXPECT_DOUBLE_EQ(character.GetHealthPercentage(), 1.0);
    
    // Test half health
    character.TakeDamage(maxHP / 2);
    EXPECT_NEAR(character.GetHealthPercentage(), 0.5, 0.1);
    
    // Test zero health
    character.TakeDamage(maxHP);
    EXPECT_DOUBLE_EQ(character.GetHealthPercentage(), 0.0);
}

TEST_F(CharacterEntityTests, ManaPercentageIsAccurate) {
    Character mage(1, "Wizard", CharacterClass::Mage, testAttributes);
    int32_t maxMP = mage.GetMaxManaPoints();
    
    // Test full mana
    EXPECT_DOUBLE_EQ(mage.GetManaPercentage(), 1.0);
    
    // Test half mana
    mage.SpendMana(maxMP / 2);
    EXPECT_NEAR(mage.GetManaPercentage(), 0.5, 0.1);
}

// ========================================
// POSITION TESTS
// ========================================

TEST_F(CharacterEntityTests, CanSetPosition) {
    Character character(1, "Traveler", CharacterClass::Rogue, testAttributes);
    Position newPos(15, 25);
    
    character.SetPosition(newPos);
    
    EXPECT_EQ(character.GetPosition(), newPos);
}

// ========================================
// ATTRIBUTE MODIFICATION TESTS
// ========================================

TEST_F(CharacterEntityTests, CanModifyAttributes) {
    Character character(1, "Hero", CharacterClass::Warrior, testAttributes);
    Attributes newAttributes = testAttributes;
    newAttributes.strength = 20;
    int32_t oldMaxHP = character.GetMaxHitPoints();
    
    character.SetAttributes(newAttributes);
    
    EXPECT_EQ(character.GetAttributes().strength, 20);
    // Max HP might change due to constitution modifier
}

TEST_F(CharacterEntityTests, AttributeChangeRecalculatesStats) {
    Character character(1, "Hero", CharacterClass::Mage, testAttributes);
    Attributes newAttributes = testAttributes;
    newAttributes.intelligence = 20; // Much higher intelligence
    int32_t oldMaxMP = character.GetMaxManaPoints();
    
    character.SetAttributes(newAttributes);
    
    EXPECT_GT(character.GetMaxManaPoints(), oldMaxMP);
}