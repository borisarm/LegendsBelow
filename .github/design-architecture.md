# LegendsBelow – Architecture Design

## 1. Purpose

This document defines the overall software architecture for LegendsBelow, including:
- Component organization and responsibilities
- Dependency relationships and layering
- Service boundaries and interfaces
- Data flow and communication patterns
- Build and deployment structure

This architecture follows Domain-Driven Design (DDD) principles with clear separation of concerns and dependency inversion.

---

## 2. Architecture Overview

LegendsBelow follows a **layered architecture** with **hexagonal/clean architecture** principles:

```
???????????????????????????????????????????????????
?              LegendsBelow (Application)         ?
?                                                 ?
???????????????????????????????????????????????????
?              LBEngineCore (Core)                ?
?                                                 ?
???????????????????????????????????????????????????
?  LBAssetSystem  ?  LBRenderSystem  ?  Systems   ?
?                                                 ?
???????????????????????????????????????????????????
?  LBCharacterSvc ?  LBCombatSvc  ?  Services     ?
?                                                 ?
???????????????????????????????????????????????????
?              LBModel (Domain)                   ?
???????????????????????????????????????????????????
```

---

## 3. Component Structure

### 3.1 Current Component Layout

```
LegendsBelow/
??? Components/
?   ??? LBModel/                    # Domain layer
?   ??? LBEngineCore/              # Core orchestration
?   ??? LBAssetSystem/             # Asset management system
?   ??? Tests/
?       ??? LBModelTests/          # Domain tests
?       ??? LBEngineTests/         # Core tests
??? Tools/
?   ??? LBAssetProcessor/          # Asset build tools
?   ??? LBGameEditor/              # Content editor
??? LegendsBelow/                  # Main application
```

### 3.2 Planned Component Expansion

```
Components/
??? LBModel/                       # Domain foundation
??? Services/
?   ??? LBCharacterService/        # Character business logic
?   ??? LBCombatService/           # Combat mechanics
?   ??? LBProgressionService/      # Character advancement
?   ??? LBDungeonService/          # World generation
?   ??? LBInventoryService/        # Item management
??? Systems/
?   ??? LBAssetSystem/             # Asset loading/management
?   ??? LBRenderingSystem/         # Graphics rendering
?   ??? LBAudioSystem/             # Audio management
?   ??? LBInputSystem/             # Input handling
?   ??? LBPersistenceSystem/       # Save/load system
??? LBEngineCore/                  # System orchestration
??? Tests/
    ??? LBModelTests/
    ??? LBServiceTests/
    ??? LBSystemTests/
```

---

## 4. Layer Definitions

### 4.1 Layer 1: Domain (LBModel)

**Purpose**: Pure business domain with no external dependencies.

**Contents**:
```cpp
LBModel/
??? Entities/                      # Core domain objects
?   ??? Character.h/cpp           # Player/NPC characters
?   ??? Spell.h/cpp               # Magic spells and effects
?   ??? Weapon.h/cpp              # Weapons and tools
?   ??? Armor.h/cpp               # Protective equipment
?   ??? Item.h/cpp                # Consumables and misc items
?   ??? Dungeon.h/cpp             # Dungeon structures
?   ??? Room.h/cpp                # Individual rooms/areas
?   ??? Quest.h/cpp               # Objectives and missions
??? ValueObjects/                 # Immutable domain values
?   ??? Attributes.h/cpp          # STR, DEX, CON, INT, WIS, CHA
?   ??? DamageType.h/cpp          # Physical, fire, cold, etc.
?   ??? StatusEffect.h/cpp        # Buffs, debuffs, conditions
?   ??? Position.h/cpp            # World coordinates
?   ??? Dice.h/cpp                # Dice rolling mechanics
??? Enums/                        # Domain enumerations
?   ??? CharacterClass.h          # Warrior, Rogue, Mage, Cleric
?   ??? SpellSchool.h             # Evocation, control, etc.
?   ??? WeaponType.h              # Sword, axe, bow, etc.
?   ??? DamageType.h              # Elemental damage types
??? Interfaces/                   # Domain service contracts
    ??? ICharacterRepository.h    # Character data access
    ??? ISpellRepository.h        # Spell data access
    ??? IWeaponRepository.h       # Weapon data access
    ??? IDungeonGenerator.h       # Dungeon creation contract
    ??? IAssetLoader.h            # Asset loading abstraction
```

