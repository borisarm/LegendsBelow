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
| LBCharacterService | `LB.CharacterService` | `LB::Services::Character` | Implemented and tested |
| LBCombatService | `LB.CombatService` | `LB::Services::Combat` | Implemented and tested |
| LBInventoryService | `LB.InventoryService` | `LB::Services::Inventory` | Implemented and tested |
| LBProgressionService | `LB.ProgressionService` (+5 sub-modules) | `LB::Services::Progression` | Implemented and tested |
| LBDungeonService | `LB.DungeonService` | `LB::Services::Dungeon` | Implemented and tested |
| LBAssetPersistenceService | `LB.Repositories` | `LB::Repositories` | Spell repository facade only (Microsoft Proxy) |

### Systems – `Components/Systems/`

| Project | Module | Namespace | Status |
|---------|--------|-----------|--------|
| LBRenderingSystem | `LB.RenderingSystem` | `LB::Systems::Rendering` | Platform-neutral draw-command API; no DX12 yet |
| LBInputSystem | `LB.InputSystem` | `LB::Systems::Input` | State tracking (keyboard/mouse/gamepad); no GameInput yet |
| LBAudioSystem | `LB.AudioSystem` | `LB::Systems::Audio` | Channel/clip bookkeeping; no audio API yet |
| LBAssetSystem | `LB.AssetSystem` | `LB::Systems::Asset` | Asset registry/lifecycle; no file I/O yet |
| LBPersistenceSystem | `LB.PersistenceSystem` | `LB::Systems::Persistence` | Save-slot model; no storage backend yet |

### Engine and application

| Project | Status |
|---------|--------|
| `Components/LBEngineCore` | Placeholder `.ixx` files only; references all Systems and Services |
| `LegendsBelow` | Still the DirectX 12 template (`Game`, `DeviceResources`, `StepTimer`, `pch.h`); references LBEngineCore but does not import it |

### Tools

| Project | Purpose | Status |
|---------|---------|--------|
| `Tools/LBAssetProcessor` | Build-time JSON validation and binary packing | Empty project |
| `Tools/LBGameEditor` | WinUI content editor | Template only |

### Tests – `Components/Tests/`

One GoogleTest project per component under `Model/`, `Services/`, and `System/`, plus `LBEngineTests` (placeholder). Coverage targets: domain >90%, services >85%, systems >75%.

### Data – `Assets/Data/`

`spells.json` (45), `weapons.json` (12), `armour.json` (15). Not yet loaded by any runtime code.

---

## 4. Target Design (Not Yet Built)

### 4.1 Engine orchestration (LB.Engine)

See `modularized-game-architecture.md` for the concrete migration. Responsibilities:
- Own the `StepTimer` and run `Tick → Update (fixed 60 Hz) → Render`
- Construct Systems and Services and inject dependencies (repositories into services)
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
- **Stub modules with wrong names.** LBEngineCore (`export module LB;`, `GameEngine`, `StepTimer`, …), LBRenderingSystem's `DeviceResources`/`RenderContext`/`GraphicsTypes`, and LBModel.Progression (three files all declaring `export module LB;`) need proper `LB.*` names when they are implemented.
- **Undeclared dependency.** `LB.Repositories` imports `<proxy/proxy.h>`, but no `vcpkg.json` declares `proxy`.
- **Leftover module.** `LBAssetPersistenceService.ixx` is an empty module (`LBAssetPersistenceService`) with no content.
