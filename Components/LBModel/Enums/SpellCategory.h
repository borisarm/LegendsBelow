#pragma once
#include <cstdint>

namespace LB {
namespace Domain {

    /// <summary>
    /// Spell categories for different magical effects
    /// Based on spells.json categories
    /// </summary>
    enum class SpellCategory : uint32_t {
        None = 0,
        Damage = 1,         // Offensive spells that deal damage
        Heal = 2,           // Restorative spells
        Buff = 3,           // Beneficial temporary effects  
        Debuff = 4,         // Harmful temporary effects
        Control = 5,        // Movement/action restriction
        Utility = 6         // Non-combat functionality
    };

    /// <summary>
    /// Convert SpellCategory to string
    /// </summary>
    constexpr const char* ToString(SpellCategory category) noexcept {
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

} // namespace Domain
} // namespace LB