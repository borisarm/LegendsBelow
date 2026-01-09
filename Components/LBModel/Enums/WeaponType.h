#pragma once
#include <cstdint>

namespace LB {
namespace Domain {

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
    constexpr const char* ToString(WeaponType type) noexcept {
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

} // namespace Domain
} // namespace LB