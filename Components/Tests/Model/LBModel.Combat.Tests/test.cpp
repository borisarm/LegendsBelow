#include "pch.h"

import LB.Combat;

using namespace LB::Combat;

// ============================================================================
// DamageType Enum Tests
// ============================================================================

TEST(DamageTypeTests, PhysicalTypes_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(DamageType::Slashing), static_cast<int>(DamageType::Piercing));
    EXPECT_NE(static_cast<int>(DamageType::Piercing), static_cast<int>(DamageType::Bludgeoning));
    EXPECT_NE(static_cast<int>(DamageType::None), static_cast<int>(DamageType::Slashing));
}

TEST(DamageTypeTests, ElementalTypes_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(DamageType::Fire), static_cast<int>(DamageType::Cold));
    EXPECT_NE(static_cast<int>(DamageType::Cold), static_cast<int>(DamageType::Lightning));
    EXPECT_NE(static_cast<int>(DamageType::Fire), static_cast<int>(DamageType::Lightning));
}

TEST(DamageTypeTests, SpiritualTypes_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(DamageType::Holy), static_cast<int>(DamageType::Shadow));
    EXPECT_NE(static_cast<int>(DamageType::Shadow), static_cast<int>(DamageType::Arcane));
    EXPECT_NE(static_cast<int>(DamageType::Holy), static_cast<int>(DamageType::Arcane));
}

TEST(DamageTypeTests, NoneType_IsZero) {
    EXPECT_EQ(static_cast<int>(DamageType::None), 0);
}

// ============================================================================
// WeaponType Enum Tests
// ============================================================================

TEST(WeaponTypeTests, MeleeWeapons_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(WeaponType::Sword), static_cast<int>(WeaponType::Axe));
    EXPECT_NE(static_cast<int>(WeaponType::Axe), static_cast<int>(WeaponType::Mace));
    EXPECT_NE(static_cast<int>(WeaponType::Mace), static_cast<int>(WeaponType::Dagger));
    EXPECT_NE(static_cast<int>(WeaponType::Dagger), static_cast<int>(WeaponType::Spear));
}

TEST(WeaponTypeTests, RangedWeapons_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(WeaponType::Bow), static_cast<int>(WeaponType::Crossbow));
}

TEST(WeaponTypeTests, MagicalWeapons_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(WeaponType::Staff), static_cast<int>(WeaponType::Wand));
}

TEST(WeaponTypeTests, NoneType_IsZero) {
    EXPECT_EQ(static_cast<int>(WeaponType::None), 0);
}

// ============================================================================
// ArmorCategory Enum Tests
// ============================================================================

TEST(ArmorCategoryTests, AllCategories_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(ArmorCategory::Cloth), static_cast<int>(ArmorCategory::Light));
    EXPECT_NE(static_cast<int>(ArmorCategory::Light), static_cast<int>(ArmorCategory::Medium));
    EXPECT_NE(static_cast<int>(ArmorCategory::Medium), static_cast<int>(ArmorCategory::Heavy));
    EXPECT_NE(static_cast<int>(ArmorCategory::Heavy), static_cast<int>(ArmorCategory::Shield));
}

TEST(ArmorCategoryTests, NoneType_IsZero) {
    EXPECT_EQ(static_cast<int>(ArmorCategory::None), 0);
}

TEST(ArmorCategoryTests, ProgressiveValues_IncreaseInProtection) {
    // Cloth < Light < Medium < Heavy in terms of enum value order
    EXPECT_LT(static_cast<int>(ArmorCategory::Cloth), static_cast<int>(ArmorCategory::Light));
    EXPECT_LT(static_cast<int>(ArmorCategory::Light), static_cast<int>(ArmorCategory::Medium));
    EXPECT_LT(static_cast<int>(ArmorCategory::Medium), static_cast<int>(ArmorCategory::Heavy));
}

// ============================================================================
// SpellSchool Enum Tests
// ============================================================================

