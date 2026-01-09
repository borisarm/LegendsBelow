#pragma once
#include <cstdint>

namespace LB {
namespace Domain {

    /// <summary>
    /// Armor categories defining protection levels and restrictions
    /// Based on armor.json category definitions
    /// </summary>
    enum class ArmorCategory : uint32_t {
        None = 0,
        Cloth = 1,          // Light fabric, no restriction, mage/cleric friendly
        Light = 2,          // Leather, minimal restriction, rogue/warrior 
        Medium = 3,         // Chain, moderate restriction, warrior/cleric
        Heavy = 4,          // Plate, high restriction, warrior only
        Shield = 5          // Blocking equipment for off-hand
    };

    /// <summary>
    /// Convert ArmorCategory to string
    /// </summary>
    constexpr const char* ToString(ArmorCategory category) noexcept {
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

} // namespace Domain
} // namespace LB