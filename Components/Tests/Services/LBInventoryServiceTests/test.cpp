#include "pch.h"

import LB.InventoryService;

using namespace LB::Services::Inventory;


// ============================================================================
// Inventory Creation Tests
// ============================================================================

TEST(InventoryServiceTests, CreateInventory_NewCharacter_Success) {
    InventoryService service;

    bool created = service.CreateInventory(1, 20);

    EXPECT_TRUE(created);
    EXPECT_TRUE(service.HasInventory(1));
    EXPECT_EQ(service.GetInventoryCapacity(1), 20);
}

TEST(InventoryServiceTests, CreateInventory_DuplicateCharacter_Fails) {
    InventoryService service;

    service.CreateInventory(1, 20);
    bool created = service.CreateInventory(1, 30);

    EXPECT_FALSE(created);
    EXPECT_EQ(service.GetInventoryCapacity(1), 20); // Original capacity unchanged
}

TEST(InventoryServiceTests, CreateInventory_DefaultCapacity_Uses20) {
    InventoryService service;

    service.CreateInventory(1);

    EXPECT_EQ(service.GetInventoryCapacity(1), 20);
}

// ============================================================================
// Add Item Tests
// ============================================================================

TEST(InventoryServiceTests, AddItem_ToEmptyInventory_Success) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item{};
    item.itemId = 100;
    item.name = "Health Potion";
    item.quantity = 1;

    auto result = service.AddItem(1, item);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.itemsAffected, 1);
    EXPECT_EQ(service.GetInventorySize(1), 1);
}

TEST(InventoryServiceTests, AddItem_StackableItems_CombinesStacks) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item{};
    item.itemId = 100;
    item.name = "Arrow";
    item.quantity = 10;
    item.maxStack = 50;

    service.AddItem(1, item);
    item.quantity = 15;
    auto result = service.AddItem(1, item);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.itemsAffected, 15);
    EXPECT_EQ(service.GetInventorySize(1), 1); // Still one stack
    EXPECT_EQ(service.GetItemCount(1, 100), 25); // Total 25 arrows
}

TEST(InventoryServiceTests, AddItem_StackOverflow_CreatesNewStack) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item{};
    item.itemId = 100;
    item.name = "Arrow";
    item.quantity = 40;
    item.maxStack = 50;

    service.AddItem(1, item);
    item.quantity = 30; // This will overflow the stack
    auto result = service.AddItem(1, item);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(service.GetInventorySize(1), 2); // Two stacks
    EXPECT_EQ(service.GetItemCount(1, 100), 70); // Total 70 arrows
}

TEST(InventoryServiceTests, AddItem_InventoryFull_Fails) {
    InventoryService service;
    service.CreateInventory(1, 2); // Very small inventory

    InventoryItem item1{};
    item1.itemId = 100;
    item1.name = "Sword";
    item1.quantity = 1;

    InventoryItem item2{};
    item2.itemId = 101;
    item2.name = "Shield";
    item2.quantity = 1;

    InventoryItem item3{};
    item3.itemId = 102;
    item3.name = "Helmet";
    item3.quantity = 1;

    service.AddItem(1, item1);
    service.AddItem(1, item2);
    auto result = service.AddItem(1, item3);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(service.GetInventorySize(1), 2);
}

TEST(InventoryServiceTests, AddItem_NoInventory_Fails) {
    InventoryService service;

    InventoryItem item{};
    item.itemId = 100;
    auto result = service.AddItem(1, item);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// Remove Item Tests
// ============================================================================

TEST(InventoryServiceTests, RemoveItem_ExistingItem_Success) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item{};
    item.itemId = 100;
    item.name = "Potion";
    item.quantity = 5;
    item.maxStack = 10;

    service.AddItem(1, item);
    auto result = service.RemoveItem(1, 100, 2);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.itemsAffected, 2);
    EXPECT_EQ(service.GetItemCount(1, 100), 3);
}

TEST(InventoryServiceTests, RemoveItem_AllQuantity_RemovesFromInventory) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item{};
    item.itemId = 100;
    item.name = "Potion";
    item.quantity = 3;

    service.AddItem(1, item);
    auto result = service.RemoveItem(1, 100, 3);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(service.GetInventorySize(1), 0);
    EXPECT_EQ(service.GetItemCount(1, 100), 0);
}

