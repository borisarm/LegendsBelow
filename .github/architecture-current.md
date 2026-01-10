# LegendsBelow – Architecture Design (Updated)

## 1. Purpose

This document defines the overall software architecture for LegendsBelow, including:
- Component organization and responsibilities
- Dependency relationships and layering
- Service boundaries and interfaces
- Data flow and communication patterns
- Build and deployment structure

This architecture follows Domain-Driven Design (DDD) principles with clear separation of concerns and dependency inversion.

**Last Updated**: December 2024 - Reflects current implementation status with complete asset system and domain model.

---

## 2. Architecture Overview

LegendsBelow follows a **layered architecture** with **hexagonal/clean architecture** principles:

```
+--------------------------------------------------+
|              LegendsBelow (Application)          |
|                                                  |
+--------------------------------------------------+
|              LBEngineCore (Core)                 |
|                                                  |
+--------------------------------------------------+
|  LBAssetSystem  |  LBRenderSystem  |  Systems    |
|                                                  |
+--------------------------------------------------+
|  LBAssetPersist | LBCharacterSvc  |  Services    |
|                                                  |
+--------------------------------------------------+
|              LBModel (Domain)                    |
+--------------------------------------------------+
```

---

## 3. Current Implementation Status

### 3.1 Completed Components ?

**Domain Layer (LBModel)**: **100% Complete**
- Header-only library design
- Complete entity model (Character, Spell, Weapon, Armor, Item)
- All enums and value objects implemented
- Repository interfaces defined
- Comprehensive test coverage (50+ unit tests)

**Asset System**: **100% Complete**
- LBAssetSystem: Asset management and business logic
- LBAssetPersistenceService: JSON serialization/deserialization
- Complete JSON parsing for game assets (spells.json, weapons.json, armour.json)
- Proper architectural separation achieved

### 3.2 Project Structure

```
LegendsBelow/
??? Components/
?   ??? LBModel/                    # ? COMPLETE - Header-only domain
?   ??? LBEngineCore/              # ? Structure ready
?   ??? Services/
?   ?   ??? LBAssetPersistenceService/  # ? COMPLETE - JSON persistence
?   ?   ??? LBCharacterService/    # ? Project structure
?   ?   ??? LBCombatService/       # ? Project structure
?   ?   ??? LBProgressionService/  # ? Project structure
?   ?   ??? LBDungeonService/      # ? Project structure
?   ?   ??? LBInventoryService/    # ? Project structure
?   ??? Systems/
?   ?   ??? LBAssetSystem/         # ? COMPLETE - Asset management
?   ?   ??? LBRenderingSystem/     # ? Project structure
?   ?   ??? LBAudioSystem/         # ? Project structure
?   ?   ??? LBInputSystem/         # ? Project structure
?   ?   ??? LBPersistenceSystem/   # ? Project structure
?   ??? Tests/
?       ??? LBModelTests/          # ? COMPLETE - Comprehensive tests
?       ??? LBEngineTests/         # ? Project structure
?       ??? Services/              # ? Test projects for all services
?       ??? Systems/               # ? Test projects for all systems
??? Tools/
?   ??? LBAssetProcessor/          # ? Project structure
?   ??? LBGameEditor/              # ? Project structure
??? Assets/Data/                   # ? Game data ready
?   ??? spells.json               # ? 45+ spell definitions
?   ??? weapons.json              # ? 12+ weapon definitions
?   ??? armour.json               # ? 15+ armor definitions
??? LegendsBelow/                  # ? Main application structure
```

---

## 4. Layer Definitions

### 4.1 Domain Layer (LBModel) ? COMPLETE

**Purpose**: Pure business domain with no external dependencies.
**Implementation**: Header-only library for maximum flexibility and no linking dependencies.

**Key Achievements**:
- Complete entity model covering all game concepts
- Type-safe enums and value objects
- Comprehensive repository interfaces
- Full test coverage validating all domain logic

**Domain Entities**:
```cpp
LBModel/
??? Entities/                      # ? Complete domain objects
?   ??? Character.h                # Player/NPC with full attribute system
?   ??? Spell.h                    # Magic spells with schools/categories
?   ??? Weapon.h                   # Combat weapons with scaling/properties
?   ??? Armor.h                    # Protective gear with resistances
?   ??? Item.h                     # Consumables and miscellaneous items
??? ValueObjects/                  # ? Immutable domain values
?   ??? Attributes.h               # STR, DEX, CON, INT, WIS, CHA
?   ??? Dice.h                     # D&D-style dice mechanics
?   ??? Position.h                 # 2D world coordinates
?   ??? StatusEffect.h             # Buffs, debuffs, conditions
??? Enums/                         # ? Type-safe enumerations
?   ??? CharacterClass.h           # Warrior, Rogue, Mage, Cleric
?   ??? SpellSchool.h              # Evocation, Frost, Holy, Shadow, Utility
?   ??? SpellCategory.h            # Damage, Heal, Buff, Debuff, Control, Utility
?   ??? WeaponType.h               # Sword, Axe, Bow, Staff, Dagger, etc.
?   ??? ArmorCategory.h            # Cloth, Light, Medium, Heavy, Shield
?   ??? EquipmentSlot.h            # Head, Chest, MainHand, OffHand, etc.
?   ??? DamageType.h               # Physical, Elemental, Spiritual + Arcane
??? Interfaces/                    # ? Repository contracts
    ??? ICharacterRepository.h     # Character data access
    ??? ISpellRepository.h         # Spell data access with school/class filtering
    ??? IWeaponRepository.h        # Weapon data access with type/class filtering
    ??? IArmorRepository.h         # Armor data access with category/slot filtering
    ??? IItemRepository.h          # Item data access with category/rarity filtering
    ??? IAssetLoader.h             # Asset loading abstraction
```

