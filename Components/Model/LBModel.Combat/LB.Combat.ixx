// LB.Combat.ixx - Combat domain module
export module LB.Combat;

// Re-export core (Combat depends on Core)
export import LB.Core;

// Re-export Combat components
export import LB.Combat.Enums;

export namespace LB::Combat {
    // Module version
    constexpr int MajorVersion = 1;
    constexpr int MinorVersion = 0;
}