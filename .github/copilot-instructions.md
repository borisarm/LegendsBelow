# GitHub Copilot Instructions for LegendsBelow

## Project Overview
LegendsBelow is a DirectX game project built with C++ for Xbox/Windows platforms.

## Code Style Guidelines
- Follow existing C++ coding conventions in the codebase
- Use DirectX Toolkit (DirectXTK) patterns and best practices
- Maintain consistent indentation and formatting with existing files
- Use meaningful variable and function names

## Project Structure
- **Components/Model**: Domain models (Character, Combat, Core, Dungeon, Inventory)
- **Components/Services**: Business logic services (Character, Combat, Inventory, Progression, Dungeon, AssetPersistence)
- **Components/Systems**: Technical systems (Asset, Audio, Input, Persistence, Rendering)
- **Components/Tests**: Unit tests for all components (Model, Services, Systems)
- **Tools/LBAssetProcessor**: Build-time asset processing tool (JSON validation, format conversion, manifest generation)
- **Tools/LBGameEditor**: Level/content editor (future - for designing dungeons, placing items, etc.)
- **LegendsBelow**: Main game executable (DirectX game loop, Game.cpp/Game.h)

## Tool Projects

### LBAssetProcessor
**Purpose**: Pre-build asset processing and validation
- Validates JSON data files (characters, items, spells, dungeons)
- Generates asset manifests for runtime loading
- Converts/optimizes assets for target platforms (Xbox, Windows)
- Runs as pre-build step, NOT at runtime

**Current Phase**: Lightweight validation and manifest generation
**Future Phase**: Texture compression, audio optimization, binary packaging

### LBGameEditor
**Purpose**: Content creation tool for designers
- Visual dungeon layout editor
- Character/item/spell database editor
- Combat encounter designer
- Level testing and preview

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

