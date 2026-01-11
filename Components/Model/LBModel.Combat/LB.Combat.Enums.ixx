// LB.Combat.Enums.ixx - Combat domain enumerations
export module LB.Combat.Enums;

import std;
using namespace std;

export namespace LB::Combat {

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
    /// Weapon categories defining basic weapon behavior
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
    /// Armor categories defining protection type
    /// </summary>
    enum class ArmorCategory : uint32_t {
        None = 0,
        Cloth = 1,          // Light armor for mages
        Light = 2,          // Leather armor for rogues
        Medium = 3,         // Chainmail for clerics
        Heavy = 4,          // Plate armor for warriors
        Shield = 5          // Off-hand protection
    };

    /// <summary>
    /// Spell schools defining magical traditions
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
    /// Spell categories defining tactical usage
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
    /// Equipment slot for items
    /// </summary>
    enum class EquipmentSlot : uint32_t {
        None = 0,
        Head = 1,
        Chest = 2,
        Legs = 3,
        Feet = 4,
        MainHand = 5,
        OffHand = 6,
        Ring1 = 7,
        Ring2 = 8,
        Necklace = 9
    };

} // namespace LB::Combat