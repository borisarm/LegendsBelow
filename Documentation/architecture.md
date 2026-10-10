# LegendsBelow – Architecture

This is the single source of truth for the software architecture: how the code is organized now, the rules that govern dependencies, and the target design still to be built.

Related documents:
- `modularized-game-architecture.md`: migration plan for the Win32 shell → `LB.Engine` → `LB.RenderingSystem` split
- `microsoft-proxy-implementation.md`: repository abstraction via Microsoft Proxy
- `asset-preprocesing-strategy-instructions.md`: JSON → binary asset pipeline
- `design-technical-directx12.md`: rendering design
- `.github/copilot-instructions.md`: coding conventions, module rules, and common build errors

**Last Updated**: 2026-10-09

---

## 1. Principles

- **Layered / clean architecture.** Dependencies point downward only; no cycles.
- **C++20 modules everywhere** except LBGameEditor (WinUI) and the test projects, which use headers and `pch.h`.
- **Domain-driven.** Domain types are small, composable value types and entities with no platform or framework dependencies.
- **Data-driven gameplay.** Tunable values (items, spells, XP curves, loot tables) live in data files, not code.
- **Testable logic.** Rules such as damage, saves, and XP are side-effect-light functions covered by GoogleTest projects.
- **Duck-typed abstractions.** Repository interfaces use Microsoft Proxy rather than virtual inheritance.

---

## 2. Layers

```
┌──────────────────────────────────────────────────────────┐
│ LegendsBelow          Win32/GDK shell (executable)       │
├──────────────────────────────────────────────────────────┤
│ LBEngineCore          LB.Engine: game loop, orchestration│
├──────────────────────────────────────────────────────────┤
│ Systems               Rendering, Input, Audio, Asset,    │
│                       Persistence  (technical, no domain)│
├──────────────────────────────────────────────────────────┤
│ Services              Character, Combat, Inventory,      │
│                       Progression, Dungeon, Repositories │
├──────────────────────────────────────────────────────────┤
│ Domain (Model)        LB.Core + Character, Combat,       │
│                       Inventory, Dungeon                 │
└──────────────────────────────────────────────────────────┘
```

Systems and Services are siblings, not stacked: Systems know nothing about the domain, Services know nothing about the platform. LBEngineCore is the only place that composes both.

### Import rules

| Layer | May import |
|-------|------------|
| Domain | `LB.Core` only (see §5 for current deviations) |
| Services | `LB.Core` + domain modules |
| Systems | `LB.Core` only |
| LBEngineCore | Systems + Services + domain |
| LegendsBelow | `LB.Engine` only |
| Tests | Component under test + `LB.Core` |

---

## 3. Solution Layout and Module Map

All libraries are static libraries (`.lib`); every module project targets x64 with `/std:c++20`.

### Domain – `Components/Model/`

| Project | Primary module | Sub-modules | Namespace |
|---------|----------------|-------------|-----------|
| LBModel.Core | `LB.Core` | `LB.Common`, `LB.CoreValueObjects`, `LB.Entity` | `LB` |
| LBModel.Character | `LB.Character` | `.Entities`, `.Enums` | `LB::Character` |
| LBModel.Combat | `LB.Combat` | `.Enums` | `LB::Combat` |
| LBModel.Inventory | `LB.Inventory` | `.Entities`, `.Enums`, `.ValueObjects` | `LB::Inventory` |
| LBModel.Dungeon | `LB.Dungeon` | `.Entities`, `.Enums`, `.ValueObjects` | `LB::Dungeon` |
| LBModel.Progression | *(stub, not in solution)* | | |

`LB.Core` provides `String`, `Id`, `Position`, `Dice`, `Attributes`, and `Entity`. Import `LB.Core`, not `LB.Common`.

### Services – `Components/Services/`