**Dependencies**: None (pure domain)

**Responsibilities**:
- Define core business entities and rules
- Provide contracts for external dependencies
- Contain domain validation logic
- Define domain events and invariants

### 4.2 Layer 2: Services (Business Logic)

**Purpose**: Implement business use cases and coordinate domain objects.

**Service Components**:

#### LBCharacterService
```cpp
??? CharacterService.h/cpp        # Character lifecycle management
??? ProgressionService.h/cpp      # Leveling and advancement
??? CreationService.h/cpp         # Character generation
??? AttributeService.h/cpp        # Stat calculations
```

#### LBCombatService  
```cpp
??? CombatService.h/cpp           # Combat orchestration
??? DamageCalculator.h/cpp        # Damage resolution
??? ActionResolver.h/cpp          # Action sequence handling
??? TargetingService.h/cpp        # Target selection logic
??? StatusEffectService.h/cpp     # Buff/debuff management
```

#### LBDungeonService
```cpp
??? DungeonService.h/cpp          # Dungeon management
??? GenerationService.h/cpp       # Procedural generation
??? RoomService.h/cpp             # Room population
??? LootService.h/cpp             # Treasure generation
??? EncounterService.h/cpp        # Enemy placement
```

**Dependencies**: LBModel (domain entities and interfaces)

**Responsibilities**:
- Implement business use cases
- Coordinate multiple domain entities
- Handle business workflow logic
- Validate business rules

### 4.3 Layer 3: Systems (Infrastructure)

**Purpose**: Provide technical infrastructure and implement domain interfaces.

#### LBAssetSystem
```cpp
??? AssetManager.h/cpp            # Asset lifecycle management
??? Repositories/                 # Domain repository implementations
?   ??? SpellRepository.h/cpp     # Implements ISpellRepository
?   ??? WeaponRepository.h/cpp    # Implements IWeaponRepository
?   ??? CharacterRepository.h/cpp # Implements ICharacterRepository
??? Loaders/                      # Asset loading implementations
?   ??? BinaryAssetLoader.h/cpp   # Binary format loader
?   ??? JSONAssetLoader.h/cpp     # JSON format loader
?   ??? HotReloadWatcher.h/cpp    # Development hot-reload
??? Cache/
    ??? AssetCache.h/cpp          # In-memory asset caching
    ??? CachePolicy.h/cpp         # Cache eviction policies
```

#### LBRenderingSystem
```cpp
??? RenderManager.h/cpp           # Rendering coordination
??? IsometricRenderer.h/cpp       # Isometric scene rendering
??? SpriteRenderer.h/cpp          # 2D sprite rendering
??? UIRenderer.h/cpp              # User interface rendering
??? ParticleRenderer.h/cpp        # Particle effects
??? TextRenderer.h/cpp            # Text and UI elements
```

#### LBAudioSystem
```cpp
??? AudioManager.h/cpp            # Audio system management
??? MusicPlayer.h/cpp             # Background music
??? SoundEffectPlayer.h/cpp       # Game sound effects
??? AudioSource.h/cpp             # Positioned audio sources
```

**Dependencies**: LBModel (interfaces), Services (use cases)

**Responsibilities**:
- Implement infrastructure concerns
- Provide technical capabilities
- Handle external resources
- Manage system-level state

### 4.4 Layer 4: Core (System Orchestration)

**Purpose**: Coordinate systems and manage application lifecycle.

