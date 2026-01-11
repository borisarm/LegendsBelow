// LB.Inventory.ixx - Inventory domain module
export module LB.Inventory;

// Re-export dependencies
export import LB.Core;
export import LB.Character;
export import LB.Combat;

// Re-export Inventory components
export import LB.Inventory.Enums;
export import LB.Inventory.ValueObjects;
export import LB.Inventory.Entities;

export namespace LB::Inventory {
    // Module version
    constexpr int MajorVersion = 1;
    constexpr int MinorVersion = 0;
}