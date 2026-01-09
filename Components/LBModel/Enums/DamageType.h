#pragma once
#include "pch.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Damage types for attacks and resistances
    /// Based on combat system design document
    /// </summary>
    enum class DamageType : uint32_t {
        None = 0,
        
        // Physical damage types
        Slashing = 1,       // Swords, axes
        Piercing = 2,       // Daggers, arrows, spears
        Bludgeoning = 3,    // Maces, hammers
        
        // Elemental damage types
        Fire = 4,           // Burning effects
        Cold = 5,           // Freezing effects
        Lightning = 6,      // Shocking effects
        Acid = 7,           // Corrosive effects
        
        // Spiritual damage types
        Radiant = 8,        // Holy/divine damage
        Necrotic = 9,       // Death/decay damage
        Psychic = 10        // Mental/mind damage
    };

    /// <summary>
    /// Convert DamageType to string
    /// </summary>
    constexpr const char* ToString(DamageType type) noexcept {
        switch (type) {
            case DamageType::None: return "None";
            case DamageType::Slashing: return "Slashing";
            case DamageType::Piercing: return "Piercing";
            case DamageType::Bludgeoning: return "Bludgeoning";
            case DamageType::Fire: return "Fire";
            case DamageType::Cold: return "Cold";
            case DamageType::Lightning: return "Lightning";
            case DamageType::Acid: return "Acid";
            case DamageType::Radiant: return "Radiant";
            case DamageType::Necrotic: return "Necrotic";
            case DamageType::Psychic: return "Psychic";
            default: return "Unknown";
        }
    }

} // namespace Domain
} // namespace LB