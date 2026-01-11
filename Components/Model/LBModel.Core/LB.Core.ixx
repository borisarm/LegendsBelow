// LB.Core.ixx - Core domain module (foundation for all domains)
export module LB.Core;

// Re-export core components
export import LB.Common;
export import LB.CoreValueObjects;
export import LB.Entity;

export namespace LB::Core {
    // Module version information
    constexpr int MajorVersion = 1;
    constexpr int MinorVersion = 0;
    constexpr int PatchVersion = 0;
    constexpr const char* ModuleVersionString = "1.0.0";
}