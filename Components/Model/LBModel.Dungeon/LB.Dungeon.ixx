// LB.Dungeon.ixx - Dungeon domain module
export module LB.Dungeon;

// Re-export dependencies
export import LB.Core;
export import LB.Character;

// Re-export Dungeon components
export import LB.Dungeon.Enums;
export import LB.Dungeon.ValueObjects;
export import LB.Dungeon.Entities;
export import LB.Dungeon.Layout;

export namespace LB::Dungeon {
    // Module version
    constexpr int MajorVersion = 1;
    constexpr int MinorVersion = 0;
}