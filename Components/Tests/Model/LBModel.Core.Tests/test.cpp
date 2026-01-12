#include "pch.h"

import LB.Core;

using namespace LB;
using namespace std;



// ============================================================================
// Position Tests
// ============================================================================

TEST(PositionTests, Constructor_DefaultValues_Success) {
    Position pos;

    EXPECT_EQ(pos.x, 0);
    EXPECT_EQ(pos.y, 0);
}

TEST(PositionTests, Constructor_WithValues_Success) {
    Position pos(10, 20);

    EXPECT_EQ(pos.x, 10);
    EXPECT_EQ(pos.y, 20);
}

TEST(PositionTests, Equality_SamePosition_ReturnsTrue) {
    Position pos1(5, 10);
    Position pos2(5, 10);

    EXPECT_TRUE(pos1 == pos2);
}

TEST(PositionTests, Equality_DifferentPosition_ReturnsFalse) {
    Position pos1(5, 10);
    Position pos2(10, 5);

    EXPECT_FALSE(pos1 == pos2);
}

TEST(PositionTests, ManhattanDistance_SamePosition_ReturnsZero) {
    Position pos1(5, 5);
    Position pos2(5, 5);

    int32_t distance = pos1.ManhattanDistance(pos2);

    EXPECT_EQ(distance, 0);
}

TEST(PositionTests, ManhattanDistance_HorizontalDistance_ReturnsCorrect) {
    Position pos1(0, 0);
    Position pos2(5, 0);

    int32_t distance = pos1.ManhattanDistance(pos2);

    EXPECT_EQ(distance, 5);
}

TEST(PositionTests, ManhattanDistance_VerticalDistance_ReturnsCorrect) {
    Position pos1(0, 0);
    Position pos2(0, 5);

    int32_t distance = pos1.ManhattanDistance(pos2);

    EXPECT_EQ(distance, 5);
}

TEST(PositionTests, ManhattanDistance_DiagonalDistance_ReturnsCorrect) {
    Position pos1(0, 0);
    Position pos2(3, 4);

    int32_t distance = pos1.ManhattanDistance(pos2);

    EXPECT_EQ(distance, 7); // 3 + 4
}

TEST(PositionTests, ManhattanDistance_NegativeCoordinates_ReturnsCorrect) {
    Position pos1(-3, -4);
    Position pos2(2, 1);

    int32_t distance = pos1.ManhattanDistance(pos2);

    EXPECT_EQ(distance, 10); // |2-(-3)| + |1-(-4)| = 5 + 5
}

// ============================================================================
// Dice Tests
// ============================================================================

TEST(DiceTests, Constructor_DefaultValues_Success) {
    Dice dice;

    EXPECT_EQ(dice.count, 1);
    EXPECT_EQ(dice.sides, 6);
    EXPECT_EQ(dice.bonus, 0);
}

TEST(DiceTests, GetMaxRoll_2d6Plus3_Returns15) {
    Dice dice{2, 6, 3};

    int32_t max = dice.GetMaxRoll();

    EXPECT_EQ(max, 15); // 2*6 + 3
}

TEST(DiceTests, GetMinRoll_2d6Plus3_Returns5) {
    Dice dice{2, 6, 3};

    int32_t min = dice.GetMinRoll();

    EXPECT_EQ(min, 5); // 2 + 3
}

TEST(DiceTests, GetAverageRoll_2d6Plus3_Returns9) {
    Dice dice{2, 6, 3};

    int32_t avg = dice.GetAverageRoll();

    EXPECT_EQ(avg, 9); // (2*6)/2 + 3 = 6 + 3
}

TEST(DiceTests, GetMaxRoll_1d20_Returns20) {
    Dice dice{1, 20, 0};

    int32_t max = dice.GetMaxRoll();

    EXPECT_EQ(max, 20);
}

TEST(DiceTests, GetMinRoll_1d20_Returns1) {
    Dice dice{1, 20, 0};

    int32_t min = dice.GetMinRoll();

    EXPECT_EQ(min, 1);
}

TEST(DiceTests, GetAverageRoll_1d20_Returns10) {
    Dice dice{1, 20, 0};

    int32_t avg = dice.GetAverageRoll();

    EXPECT_EQ(avg, 10);
}

TEST(DiceTests, GetAverageRoll_3d8Plus5_Returns17) {
    Dice dice{3, 8, 5};

    int32_t avg = dice.GetAverageRoll();

    EXPECT_EQ(avg, 17); // (3*8)/2 + 5 = 12 + 5
}

// ============================================================================
// Attributes Tests
// ============================================================================

TEST(AttributesTests, Constructor_DefaultValues_AllTen) {
    Attributes attribs;

    EXPECT_EQ(attribs.strength, 10);
    EXPECT_EQ(attribs.dexterity, 10);
    EXPECT_EQ(attribs.constitution, 10);
    EXPECT_EQ(attribs.intelligence, 10);
    EXPECT_EQ(attribs.wisdom, 10);
    EXPECT_EQ(attribs.charisma, 10);
}