```cpp
LBEngineCore/
??? Application/
?   ??? GameLoop.h/cpp            # Main application loop
?   ??? SystemManager.h/cpp       # System lifecycle management
?   ??? ServiceLocator.h/cpp      # Dependency injection container
?   ??? GameState.h/cpp           # Game state management
??? Events/
?   ??? EventBus.h/cpp            # Inter-system communication
?   ??? EventDispatcher.h/cpp     # Event routing
?   ??? GameEvents.h/cpp          # Domain event definitions
??? Timing/
?   ??? StepTimer.h/cpp           # Game timing utilities
?   ??? ScheduledTask.h/cpp       # Task scheduling
?   ??? UpdateManager.h/cpp       # Update loop coordination
??? Memory/
    ??? MemoryManager.h/cpp       # Memory pool management
    ??? ObjectPool.h/cpp          # Object pooling
    ??? SmartPointers.h/cpp       # Custom smart pointer types
```

**Dependencies**: All Systems and Services

**Responsibilities**:
- System initialization and shutdown
- Cross-system communication
- Resource management
- Performance monitoring

### 4.5 Layer 5: Application (Presentation)

**Purpose**: User interface and application-specific logic.

```cpp
LegendsBelow/
??? Application/
?   ??? Game.h/cpp                # Main game class
?   ??? GameConfiguration.h/cpp   # App configuration
?   ??? Platform/                 # Platform-specific code
??? UI/
?   ??? Screens/                  # Game screens
?   ?   ??? MainMenuScreen.h/cpp
?   ?   ??? GameplayScreen.h/cpp
?   ?   ??? CharacterScreen.h/cpp
?   ?   ??? InventoryScreen.h/cpp
?   ??? Controls/                 # UI controls
?   ??? Themes/                   # UI styling
??? Input/
    ??? InputMapper.h/cpp         # Input configuration
    ??? GameControls.h/cpp        # Game-specific controls
```

**Dependencies**: LBEngineCore, direct system access as needed

---

## 5. Dependency Flow

### 5.1 Dependency Graph
```
LegendsBelow
    ? uses
LBEngineCore 
    ? orchestrates
LBAssetSystem, LBRenderingSystem, LBAudioSystem, etc.
    ? uses
LBCharacterService, LBCombatService, LBDungeonService, etc.
    ? implements interfaces + uses
LBModel
```

### 5.2 Dependency Rules

1. **Lower layers never depend on higher layers**
2. **Dependencies point toward abstractions, not concretions**
3. **Cross-layer communication through interfaces only**
4. **No circular dependencies between components**

### 5.3 Interface Implementation

```cpp
// Domain defines contract (LBModel)
class ISpellRepository {
public:
    virtual ~ISpellRepository() = default;
    virtual const Spell* GetSpell(SpellId id) const = 0;
    virtual std::vector<SpellId> GetSpellsBySchool(SpellSchool school) const = 0;
};

// Service uses contract (LBCharacterService)
class SpellcastingService {
private:
    std::shared_ptr<ISpellRepository> spellRepo_;
public:
    SpellcastingService(std::shared_ptr<ISpellRepository> repo);
    bool CanCharacterCastSpell(const Character& character, SpellId spell);
};

// Infrastructure implements contract (LBAssetSystem)
class SpellRepository : public ISpellRepository {
private:
    std::unordered_map<SpellId, Spell> spells_;
    std::shared_ptr<IAssetLoader> assetLoader_;
public:
    const Spell* GetSpell(SpellId id) const override;
    // Implementation details...
};
```

---

## 6. Data Flow Patterns

### 6.1 Asset Loading Flow
```
1. Game.Initialize()
   ?
2. AssetManager.LoadAllAssets()
   ?
3. SpellRepository.LoadSpells()
   ?
4. BinaryAssetLoader.LoadFile("spells.bin")
   ?
5. Parse binary data into Spell entities
   ?
6. Store in repository for service access
```

### 6.2 Gameplay Flow
```
1. Player Input (UI Layer)
   ?
2. GameplayScreen.HandleInput()
   ?
3. CombatService.ExecuteAction()
   ?
4. DamageCalculator.CalculateDamage()
   ?
5. Character.TakeDamage()
   ?
6. EventBus.Publish(CharacterDamagedEvent)
   ?
7. RenderingSystem.UpdateHealthBar()
```

### 6.3 Save/Load Flow
```
1. Game.SaveGame()
   ?
2. PersistenceService.SaveWorldState()
   ?
3. Collect state from all services
   ?
4. Serialize to binary format
   ?
5. Write to storage system
```

