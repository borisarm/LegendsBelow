#include "pch.h"

import LB.Inventory;

using namespace LB::Inventory;
using namespace LB::Combat;

// ============================================================================
// InventoryItem Tests
// ============================================================================

TEST(InventoryItemTests, DefaultConstructor_SetsDefaults) {
    InventoryItem item{};

    EXPECT_EQ(item.quantity, 1);
    EXPECT_EQ(item.maxStack, 1);
    EXPECT_FALSE(item.isEquippable);
    EXPECT_FALSE(item.isConsumable);
    EXPECT_EQ(item.equipmentSlot, EquipmentSlot::None);
}

TEST(InventoryItemTests, CanStack_MaxStackGreaterThanOne_ReturnsTrue) {
    InventoryItem item{};
    item.maxStack = 99;

    EXPECT_TRUE(item.CanStack());
}

TEST(InventoryItemTests, CanStack_MaxStackOne_ReturnsFalse) {
    InventoryItem item{};
    item.maxStack = 1;

    EXPECT_FALSE(item.CanStack());
}

TEST(InventoryItemTests, IsStackFull_QuantityEqualsMax_ReturnsTrue) {
    InventoryItem item{};
    item.quantity = 50;
    item.maxStack = 50;

    EXPECT_TRUE(item.IsStackFull());
}

TEST(InventoryItemTests, IsStackFull_QuantityExceedsMax_ReturnsTrue) {
    InventoryItem item{};
    item.quantity = 60;
    item.maxStack = 50;

    EXPECT_TRUE(item.IsStackFull());
}

TEST(InventoryItemTests, IsStackFull_QuantityLessThanMax_ReturnsFalse) {
    InventoryItem item{};
    item.quantity = 30;
    item.maxStack = 50;

    EXPECT_FALSE(item.IsStackFull());
}

TEST(InventoryItemTests, GetRemainingStackSpace_ReturnsCorrectValue) {
    InventoryItem item{};
    item.quantity = 30;
    item.maxStack = 50;

    int32_t remaining = item.GetRemainingStackSpace();

    EXPECT_EQ(remaining, 20);
}

TEST(InventoryItemTests, GetRemainingStackSpace_FullStack_ReturnsZero) {
    InventoryItem item{};
    item.quantity = 50;
    item.maxStack = 50;

    int32_t remaining = item.GetRemainingStackSpace();

    EXPECT_EQ(remaining, 0);
}

TEST(InventoryItemTests, GetRemainingStackSpace_OverFull_ReturnsZero) {
    InventoryItem item{};
    item.quantity = 60;
    item.maxStack = 50;

    int32_t remaining = item.GetRemainingStackSpace();

    EXPECT_EQ(remaining, 0);
}

// ============================================================================
// Equipment Tests
// ============================================================================

TEST(EquipmentTests, DefaultConstructor_SetsDefaults) {
    Equipment equipment{};

    EXPECT_EQ(equipment.weaponType, WeaponType::None);
    EXPECT_EQ(equipment.armorCategory, ArmorCategory::None);
    EXPECT_EQ(equipment.strengthBonus, 0);
    EXPECT_EQ(equipment.dexterityBonus, 0);
    EXPECT_EQ(equipment.armorClassBonus, 0);
}

TEST(EquipmentTests, GetAttributeBonuses_ReturnsAllBonuses) {
    Equipment equipment{};
    equipment.strengthBonus = 2;
    equipment.dexterityBonus = 3;
    equipment.constitutionBonus = 1;
    equipment.intelligenceBonus = 4;
    equipment.wisdomBonus = 2;
    equipment.charismaBonus = 1;

    auto bonuses = equipment.GetAttributeBonuses();

    EXPECT_EQ(bonuses.strength, 2);
    EXPECT_EQ(bonuses.dexterity, 3);
    EXPECT_EQ(bonuses.constitution, 1);
    EXPECT_EQ(bonuses.intelligence, 4);
    EXPECT_EQ(bonuses.wisdom, 2);
    EXPECT_EQ(bonuses.charisma, 1);
}

TEST(EquipmentTests, IsWeapon_MainHandSlot_ReturnsTrue) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::MainHand;

    EXPECT_TRUE(equipment.IsWeapon());
}

