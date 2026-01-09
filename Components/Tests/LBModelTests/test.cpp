#include "pch.h"
#include "../../LBModel/Enums/CharacterClass.h"
#include "../../LBModel/Enums/DamageType.h"
#include "../../LBModel/Enums/SpellSchool.h"
#include "../../LBModel/Enums/WeaponType.h"
#include "../../LBModel/ValueObjects/Attributes.h"
#include "../../LBModel/ValueObjects/Dice.h"
#include "../../LBModel/ValueObjects/Position.h"
#include "../../LBModel/ValueObjects/StatusEffect.h"
#include "../../LBModel/Entities/Entity.h"
#include "../../LBModel/Entities/Character.h"

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
    EXPECT_STREQ(ToString(DamageType::Radiant), "Radiant");
}

TEST(SpellSchoolTests, ToStringReturnsCorrectValues) {
    EXPECT_STREQ(ToString(SpellSchool::None), "None");
    EXPECT_STREQ(ToString(SpellSchool::Evocation), "Evocation");
    EXPECT_STREQ(ToString(SpellSchool::Frost), "Frost");
    EXPECT_STREQ(ToString(SpellSchool::Holy), "Holy");
}

// ========== Attributes Tests ==========

TEST(AttributesTests, DefaultConstructorCreatesValidAttributes) {
    Attributes attr;
    EXPECT_EQ(attr.Strength(), 10);
    EXPECT_EQ(attr.Dexterity(), 10);
    EXPECT_EQ(attr.Constitution(), 10);
    EXPECT_EQ(attr.Intelligence(), 10);
    EXPECT_EQ(attr.Wisdom(), 10);
    EXPECT_EQ(attr.Charisma(), 10);
}

TEST(AttributesTests, ModifiersCalculatedCorrectly) {
    Attributes attr(18, 16, 14, 12, 10, 8); // High to low
    EXPECT_EQ(attr.StrengthModifier(), 4);      // (18-10)/2 = 4
    EXPECT_EQ(attr.DexterityModifier(), 3);     // (16-10)/2 = 3
    EXPECT_EQ(attr.ConstitutionModifier(), 2);  // (14-10)/2 = 2
    EXPECT_EQ(attr.IntelligenceModifier(), 1);  // (12-10)/2 = 1
    EXPECT_EQ(attr.WisdomModifier(), 0);        // (10-10)/2 = 0
    EXPECT_EQ(attr.CharismaModifier(), -1);     // (8-10)/2 = -1
}

TEST(AttributesTests, ValuesClampedToValidRange) {
    Attributes attr(100, -10, 25, 0, 50, 1); // Extreme values
    EXPECT_EQ(attr.Strength(), 18);     // Clamped to max
    EXPECT_EQ(attr.Dexterity(), 3);     // Clamped to min
    EXPECT_EQ(attr.Constitution(), 18); // Clamped to max
    EXPECT_EQ(attr.Intelligence(), 3);  // Clamped to min
    EXPECT_EQ(attr.Wisdom(), 18);       // Clamped to max
    EXPECT_EQ(attr.Charisma(), 3);      // Clamped to min
}

TEST(AttributesTests, ApplyDeltaCreatesNewInstance) {
    Attributes original(10, 10, 10, 10, 10, 10);
    Attributes modified = original.ApplyDelta(2, 1, 0, -1, 0, 3);

    // Original unchanged
    EXPECT_EQ(original.Strength(), 10);

    // New instance has changes
    EXPECT_EQ(modified.Strength(), 12);
    EXPECT_EQ(modified.Dexterity(), 11);
    EXPECT_EQ(modified.Constitution(), 10);
    EXPECT_EQ(modified.Intelligence(), 9);
    EXPECT_EQ(modified.Charisma(), 13);
}

// ========== Dice Tests ==========

TEST(DiceTests, DefaultConstructorCreates1d6) {
    Dice dice;
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
    EXPECT_EQ(Dice(1, 6, 0).ToString(), "1d6");
    EXPECT_EQ(Dice(2, 8, 3).ToString(), "2d8+3");
    EXPECT_EQ(Dice(1, 20, -2).ToString(), "1d20-2");
}

