// LB.Enums.ixx - Domain enumerations module interface  
export module LB.Enums;

import std;


using namespace std;

export namespace LB::Domain {

    /// <summary>
    /// Character classes defining gameplay archetypes
    /// Based on classic RPG design principles
    /// </summary>
    enum class CharacterClass : uint32_t {
        None = 0,
        Warrior = 1,    // Melee combat specialist
        Rogue = 2,      // Stealth and precision
        Mage = 3,       // Arcane spellcaster
        Cleric = 4      // Divine spellcaster and healer
    };

    /// <summary>
    /// Convert CharacterClass to string
    /// </summary>
    export constexpr const char* ToString(CharacterClass characterClass) noexcept {
        switch (characterClass) {
            case CharacterClass::None: return "None";
            case CharacterClass::Warrior: return "Warrior";
            case CharacterClass::Rogue: return "Rogue";
            case CharacterClass::Mage: return "Mage";
            case CharacterClass::Cleric: return "Cleric";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Damage types for combat mechanics
    /// Physical damage affected by armor, magical damage by resistances
    /// </summary>
    enum class DamageType : uint32_t {
        None = 0,
        // Physical damage types
        Slashing = 1,       // Swords, axes - blocked by armor
        Piercing = 2,       // Arrows, spears - penetrates some armor
        Bludgeoning = 3,    // Maces, hammers - effective vs armor
        
        // Elemental damage types  
        Fire = 4,           // Burning damage over time
        Cold = 5,           // Slows and freezes
        Lightning = 6,      // Fast, chaining damage
        
        // Spiritual damage types
        Holy = 7,           // Extra damage to undead
        Shadow = 8,         // Drains life force
        Arcane = 9          // Raw magical energy
    };

    /// <summary>
    /// Convert DamageType to string
    /// </summary>
    export constexpr const char* ToString(DamageType damageType) noexcept {
        switch (damageType) {
            case DamageType::None: return "None";
            case DamageType::Slashing: return "Slashing";
            case DamageType::Piercing: return "Piercing";
            case DamageType::Bludgeoning: return "Bludgeoning";
            case DamageType::Fire: return "Fire";
            case DamageType::Cold: return "Cold";
            case DamageType::Lightning: return "Lightning";
            case DamageType::Holy: return "Holy";
            case DamageType::Shadow: return "Shadow";
            case DamageType::Arcane: return "Arcane";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Spell schools defining magical traditions and specializations
    /// Each school has distinct characteristics and gameplay mechanics
    /// </summary>
    enum class SpellSchool : uint32_t {
        None = 0,
        Evocation = 1,      // Direct damage and energy manipulation
        Frost = 2,          // Cold-based magic with slowing effects
        Holy = 3,           // Divine magic for healing and protection
        Shadow = 4,         // Dark magic affecting life force
        Utility = 5         // Non-combat spells for exploration
    };

    /// <summary>
    /// Convert SpellSchool to string
    /// </summary>
    export constexpr const char* ToString(SpellSchool school) noexcept {
        switch (school) {
            case SpellSchool::None: return "None";
            case SpellSchool::Evocation: return "Evocation";
            case SpellSchool::Frost: return "Frost";
            case SpellSchool::Holy: return "Holy";
            case SpellSchool::Shadow: return "Shadow";
            case SpellSchool::Utility: return "Utility";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Spell categories defining tactical usage and effects
    /// Used for AI decision making and player spell organization
    /// </summary>
    enum class SpellCategory : uint32_t {
        None = 0,
        Damage = 1,         // Direct damage dealing
        Heal = 2,           // Health restoration
        Buff = 3,           // Temporary beneficial effects
        Debuff = 4,         // Temporary harmful effects
        Control = 5,        // Movement and action control
        Utility = 6         // Non-combat utility effects
    };

    /// <summary>
    /// Convert SpellCategory to string
    /// </summary>
    export constexpr const char* ToString(SpellCategory category) noexcept {
        switch (category) {
            case SpellCategory::None: return "None";
            case SpellCategory::Damage: return "Damage";
            case SpellCategory::Heal: return "Heal";
            case SpellCategory::Buff: return "Buff";
            case SpellCategory::Debuff: return "Debuff";
            case SpellCategory::Control: return "Control";
            case SpellCategory::Utility: return "Utility";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Weapon categories defining basic weapon behavior and properties
    /// Based on weapons system design document
    /// </summary>
    enum class WeaponType : uint32_t {
        None = 0,
        Sword = 1,          // Balanced melee, slashing damage
        Axe = 2,            // High damage, higher variance, slashing
        Mace = 3,           // Bludgeoning, better vs. armored targets
        Dagger = 4,         // Low damage, high crit chance, piercing
        Spear = 5,          // Extended reach melee, piercing
        Bow = 6,            // Ranged, requires arrows, piercing
        Crossbow = 7,       // Ranged, slower but higher impact, piercing
        Staff = 8,          // Melee + spell focus for casters
        Wand = 9            // Purely spell focus, low melee damage
    };

    /// <summary>
    /// Convert WeaponType to string
    /// </summary>
    export constexpr const char* ToString(WeaponType type) noexcept {
        switch (type) {
            case WeaponType::None: return "None";
            case WeaponType::Sword: return "Sword";
            case WeaponType::Axe: return "Axe";
            case WeaponType::Mace: return "Mace";
            case WeaponType::Dagger: return "Dagger";
            case WeaponType::Spear: return "Spear";
            case WeaponType::Bow: return "Bow";
            case WeaponType::Crossbow: return "Crossbow";
            case WeaponType::Staff: return "Staff";
            case WeaponType::Wand: return "Wand";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Armor categories defining protection level and restrictions
    /// Lighter armor allows more mobility but less protection
    /// </summary>
    enum class ArmorCategory : uint32_t {
        None = 0,
        Cloth = 1,          // Minimal protection, no movement penalty
        Light = 2,          // Light protection, minimal penalties
        Medium = 3,         // Balanced protection and mobility
        Heavy = 4,          // Maximum protection, movement penalties
        Shield = 5          // Off-hand defensive equipment
    };

    /// <summary>
    /// Convert ArmorCategory to string
    /// </summary>
    export constexpr const char* ToString(ArmorCategory category) noexcept {
        switch (category) {
            case ArmorCategory::None: return "None";
            case ArmorCategory::Cloth: return "Cloth";
            case ArmorCategory::Light: return "Light";
            case ArmorCategory::Medium: return "Medium";
            case ArmorCategory::Heavy: return "Heavy";
            case ArmorCategory::Shield: return "Shield";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Equipment slots for character equipment system
    /// Defines where items can be equipped on character
    /// </summary>
    enum class EquipmentSlot : uint32_t {
        None = 0,
        Head = 1,           // Helmets, hats, circlets
        Chest = 2,          // Armor, robes, shirts
        Legs = 3,           // Pants, leggings, tassets
        Feet = 4,           // Boots, shoes, sandals
        MainHand = 5,       // Primary weapon or tool
        OffHand = 6,        // Shield, secondary weapon, or focus
        Ring1 = 7,          // First ring slot
        Ring2 = 8,          // Second ring slot
        Necklace = 9        // Amulets, necklaces, torcs
    };

    /// <summary>
    /// Convert EquipmentSlot to string
    /// </summary>
    export constexpr const char* ToString(EquipmentSlot slot) noexcept {
        switch (slot) {
            case EquipmentSlot::None: return "None";
            case EquipmentSlot::Head: return "Head";
            case EquipmentSlot::Chest: return "Chest";
            case EquipmentSlot::Legs: return "Legs";
            case EquipmentSlot::Feet: return "Feet";
            case EquipmentSlot::MainHand: return "MainHand";
            case EquipmentSlot::OffHand: return "OffHand";
            case EquipmentSlot::Ring1: return "Ring1";
            case EquipmentSlot::Ring2: return "Ring2";
            case EquipmentSlot::Necklace: return "Necklace";
            default: return "Unknown";
        }
    }

} // namespace LB::Domain