TEST(EquipmentTests, IsWeapon_OffHandSlot_ReturnsTrue) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::OffHand;

    EXPECT_TRUE(equipment.IsWeapon());
}

TEST(EquipmentTests, IsWeapon_ChestSlot_ReturnsFalse) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::Chest;

    EXPECT_FALSE(equipment.IsWeapon());
}

TEST(EquipmentTests, IsArmor_HeadSlot_ReturnsTrue) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::Head;

    EXPECT_TRUE(equipment.IsArmor());
}

TEST(EquipmentTests, IsArmor_ChestSlot_ReturnsTrue) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::Chest;

    EXPECT_TRUE(equipment.IsArmor());
}

TEST(EquipmentTests, IsArmor_LegsSlot_ReturnsTrue) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::Legs;

    EXPECT_TRUE(equipment.IsArmor());
}

TEST(EquipmentTests, IsArmor_FeetSlot_ReturnsTrue) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::Feet;

    EXPECT_TRUE(equipment.IsArmor());
}

TEST(EquipmentTests, IsArmor_MainHandSlot_ReturnsFalse) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::MainHand;

    EXPECT_FALSE(equipment.IsArmor());
}

TEST(EquipmentTests, IsArmor_RingSlot_ReturnsFalse) {
    Equipment equipment{};
    equipment.slot = EquipmentSlot::Ring1;

    EXPECT_FALSE(equipment.IsArmor());
}

// ============================================================================
// EquippedItems Tests
// ============================================================================

TEST(EquippedItemsTests, DefaultConstructor_EmptyEquipment) {
    EquippedItems equipped{};

    EXPECT_FALSE(equipped.HasItemInSlot(EquipmentSlot::MainHand));
    EXPECT_FALSE(equipped.HasItemInSlot(EquipmentSlot::Chest));
}

TEST(EquippedItemsTests, HasItemInSlot_WithEquipment_ReturnsTrue) {
    EquippedItems equipped{};
    Equipment sword{};
    sword.slot = EquipmentSlot::MainHand;
    equipped.equipment[EquipmentSlot::MainHand] = sword;

    EXPECT_TRUE(equipped.HasItemInSlot(EquipmentSlot::MainHand));
}

TEST(EquippedItemsTests, HasItemInSlot_NoEquipment_ReturnsFalse) {
    EquippedItems equipped{};

    EXPECT_FALSE(equipped.HasItemInSlot(EquipmentSlot::OffHand));
}

TEST(EquippedItemsTests, GetEquipment_ExistingSlot_ReturnsEquipment) {
    EquippedItems equipped{};
    Equipment sword{};
    sword.itemId = 42;
    sword.slot = EquipmentSlot::MainHand;
    equipped.equipment[EquipmentSlot::MainHand] = sword;

    auto result = equipped.GetEquipment(EquipmentSlot::MainHand);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->itemId, 42);
}

TEST(EquippedItemsTests, GetEquipment_EmptySlot_ReturnsNullopt) {
    EquippedItems equipped{};

    auto result = equipped.GetEquipment(EquipmentSlot::MainHand);

    EXPECT_FALSE(result.has_value());
}

TEST(EquippedItemsTests, GetTotalAttributeBonuses_NoEquipment_ReturnsZero) {
    EquippedItems equipped{};

    auto bonuses = equipped.GetTotalAttributeBonuses();

    EXPECT_EQ(bonuses.strength, 0);
    EXPECT_EQ(bonuses.dexterity, 0);
    EXPECT_EQ(bonuses.constitution, 0);
    EXPECT_EQ(bonuses.intelligence, 0);
    EXPECT_EQ(bonuses.wisdom, 0);
    EXPECT_EQ(bonuses.charisma, 0);
}

TEST(EquippedItemsTests, GetTotalAttributeBonuses_SingleItem_ReturnsItemBonuses) {
    EquippedItems equipped{};
    Equipment ring{};
    ring.slot = EquipmentSlot::Ring1;
    ring.strengthBonus = 2;
    ring.dexterityBonus = 1;
    equipped.equipment[EquipmentSlot::Ring1] = ring;

    auto bonuses = equipped.GetTotalAttributeBonuses();

    EXPECT_EQ(bonuses.strength, 2);
    EXPECT_EQ(bonuses.dexterity, 1);
}

