// LB.Domain.ixx - Pure domain model module interface
export module LB.Domain;

// Re-export core domain components only (no repositories/infrastructure)
export import LB.Common;
export import LB.Enums;
export import LB.ValueObjects;
export import LB.Entities;

import std;

export namespace LB::Domain {
    // Module version information
    constexpr int MajorVersion = 1;
    constexpr int MinorVersion = 0;
    constexpr int PatchVersion = 0;
    constexpr const char* ModuleVersionString = "1.0.0";
}