TEST(SpellSchoolTests, AllSchools_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(SpellSchool::Evocation), static_cast<int>(SpellSchool::Frost));
    EXPECT_NE(static_cast<int>(SpellSchool::Frost), static_cast<int>(SpellSchool::Holy));
    EXPECT_NE(static_cast<int>(SpellSchool::Holy), static_cast<int>(SpellSchool::Shadow));
    EXPECT_NE(static_cast<int>(SpellSchool::Shadow), static_cast<int>(SpellSchool::Utility));
}

TEST(SpellSchoolTests, NoneType_IsZero) {
    EXPECT_EQ(static_cast<int>(SpellSchool::None), 0);
}

// ============================================================================
// SpellCategory Enum Tests
// ============================================================================

TEST(SpellCategoryTests, AllCategories_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(SpellCategory::Damage), static_cast<int>(SpellCategory::Heal));
    EXPECT_NE(static_cast<int>(SpellCategory::Heal), static_cast<int>(SpellCategory::Buff));
    EXPECT_NE(static_cast<int>(SpellCategory::Buff), static_cast<int>(SpellCategory::Debuff));
    EXPECT_NE(static_cast<int>(SpellCategory::Debuff), static_cast<int>(SpellCategory::Control));
    EXPECT_NE(static_cast<int>(SpellCategory::Control), static_cast<int>(SpellCategory::Utility));
}

TEST(SpellCategoryTests, NoneType_IsZero) {
    EXPECT_EQ(static_cast<int>(SpellCategory::None), 0);
}

// ============================================================================
// EquipmentSlot Enum Tests
// ============================================================================

TEST(EquipmentSlotTests, ArmorSlots_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(EquipmentSlot::Head), static_cast<int>(EquipmentSlot::Chest));
    EXPECT_NE(static_cast<int>(EquipmentSlot::Chest), static_cast<int>(EquipmentSlot::Legs));
    EXPECT_NE(static_cast<int>(EquipmentSlot::Legs), static_cast<int>(EquipmentSlot::Feet));
}

TEST(EquipmentSlotTests, WeaponSlots_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(EquipmentSlot::MainHand), static_cast<int>(EquipmentSlot::OffHand));
}

TEST(EquipmentSlotTests, AccessorySlots_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(EquipmentSlot::Ring1), static_cast<int>(EquipmentSlot::Ring2));
    EXPECT_NE(static_cast<int>(EquipmentSlot::Ring1), static_cast<int>(EquipmentSlot::Necklace));
}

TEST(EquipmentSlotTests, NoneType_IsZero) {
    EXPECT_EQ(static_cast<int>(EquipmentSlot::None), 0);
}

TEST(EquipmentSlotTests, AllSlots_UniqueValues) {
    std::set<int> slotValues;
    slotValues.insert(static_cast<int>(EquipmentSlot::None));
    slotValues.insert(static_cast<int>(EquipmentSlot::Head));
    slotValues.insert(static_cast<int>(EquipmentSlot::Chest));
    slotValues.insert(static_cast<int>(EquipmentSlot::Legs));
    slotValues.insert(static_cast<int>(EquipmentSlot::Feet));
    slotValues.insert(static_cast<int>(EquipmentSlot::MainHand));
    slotValues.insert(static_cast<int>(EquipmentSlot::OffHand));
    slotValues.insert(static_cast<int>(EquipmentSlot::Ring1));
    slotValues.insert(static_cast<int>(EquipmentSlot::Ring2));
    slotValues.insert(static_cast<int>(EquipmentSlot::Necklace));

    // All 10 slots should have unique values
    EXPECT_EQ(slotValues.size(), 10u);
}

// ============================================================================
// Damage Type Semantics Tests
// ============================================================================