| Project | Module | Namespace | Status |
|---------|--------|-----------|--------|
| LBCharacterService | `LB.CharacterService` | `LB::Services::Character` | Implemented and tested; data-driven class rules (`ClassRules` from classes.json): HP/MP from the progression tables (design 4.2 formulas), movement speed, and `GetAttackBonus` with per-class weapon adjustments; a class without rules cannot be created |
| LBCombatService | `LB.CombatService` | `LB::Services::Combat` | Implemented and tested |
| LBInventoryService | `LB.InventoryService` | `LB::Services::Inventory` | Implemented and tested |
| LBProgressionService | `LB.ProgressionService` (+5 sub-modules) | `LB::Services::Progression` | Implemented and tested |
| LBDungeonService | `LB.DungeonService` | `LB::Services::Dungeon` | Implemented and tested |
| LBAssetPersistenceService | `LB.Repositories`, `LB.Content`, `LB.ContentRepositories` | `LB::Repositories`, `LB::Content` | Content definitions (`SpellDefinition`, `WeaponDefinition`, `ArmorDefinition`) loaded strictly from `Assets/Data` JSON (nlohmann-json); spell repository facade (Microsoft Proxy 4) with a content-backed implementation |

### Systems – `Components/Systems/`

| Project | Module | Namespace | Status |
|---------|--------|-----------|--------|
| LBRenderingSystem | `LB.RenderingSystem` | `LB::Systems::Rendering` | Draw-command API with a Direct3D 12 backend (device, swap chain, clear/present); headless when no window is given. Sprite/text drawing not implemented yet |
| LBInputSystem | `LB.InputSystem` | `LB::Systems::Input` | Keyboard/mouse pushed by the host; gamepads read through GameInput (internal partition `:GameInputBackend`) with radial dead zones; transition-based edge detection (no lost taps); actions with multiple bindings |
| LBAudioSystem | `LB.AudioSystem` | `LB::Systems::Audio` | XAudio2 output device (internal partition `:XAudio2Backend`), master volume, suspend/resume, channel pause; clip playback awaits audio assets |
| LBAssetSystem | `LB.AssetSystem` | `LB::Systems::Asset` | Asset registry/lifecycle; no file I/O yet |
| LBPersistenceSystem | `LB.PersistenceSystem` | `LB::Systems::Persistence` | Save slots (metadata + key/value `SaveData`) and preferences as JSON files, written atomically; existing saves discovered at startup; in-memory when no directory is given |

### Engine and application

| Project | Status |
|---------|--------|
| `Components/LBEngineCore` | `LB.Engine` (`:StepTimer`, `:GameEngine`): fixed 60 Hz loop driving Rendering, Input, Audio, and Persistence; loads content at startup (`EngineOptions`); owns the player character (CharacterService) moving in a placeholder test arena; tactical pause (Esc/Start); quick save/load (F5/F9). No Win32 dependency. Asset system, combat, and dungeon generation are not wired in yet |
| `LegendsBelow` | Win32/GDK shell only: `Main.cpp` creates the window, runs the message loop, and forwards events to `LB::Engine::GameEngine` |

The native window crosses module boundaries only as an opaque `void*`. Direct3D types stay inside LBRenderingSystem's internal partition `LB.RenderingSystem:DeviceResources` (`DeviceResources.cpp`), and GameInput types inside LBInputSystem's `LB.InputSystem:GameInputBackend` (`GameInputBackend.cpp`); consumers cannot import either. `gameinput.lib` is a static loader merged into `LBInputSystem.lib`, so the game starts without the GameInput runtime (it then runs without gamepad support).

All projects share one vcpkg manifest at the repository root (`vcpkg.json`); LegendsBelow, LBRenderingSystem, and LBAssetPersistenceService enable manifest mode. `Directory.Build.targets` makes vcpkg install before C++ module dependency scanning, so a fresh clone builds on the first try.

### Tools

| Project | Purpose | Status |
|---------|---------|--------|
| `Tools/LBAssetProcessor` | Build-time JSON validation and binary packing | Empty project |
| `Tools/LBGameEditor` | WinUI content editor | Template only |

### Tests – `Components/Tests/`

One GoogleTest project per component under `Model/`, `Services/`, and `System/`, plus `LBEngineTests` (StepTimer and headless GameEngine). Test packages come from NuGet `packages.config`; restore with `msbuild LegendsBelow.slnx -t:restore -p:RestorePackagesConfig=true` or Visual Studio's package restore. Coverage targets: domain >90%, services >85%, systems >75%.

### Data – `Assets/Data/`

`spells.json` (45), `weapons.json` (12), `armour.json` (15), `classes.json` (4: starting attributes, primary attributes and growth, weapon and armour adjustments, spell schools, movement speed, level 1-20 progression from design-character-system.md). Loaded at startup by `LB::Content::ContentDatabase` from `<exe dir>/Assets/Data` (the build copies them there). Invalid content stops startup with every error listed; `ShippedContentTests` validate the files in the test run. Cross-file rule: class-restricted spells must be in one of the class's schools.

