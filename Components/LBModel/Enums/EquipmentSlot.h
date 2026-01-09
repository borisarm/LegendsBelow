#pragma once
#include <cstdint>

namespace LB {
namespace Domain {

    /// <summary>
    /// Equipment slots for armor and accessories
    /// Based on armor.json slot definitions
    /// </summary>
    enum class EquipmentSlot : uint32_t {
        None = 0,
        Head = 1,           // Helmets, hoods, caps
        Chest = 2,          // Armor, robes, shirts
        Hands = 3,          // Gloves, gauntlets
        Feet = 4,           // Boots, sandals
        MainHand = 5,       // Primary weapon
        OffHand = 6,        // Shields, off-hand weapons
        Ring1 = 7,          // First ring slot
        Ring2 = 8,          // Second ring slot
        Neck = 9,           // Amulets, necklaces
        Belt = 10           // Belts, sashes
    };

    /// <summary>
    /// Convert EquipmentSlot to string
    /// </summary>
    constexpr const char* ToString(EquipmentSlot slot) noexcept {
        switch (slot) {
            case EquipmentSlot::None: return "None";
            case EquipmentSlot::Head: return "Head";
            case EquipmentSlot::Chest: return "Chest";
            case EquipmentSlot::Hands: return "Hands";
            case EquipmentSlot::Feet: return "Feet";
            case EquipmentSlot::MainHand: return "MainHand";
            case EquipmentSlot::OffHand: return "OffHand";
            case EquipmentSlot::Ring1: return "Ring1";
            case EquipmentSlot::Ring2: return "Ring2";
            case EquipmentSlot::Neck: return "Neck";
            case EquipmentSlot::Belt: return "Belt";
            default: return "Unknown";
        }
    }

} // namespace Domain
} // namespace LB