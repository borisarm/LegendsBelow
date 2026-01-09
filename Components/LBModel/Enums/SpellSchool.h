#pragma once
#include "pch.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Spell schools defining magical disciplines
    /// Based on game design document spell categories
    /// </summary>
    enum class SpellSchool : uint32_t {
        None = 0,
        Evocation = 1,      // Direct damage (fire, lightning, force)
        Frost = 2,          // Control and slows (cold damage)
        Holy = 3,           // Healing, protection, radiant damage
        Shadow = 4,         // Curses, drains, necrotic damage
        Utility = 5         // Movement, vision, detection, teleportation
    };

    /// <summary>
    /// Convert SpellSchool to string
    /// </summary>
    constexpr const char* ToString(SpellSchool school) noexcept {
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

} // namespace Domain
} // namespace LB