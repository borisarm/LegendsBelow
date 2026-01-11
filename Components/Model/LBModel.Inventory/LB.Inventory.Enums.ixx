// LB.Inventory.Enums.ixx - Inventory domain enumerations
export module LB.Inventory.Enums;

// EquipmentSlot is defined in Combat domain
export import LB.Combat.Enums;

export namespace LB::Inventory {
    // Re-export EquipmentSlot from Combat for convenience
    using LB::Combat::EquipmentSlot;
}