export namespace LB {  // Note: Use LB namespace, not LB::Domain
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

export namespace LB {  // Use LB namespace for core types
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

Partitions are for **organizing a single cohesive module** into multiple files, NOT for splitting unrelated code.

**Use partitions when:**
- Types share a strong conceptual relationship (e.g., `Entity`, `Character`, `Spell` are all game entities)
- There's internal coupling that shouldn't be exposed (shared implementation details)
- You want ONE public module interface but multiple implementation files
- Types form a clear hierarchy within the same bounded context
- You need to **hide implementation details** while exposing only the public interface

**Don't use partitions when:**
- Types are conceptually independent (use separate modules instead)
- You're just trying to reduce file size (size is not a design criterion)
- Types belong to different domains or bounded contexts
- There's no shared internal coupling between the types

**Key Question**: Would a consumer ever want to import just ONE of these types independently?
- **Yes** ? Separate modules (`LB.Character`, `LB.Spell`)
- **No** ? Partitions within one module (`LB.Entities:Character`, `LB.Entities:Spell`)

**Information Hiding Example**:
```cpp
// CharacterImpl.ixx - Partition (NOT exported in primary module)
module LB.Character:Impl;

// Internal helper - NOT visible to consumers
class CharacterValidator {
    static bool ValidateStats(const Attributes& attribs);
};

// Character.ixx - Partition (exported)
export module LB.Character:Character;
import :Impl;  // Can use CharacterValidator internally

export class Character {
    // Public interface only
};

// LB.Character.ixx - Primary module
export module LB.Character;
export import :Character;  // Export Character
// :Impl is NOT exported - CharacterValidator stays hidden!
```

### Module Partition Benefits

1. **Information Hiding**: Partitions allow implementation details to remain non-exported while keeping the public interface clean
2. **Encapsulation**: Hide helper classes, internal types, and implementation functions from module consumers
3. **Separation of Concerns**: Each entity type in its own file while maintaining module cohesion
4. **Cleaner Public Interface**: Only export what's needed; keep implementation private within partitions
5. **Better Navigation**: Easier to find and edit specific entity types
6. **Maintainability**: Separate implementation from interface without breaking the module boundary

**Key Insight**: Partitions are about **information hiding and encapsulation**, not just file organization. They let you split a module into multiple files while controlling what gets exported to consumers.

## Critical Development Principles (Lessons Learned)

### ?? Module Import Rules (IMPORTANT)
**Problem**: Systems fail to compile with "Position not found" errors

**Rule**: Systems and Services must import `LB.Core`, NOT `LB.Common`
```cpp
// ? CORRECT - LB.Core provides all core types
import LB.Core;  // Exports: LB::String, LB::Position, LB::Id, LB::Attributes, LB::Entity

// ? WRONG - LB.Common is too limited
import LB.Common;  // Only exports: LB::String, LB::Id (missing Position!)
```

### ?? No LB::Domain Namespace (IMPORTANT)
**Problem**: Compilation errors about `LB::Domain` not existing

**Rule**: Use specific domain namespaces, not `LB::Domain`
```cpp
// ? CORRECT - Use specific namespaces
using LB::Character::CharacterId;
using LB::Character::CharacterClass;
using LB::Combat::DamageType;
using LB::Combat::EquipmentSlot;
using LB::Inventory::InventoryItem;

// ? WRONG - This namespace doesn't exist!
using LB::Domain::CharacterId;
using LB::Domain::ItemId;
```

### ?? Never Redefine Domain Types in Services
**Problem**: "Symbol cannot be defined" compilation errors

**Rule**: Re-export domain types, never redefine them
```cpp
// ? CORRECT - Re-export existing types from domain modules
using LB::Inventory::InventoryItem;
using LB::Inventory::Equipment;
using LB::Inventory::EquippedItems;

// ? WRONG - These already exist in LB.Inventory module!
struct InventoryItem { ... };  // Duplicate definition error
struct Equipment { ... };
```

### ?? Input System Frame Logic
**Problem**: `IsKeyReleased()` tests fail unexpectedly

**Rule**: Check input state BEFORE calling Update(), not after
```cpp
// ? CORRECT - Check state before Update() overwrites previous
system.SetKeyState(key, KeyState::Pressed);
system.Update();  // Copies current state ? previous state
system.SetKeyState(key, KeyState::Released);
EXPECT_TRUE(system.IsKeyReleased(key));  // current=Released, previous=Pressed ?

// ? WRONG - Second Update() makes previous=Released too!
system.SetKeyState(key, KeyState::Released);
system.Update();  // Now previous = Released
EXPECT_TRUE(system.IsKeyReleased(key));  // FAILS: both states are Released
```

### Project Configuration Requirements

Every new project MUST have these in `.vcxproj`:

1. **C++20 Language Standard** (in ALL ItemDefinitionGroup sections):
```xml
<ClCompile>
  <LanguageStandard>stdcpp20</LanguageStandard>
</ClCompile>
```

2. **Project References** (Test projects need BOTH component AND LBModel.Core):
```xml
<ItemGroup>
  <!-- The component being tested -->
  <ProjectReference Include="..\..\Systems\LBAssetSystem\LBAssetSystem.vcxproj">
    <Project>{component-guid}</Project>
  </ProjectReference>
  <!-- LBModel.Core required for LB.Core module -->
  <ProjectReference Include="..\..\Model\LBModel.Core\LBModel.Core.vcxproj">
    <Project>{990b9ac5-23ad-45d1-b267-5bef0a4c6ebf}</Project>
  </ProjectReference>
</ItemGroup>
```

### Namespace Organization Reference

```
LB::                                    # Root namespace
??? String, Id, InvalidId               # from LB.Common
??? Position, Dice, Attributes          # from LB.CoreValueObjects
??? Entity                              # from LB.Entity
?
??? Character::                         # Character domain (LB.Character)
?   ??? CharacterClass
?   ??? CharacterId, SpellId, ItemId
?   ??? Character
?
??? Combat::                            # Combat domain (LB.Combat)
?   ??? DamageType, WeaponType, ArmorCategory
?   ??? SpellSchool, SpellCategory
?   ??? EquipmentSlot
?
??? Inventory::                         # Inventory domain (LB.Inventory)
?   ??? InventoryItem, Equipment
?   ??? EquippedItems
?   ??? InventoryOperationResult
?
??? Dungeon::                           # Dungeon domain (LB.Dungeon)
?   ??? RoomType, RoomDifficulty
?   ??? Direction
?
??? Services::                          # Business logic layer
?   ??? Character::CharacterService
?   ??? Combat::CombatService
?   ??? Inventory::InventoryService
?   ??? Progression::ProgressionService
?   ??? Dungeon::DungeonService
?
??? Systems::                           # Technical systems layer
    ??? Asset::AssetSystem
    ??? Audio::AudioSystem
    ??? Input::InputSystem
    ??? Persistence::PersistenceSystem
    ??? Rendering::RenderingSystem
```

### Module Dependency Rules

```
Layer           Allowed Imports
?????????????????????????????????????????????????
Systems      ?  LB.Core ONLY (no domain knowledge)
Services     ?  LB.Core + Domain modules
Domains      ?  LB.Core ONLY (no cross-domain)
Tests        ?  Component under test + LB.Core
```

### Quick Error Solutions Reference

| Error Message | Solution |
|---------------|----------|
| `'LB::Domain': namespace does not exist` | Replace `LB::Domain::` with `LB::Character::`, `LB::Combat::`, etc. |
| `'Position': is not a member of 'LB'` | Change `import LB.Common;` to `import LB.Core;` |
| `'InventoryItem' cannot be defined` | Remove duplicate struct, use `using LB::Inventory::InventoryItem;` |
| `could not find module 'LB.Common'` | Add LBModel.Core project reference to `.vcxproj` |
| `IsKeyReleased` test fails | Check state BEFORE second `Update()` call |
| `'X': is not a member of 'Y'` | Verify namespace and check if type uses `.x` vs `.X()` |

### Testing Standards

1. **No Placeholder Tests** - Replace `EXPECT_EQ(1, 1)` with real tests
2. **Coverage Targets**:
   - Domain Models: >90% (entities, value objects, enums)
   - Services: >85% (business logic)
   - Systems: >75% (platform abstractions)
3. **Always Test**:
   - Success cases
   - Failure/error cases
   - Edge cases (min, max, zero, negative, empty)
   - State transitions

### Pre-Commit Checklist

- [ ] Correct module imports (`LB.Core` not `LB.Common`)
- [ ] Correct namespaces (no `LB::Domain::`)
- [ ] No duplicate type definitions
- [ ] Project references in `.vcxproj`
- [ ] C++20 language standard set
- [ ] All placeholder tests replaced
- [ ] All tests pass
- [ ] No compilation warnings