TEST(DiceTests, PresetDiceHaveCorrectValues) {
    EXPECT_EQ(Dice::D4().Sides(), 4);
    EXPECT_EQ(Dice::D6().Sides(), 6);
    EXPECT_EQ(Dice::D8().Sides(), 8);
    EXPECT_EQ(Dice::D20().Sides(), 20);
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

    EXPECT_EQ(pos1.ManhattanDistance(pos2), 7); // |3-0| + |4-0|
    EXPECT_DOUBLE_EQ(pos1.EuclideanDistance(pos2), 5.0); // sqrt(3² + 4²)
}

TEST(PositionTests, AdjacencyDetectionWorks) {
    Position center(5, 5);
    Position adjacent(5, 6);   // North
    Position diagonal(6, 6);   // Northeast
    Position distant(5, 8);    // Far north

    EXPECT_TRUE(center.IsAdjacentTo(adjacent));
    EXPECT_TRUE(center.IsAdjacentTo(diagonal));
    EXPECT_FALSE(center.IsAdjacentTo(distant));
    EXPECT_FALSE(center.IsAdjacentTo(center)); // Not adjacent to self
}

// ========== StatusEffect Tests ==========

TEST(StatusEffectTests, DefaultConstructorCreatesValidEffect) {
    StatusEffect effect(StatusEffectType::Strength);
    EXPECT_EQ(effect.GetType(), StatusEffectType::Strength);
    EXPECT_EQ(effect.GetMagnitude(), 1);
    EXPECT_EQ(effect.GetDuration(), 10.0f);
}

TEST(StatusEffectTests, EffectCategoriesDetectedCorrectly) {
    StatusEffect buff(StatusEffectType::Strength);
    StatusEffect debuff(StatusEffectType::Poison);
    StatusEffect cc(StatusEffectType::Stun);
    StatusEffect dot(StatusEffectType::Burning);

    EXPECT_TRUE(buff.IsBuff());
    EXPECT_FALSE(buff.IsDebuff());

    EXPECT_TRUE(debuff.IsDebuff());
    EXPECT_FALSE(debuff.IsBuff());

    EXPECT_TRUE(cc.IsCrowdControl());
    EXPECT_TRUE(dot.IsDamageOverTime());
}

TEST(StatusEffectTests, DamageTypeAssociationWorks) {
    StatusEffect poison(StatusEffectType::Poison);
    StatusEffect burning(StatusEffectType::Burning);
    StatusEffect strength(StatusEffectType::Strength);

    EXPECT_EQ(poison.GetDamageType(), DamageType::Acid);
    EXPECT_EQ(burning.GetDamageType(), DamageType::Fire);
    EXPECT_EQ(strength.GetDamageType(), std::nullopt);
}

TEST(StatusEffectTests, DurationManagementWorks) {
    StatusEffect effect(StatusEffectType::Slow, 1, 5.0f);
    EXPECT_FALSE(effect.IsExpired());

    StatusEffect reduced = effect.WithReducedDuration(3.0f);
    EXPECT_EQ(reduced.GetDuration(), 2.0f);
    EXPECT_FALSE(reduced.IsExpired());

    StatusEffect expired = effect.WithReducedDuration(10.0f);
    EXPECT_EQ(expired.GetDuration(), 0.0f);
    EXPECT_TRUE(expired.IsExpired());
}

// ========== Entity Tests ==========

TEST(EntityTests, EntitiesHaveUniqueIds) {
    Entity entity1;
    Entity entity2;

    EXPECT_NE(entity1.GetId(), entity2.GetId());
    EXPECT_NE(entity1, entity2);
}

TEST(EntityTests, EntitiesWithSameIdAreEqual) {
    Entity entity1(42);
    Entity entity2(42);

    EXPECT_EQ(entity1.GetId(), entity2.GetId());
    EXPECT_EQ(entity1, entity2);
}