TEST(InventoryServiceTests, RemoveItem_NotEnough_PartialSuccess) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item{};
    item.itemId = 100;
    item.name = "Potion";
    item.quantity = 2;

    service.AddItem(1, item);
    auto result = service.RemoveItem(1, 100, 5);

    EXPECT_TRUE(result.success); // Partial success
    EXPECT_EQ(result.itemsAffected, 2);
    EXPECT_EQ(service.GetItemCount(1, 100), 0);
}

TEST(InventoryServiceTests, RemoveItem_NotFound_Fails) {
    InventoryService service;
    service.CreateInventory(1, 20);

    auto result = service.RemoveItem(1, 999, 1);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// Equipment Tests
// ============================================================================

TEST(InventoryServiceTests, EquipItem_ValidSlot_Success) {
    InventoryService service;
    service.CreateInventory(1, 20);

    Equipment sword{};
    sword.itemId = 200;
    sword.name = "Iron Sword";
    sword.slot = EquipmentSlot::MainHand;
    sword.attackBonus = 5;

    bool equipped = service.EquipItem(1, sword);

    EXPECT_TRUE(equipped);
    auto equippedItems = service.GetEquippedItems(1);
    ASSERT_TRUE(equippedItems.has_value());
    EXPECT_TRUE(equippedItems->HasItemInSlot(EquipmentSlot::MainHand));
}

TEST(InventoryServiceTests, EquipItem_ReplaceExisting_Success) {
    InventoryService service;
    service.CreateInventory(1, 20);

    Equipment ironSword{};
    ironSword.itemId = 200;
    ironSword.name = "Iron Sword";
    ironSword.slot = EquipmentSlot::MainHand;
    ironSword.attackBonus = 5;

    Equipment steelSword{};
    steelSword.itemId = 201;
    steelSword.name = "Steel Sword";
    steelSword.slot = EquipmentSlot::MainHand;
    steelSword.attackBonus = 8;

    service.EquipItem(1, ironSword);
    bool equipped = service.EquipItem(1, steelSword);

    EXPECT_TRUE(equipped);
    auto equippedItems = service.GetEquippedItems(1);
    auto mainHand = equippedItems->GetEquipment(EquipmentSlot::MainHand);
    ASSERT_TRUE(mainHand.has_value());
    EXPECT_EQ(mainHand->itemId, 201); // Steel sword
}

TEST(InventoryServiceTests, EquipItem_InvalidSlot_Fails) {
    InventoryService service;
    service.CreateInventory(1, 20);

    Equipment item{};
    item.itemId = 200;
    item.slot = EquipmentSlot::None;

    bool equipped = service.EquipItem(1, item);

    EXPECT_FALSE(equipped);
}

TEST(InventoryServiceTests, UnequipItem_ExistingSlot_Success) {
    InventoryService service;
    service.CreateInventory(1, 20);

    Equipment helmet{};
    helmet.itemId = 300;
    helmet.name = "Iron Helmet";
    helmet.slot = EquipmentSlot::Head;

    service.EquipItem(1, helmet);
    bool unequipped = service.UnequipItem(1, EquipmentSlot::Head);

    EXPECT_TRUE(unequipped);
    auto equippedItems = service.GetEquippedItems(1);
    EXPECT_FALSE(equippedItems->HasItemInSlot(EquipmentSlot::Head));
}

TEST(InventoryServiceTests, UnequipItem_EmptySlot_Fails) {
    InventoryService service;
    service.CreateInventory(1, 20);

    bool unequipped = service.UnequipItem(1, EquipmentSlot::Head);

    EXPECT_FALSE(unequipped);
}

// ============================================================================
// Equipment Bonus Tests
// ============================================================================

TEST(InventoryServiceTests, GetTotalAttributeBonuses_MultipleItems_SumsCorrectly) {
    InventoryService service;
    service.CreateInventory(1, 20);

    Equipment helmet{};
    helmet.itemId = 300;
    helmet.slot = EquipmentSlot::Head;
    helmet.strengthBonus = 2;
    helmet.constitutionBonus = 1;

    Equipment chest{};
    chest.itemId = 301;
    chest.slot = EquipmentSlot::Chest;
    chest.strengthBonus = 3;
    chest.dexterityBonus = 2;

    service.EquipItem(1, helmet);
    service.EquipItem(1, chest);

    auto equippedItems = service.GetEquippedItems(1);
    auto bonuses = equippedItems->GetTotalAttributeBonuses();

    EXPECT_EQ(bonuses.strength, 5);
    EXPECT_EQ(bonuses.dexterity, 2);
    EXPECT_EQ(bonuses.constitution, 1);
}

TEST(InventoryServiceTests, GetTotalArmorClass_MultipleArmor_SumsCorrectly) {
    InventoryService service;
    service.CreateInventory(1, 20);

    Equipment helmet{};
    helmet.itemId = 300;
    helmet.slot = EquipmentSlot::Head;
    helmet.armorClassBonus = 2;

    Equipment chest{};
    chest.itemId = 301;
    chest.slot = EquipmentSlot::Chest;
    chest.armorClassBonus = 5;

    service.EquipItem(1, helmet);
    service.EquipItem(1, chest);

    auto equippedItems = service.GetEquippedItems(1);
    int32_t totalAC = equippedItems->GetTotalArmorClass();

    EXPECT_EQ(totalAC, 7);
}

TEST(InventoryServiceTests, GetTotalAttackBonus_Weapon_ReturnsBonus) {
    InventoryService service;
    service.CreateInventory(1, 20);

    Equipment sword{};
    sword.itemId = 200;
    sword.slot = EquipmentSlot::MainHand;
    sword.attackBonus = 3;

    service.EquipItem(1, sword);

    auto equippedItems = service.GetEquippedItems(1);
    int32_t totalAttack = equippedItems->GetTotalAttackBonus();

    EXPECT_EQ(totalAttack, 3);
}

// ============================================================================
// Inventory Management Tests
// ============================================================================

TEST(InventoryServiceTests, GetInventory_WithItems_ReturnsAll) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item1{};
    item1.itemId = 100;
    item1.name = "Potion";

    InventoryItem item2{};
    item2.itemId = 101;
    item2.name = "Scroll";

    service.AddItem(1, item1);
    service.AddItem(1, item2);

    auto inventory = service.GetInventory(1);

    EXPECT_EQ(inventory.size(), size_t(2));
}