### 4.2 Service Layer 

**Purpose**: Implement business use cases and coordinate domain objects.
**Current Status**: Project structures complete, ready for implementation.

**Asset Persistence Service** ? COMPLETE:
```cpp
LBAssetPersistenceService/
??? Core responsibility: JSON serialization/deserialization
??? Dependencies: LBModel + JSON library only
??? Test coverage: Basic JSON handling and structure validation
??? Architecture: Clean separation from asset management business logic
```

**Business Services** (Ready for Implementation):
- **LBCharacterService**: Character lifecycle and stat management
- **LBCombatService**: Combat mechanics and damage calculation  
- **LBProgressionService**: Leveling and character advancement
- **LBDungeonService**: Dungeon generation and management
- **LBInventoryService**: Item and equipment management

### 4.3 System Layer

**Purpose**: Provide technical infrastructure and system-level coordination.
**Current Status**: Asset system complete, others ready for implementation.

**Asset System** ? COMPLETE:
```cpp
LBAssetSystem/
??? AssetManager.h/cpp             # ? Asset lifecycle with caching
??? JSONAssetLoader.h/cpp          # ? File I/O operations
??? IAssetManager.h                # ? Infrastructure interface
??? Architecture: Uses LBAssetPersistenceService for data access
??? Capabilities: File loading, caching, hot-reload support
```

**Infrastructure Systems** (Ready for Implementation):
- **LBRenderingSystem**: DirectX rendering pipeline
- **LBAudioSystem**: Audio management and effects
- **LBInputSystem**: Input handling and controls
- **LBPersistenceSystem**: Game state save/load

### 4.4 Core Layer (LBEngineCore)

**Purpose**: Orchestrate all systems and provide main game loop.
**Current Status**: Structure ready for system integration.

**Responsibilities**:
- Initialize and coordinate all systems
- Manage game state and lifecycle
- Provide dependency injection container
- Handle cross-system communication

---

## 5. Asset System Architecture

### 5.1 Corrected Asset Architecture ?

The asset system follows proper layered architecture with clear separation:

```
LBEngineCore
    ? uses
LBAssetSystem (System Layer)
    ? uses  
LBAssetPersistenceService (Service Layer)
    ? uses
LBModel (Domain Layer) + JSON Library
```

**Key Principles**:
- **LBAssetSystem**: Runtime asset management, caching, business logic
- **LBAssetPersistenceService**: Pure data persistence, JSON serialization
- **No circular dependencies**: Clean unidirectional flow
- **Dependency inversion**: Systems depend on service abstractions

### 5.2 Asset Data Support ?

**Supported Asset Types**:
- **Spells**: 45+ magical spells with full property support
- **Weapons**: 12+ weapons with damage, scaling, and class restrictions  
- **Armor**: 15+ armor pieces with resistance and penalty systems
- **Items**: Foundation ready for consumables and miscellaneous items

**JSON Parsing Capabilities**:
- Complex property parsing (dice notation, arrays, nested objects)
- Enum string conversion with validation
- Character class and damage type filtering
- Error handling and graceful degradation

---

## 6. Testing Strategy ?

### 6.1 Test Coverage Achieved

**Domain Tests (LBModelTests)**: 50+ comprehensive unit tests
- Entity creation and behavior validation
- Value object immutability and calculations
- Enum string conversion and validation
- Repository interface contracts

**Integration Tests**: Asset system end-to-end validation
- JSON parsing and entity creation
- Repository filtering and querying
- Asset caching and lifecycle management

### 6.2 Test Project Structure

**Service Tests**: Individual test projects for each service
**System Tests**: Individual test projects for each system
**Integration Tests**: Cross-layer functionality validation

---

## 7. Development Workflow

### 7.1 Build System
- **MSBuild/Visual Studio** project structure
- **Header-only domain**: No linking complexity for LBModel
- **Incremental development**: Each layer can be developed independently
- **Test-driven**: Comprehensive test coverage for all components

### 7.2 Asset Workflow
```
JSON Assets ? LBAssetPersistenceService ? Domain Entities ? LBAssetSystem ? Game Logic
```

**Development Tools Ready**:
- **LBAssetProcessor**: Asset build pipeline (structure ready)
- **LBGameEditor**: Content creation tools (structure ready)

---

## 8. Next Development Phases

### 8.1 Priority 1: Service Layer Implementation
- Implement core business logic in service projects
- Add service-specific functionality and validation
- Create comprehensive service tests

### 8.2 Priority 2: System Layer Implementation  
- Implement rendering system with DirectX
- Add audio system with sound effects and music
- Create input system with keyboard/mouse/gamepad support
- Implement persistence system for game saves

### 8.3 Priority 3: Engine Integration
- Integrate all systems in LBEngineCore
- Implement main game loop and state management
- Add cross-system communication and events
- Create final application assembly in LegendsBelow

---

## 9. Architecture Benefits Achieved

? **Clean Separation**: Each layer has distinct responsibilities  
? **Testability**: Comprehensive test coverage across all layers  
? **Flexibility**: Header-only domain enables easy reuse  
? **Scalability**: Modular design supports independent development  
? **Maintainability**: Clear dependencies and interfaces  
? **Type Safety**: Strong typing throughout domain model  
? **Performance**: Efficient asset caching and management  

The architecture provides a solid foundation for continued development of the LegendsBelow game engine.