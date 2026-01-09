#include "pch.h"

// Include individual LBModel headers for minimal dependencies
#include "../../LBModel/Common/Types.h"
#include "../../LBModel/Enums/CharacterClass.h"
#include "../../LBModel/Enums/DamageType.h"
#include "../../LBModel/Enums/SpellSchool.h"
#include "../../LBModel/Enums/SpellCategory.h"
#include "../../LBModel/Enums/WeaponType.h"
#include "../../LBModel/Enums/EquipmentSlot.h"
#include "../../LBModel/Enums/ArmorCategory.h"
#include "../../LBModel/ValueObjects/Attributes.h"
#include "../../LBModel/ValueObjects/Dice.h"
#include "../../LBModel/ValueObjects/Position.h"
#include "../../LBModel/ValueObjects/StatusEffect.h"
#include "../../LBModel/Entities/Entity.h"
#include "../../LBModel/Entities/Character.h"
#include "../../LBModel/Entities/Spell.h"
#include "../../LBModel/Entities/Weapon.h"
#include "../../LBModel/Entities/Armor.h"
#include "../../LBModel/Entities/Item.h"

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

    // ========== New Entity Tests ==========

    // ========== Spell Tests ==========

    TEST(SpellTests, ConstructorInitializesCorrectly) {
        std::vector<SpellCategory> categories = {SpellCategory::Damage};
        Spell spell(1, "Fireball", SpellSchool::Evocation, 2, categories, 
                   CharacterClass::Mage, 5, 25, 1.2f, 6.0f, 10.0f, 3.0f);

        EXPECT_EQ(spell.GetId(), 1);
        EXPECT_EQ(spell.GetName(), "Fireball");
        EXPECT_EQ(spell.GetSchool(), SpellSchool::Evocation);
        EXPECT_EQ(spell.GetLevel(), 2);
        EXPECT_EQ(spell.GetRequiredLevel(), 5);
        EXPECT_EQ(spell.GetManaCost(), 25);
        EXPECT_FLOAT_EQ(spell.GetCastTimeSeconds(), 1.2f);
        EXPECT_FLOAT_EQ(spell.GetCooldownSeconds(), 6.0f);
        EXPECT_FLOAT_EQ(spell.GetRange(), 10.0f);
        EXPECT_FLOAT_EQ(spell.GetRadius(), 3.0f);
    }

    TEST(SpellTests, CategoryChecksWork) {
        std::vector<SpellCategory> damageCategories = {SpellCategory::Damage};
        std::vector<SpellCategory> healCategories = {SpellCategory::Heal};
        std::vector<SpellCategory> mixedCategories = {SpellCategory::Damage, SpellCategory::Control};

        Spell damageSpell(1, "Fireball", SpellSchool::Evocation, 2, damageCategories, 
                         CharacterClass::Mage, 5, 25, 1.2f, 6.0f);
        Spell healSpell(2, "Cure Wounds", SpellSchool::Holy, 1, healCategories, 
                       CharacterClass::Cleric, 1, 15, 0.8f, 2.0f);
        Spell mixedSpell(3, "Glacial Spike", SpellSchool::Frost, 3, mixedCategories, 
                        CharacterClass::Mage, 9, 34, 1.4f, 12.0f);

        EXPECT_TRUE(damageSpell.IsDamageSpell());
        EXPECT_FALSE(damageSpell.IsHealSpell());

        EXPECT_TRUE(healSpell.IsHealSpell());
        EXPECT_FALSE(healSpell.IsDamageSpell());

        EXPECT_TRUE(mixedSpell.IsDamageSpell());
        EXPECT_TRUE(mixedSpell.IsControlSpell());
        EXPECT_FALSE(mixedSpell.IsHealSpell());
    }

    TEST(SpellTests, ClassRestrictionsWork) {
        std::vector<SpellCategory> categories = {SpellCategory::Damage};

        // Class-specific spell
        Spell mageSpell(1, "Fireball", SpellSchool::Evocation, 2, categories, 
                       CharacterClass::Mage, 5, 25, 1.2f, 6.0f);

        // Generic spell (no class restriction)
        Spell genericSpell(2, "Magic Missile", SpellSchool::Evocation, 1, categories, 
                          std::nullopt, 1, 15, 0.5f, 2.0f);

        EXPECT_TRUE(mageSpell.CanBeCastBy(CharacterClass::Mage));
        EXPECT_FALSE(mageSpell.CanBeCastBy(CharacterClass::Warrior));

        EXPECT_TRUE(genericSpell.CanBeCastBy(CharacterClass::Mage));
        EXPECT_TRUE(genericSpell.CanBeCastBy(CharacterClass::Warrior));
        EXPECT_TRUE(genericSpell.CanBeCastBy(CharacterClass::Cleric));
    }

    TEST(SpellTests, DamagePropertiesWork) {
        std::vector<SpellCategory> categories = {SpellCategory::Damage};
        Spell spell(1, "Fireball", SpellSchool::Evocation, 2, categories, 
                   CharacterClass::Mage, 5, 25, 1.2f, 6.0f);

        // Test setting damage properties
        spell.SetDamageProperties(30, 1.5f, DamageType::Fire);

        EXPECT_EQ(spell.GetBaseDamage(), 30);
        EXPECT_FLOAT_EQ(spell.GetPowerCoefficient(), 1.5f);
        EXPECT_EQ(spell.GetDamageType(), DamageType::Fire);
    }

    // ========== Weapon Tests ==========

    TEST(WeaponTests, ConstructorInitializesCorrectly) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Warrior, CharacterClass::Rogue};
        std::vector<DamageType> damageTypes = {DamageType::Slashing};
        Dice damageDice(1, 8, 1); // 1d8+1

        Weapon weapon(1, "Iron Longsword", WeaponType::Sword, EquipmentSlot::MainHand,
                     4, allowedClasses, damageDice, 1, damageTypes, 1.0f,
                     AttributeScale::STR, 1.0f, false, 2);

        EXPECT_EQ(weapon.GetId(), 1);
        EXPECT_EQ(weapon.GetName(), "Iron Longsword");
        EXPECT_EQ(weapon.GetCategory(), WeaponType::Sword);
        EXPECT_EQ(weapon.GetSlot(), EquipmentSlot::MainHand);
        EXPECT_EQ(weapon.GetRequiredLevel(), 4);
        EXPECT_EQ(weapon.GetDamageBonus(), 1);
        EXPECT_EQ(weapon.GetAttributeScale(), AttributeScale::STR);
        EXPECT_FALSE(weapon.IsTwoHanded());
        EXPECT_EQ(weapon.GetAffixSlots(), 2);
        EXPECT_FLOAT_EQ(weapon.GetScaleFactor(), 1.0f);
    }

    TEST(WeaponTests, ClassRestrictionsWork) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Warrior};
        std::vector<DamageType> damageTypes = {DamageType::Slashing};
        Dice damageDice(2, 8);

        Weapon weapon(1, "Greatsword", WeaponType::Sword, EquipmentSlot::MainHand,
                     10, allowedClasses, damageDice, 2, damageTypes, 1.5f,
                     AttributeScale::STR, 1.2f, true, 3);

        EXPECT_TRUE(weapon.CanBeUsedBy(CharacterClass::Warrior));
        EXPECT_FALSE(weapon.CanBeUsedBy(CharacterClass::Mage));
        EXPECT_FALSE(weapon.CanBeUsedBy(CharacterClass::Rogue));
        EXPECT_FALSE(weapon.CanBeUsedBy(CharacterClass::Cleric));
    }

    TEST(WeaponTests, DamageTypesWork) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Warrior};
        std::vector<DamageType> damageTypes = {DamageType::Slashing, DamageType::Piercing};
        Dice damageDice(1, 10);

        Weapon weapon(1, "Halberd", WeaponType::Axe, EquipmentSlot::MainHand,
                     8, allowedClasses, damageDice, 2, damageTypes, 1.4f,
                     AttributeScale::STR, 1.1f, true, 2);

        EXPECT_EQ(weapon.GetPrimaryDamageType(), DamageType::Slashing);
        EXPECT_EQ(weapon.GetDamageTypes().size(), 2);
        EXPECT_TRUE(weapon.IsTwoHanded());
    }

    TEST(WeaponTests, CriticalPropertiesWork) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Rogue};
        std::vector<DamageType> damageTypes = {DamageType::Piercing};
        Dice damageDice(1, 4);

        Weapon weapon(1, "Dagger", WeaponType::Dagger, EquipmentSlot::MainHand,
                     1, allowedClasses, damageDice, 0, damageTypes, 0.7f,
                     AttributeScale::DEX, 1.0f, false, 1);

        // Test setting critical properties
        weapon.SetCriticalProperties(19, 2.0f); // 19-20 crit, 2x multiplier

        EXPECT_EQ(weapon.GetCritRange(), 19);
        EXPECT_FLOAT_EQ(weapon.GetCritMultiplier(), 2.0f);
    }

    // ========== Armor Tests ==========

    TEST(ArmorTests, ConstructorInitializesCorrectly) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Warrior, CharacterClass::Cleric};
        ResistanceMap elementalAR = {{DamageType::Fire, 1}};
        ResistanceMap spiritualAR = {};

        Armor armor(1, "Chain Shirt", EquipmentSlot::Chest, ArmorCategory::Medium,
                   1, allowedClasses, 5, elementalAR, spiritualAR, 8, 0.1f, 0.03f, 2);

        EXPECT_EQ(armor.GetId(), 1);
        EXPECT_EQ(armor.GetName(), "Chain Shirt");
        EXPECT_EQ(armor.GetSlot(), EquipmentSlot::Chest);
        EXPECT_EQ(armor.GetCategory(), ArmorCategory::Medium);
        EXPECT_EQ(armor.GetRequiredLevel(), 1);
        EXPECT_EQ(armor.GetPhysicalAR(), 5);
        EXPECT_EQ(armor.GetWeight(), 8);
        EXPECT_FLOAT_EQ(armor.GetMovementPenalty(), 0.1f);
        EXPECT_FLOAT_EQ(armor.GetCastPenalty(), 0.03f);
        EXPECT_EQ(armor.GetAffixSlots(), 2);
    }

    TEST(ArmorTests, ResistanceSystemWorks) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Mage};
        ResistanceMap elementalAR = {{DamageType::Fire, 2}, {DamageType::Cold, 1}};
        ResistanceMap spiritualAR = {{DamageType::Radiant, 3}};

        Armor armor(1, "Fire Resistant Robe", EquipmentSlot::Chest, ArmorCategory::Cloth,
                   5, allowedClasses, 0, elementalAR, spiritualAR, 2, 0.0f, 0.0f, 1);

        // Physical damage uses physical AR
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Slashing), 0);
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Piercing), 0);
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Bludgeoning), 0);

        // Elemental resistances
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Fire), 2);
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Cold), 1);
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Lightning), 0);

        // Spiritual resistances
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Radiant), 3);
        EXPECT_EQ(armor.GetTotalResistance(DamageType::Necrotic), 0);
    }

    TEST(ArmorTests, ClassRestrictionsWork) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Mage, CharacterClass::Cleric};
        ResistanceMap elementalAR = {};
        ResistanceMap spiritualAR = {};

        Armor armor(1, "Cloth Robe", EquipmentSlot::Chest, ArmorCategory::Cloth,
                   1, allowedClasses, 0, elementalAR, spiritualAR, 2, 0.0f, 0.0f, 1);

        EXPECT_TRUE(armor.CanBeWornBy(CharacterClass::Mage));
        EXPECT_TRUE(armor.CanBeWornBy(CharacterClass::Cleric));
        EXPECT_FALSE(armor.CanBeWornBy(CharacterClass::Warrior));
        EXPECT_FALSE(armor.CanBeWornBy(CharacterClass::Rogue));
    }

    TEST(ArmorTests, ShieldPropertiesWork) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Warrior, CharacterClass::Cleric};
        ResistanceMap elementalAR = {};
        ResistanceMap spiritualAR = {};

        Armor shield(1, "Iron Shield", EquipmentSlot::OffHand, ArmorCategory::Shield,
                    6, allowedClasses, 0, elementalAR, spiritualAR, 8, 0.05f, 0.0f, 2);

        // Test setting shield properties
        shield.SetShieldProperties(3, 20, 10);

        EXPECT_TRUE(shield.IsShield());
        EXPECT_EQ(shield.GetArmorBonus(), 3);
        EXPECT_EQ(shield.GetBlockChance(), 20);
        EXPECT_EQ(shield.GetBlockValue(), 10);
    }

    TEST(ArmorTests, PenaltyChecksWork) {
        std::vector<CharacterClass> allowedClasses = {CharacterClass::Warrior};
        ResistanceMap elementalAR = {};
        ResistanceMap spiritualAR = {};

        Armor heavyArmor(1, "Plate Mail", EquipmentSlot::Chest, ArmorCategory::Heavy,
                        10, allowedClasses, 8, elementalAR, spiritualAR, 25, 0.2f, 0.1f, 3);

        EXPECT_TRUE(heavyArmor.HasMovementPenalty());
        EXPECT_TRUE(heavyArmor.HasCastPenalty());

        Armor clothArmor(2, "Robe", EquipmentSlot::Chest, ArmorCategory::Cloth,
                        1, {CharacterClass::Mage}, 0, elementalAR, spiritualAR, 2, 0.0f, 0.0f, 1);

        EXPECT_FALSE(clothArmor.HasMovementPenalty());
        EXPECT_FALSE(clothArmor.HasCastPenalty());
    }

    // ========== Item Tests ==========

    TEST(ItemTests, ConstructorInitializesCorrectly) {
        Item item(1, "Health Potion", ItemCategory::Consumable, ItemRarity::Common,
                 5, 1, 25, "Restores 50 hit points when consumed");

        EXPECT_EQ(item.GetId(), 1);
        EXPECT_EQ(item.GetName(), "Health Potion");
        EXPECT_EQ(item.GetCategory(), ItemCategory::Consumable);
        EXPECT_EQ(item.GetRarity(), ItemRarity::Common);
        EXPECT_EQ(item.GetMaxStackSize(), 5);
        EXPECT_EQ(item.GetWeight(), 1);
        EXPECT_EQ(item.GetValue(), 25);
        EXPECT_EQ(item.GetDescription(), "Restores 50 hit points when consumed");
    }

    TEST(ItemTests, CategoryChecksWork) {
        Item consumable(1, "Health Potion", ItemCategory::Consumable, ItemRarity::Common,
                       5, 1, 25, "Restores health");
        Item quest(2, "Ancient Key", ItemCategory::Quest, ItemRarity::Epic,
                  1, 1, 0, "Opens the ancient door");
        Item ammo(3, "Iron Arrow", ItemCategory::Ammunition, ItemRarity::Common,
                 50, 1, 2, "Standard arrow");

        EXPECT_TRUE(consumable.IsConsumable());
        EXPECT_FALSE(consumable.IsQuestItem());
        EXPECT_TRUE(consumable.IsStackable());

        EXPECT_TRUE(quest.IsQuestItem());
        EXPECT_FALSE(quest.IsConsumable());
        EXPECT_FALSE(quest.IsStackable());

        EXPECT_TRUE(ammo.IsAmmunition());
        EXPECT_TRUE(ammo.IsStackable());
        EXPECT_FALSE(ammo.IsQuestItem());
    }

    TEST(ItemTests, RaritySystemWork) {
        Item common(1, "Iron Sword", ItemCategory::Treasure, ItemRarity::Common, 1, 5, 10, "Common weapon");
        Item rare(2, "Dragon Blade", ItemCategory::Treasure, ItemRarity::Rare, 1, 5, 1000, "Rare weapon");
        Item legendary(3, "Excalibur", ItemCategory::Treasure, ItemRarity::Legendary, 1, 3, 10000, "Legendary weapon");

        EXPECT_EQ(common.GetRarity(), ItemRarity::Common);
        EXPECT_EQ(rare.GetRarity(), ItemRarity::Rare);
        EXPECT_EQ(legendary.GetRarity(), ItemRarity::Legendary);

        // Higher rarity should generally have higher value
        EXPECT_LT(common.GetValue(), rare.GetValue());
        EXPECT_LT(rare.GetValue(), legendary.GetValue());
    }

    // ========== New Enum Tests ==========

    TEST(EquipmentSlotTests, ToStringReturnsCorrectValues) {
        EXPECT_STREQ(ToString(EquipmentSlot::None), "None");
        EXPECT_STREQ(ToString(EquipmentSlot::Head), "Head");
        EXPECT_STREQ(ToString(EquipmentSlot::MainHand), "MainHand");
        EXPECT_STREQ(ToString(EquipmentSlot::OffHand), "OffHand");
    }

    TEST(ArmorCategoryTests, ToStringReturnsCorrectValues) {
        EXPECT_STREQ(ToString(ArmorCategory::None), "None");
        EXPECT_STREQ(ToString(ArmorCategory::Cloth), "Cloth");
        EXPECT_STREQ(ToString(ArmorCategory::Light), "Light");
        EXPECT_STREQ(ToString(ArmorCategory::Medium), "Medium");
        EXPECT_STREQ(ToString(ArmorCategory::Heavy), "Heavy");
        EXPECT_STREQ(ToString(ArmorCategory::Shield), "Shield");
    }

    TEST(SpellCategoryTests, ToStringReturnsCorrectValues) {
        EXPECT_STREQ(ToString(SpellCategory::None), "None");
        EXPECT_STREQ(ToString(SpellCategory::Damage), "Damage");
        EXPECT_STREQ(ToString(SpellCategory::Heal), "Heal");
        EXPECT_STREQ(ToString(SpellCategory::Buff), "Buff");
        EXPECT_STREQ(ToString(SpellCategory::Debuff), "Debuff");
        EXPECT_STREQ(ToString(SpellCategory::Control), "Control");
        EXPECT_STREQ(ToString(SpellCategory::Utility), "Utility");
    }

    TEST(ItemCategoryTests, ToStringReturnsCorrectValues) {
        EXPECT_STREQ(ToString(ItemCategory::None), "None");
        EXPECT_STREQ(ToString(ItemCategory::Consumable), "Consumable");
        EXPECT_STREQ(ToString(ItemCategory::Quest), "Quest");
        EXPECT_STREQ(ToString(ItemCategory::Ammunition), "Ammunition");
    }

    TEST(ItemRarityTests, ToStringReturnsCorrectValues) {
        EXPECT_STREQ(ToString(ItemRarity::Common), "Common");
        EXPECT_STREQ(ToString(ItemRarity::Uncommon), "Uncommon");
        EXPECT_STREQ(ToString(ItemRarity::Rare), "Rare");
        EXPECT_STREQ(ToString(ItemRarity::Epic), "Epic");
        EXPECT_STREQ(ToString(ItemRarity::Legendary), "Legendary");
    }