TEST(InventoryServiceTests, GetItemCount_MultipleStacks_SumsTotal) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem arrows{};
    arrows.itemId = 100;
    arrows.name = "Arrow";
    arrows.quantity = 50;
    arrows.maxStack = 50;

    service.AddItem(1, arrows);
    arrows.quantity = 30;
    service.AddItem(1, arrows);

    int32_t count = service.GetItemCount(1, 100);

    EXPECT_EQ(count, 80);
}

TEST(InventoryServiceTests, HasInventorySpace_BelowCapacity_ReturnsTrue) {
    InventoryService service;
    service.CreateInventory(1, 5);

    InventoryItem item{};
    item.itemId = 100;
    service.AddItem(1, item);

    EXPECT_TRUE(service.HasInventorySpace(1));
}

TEST(InventoryServiceTests, HasInventorySpace_AtCapacity_ReturnsFalse) {
    InventoryService service;
    service.CreateInventory(1, 2);

    InventoryItem item1{};
    item1.itemId = 100;
    InventoryItem item2{};
    item2.itemId = 101;

    service.AddItem(1, item1);
    service.AddItem(1, item2);

    EXPECT_FALSE(service.HasInventorySpace(1));
}

TEST(InventoryServiceTests, SetInventoryCapacity_ValidCapacity_Success) {
    InventoryService service;
    service.CreateInventory(1, 20);

    bool set = service.SetInventoryCapacity(1, 50);

    EXPECT_TRUE(set);
    EXPECT_EQ(service.GetInventoryCapacity(1), 50);
}

