// LB.Domain.ixx - Aggregate domain model (re-exports all domains)
export module LB.Domain;

// Re-export all domain models
export import LB.Core;
export import LB.Character;
export import LB.Combat;
export import LB.Dungeon;
export import LB.Inventory;

import std;

export namespace LB::Domain {
    // For backward compatibility, re-export commonly used types
    using LB::Id;
    using LB::String;
    using LB::Position;
    using LB::Dice;
    using LB::Attributes;

    using LB::Character::CharacterClass;
    using LB::Character::CharacterId;
    using LB::Character::ItemId;
    using LB::Character::SpellId;

    using LB::Combat::DamageType;
    using LB::Combat::WeaponType;
    using LB::Combat::ArmorCategory;
    using LB::Combat::EquipmentSlot;

    using LB::Dungeon::RoomType;
    using LB::Dungeon::RoomDifficulty;
    using LB::Dungeon::Direction;

    // Module version information
    constexpr int MajorVersion = 2;
    constexpr int MinorVersion = 0;
    constexpr int PatchVersion = 0;
    constexpr const char* ModuleVersionString = "2.0.0";
}