---

## 7. Build and Deployment

### 7.1 Project Dependencies (MSBuild)

```xml
<!-- LBModel.vcxproj - No dependencies -->

<!-- LBCharacterService.vcxproj -->
<ProjectReference Include="..\LBModel\LBModel.vcxproj" />

<!-- LBAssetSystem.vcxproj -->
<ProjectReference Include="..\LBModel\LBModel.vcxproj" />
<ProjectReference Include="..\Services\LBCharacterService\LBCharacterService.vcxproj" />

<!-- LBEngineCore.vcxproj -->
<ProjectReference Include="..\LBModel\LBModel.vcxproj" />
<ProjectReference Include="..\Systems\LBAssetSystem\LBAssetSystem.vcxproj" />
<!-- All other systems and services -->

<!-- LegendsBelow.vcxproj -->
<ProjectReference Include="..\Components\LBEngineCore\LBEngineCore.vcxproj" />
```

### 7.2 Library Types

- **LBModel**: Static library (pure domain, no external dependencies)
- **Services**: Static libraries (business logic, testable)
- **Systems**: Static libraries (infrastructure implementations)
- **LBEngineCore**: Static library (system coordination)
- **LegendsBelow**: Executable (application entry point)

### 7.3 Asset Pipeline Integration

```msbuild
<!-- Integrated asset processing -->
<Target Name="ProcessAssets" BeforeTargets="Build">
  <Exec Command="$(OutDir)LBAssetProcessor.exe pack --input Data --out $(OutDir)Assets" />
</Target>
```

---

## 8. Testing Strategy

### 8.1 Test Project Structure

```
Tests/
??? LBModelTests/              # Pure domain testing
??? LBServiceTests/            # Business logic testing
?   ??? LBCharacterServiceTests/
?   ??? LBCombatServiceTests/
?   ??? LBDungeonServiceTests/
??? LBSystemTests/             # Infrastructure testing
?   ??? LBAssetSystemTests/
?   ??? LBRenderingSystemTests/
??? LBIntegrationTests/        # Cross-component testing
??? LBPerformanceTests/        # Performance benchmarks
```

### 8.2 Testing Approaches

- **Unit Tests**: Each layer tested in isolation with mocks
- **Integration Tests**: Cross-layer interaction testing
- **Contract Tests**: Interface implementation validation
- **Performance Tests**: System benchmarks and profiling

---

## 9. Key Design Principles

### 9.1 SOLID Principles
- **Single Responsibility**: Each component has one clear purpose
- **Open/Closed**: Extensible through interfaces, not modification
- **Liskov Substitution**: Implementations are interchangeable
- **Interface Segregation**: Small, focused interfaces
- **Dependency Inversion**: Depend on abstractions, not concretions

### 9.2 Domain-Driven Design
- **Domain Model**: Pure business logic in LBModel
- **Services**: Business use case implementations
- **Repositories**: Data access abstractions
- **Infrastructure**: Technical implementations

### 9.3 Clean Architecture
- **Framework Independence**: Core logic independent of DirectX, etc.
- **Testability**: Business logic testable without infrastructure
- **UI Independence**: Core logic independent of presentation
- **Database Independence**: Core logic independent of storage format

---

## 10. Future Considerations

### 10.1 Modding Support
- Plugin architecture for custom content
- Scripting system integration
- Asset replacement capabilities

### 10.2 Multiplayer
- Network communication layer
- State synchronization services
- Client/server separation

### 10.3 Platform Expansion
- Console-specific optimizations
- Mobile/touch adaptations
- Cross-platform asset compatibility

---

## 11. Tools and Utilities

### 11.1 LBAssetProcessor
- Command-line asset processing tool
- JSON validation and normalization
- Binary asset compilation
- Development workflow integration

### 11.2 LBGameEditor
- Visual content creation tool
- Real-time asset editing
- Integration with asset pipeline
- Level/dungeon design tools

---

This architecture provides a solid foundation for the LegendsBelow project while maintaining flexibility for future expansion and maintaining clear separation of concerns across all layers.