TEST(InventoryServiceTests, SetInventoryCapacity_NegativeCapacity_Fails) {
    InventoryService service;
    service.CreateInventory(1, 20);

    bool set = service.SetInventoryCapacity(1, -10);

    EXPECT_FALSE(set);
    EXPECT_EQ(service.GetInventoryCapacity(1), 20); // Unchanged
}

TEST(InventoryServiceTests, ClearInventory_WithItems_RemovesAll) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem item{};
    item.itemId = 100;
    service.AddItem(1, item);

    bool cleared = service.ClearInventory(1);

    EXPECT_TRUE(cleared);
    EXPECT_EQ(service.GetInventorySize(1), 0);
}

TEST(InventoryServiceTests, RemoveInventory_ExistingInventory_Success) {
    InventoryService service;
    service.CreateInventory(1, 20);

    bool removed = service.RemoveInventory(1);

    EXPECT_TRUE(removed);
    EXPECT_FALSE(service.HasInventory(1));
}

// ============================================================================
// Search and Query Tests
// ============================================================================

TEST(InventoryServiceTests, FindItems_ByName_ReturnsMatches) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem healthPotion{};
    healthPotion.itemId = 100;
    healthPotion.name = "Health Potion";

    InventoryItem manaPotion{};
    manaPotion.itemId = 101;
    manaPotion.name = "Mana Potion";

    InventoryItem sword{};
    sword.itemId = 200;
    sword.name = "Iron Sword";

    service.AddItem(1, healthPotion);
    service.AddItem(1, manaPotion);
    service.AddItem(1, sword);

    auto results = service.FindItems(1, "Potion");

    EXPECT_EQ(results.size(), size_t(2));
}

TEST(InventoryServiceTests, FindItems_NoMatches_ReturnsEmpty) {
    InventoryService service;
    service.CreateInventory(1, 20);

    InventoryItem sword{};
    sword.itemId = 200;
    sword.name = "Iron Sword";
    service.AddItem(1, sword);

    auto results = service.FindItems(1, "Potion");

    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Item Properties Tests
// ============================================================================

TEST(InventoryServiceTests, InventoryItem_CanStack_ReturnsCorrectValue) {
    InventoryItem stackable{};
    stackable.maxStack = 50;

    InventoryItem nonStackable{};
    nonStackable.maxStack = 1;

    EXPECT_TRUE(stackable.CanStack());
    EXPECT_FALSE(nonStackable.CanStack());
}

TEST(InventoryServiceTests, InventoryItem_IsStackFull_DetectsFullStack) {
    InventoryItem item{};
    item.quantity = 50;
    item.maxStack = 50;

    EXPECT_TRUE(item.IsStackFull());

    item.quantity = 30;
    EXPECT_FALSE(item.IsStackFull());
}

TEST(InventoryServiceTests, InventoryItem_GetRemainingStackSpace_CalculatesCorrectly) {
    InventoryItem item{};
    item.quantity = 30;
    item.maxStack = 50;

    EXPECT_EQ(item.GetRemainingStackSpace(), 20);
}

// ============================================================================
// Equipment Type Tests
// ============================================================================

TEST(InventoryServiceTests, Equipment_IsWeapon_DetectsWeaponSlots) {
    Equipment mainHandSword{};
    mainHandSword.slot = EquipmentSlot::MainHand;

    Equipment offHandDagger{};
    offHandDagger.slot = EquipmentSlot::OffHand;

    Equipment helmet{};
    helmet.slot = EquipmentSlot::Head;

    EXPECT_TRUE(mainHandSword.IsWeapon());
    EXPECT_TRUE(offHandDagger.IsWeapon());
    EXPECT_FALSE(helmet.IsWeapon());
}

TEST(InventoryServiceTests, Equipment_IsArmor_DetectsArmorSlots) {
    Equipment helmet{};
    helmet.slot = EquipmentSlot::Head;

    Equipment chest{};
    chest.slot = EquipmentSlot::Chest;

    Equipment sword{};
    sword.slot = EquipmentSlot::MainHand;

    EXPECT_TRUE(helmet.IsArmor());
    EXPECT_TRUE(chest.IsArmor());
    EXPECT_FALSE(sword.IsArmor());
}