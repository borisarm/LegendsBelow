// LB.Character.ixx - Character domain module
export module LB.Character;

// Re-export core (Character depends on Core)
export import LB.Core;

// Re-export Character components
export import LB.Character.Enums;
export import LB.Character.Entities;

export namespace LB::Character {
    // Module version
    constexpr int MajorVersion = 1;
    constexpr int MinorVersion = 0;
}