TEST(EquippedItemsTests, GetTotalAttributeBonuses_MultipleItems_SumsBonuses) {
    EquippedItems equipped{};

    Equipment ring1{};
    ring1.slot = EquipmentSlot::Ring1;
    ring1.strengthBonus = 2;
    ring1.dexterityBonus = 1;

    Equipment ring2{};
    ring2.slot = EquipmentSlot::Ring2;
    ring2.strengthBonus = 1;
    ring2.intelligenceBonus = 3;

    equipped.equipment[EquipmentSlot::Ring1] = ring1;
    equipped.equipment[EquipmentSlot::Ring2] = ring2;

    auto bonuses = equipped.GetTotalAttributeBonuses();

    EXPECT_EQ(bonuses.strength, 3);
    EXPECT_EQ(bonuses.dexterity, 1);
    EXPECT_EQ(bonuses.intelligence, 3);
}

TEST(EquippedItemsTests, GetTotalArmorClass_NoEquipment_ReturnsZero) {
    EquippedItems equipped{};

    int32_t ac = equipped.GetTotalArmorClass();

    EXPECT_EQ(ac, 0);
}

TEST(EquippedItemsTests, GetTotalArmorClass_SingleItem_ReturnsItemAC) {
    EquippedItems equipped{};
    Equipment chest{};
    chest.slot = EquipmentSlot::Chest;
    chest.armorClassBonus = 5;
    equipped.equipment[EquipmentSlot::Chest] = chest;

    int32_t ac = equipped.GetTotalArmorClass();

    EXPECT_EQ(ac, 5);
}

TEST(EquippedItemsTests, GetTotalArmorClass_MultipleItems_SumsAC) {
    EquippedItems equipped{};

    Equipment chest{};
    chest.slot = EquipmentSlot::Chest;
    chest.armorClassBonus = 5;

    Equipment legs{};
    legs.slot = EquipmentSlot::Legs;
    legs.armorClassBonus = 3;

    equipped.equipment[EquipmentSlot::Chest] = chest;
    equipped.equipment[EquipmentSlot::Legs] = legs;

    int32_t ac = equipped.GetTotalArmorClass();

    EXPECT_EQ(ac, 8);
}

TEST(EquippedItemsTests, GetTotalAttackBonus_NoWeapons_ReturnsZero) {
    EquippedItems equipped{};

    int32_t attackBonus = equipped.GetTotalAttackBonus();

    EXPECT_EQ(attackBonus, 0);
}

TEST(EquippedItemsTests, GetTotalAttackBonus_WithWeapon_ReturnsBonus) {
    EquippedItems equipped{};
    Equipment sword{};
    sword.slot = EquipmentSlot::MainHand;
    sword.attackBonus = 3;
    equipped.equipment[EquipmentSlot::MainHand] = sword;

    int32_t attackBonus = equipped.GetTotalAttackBonus();

    EXPECT_EQ(attackBonus, 3);
}

TEST(EquippedItemsTests, GetTotalDamageBonus_NoWeapons_ReturnsZero) {
    EquippedItems equipped{};

    int32_t damageBonus = equipped.GetTotalDamageBonus();

    EXPECT_EQ(damageBonus, 0);
}

TEST(EquippedItemsTests, GetTotalDamageBonus_WithWeapon_ReturnsBonus) {
    EquippedItems equipped{};
    Equipment sword{};
    sword.slot = EquipmentSlot::MainHand;
    sword.damageBonus = 5;
    equipped.equipment[EquipmentSlot::MainHand] = sword;

    int32_t damageBonus = equipped.GetTotalDamageBonus();

    EXPECT_EQ(damageBonus, 5);
}

// ============================================================================
// InventoryOperationResult Tests
// ============================================================================

TEST(InventoryOperationResultTests, DefaultConstructor_FailureState) {
    InventoryOperationResult result{};

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.empty());
    EXPECT_EQ(result.itemsAffected, 0);
}

TEST(InventoryOperationResultTests, SuccessState_PropertiesSet) {
    InventoryOperationResult result{};
    result.success = true;
    result.itemsAffected = 5;

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.itemsAffected, 5);
}

TEST(InventoryOperationResultTests, FailureState_ErrorMessageSet) {
    InventoryOperationResult result{};
    result.success = false;
    result.errorMessage = "Inventory full";

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Inventory full");
}