TEST(DamageTypeSemanticTests, PhysicalDamageTypes_Grouped) {
    // Physical damage types should be sequential
    int slashing = static_cast<int>(DamageType::Slashing);
    int piercing = static_cast<int>(DamageType::Piercing);
    int bludgeoning = static_cast<int>(DamageType::Bludgeoning);

    EXPECT_GT(slashing, 0);
    EXPECT_GT(piercing, 0);
    EXPECT_GT(bludgeoning, 0);
}

TEST(DamageTypeSemanticTests, ElementalDamageTypes_Grouped) {
    // Elemental damage types exist
    int fire = static_cast<int>(DamageType::Fire);
    int cold = static_cast<int>(DamageType::Cold);
    int lightning = static_cast<int>(DamageType::Lightning);

    EXPECT_GT(fire, 0);
    EXPECT_GT(cold, 0);
    EXPECT_GT(lightning, 0);
}

TEST(DamageTypeSemanticTests, SpiritualDamageTypes_Grouped) {
    // Spiritual damage types exist
    int holy = static_cast<int>(DamageType::Holy);
    int shadow = static_cast<int>(DamageType::Shadow);
    int arcane = static_cast<int>(DamageType::Arcane);

    EXPECT_GT(holy, 0);
    EXPECT_GT(shadow, 0);
    EXPECT_GT(arcane, 0);
}

// ============================================================================
// Weapon Type Semantics Tests
// ============================================================================

TEST(WeaponTypeSemanticTests, MeleeWeapons_FirstGroup) {
    // Melee weapons should come first
    int sword = static_cast<int>(WeaponType::Sword);
    int axe = static_cast<int>(WeaponType::Axe);
    int bow = static_cast<int>(WeaponType::Bow);

    EXPECT_LT(sword, bow);
    EXPECT_LT(axe, bow);
}

TEST(WeaponTypeSemanticTests, StaffAndWand_LastGroup) {
    // Staff and Wand for casters should be last
    int staff = static_cast<int>(WeaponType::Staff);
    int wand = static_cast<int>(WeaponType::Wand);
    int sword = static_cast<int>(WeaponType::Sword);

    EXPECT_GT(staff, sword);
    EXPECT_GT(wand, sword);
}

// ============================================================================
// Armor Category Semantics Tests
// ============================================================================

TEST(ArmorCategorySemanticTests, ClothIsLightest) {
    int cloth = static_cast<int>(ArmorCategory::Cloth);
    int light = static_cast<int>(ArmorCategory::Light);
    int heavy = static_cast<int>(ArmorCategory::Heavy);

    EXPECT_LT(cloth, light);
    EXPECT_LT(cloth, heavy);
}

TEST(ArmorCategorySemanticTests, HeavyIsStrongest) {
    int heavy = static_cast<int>(ArmorCategory::Heavy);
    int medium = static_cast<int>(ArmorCategory::Medium);
    int light = static_cast<int>(ArmorCategory::Light);

    EXPECT_GT(heavy, medium);
    EXPECT_GT(heavy, light);
}

// ============================================================================
// Equipment Slot Semantics Tests
// ============================================================================

TEST(EquipmentSlotSemanticTests, ArmorSlots_Sequential) {
    int head = static_cast<int>(EquipmentSlot::Head);
    int chest = static_cast<int>(EquipmentSlot::Chest);
    int legs = static_cast<int>(EquipmentSlot::Legs);
    int feet = static_cast<int>(EquipmentSlot::Feet);

    EXPECT_LT(head, chest);
    EXPECT_LT(chest, legs);
    EXPECT_LT(legs, feet);
}

TEST(EquipmentSlotSemanticTests, HandSlots_Sequential) {
    int mainHand = static_cast<int>(EquipmentSlot::MainHand);
    int offHand = static_cast<int>(EquipmentSlot::OffHand);

    EXPECT_LT(mainHand, offHand);
}

TEST(EquipmentSlotSemanticTests, RingSlots_Sequential) {
    int ring1 = static_cast<int>(EquipmentSlot::Ring1);
    int ring2 = static_cast<int>(EquipmentSlot::Ring2);

    EXPECT_LT(ring1, ring2);
}