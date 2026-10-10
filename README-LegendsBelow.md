# LegendsBelow

An isometric, D&D-inspired dungeon crawler for Xbox and Windows, built on a custom C++20 engine using DirectX 12 and the Microsoft GDK.

| | |
|---|---|
| **Genre** | Isometric dungeon crawler / action RPG (real-time with tactical pause) |
| **Platform** | Xbox Series X\|S and Windows via Microsoft GDK |
| **Language** | C++20 with modules |
| **Graphics** | DirectX 12 |
| **Architecture** | Layered / clean architecture with domain-driven design |

## Repository Layout

```
LegendsBelow.slnx          Solution
LegendsBelow/              Win32/GDK shell: window, message loop, forwards events to LB.Engine
Components/
├── Model/                 Domain modules: LB.Core, LB.Character, LB.Combat, LB.Inventory, LB.Dungeon
├── Services/              Gameplay rules: Character, Combat, Inventory, Progression, Dungeon, Repositories
├── Systems/               Technical systems: Rendering, Input, Audio, Asset, Persistence
├── LBEngineCore/          LB.Engine: fixed 60 Hz game loop and system orchestration
└── Tests/                 GoogleTest project per component (Model/, Services/, System/)
Tools/
├── LBAssetProcessor/      JSON validation and binary packing (empty project)
└── LBGameEditor/          WinUI content editor (template)
Assets/Data/               spells.json, weapons.json, armour.json, classes.json
Documentation/             Game design and architecture documents
```

## Status

| Area | State |
|------|-------|
| Domain model | Implemented as C++20 modules, unit tested |
| Services (Character, Combat, Inventory, Progression, Dungeon) | Implemented, unit tested; CharacterService uses the class data (HP/MP progression, movement, weapon adjustments) |
| Systems | Unit tested. Rendering: Direct3D 12 with DirectXTK12: a lit 3D world pass of glTF models (fastgltf) and primitives through an isometric camera, plus a 2D rectangle overlay. Input: keyboard/mouse from the window, gamepads through GameInput. Audio: XAudio2 output device. Persistence: JSON save slots and preferences in `%LOCALAPPDATA%\LegendsBelow\Saves`. The Asset system has no file backend yet |
| Repositories | Spell facade via Microsoft Proxy 4 with a content-backed implementation |
| Engine / game loop | `LB.Engine` runs the loop and all wired systems. In the game: a Warrior (the KayKit knight) walks around a 3D test arena of KayKit floor tiles and walls with WASD/arrows or the left stick (screen-relative); Q/E or right stick left/right rotate the camera 90 degrees, the mouse wheel or right stick up/down zoom; Esc/Start toggles the tactical pause; F5/F9 quick save and load |
| Asset pipeline | Not started (see `Documentation/asset-preprocesing-strategy-instructions.md`) |
| Content | 4 classes, 45 spells, 12 weapons, 15 armour pieces authored in JSON, validated (including cross-file class rules) and loaded at startup. 3D models: a starter set of the KayKit Dungeon Pack (CC0) in `Assets/Models` |

## Getting Started

### Prerequisites
- Visual Studio 2022 17.14+ (or later) with the C++ desktop and game development workloads; `.slnx` solution support is required
- Microsoft Game Development Kit (GDK)
- vcpkg with Visual Studio integration enabled

### Build
1. Open `LegendsBelow.slnx` in Visual Studio.
2. vcpkg packages restore automatically from the root `vcpkg.json` on first build.
3. Restore the test projects' NuGet packages (GoogleTest) via Visual Studio's package restore, or `msbuild LegendsBelow.slnx -t:restore -p:RestorePackagesConfig=true`.
4. Build the `x64` platform. Profile configurations map to Release for the component libraries.

### Tests
Run the projects under `Components/Tests/` from Test Explorer. Each Model, Service, and System component has its own GoogleTest project.

## Documentation

- [Game Design Document](Documentation/game-design.md): vision, core loop, and an overview of every system
- System designs: [character](Documentation/design-character-system.md), [combat](Documentation/design-combat-system.md), [spells](Documentation/design-spells-system.md), [weapons](Documentation/design-weapons-system.md), [armour](Documentation/design-armour-system.md), [loot & progression](Documentation/design-loot-and-progression.md), [dungeon & world](Documentation/design-dungeon-and-world.md), [UI/UX & audio](Documentation/design-ui-ux-and-audio.md)
- [Architecture](Documentation/architecture.md): layers, module map, dependency rules, and known issues
- [Engine Modularization Plan](Documentation/modularized-game-architecture.md)
- [DirectX 12 Technical Design](Documentation/design-technical-directx12.md)
- [Asset Processing Strategy](Documentation/asset-preprocesing-strategy-instructions.md)
- [Microsoft Proxy Repositories](Documentation/microsoft-proxy-implementation.md)
- [Coding Guidelines](.github/copilot-instructions.md): module conventions, namespaces, and common build errors

## Origin

The project started from the Microsoft GDK DirectX 12 desktop template. `LegendsBelow/README.md` contains the original template notes.
