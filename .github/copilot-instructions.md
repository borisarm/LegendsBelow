# GitHub Copilot Instructions for LegendsBelow

## Project Overview
LegendsBelow is a DirectX game project built with C++ for Xbox/Windows platforms.

## Code Style Guidelines
- Follow existing C++ coding conventions in the codebase
- Use DirectX Toolkit (DirectXTK) patterns and best practices
- Maintain consistent indentation and formatting with existing files
- Use meaningful variable and function names

## Project Structure
- **Common**: Shared utility classes (StepTimer, DeviceResources)
- **Assets**: Game assets (images, icons, manifests)
- Main game logic in `Game.cpp` and `Game.h`

## Best Practices
- Keep DirectX resource management patterns consistent with DeviceResources
- Follow the existing game loop structure in Game class
- Use precompiled headers (pch.h) for commonly included headers
- Maintain Xbox compatibility when adding features

## Suggested Patterns
- Use RAII for resource management
- Prefer smart pointers (ComPtr) for DirectX COM objects
- Follow the existing Update/Render separation pattern
- Use the StepTimer for game timing logic

## Architectural Guidelines
- Favor data-driven design for gameplay systems (characters, items, abilities, dungeons) using simple JSON or equivalent.
- Keep engine/technical concerns (rendering, input, audio) separated from gameplay logic (stats, combat, AI).
- Model core concepts (character, stats, status effects, abilities) as small, composable types instead of deep inheritance trees.
- Isolate combat and stat calculation logic into testable, side-effect-light functions or modules.
- Avoid coupling UI directly to gameplay data; use view models or thin adapters where possible.

## Coding Design Guidelines
- Follow the patterns and terminology defined in `.github/design-*.md` documents when adding gameplay features.
- Assume C++20 as the baseline language standard and use its features where they improve clarity and safety (e.g., `enum class`, `std::optional`, ranges, `std::span`), while remaining consistent with existing code.
- Prefer clear, explicit functions for gameplay rules (e.g., damage, saves, XP) over inlined magic numbers.
- Keep configuration constants and tables (XP curve, class growth, loot tables) in data files rather than hard-coded.
- When adding new systems, start with simple structs and free functions; introduce classes only when lifetime or polymorphism require it.
- Add brief comments for non-obvious game rules or formulas, especially where behavior matches the design documents.

## C++20 Module Guidelines

### Module Naming Conventions
- Use dot notation for module names: `LB.ModuleName` (e.g., `LB.Domain`, `LB.Entities`)
- Use Pascal case for module names: `LB.CharacterService`, `LB.ValueObjects`
- Module files use `.ixx` extension for interface modules

### Module Partition Syntax
When a module becomes too large (>500 lines), split it using **module partitions**:

#### 1. Partition Files
Partition files declare themselves as part of a parent module using the `:PartitionName` syntax:

```cpp
// Entity.ixx - Partition file
export module LB.Entities:Entity;  // Note the colon syntax

import LB.Common;
import LB.Enums;
import :OtherPartition;  // Import sibling partition with relative syntax

export namespace LB::Domain {
    export class Entity {
        // Entity implementation
    };
}
```

#### 2. Importing Sibling Partitions
Within partition files, reference sibling partitions using **relative import** syntax:

```cpp
// Character.ixx - Partition file that depends on Entity
export module LB.Entities:Character;

import LB.Common;
import :Entity;  // Relative import of Entity partition (NO module name prefix)

export namespace LB::Domain {
    export class Character : public Entity {
        // Character implementation inheriting from Entity
    };
}
```

#### 3. Primary Module Interface
The primary module interface re-exports all partitions:

```cpp
// LB.Entities.ixx - Primary module interface
export module LB.Entities;

import LB.Common;  // Import dependencies
import LB.Enums;
import LB.ValueObjects;
import std;

export import :Entity;     // Re-export partitions
export import :Character;
export import :Spell;
```

### Key Rules for Module Partitions

1. **Partition Declaration**: `export module ParentModule:PartitionName;`
2. **Relative Import**: Use `:PartitionName` (with colon, no module prefix) to import sibling partitions
3. **Absolute Import**: Use `ModuleName` for external module dependencies
4. **Re-export**: Primary module uses `export import :PartitionName;` to expose partitions
5. **File Naming**: Use simple names for partition files (e.g., `Entity.ixx`, `Character.ixx`) 
6. **Project Configuration**: Add all partition files to the `.vcxproj` with `<CompileAs>CompileAsCppModule</CompileAs>`

### Example: LB.Entities Module Structure

```
Components/LBModel/
??? LB.Entities.ixx      // Primary module (exports all partitions)
??? Entity.ixx           // Partition: LB.Entities:Entity
??? Character.ixx        // Partition: LB.Entities:Character (imports :Entity)
??? Spell.ixx            // Partition: LB.Entities:Spell (imports :Entity)
```

### When to Use Module Partitions

- ? **Use partitions** when a single module exceeds 500-700 lines
- ? **Use partitions** for logical separation (Entity, Character, Spell as separate concerns)
- ? **Use partitions** when classes have clear inheritance/dependency relationships
- ? **Don't use partitions** for small modules (<300 lines)
- ? **Don't use partitions** when classes have no logical grouping

### Module Partition Benefits

1. **Separation of Concerns**: Each entity type in its own file
2. **Faster Compilation**: Changes to one partition don't rebuild others
3. **Better Navigation**: Easier to find and edit specific entity types
4. **Cleaner Diffs**: Git changes are isolated to specific partition files
5. **Maintainability**: Large modules become manageable chunks
