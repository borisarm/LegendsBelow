// LB.Core.ixx - Core domain module (foundation for all domains)
export module LB.Core;

// The core is one module; its parts are partitions, so nothing can import a part on its own
export import :Common;          // String, Id, InvalidId
export import :ValueObjects;    // Position, Dice, Attributes, ...
export import :Entity;          // Entity
export import :Events;          // EventBus

export namespace LB::Core {
    // Module version information
    constexpr int MajorVersion = 1;
    constexpr int MinorVersion = 0;
    constexpr int PatchVersion = 0;
    constexpr const char* ModuleVersionString = "1.0.0";
}