// ========== Character Tests ==========

TEST(CharacterTests, ConstructorInitializesCorrectly) {
    Attributes attr(16, 14, 15, 12, 10, 8);
    Character character(1, "TestHero", CharacterClass::Warrior, attr, 3);

    EXPECT_EQ(character.GetId(), 1);
    EXPECT_EQ(character.GetName(), "TestHero");
    EXPECT_EQ(character.GetCharacterClass(), CharacterClass::Warrior);
    EXPECT_EQ(character.GetLevel(), 3);
    EXPECT_TRUE(character.IsAlive());
}

TEST(CharacterTests, HitPointsCalculatedByClass) {
    Attributes attr(16, 14, 15, 12, 10, 8); // CON modifier = +2
    Character warrior(1, "Warrior", CharacterClass::Warrior, attr, 1);
    Character mage(2, "Mage", CharacterClass::Mage, attr, 1);

    // Warriors should have more HP than mages at same level
    EXPECT_GT(warrior.GetCurrentHitPoints(), mage.GetCurrentHitPoints());
}

TEST(CharacterTests, ManaPointsCalculatedByClass) {
    Attributes attr(10, 10, 10, 16, 16, 10); // INT/WIS modifier = +3
    Character warrior(1, "Warrior", CharacterClass::Warrior, attr, 1);
    Character mage(2, "Mage", CharacterClass::Mage, attr, 1);
    Character cleric(3, "Cleric", CharacterClass::Cleric, attr, 1);

    // Warriors have no mana, mages have more than clerics
    EXPECT_EQ(warrior.GetCurrentManaPoints(), 0);
    EXPECT_GT(mage.GetCurrentManaPoints(), cleric.GetCurrentManaPoints());
    EXPECT_GT(cleric.GetCurrentManaPoints(), 0);
}

TEST(CharacterTests, DamageAndHealingWork) {
    Attributes attr;
    Character character(1, "TestHero", CharacterClass::Warrior, attr);
    int32_t originalHP = character.GetCurrentHitPoints();

    character.TakeDamage(10);
    EXPECT_EQ(character.GetCurrentHitPoints(), originalHP - 10);
    EXPECT_TRUE(character.IsAlive());

    character.Heal(5);
    EXPECT_EQ(character.GetCurrentHitPoints(), originalHP - 5);

    // Test defeat
    character.TakeDamage(1000);
    EXPECT_TRUE(character.IsDefeated());
    EXPECT_FALSE(character.IsAlive());
}

TEST(CharacterTests, LevelUpIncreasesStats) {
    Attributes attr;
    Character character(1, "TestHero", CharacterClass::Warrior, attr, 1);
    int32_t originalHP = character.CalculateMaxHitPoints();

    character.LevelUp();
    EXPECT_EQ(character.GetLevel(), 2);
    EXPECT_GT(character.CalculateMaxHitPoints(), originalHP);
    EXPECT_EQ(character.GetCurrentHitPoints(), character.CalculateMaxHitPoints()); // Full health after level up
}

TEST(CharacterTests, PositionManagementWorks) {
    Attributes attr;
    Character character(1, "TestHero", CharacterClass::Rogue, attr);

    EXPECT_EQ(character.GetPosition(), Position::Origin());

    character.SetPosition(Position(5, 3));
    EXPECT_EQ(character.GetPosition(), Position(5, 3));

    character.Move(Position(2, -1));
    EXPECT_EQ(character.GetPosition(), Position(7, 2));
}

TEST(CharacterTests, StatusEffectsCanBeAdded) {
    Attributes attr;
    Character character(1, "TestHero", CharacterClass::Mage, attr);

    StatusEffect strength(StatusEffectType::Strength, 2, 30.0f);
    character.AddStatusEffect(strength);

    const auto& effects = character.GetStatusEffects();
    EXPECT_EQ(effects.size(), 1);
    EXPECT_EQ(effects[0].GetType(), StatusEffectType::Strength);
}