TEST(AttributesTests, Constructor_CustomValues_Success) {
    Attributes attribs{15, 14, 13, 12, 11, 10};

    EXPECT_EQ(attribs.strength, 15);
    EXPECT_EQ(attribs.dexterity, 14);
    EXPECT_EQ(attribs.constitution, 13);
    EXPECT_EQ(attribs.intelligence, 12);
    EXPECT_EQ(attribs.wisdom, 11);
    EXPECT_EQ(attribs.charisma, 10);
}

TEST(AttributesTests, GetModifier_Score10_Returns0) {
    Attributes attribs;

    int32_t mod = attribs.GetModifier(10);

    EXPECT_EQ(mod, 0);
}

TEST(AttributesTests, GetModifier_Score11_Returns0) {
    Attributes attribs;

    int32_t mod = attribs.GetModifier(11);

    EXPECT_EQ(mod, 0); // (11-10)/2 = 0
}

TEST(AttributesTests, GetModifier_Score12_Returns1) {
    Attributes attribs;

    int32_t mod = attribs.GetModifier(12);

    EXPECT_EQ(mod, 1); // (12-10)/2 = 1
}

TEST(AttributesTests, GetModifier_Score20_Returns5) {
    Attributes attribs;

    int32_t mod = attribs.GetModifier(20);

    EXPECT_EQ(mod, 5); // (20-10)/2 = 5
}

TEST(AttributesTests, GetModifier_Score8_ReturnsMinus1) {
    Attributes attribs;

    int32_t mod = attribs.GetModifier(8);

    EXPECT_EQ(mod, -1); // (8-10)/2 = -1
}

TEST(AttributesTests, GetModifier_Score1_ReturnsMinus4) {
    Attributes attribs;

    int32_t mod = attribs.GetModifier(1);

    EXPECT_EQ(mod, -4); // (1-10)/2 = -4
}

TEST(AttributesTests, GetStrengthModifier_Strength16_Returns3) {
    Attributes attribs{16, 10, 10, 10, 10, 10};

    int32_t mod = attribs.GetStrengthModifier();

    EXPECT_EQ(mod, 3);
}

TEST(AttributesTests, GetDexterityModifier_Dexterity18_Returns4) {
    Attributes attribs{10, 18, 10, 10, 10, 10};

    int32_t mod = attribs.GetDexterityModifier();

    EXPECT_EQ(mod, 4);
}

TEST(AttributesTests, GetConstitutionModifier_Constitution14_Returns2) {
    Attributes attribs{10, 10, 14, 10, 10, 10};

    int32_t mod = attribs.GetConstitutionModifier();

    EXPECT_EQ(mod, 2);
}

TEST(AttributesTests, GetIntelligenceModifier_Intelligence20_Returns5) {
    Attributes attribs{10, 10, 10, 20, 10, 10};

    int32_t mod = attribs.GetIntelligenceModifier();

    EXPECT_EQ(mod, 5);
}

TEST(AttributesTests, GetWisdomModifier_Wisdom8_ReturnsMinus1) {
    Attributes attribs{10, 10, 10, 10, 8, 10};

    int32_t mod = attribs.GetWisdomModifier();

    EXPECT_EQ(mod, -1);
}

TEST(AttributesTests, GetCharismaModifier_Charisma12_Returns1) {
    Attributes attribs{10, 10, 10, 10, 10, 12};

    int32_t mod = attribs.GetCharismaModifier();

    EXPECT_EQ(mod, 1);
}

TEST(AttributesTests, IsValid_AllInRange_ReturnsTrue) {
    Attributes attribs{15, 14, 13, 12, 11, 10};

    bool valid = attribs.IsValid();

    EXPECT_TRUE(valid);
}

TEST(AttributesTests, IsValid_StrengthTooLow_ReturnsFalse) {
    Attributes attribs{0, 10, 10, 10, 10, 10};

    bool valid = attribs.IsValid();

    EXPECT_FALSE(valid);
}

TEST(AttributesTests, IsValid_DexterityTooHigh_ReturnsFalse) {
    Attributes attribs{10, 31, 10, 10, 10, 10};

    bool valid = attribs.IsValid();

    EXPECT_FALSE(valid);
}

TEST(AttributesTests, IsValid_MinimumValues_ReturnsTrue) {
    Attributes attribs{1, 1, 1, 1, 1, 1};

    bool valid = attribs.IsValid();

    EXPECT_TRUE(valid);
}

TEST(AttributesTests, IsValid_MaximumValues_ReturnsTrue) {
    Attributes attribs{30, 30, 30, 30, 30, 30};

    bool valid = attribs.IsValid();

    EXPECT_TRUE(valid);
}

// ============================================================================
// Id Tests
// ============================================================================

TEST(IdTests, InvalidId_IsMaxValue) {
    EXPECT_EQ(InvalidId, std::numeric_limits<Id>::max());
}

TEST(IdTests, ValidId_NotEqualInvalidId) {
    Id validId = 1;

    EXPECT_NE(validId, InvalidId);
}

// ============================================================================
// Entity Tests
// ============================================================================

TEST(EntityTests, Constructor_SetsIdAndName) {
    Entity entity(42, "TestEntity");

    EXPECT_EQ(entity.GetId(), 42);
    EXPECT_EQ(entity.GetName(), "TestEntity");
}

TEST(EntityTests, SetName_UpdatesName) {
    Entity entity(1, "Original");

    entity.SetName("Updated");

    EXPECT_EQ(entity.GetName(), "Updated");
}