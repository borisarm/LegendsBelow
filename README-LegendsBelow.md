# LegendsBelow - Dungeon Crawler Game Engine

LegendsBelow is a comprehensive game engine built with C++ and DirectX for creating isometric dungeon crawler experiences. The project follows clean architecture principles with a modular, layered design.

## ?? Project Overview

**Genre**: Isometric Dungeon Crawler  
**Platform**: Xbox/Windows (Microsoft GDK)  
**Language**: C++20 with DirectX 12  
**Architecture**: Domain-Driven Design with Clean Architecture  

## ??? Current Implementation Status

### ? **Completed Components**

**Domain Layer (LBModel)** - 100% Complete
- Header-only domain library with 50+ unit tests
- Complete entity model: Character, Spell, Weapon, Armor, Item
- Type-safe enums and value objects
- Repository interfaces and contracts

**Asset System** - 100% Complete  
- LBAssetSystem: Asset management with caching
- LBAssetPersistenceService: JSON serialization
- Complete JSON parsing for game assets
- 45+ spells, 12+ weapons, 15+ armor pieces ready

### ?? **Project Structure Ready**
```
LegendsBelow/
??? Components/
?   ??? LBModel/                    # ? Complete domain model
?   ??? LBEngineCore/              # ??? Core engine orchestration
?   ??? Services/                  # ??? Business logic services
?   ?   ??? LBCharacterService/    # Character management
?   ?   ??? LBCombatService/       # Combat mechanics
?   ?   ??? LBProgressionService/  # Character advancement
?   ?   ??? LBDungeonService/      # World generation
?   ?   ??? LBInventoryService/    # Item management
?   ?   ??? LBAssetPersistenceService/ # ? JSON persistence
?   ??? Systems/                   # ??? Infrastructure systems
?   ?   ??? LBAssetSystem/         # ? Asset management
?   ?   ??? LBRenderingSystem/     # DirectX rendering
?   ?   ??? LBAudioSystem/         # Audio management
?   ?   ??? LBInputSystem/         # Input handling
?   ?   ??? LBPersistenceSystem/   # Save/load system
?   ??? Tests/                     # ? Comprehensive test coverage
??? Tools/                         # ??? Development tools
?   ??? LBAssetProcessor/          # Asset build pipeline
?   ??? LBGameEditor/              # Content creation tools
??? Assets/Data/                   # ? Game content ready
?   ??? spells.json               # 45+ spell definitions
?   ??? weapons.json              # 12+ weapon definitions
?   ??? armour.json               # 15+ armor pieces
??? LegendsBelow/                  # ??? Main application
```

## ??? Technical Foundation

### **Microsoft GDK Integration**
This project uses the Microsoft GDK with vcpkg for dependency management, enabling deployment to both PC and Xbox platforms.

### **Architecture Highlights**
- **Clean Architecture**: Domain ? Services ? Systems ? Core ? Application
- **Header-Only Domain**: Zero linking dependencies for domain model
- **Dependency Inversion**: Systems depend on service abstractions
- **Comprehensive Testing**: Unit and integration tests for all layers

### **Asset System**
```
Game Logic ? LBAssetSystem ? LBAssetPersistenceService ? JSON Files
```
- Efficient asset caching and management
- Hot-reload support for development
- Type-safe entity creation from JSON data
- Complete D&D-inspired content system

## ?? Game Features Ready

### **Character System** ?
- 4 Character Classes: Warrior, Rogue, Mage, Cleric
- 6 Core Attributes: STR, DEX, CON, INT, WIS, CHA
- Level progression and stat scaling
- Equipment and inventory management

### **Combat System** ?  
- 5 Spell Schools: Evocation, Frost, Holy, Shadow, Utility
- Multiple damage types: Physical, Elemental, Spiritual
- Weapon scaling and critical hit mechanics
- Armor resistances and damage mitigation

### **Content Assets** ?
- **45+ Spells**: From Magic Missile to Meteor
- **12+ Weapons**: Swords, axes, bows, staves
- **15+ Armor Pieces**: From cloth robes to plate mail
- **D&D-Inspired**: Familiar mechanics and progression

## ?? Getting Started

### **Prerequisites**
- Visual Studio 2022 v17.6+
- Microsoft Game Development Kit (GDK)
- vcpkg integration enabled

### **Building the Project**
1. Clone the repository
2. Open `LegendsBelow.sln` in Visual Studio
3. Restore vcpkg packages (automatic)
4. Build solution

### **Running Tests**
- **LBModelTests**: Complete domain model validation
- **LBAssetPersistenceServiceTests**: JSON parsing validation
- **Integration Tests**: End-to-end asset loading

## ?? Documentation

- **[Architecture Design](.github/architecture-current.md)**: Detailed system architecture
- **[Design Documents](.github/)**: Game design and mechanics
- **[Copilot Instructions](.github/copilot-instructions.md)**: Development guidelines

## ?? Next Development Phases

1. **Service Layer**: Implement business logic in service projects
2. **Rendering System**: DirectX 12 isometric rendering pipeline  
3. **Game Loop**: Engine integration and state management
4. **Content Tools**: Asset processor and game editor

## ?? Original Template

This project started from the Microsoft GDK template with DirectX 12 and has been extensively modified for the LegendsBelow game engine architecture.

### **VCPKG Integration**
The project uses Visual Studio's built-in vcpkg support. To update dependencies, modify `vcpkg.json` and `vcpkg-configuration.json`.

## ?? Contributing

The project follows clean architecture principles. See the architecture documentation for layer responsibilities and development guidelines.

---

*LegendsBelow - Building worlds, one dungeon at a time.* ??????