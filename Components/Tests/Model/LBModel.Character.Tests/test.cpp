#include "pch.h"

import LB.Character;

using namespace LB::Character;

// ============================================================================
// CharacterClass Enum Tests
// ============================================================================

TEST(CharacterClassTests, EnumValues_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(CharacterClass::Warrior), static_cast<int>(CharacterClass::Rogue));
    EXPECT_NE(static_cast<int>(CharacterClass::Rogue), static_cast<int>(CharacterClass::Mage));
    EXPECT_NE(static_cast<int>(CharacterClass::Mage), static_cast<int>(CharacterClass::Cleric));
}

// ============================================================================
// Character Entity Tests
// ============================================================================

TEST(CharacterTests, Constructor_SetsBasicProperties) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    EXPECT_EQ(character.GetId(), 1);
    EXPECT_EQ(character.GetName(), "TestHero");
    EXPECT_EQ(character.GetClass(), CharacterClass::Warrior);
    EXPECT_EQ(character.GetLevel(), 1);
}

TEST(CharacterTests, Constructor_SetsDefaultHP) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    EXPECT_EQ(character.GetCurrentHitPoints(), 20);
    EXPECT_EQ(character.GetMaxHitPoints(), 20);
}

TEST(CharacterTests, Constructor_SetsDefaultMana) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestMage", CharacterClass::Mage, attribs);

    EXPECT_EQ(character.GetCurrentManaPoints(), 0);
    EXPECT_EQ(character.GetMaxManaPoints(), 0);
}

TEST(CharacterTests, Constructor_SetsDefaultArmorClass) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    EXPECT_EQ(character.GetArmorClass(), 10);
}

TEST(CharacterTests, Constructor_SetsDefaultPosition) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    const auto& pos = character.GetPosition();
    EXPECT_EQ(pos.x, 0);
    EXPECT_EQ(pos.y, 0);
}

TEST(CharacterTests, Constructor_InitiallyAlive) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    EXPECT_TRUE(character.IsAlive());
}

TEST(CharacterTests, IsAlive_PositiveHP_ReturnsTrue) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);
    character.SetHitPoints(10, 20);

    EXPECT_TRUE(character.IsAlive());
}

TEST(CharacterTests, IsAlive_ZeroHP_ReturnsFalse) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);
    character.SetHitPoints(0, 20);

    EXPECT_FALSE(character.IsAlive());
}

TEST(CharacterTests, IsAlive_NegativeHP_ReturnsFalse) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);
    character.SetHitPoints(-5, 20);

    EXPECT_FALSE(character.IsAlive());
}

// ============================================================================
// Character Attribute Tests
// ============================================================================

TEST(CharacterTests, GetAttributes_ReturnsSetAttributes) {
    LB::Attributes attribs{15, 14, 13, 12, 11, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    const auto& charAttribs = character.GetAttributes();

    EXPECT_EQ(charAttribs.strength, 15);
    EXPECT_EQ(charAttribs.dexterity, 14);
    EXPECT_EQ(charAttribs.constitution, 13);
    EXPECT_EQ(charAttribs.intelligence, 12);
    EXPECT_EQ(charAttribs.wisdom, 11);
    EXPECT_EQ(charAttribs.charisma, 10);
}

TEST(CharacterTests, SetAttributes_UpdatesAttributes) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    LB::Attributes newAttribs{20, 18, 16, 14, 12, 10};
    character.SetAttributes(newAttribs);

    const auto& charAttribs = character.GetAttributes();
    EXPECT_EQ(charAttribs.strength, 20);
    EXPECT_EQ(charAttribs.intelligence, 14);
}

// ============================================================================
// Character Level Tests
// ============================================================================

TEST(CharacterTests, SetLevel_UpdatesLevel) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    character.SetLevel(5);

    EXPECT_EQ(character.GetLevel(), 5);
}

TEST(CharacterTests, GetExperience_DefaultZero) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    EXPECT_EQ(character.GetExperience(), 0);
}

TEST(CharacterTests, SetExperience_UpdatesExperience) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    character.SetExperience(1000);

    EXPECT_EQ(character.GetExperience(), 1000);
}

// ============================================================================
// Character HP Tests
// ============================================================================

