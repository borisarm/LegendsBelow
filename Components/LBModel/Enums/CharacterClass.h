#pragma once
#include <cstdint>

namespace LB {
namespace Domain {

    /// <summary>
    /// Character classes defining core gameplay archetypes
    /// Based on D&D-inspired design from game specification
    /// </summary>
    enum class CharacterClass : uint32_t {
        None = 0,
        Warrior = 1,        // Frontline tank/melee DPS
        Rogue = 2,          // Agile striker with stealth and crits  
        Mage = 3,           // Ranged magical glass cannon
        Cleric = 4          // Support caster with healing and buffs
    };

    /// <summary>
    /// Convert CharacterClass to string for debugging/serialization
    /// </summary>
    constexpr const char* ToString(CharacterClass charClass) noexcept {
        switch (charClass) {
            case CharacterClass::None: return "None";
            case CharacterClass::Warrior: return "Warrior";
            case CharacterClass::Rogue: return "Rogue";
            case CharacterClass::Mage: return "Mage";
            case CharacterClass::Cleric: return "Cleric";
            default: return "Unknown";
        }
    }

} // namespace Domain
} // namespace LB