---

## 4. Target Design (Not Yet Built)

### 4.1 Engine orchestration (LB.Engine)

The shell → engine → renderer split from `modularized-game-architecture.md` is done. Remaining responsibilities:
- [x] Own the `StepTimer` and run `Tick → Update (fixed 60 Hz) → Render`
- [x] Input: the host forwards keyboard/mouse events (`OnKeyDown`, `OnMouseMove`, …); each simulation step reads input, then ends the input frame
- [x] Audio and Persistence systems; content loading; the player character through CharacterService
- Construct the remaining Services (combat, inventory, progression, dungeon) and inject repositories over `GetContent()`
- [x] Gamepad input through GameInput: `LB.InputSystem:GameInputBackend` (`GameInputBackend.cpp`) reads the most recently used gamepad; the engine enables it only with a real window and polls once per simulation step while focused
- Game state machine (main menu, town, dungeon, pause)
- Cross-system events (`EventBus`) so Services never call Systems directly

### 4.2 Repositories

Services depend on Proxy facades (`LB::Repositories::SpellRepository`, and later Weapon/Armor/Character). Concrete implementations live alongside the asset loading code and are wired up by LB.Engine.

### 4.3 Data flows

**Asset loading**
```
LB.Engine::Initialize
  → AssetSystem loads Assets/Data/*.bin (packed by LBAssetProcessor from JSON)
  → concrete repositories populated
  → repositories injected into Services
```

**Gameplay action**
```
Input (InputSystem) → GameState maps to command
  → CombatService resolves action (d20 roll, damage, status effects)
  → domain entities updated
  → EventBus publishes CharacterDamaged, etc.
  → presentation reacts (RenderingSystem draws health bar, AudioSystem plays hit)
```

**Save/load**
```
LB.Engine::Save → collect state from Services
  → PersistenceSystem serializes to slot → GDK storage (XGameSave)
```

### 4.4 Future considerations

- Modding: data/asset replacement, possible scripting layer
- Multiplayer/party: state sync and client/server separation (out of scope for now)
- Platform: Xbox Series X|S and Windows via GDK; ARM64 configurations exist but are not built

---

## 5. Known Deviations and Open Issues

- **Cross-domain imports.** `LB.Dungeon` imports `LB.Character`; `LB.Inventory` imports `LB.Character` and `LB.Combat`. Either relax the rule to an explicit domain ordering (Core → Character/Combat → Inventory/Dungeon) or move the shared IDs (e.g. `CharacterId`, `ItemId`) into `LB.Core`.
- **Stub module.** LBModel.Progression has three files all declaring `export module LB;`; it needs proper `LB.Progression*` names when implemented.
- **No vcpkg baseline.** The root `vcpkg.json` has no `builtin-baseline`, so a fresh install takes whatever port versions the local vcpkg provides (GDK, Agility SDK). Pin a baseline for reproducible builds.
- **Release|Win32 / ARM64 configurations** of the component projects are untested; build Debug|x64 or Release|x64.
- **Shadow spells are reserved.** The six Shadow spells in `spells.json` have no `requiredClass` on purpose: they belong to a special Cleric class that is not defined yet. Until then any class can cast them.
- **Class rules are data-driven.** Attribute growth is guided with a free share: each level gives 1 automatic point to the class's primary attributes (alternating; a capped primary passes it on) and 1 free point the player assigns (`SpendAttributePoint`). Primary attributes cap at 25, others at 18 (`attributeGrowth` in classes.json); the old hidden creation bonus is gone. Respec (game-design.md) is not implemented yet.
- **Armour penalties are class-scaled, not banned.** Any class can wear any armour; each piece's `movementPenalty` and `castPenalty` are multiplied by the class's `armorAdjustments` for its category (0.5 signature, 1 trained, 1.5 untrained, 2 restricted). Speed = (base + DEX_mod x 0.1) x (1 - sum of scaled movement penalties); cast time x (1 + sum of scaled cast penalties), exposed as `GetCastTimeMultiplier` (no spell casting consumes it yet). The weight-based penalty of design-armour-system.md 4.2 waits for inventory weight, and `CharacterService` tracks equipped armour itself until the inventory owns equipment.
- **Placeholder rendering.** The arena and player are solid rectangles drawn with render-target clears in a top-down view; isometric sprite rendering is still to be built.