TEST(CharacterTests, SetHitPoints_UpdatesBoth) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    character.SetHitPoints(15, 30);

    EXPECT_EQ(character.GetCurrentHitPoints(), 15);
    EXPECT_EQ(character.GetMaxHitPoints(), 30);
}

TEST(CharacterTests, SetHitPoints_CurrentExceedsMax_Allowed) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    character.SetHitPoints(50, 30);

    EXPECT_EQ(character.GetCurrentHitPoints(), 50);
    EXPECT_EQ(character.GetMaxHitPoints(), 30);
}

// ============================================================================
// Character Mana Tests
// ============================================================================

TEST(CharacterTests, SetManaPoints_UpdatesBoth) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestMage", CharacterClass::Mage, attribs);

    character.SetManaPoints(25, 50);

    EXPECT_EQ(character.GetCurrentManaPoints(), 25);
    EXPECT_EQ(character.GetMaxManaPoints(), 50);
}

TEST(CharacterTests, SetManaPoints_ZeroValues) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestWarrior", CharacterClass::Warrior, attribs);

    character.SetManaPoints(0, 0);

    EXPECT_EQ(character.GetCurrentManaPoints(), 0);
    EXPECT_EQ(character.GetMaxManaPoints(), 0);
}

// ============================================================================
// Character Armor Class Tests
// ============================================================================

TEST(CharacterTests, SetArmorClass_UpdatesAC) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    character.SetArmorClass(18);

    EXPECT_EQ(character.GetArmorClass(), 18);
}

TEST(CharacterTests, SetArmorClass_NegativeValue) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    character.SetArmorClass(-5);

    EXPECT_EQ(character.GetArmorClass(), -5);
}

// ============================================================================
// Character Position Tests
// ============================================================================

TEST(CharacterTests, SetPosition_UpdatesPosition) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    LB::Position newPos(10, 20);
    character.SetPosition(newPos);

    const auto& pos = character.GetPosition();
    EXPECT_EQ(pos.x, 10);
    EXPECT_EQ(pos.y, 20);
}

TEST(CharacterTests, SetPosition_NegativeCoordinates) {
    LB::Attributes attribs{10, 10, 10, 10, 10, 10};
    Character character(1, "TestHero", CharacterClass::Warrior, attribs);

    LB::Position newPos(-5, -10);
    character.SetPosition(newPos);

    const auto& pos = character.GetPosition();
    EXPECT_EQ(pos.x, -5);
    EXPECT_EQ(pos.y, -10);
}

// ============================================================================
// Character Class Specific Tests
// ============================================================================

TEST(CharacterTests, CreateWarrior_CorrectClass) {
    LB::Attributes attribs{15, 10, 14, 8, 10, 10};
    Character warrior(1, "Conan", CharacterClass::Warrior, attribs);

    EXPECT_EQ(warrior.GetClass(), CharacterClass::Warrior);
}

TEST(CharacterTests, CreateRogue_CorrectClass) {
    LB::Attributes attribs{10, 16, 12, 10, 10, 14};
    Character rogue(2, "Shadow", CharacterClass::Rogue, attribs);

    EXPECT_EQ(rogue.GetClass(), CharacterClass::Rogue);
}

TEST(CharacterTests, CreateMage_CorrectClass) {
    LB::Attributes attribs{8, 10, 10, 18, 14, 10};
    Character mage(3, "Gandalf", CharacterClass::Mage, attribs);

    EXPECT_EQ(mage.GetClass(), CharacterClass::Mage);
}

TEST(CharacterTests, CreateCleric_CorrectClass) {
    LB::Attributes attribs{10, 10, 12, 10, 16, 12};
    Character cleric(4, "Pelor", CharacterClass::Cleric, attribs);

    EXPECT_EQ(cleric.GetClass(), CharacterClass::Cleric);
}

// ============================================================================
// Id Type Tests
// ============================================================================

TEST(CharacterIdTests, CharacterId_IsInt32) {
    CharacterId id = 42;
    EXPECT_EQ(id, 42);
}

TEST(SpellIdTests, SpellId_IsInt32) {
    SpellId id = 10;
    EXPECT_EQ(id, 10);
}

TEST(ItemIdTests, ItemId_IsInt32) {
    ItemId id = 99;
    EXPECT_EQ(id, 99);
}