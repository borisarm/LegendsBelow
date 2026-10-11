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
| LBModel.Core | `LB.Core` | partitions `:Common`, `:ValueObjects`, `:Entity` | `LB` |
| LBModel.Character | `LB.Character` | `.Entities`, `.Enums` | `LB::Character` |
| LBModel.Combat | `LB.Combat` | `.Enums` | `LB::Combat` |
| LBModel.Inventory | `LB.Inventory` | `.Entities`, `.Enums`, `.ValueObjects` | `LB::Inventory` |
| LBModel.Dungeon | `LB.Dungeon` | `.Entities`, `.Enums`, `.ValueObjects` | `LB::Dungeon` |
| LBModel.Progression | *(stub, not in solution)* | | |

`LB.Core` provides `String`, `Id`, `Position`, `Dice`, `Attributes`, and `Entity`. Its parts are partitions of `LB.Core`, so `import LB.Core` is the only way to get them.

### Services – `Components/Services/`

| Project | Module | Namespace | Status |
|---------|--------|-----------|--------|
| LBCharacterService | `LB.CharacterService` | `LB::Services::Character` | Implemented and tested; data-driven class rules (`ClassRules` from classes.json): HP/MP from the progression tables (design 4.2 formulas), movement speed, and `GetAttackBonus` with per-class weapon adjustments; a class without rules cannot be created |
| LBCombatService | `LB.CombatService` | `LB::Services::Combat` | Implemented and tested; `ResolveWeaponAttack` (d20 vs AC, dice, crit range and multiplier) and `ApplyDefenses` (flat physical armour, then resistance fractions) resolve the arena's melee attacks with a seedable RNG |
| LBInventoryService | `LB.InventoryService` | `LB::Services::Inventory` | Implemented and tested |
| LBProgressionService | `LB.ProgressionService` (+5 sub-modules) | `LB::Services::Progression` | Implemented and tested |
| LBDungeonService | `LB.DungeonService` | `LB::Services::Dungeon` | Implemented and tested; `GenerateFloor` (seeded floor layouts: critical path, branches, rooms, corridors) is what the game uses; the older room-graph `GenerateDungeon` is unused |
| LBAssetPersistenceService | `LB.Repositories`, `LB.Content`, `LB.ContentRepositories` | `LB::Repositories`, `LB::Content` | Content definitions (`SpellDefinition`, `WeaponDefinition`, `ArmorDefinition`, `ClassDefinition`, `EnemyDefinition`) loaded strictly from `Assets/Data` JSON (nlohmann-json); spell repository facade (Microsoft Proxy 4) with a content-backed implementation |

### Systems – `Components/Systems/`

| Project | Module | Namespace | Status |
|---------|--------|-----------|--------|
| LBRenderingSystem | `LB.RenderingSystem` | `LB::Systems::Rendering` | Draw-command API with a Direct3D 12 backend: a depth-tested world pass of lit DirectXTK12 primitives (`DrawMesh`: box, cylinder, sphere, cone) through the camera set with `SetCamera`, then a 2D overlay of rectangles and text (`DrawRectangle`, `DrawText`), alpha-blended, one draw per atlas. Text uses TrueType fonts (`LoadFont`: stb_truetype in `FontLoader.cpp` bakes a signed distance field atlas, `:Font` lays out UTF-8 lines with kerning; the shader antialiases and outlines) or the built-in pixel font of `:BitmapFont`. `:Camera` exports `IsometricCamera` (pure math: 40 degree pitch, 90 degree rotation steps with easing, three zoom levels, dead-zone follow, `WorldToScreen`, `ScreenToGround`). glTF 2.0 models (`LoadModel`, `DrawModel`): read with fastgltf in `GltfLoader.cpp` into CPU `ModelData` (`:Model`: one part per mesh primitive with its node transform, base-color materials with an optional texture (stb_image, sRGB, mip chain, shared between models by image content) and an emissive color, one skin, bounds), uploaded to default-heap buffers and a shader-visible texture heap; winding turned to clockwise at load. Skinned models are posed by `ModelRenderData::animation` (clips from `LoadAnimations`, matched to joints by name, sampled and cross-faded on the CPU in `:Animation`); `GetJointTransform` places held items, `transform` attaches them, and `partPoses` pose rigid parts. Models are batched per frame: every copy of a model is one instanced draw per part with custom Shader Model 6.0 shaders (`Shaders/ModelInstanced_*.hlsl`, `ModelSkinned_VS.hlsl`, compiled by FXCompile into `ShaderBytecode.cpp`, same lights as the primitives' BasicEffect); skinned copies read their joint matrices from a per-frame palette (root SRV). A tint alpha below 1 draws a copy see-through after the opaque world: a depth-only pass, then a premultiplied-alpha pass, so each faded copy shows only its nearest surface. sRGB render target (linear shading). Headless when no window is given (models and animations still load, for tests). Textured sprites are not implemented yet |
| LBInputSystem | `LB.InputSystem` | `LB::Systems::Input` | Keyboard/mouse pushed by the host; gamepads read through GameInput (internal partition `:GameInputBackend`) with radial dead zones; transition-based edge detection (no lost taps); actions with multiple bindings |
| LBAudioSystem | `LB.AudioSystem` | `LB::Systems::Audio` | Ogg Vorbis, MP3, and WAV clips (stb_vorbis, dr_mp3, dr_wav; `AudioDecoders.cpp`) played through XAudio2 (internal partition `:XAudio2Backend`: a stereo submix per channel, a source voice per sound with volume and constant-power pan; COM initialized by the backend itself); channel volumes, pause, suspend/resume; sources end with their clip, tracked by time so headless runs behave the same; at most 48 at once (the oldest one-shot makes room) |
| LBAssetSystem | `LB.AssetSystem` | `LB::Systems::Asset` | Asset registry/lifecycle; no file I/O yet |
| LBPersistenceSystem | `LB.PersistenceSystem` | `LB::Systems::Persistence` | Save slots (metadata + key/value `SaveData`) and preferences as JSON files, written atomically; existing saves discovered at startup; in-memory when no directory is given |

### Engine and application

| Project | Status |
|---------|--------|
| `Components/LBEngineCore` | `LB.Engine` (`:StepTimer`, `:World`, `:Animator`, `:Sounds`, `:Music`, `:Menus`, `:Enemies`, `:EnemyBehavior`, `:Projectiles`, `:Statuses`, `:Waves`, `:Encounter`, `:Loot`, `:Particles`, `:Effects`, `:Walls`, `:Torches`, `:Minimap`, `:CombatText`, `:Hud`, `:CharacterScreen`, `:GameEngine`; implementation units `EngineSetup.cpp`, `Simulation.cpp`, `WorldView.cpp`, `PlayerProgress.cpp`, `DungeonFloor.cpp`, `Effects.cpp`): `GameEngine` owns the systems and one run (`Session`, replaced wholesale on `Shutdown`) and coordinates the parts; fixed 60 Hz loop driving Rendering, Input, Audio, and Persistence; loads content at startup (`EngineOptions`); owns the player character (CharacterService) moving screen-relative in a 3D test arena built from KayKit models (`EngineOptions::modelDirectory`; primitives without it); wall pieces in the camera's sight lines to the player fade to 30% opacity over 0.2 s (`GetWallOpacities`) and the isometric camera following it (Q/E and right stick rotate, mouse wheel and right stick zoom; usable while paused); tactical pause (Esc/Start); quick save/load (F5/F9). Combat: `:Encounter` runs waves of enemies from enemies.json (Tier 1 AI, attacks on a timer, stagger, death, corpse fade; ranged enemies shoot only with a line of sight and back away while reloading) and resolves attacks both ways with CombatService, including the arrows and bolts of its `ProjectileSet` (`:Projectiles`: they fly at chest height, stick in walls, and hit the first body of the other side, resolved on arrival); the engine adds the player's attack (Attack: left click, Space, gamepad A: a swing, or a shot with a bow or crossbow at the nearest enemy in range and sight), applies enemy hits through CharacterService, respawns a downed player, and draws the enemies (animated through `:Animator`, weapons at their hand slots), floating damage numbers, and a HUD (health bars, hit points, level and experience, gold, potions, wave). Progression: kills award experience through CharacterService; `:Loot` rolls loot.json tables into items on the floor; the bag is an InventoryService inventory; `PlayerProgress.cpp` handles equipping, potions, the character screen (`:CharacterScreen` rows and cursor), and saving progress. World: `:World` places a DungeonService floor layout (or the one-room test arena) on a two-tile cell grid for collision, sight lines, and the enemies' flow field; Self-contained parts own their state: `WallSet` (wall pieces and their fading), `TorchSet` (torches, their lights and fire), `Minimap` (explored cells, the map), `EffectsSystem` (particles and effect recipes, simulated by `:Particles`), `CombatTextLog` (floating texts), `Hud` (drawn from a `HudModel` snapshot). `Effects.cpp` decides when and where effects play; `DungeonFloor.cpp` starts floors, raises room enemies, opens chests, takes the trapdoor, builds the floor's walls, torches and map, and saves the floor. `LB.Engine` re-exports `LB.InputSystem` and `LB.RenderingSystem`. No Win32 dependency. Asset system and dungeon generation are not wired in yet |
| `LegendsBelow` | Win32/GDK shell only: `Main.cpp` creates the window, runs the message loop, and forwards events to `LB::Engine::GameEngine` |

The native window crosses module boundaries only as an opaque `void*`. Direct3D types stay inside LBRenderingSystem's internal partitions (`:DeviceResources` the device and swap chain, `:GpuResources` upload memory and textures, `:ModelPipeline`, `:PrimitivePipeline`, `:OverlayPipeline`, `:ParticlePipeline`, and `:GpuRenderer` composing them; all `.cpp` files including `Direct3D.h` in their global module fragments), and GameInput types inside LBInputSystem's `LB.InputSystem:GameInputBackend` (`GameInputBackend.cpp`); consumers cannot import either. `gameinput.lib` is a static loader merged into `LBInputSystem.lib`, so the game starts without the GameInput runtime (it then runs without gamepad support). DirectXTK12 is used only inside those partitions and fastgltf only inside `GltfLoader.cpp` (an implementation unit nobody imports: textual standard headers in an imported partition break `import std` with MSVC C1116, which is also why model uploads avoid DirectXTK12's `ResourceUploadBatch`). Both are linked statically and merged into `LBRenderingSystem.lib`, so consumers need no extra libraries or DLLs.

All projects share one vcpkg manifest at the repository root (`vcpkg.json`). `vcpkg-configuration.json` adds the overlay triplets in `triplets/`: the stock `x64-windows`/`arm64-windows` (dynamic libraries, dynamic CRT) except that DirectXTK12, fastgltf, and simdjson are built as static libraries (fastgltf and simdjson with `/guard:cf /guard:ehcont`, which the game's Release link requires). Editing a triplet does not re-run the build's vcpkg install by itself; touch `vcpkg-configuration.json`; LegendsBelow, LBRenderingSystem, and LBAssetPersistenceService enable manifest mode. `Directory.Build.targets` makes vcpkg install before C++ module dependency scanning, so a fresh clone builds on the first try.

### Tools

| Project | Purpose | Status |
|---------|---------|--------|
| `Tools/LBAssetProcessor` | Build-time JSON validation and binary packing | Empty project |
| `Tools/LBGameEditor` | WinUI content editor | Template only |

### Tests – `Components/Tests/`

One GoogleTest project per component under `Model/`, `Services/`, and `System/`, plus `LBEngineTests` (StepTimer and headless GameEngine). Test packages come from NuGet `packages.config`; restore with `msbuild LegendsBelow.slnx -t:restore -p:RestorePackagesConfig=true` or Visual Studio's package restore. Coverage targets: domain >90%, services >85%, systems >75%.

### Models – `Assets/Models/`

A starter set from the KayKit Dungeon Pack 1.0 by Kay Lousberg (CC0, `LICENSE-KayKit.txt`): the four characters (`character_*.gltf`, rigid parts in a node hierarchy, no skeleton; no longer drawn) and the arena pieces `tileBrickB_small`, `tileBrickA_small`, `wall`, and `pillar` (`.glb`). The pack is built on a 2-unit grid, so the engine draws it at `ModelScale` 0.5 (a small tile is one world tile, a wall segment two). Materials are flat base colors; no textures. The build copies the folder to `<exe dir>/Assets/Models`; a missing or invalid model stops startup with every problem listed.

### Data – `Assets/Data/`

`spells.json` (45), `weapons.json` (12), `armour.json` (15), `enemies.json` (2: stat blocks, basic attack, resistances, movement, models, loot table), `consumables.json` (1), `loot.json` (2: gold and weighted item drops), `classes.json` (4: starting attributes, primary attributes and growth, weapon and armour adjustments, spell schools, movement speed, level 1-20 progression from design-character-system.md). Loaded at startup by `LB::Content::ContentDatabase` from `<exe dir>/Assets/Data` (the build copies them there). Invalid content stops startup with every error listed; `ShippedContentTests` validate the files in the test run. Cross-file rule: class-restricted spells must be in one of the class's schools.

---

## 4. Target Design (Not Yet Built)

### 4.1 Engine orchestration (LB.Engine)

The shell → engine → renderer split from `modularized-game-architecture.md` is done. Remaining responsibilities:
- [x] Own the `StepTimer` and run `Tick → Update (fixed 60 Hz) → Render`
- [x] Input: the host forwards keyboard/mouse events (`OnKeyDown`, `OnMouseMove`, …); each simulation step reads input, then ends the input frame
- [x] Audio and Persistence systems; content loading; the player character through CharacterService
- [x] Combat in the test arena: `CombatService` owned by `:Encounter` (enemy waves, AI, attacks both ways)
- [x] Ranged combat: `:Projectiles` (arrows and bolts that can be dodged and stop at walls), skeleton crossbowmen that kite, and the player's bows and crossbows
- [x] Spells: the Mage (class choice on New Game), mana and its regeneration, the spell bar (missiles and bursts from spells.json), status effects (`:Statuses`, statuses.json), skeleton mages that cast
- Construct the remaining Services (inventory, progression, dungeon) and inject repositories over `GetContent()`
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
- **Rendering is early.** The arena and characters are KayKit models under a dimmed version of DirectXTK12's three-light rig. Torches are point lights (up to 16 per frame, the nearest to the camera's target, per-pixel in the model shader; primitives only get the directional and ambient light). There are no shadows or normal mapping yet. Skinning poses are computed on the CPU every frame (fine for a handful of characters); animation clips only support linear keyframes. Textures upload uncompressed (RGBA8 with CPU-built mips); the asset pipeline should compress them. Wall fading uses the walls' bounding boxes against three sight lines (feet, chest, head) and covers the arena walls only; there is no ceiling yet. Instance data is rebuilt every frame in upload memory (fine for the arena; static geometry could keep its instance buffers). The player is the skinned KayKit Adventurers knight (9 parts) on the skeletons' Rig_Medium. Primitives (fallback without models) still use BasicEffect, one draw each. Models upload synchronously at startup. The 2D overlay draws rectangles and single-line text (no wrapping or rich text yet); textured sprites are still to be built.
- **No /GL in LBRenderingSystem Release.** As in LBAssetPersistenceService, whole-program optimization corrupts this library's module debug info (LNK4020), so it is off and `/guard:ehcont` is set at compile time.
- **Combat is a first slice.** One weapon (the Rusted Shortsword) and no equipment UI; the player's AC is 10 + DEX until armour adds to it; enemy attack clips (CombatMelee set) and their strike moments are named per enemy in enemies.json; the strike fractions were read off the clips' hand motion. Waves, the arena, and the respawn rule are placeholders for dungeon rooms. There are no opportunity attacks or class abilities yet (statuses come from spells only), and enemies are stat blocks in `:Encounter` rather than `Character` entities. Damage numbers and HUD labels use MedievalSharp. Bows and crossbows need no ammunition yet (`ammoType` is read but unused), and the skeletons resist piercing (50%), so arrows do about a third of a sword's damage to them. Crossbows are drawn with a quarter turn in the hand (`PistolGripModels`): the KayKit shooting clips hold the fist like a pistol grip.

- **Spells are a first slice.** Only damaging missiles and bursts around the caster can be cast (`SpellDefinition::IsCastable`); targeted, utility, healing, and summoning spells are data only. Spells do not roll to hit or crit (design-spells-system.md 5.1 leaves crits optional), taking damage does not interrupt a cast (4.3), and there is no recovery time after one. Status effects are a flat subset of design-character-system.md 6.1 (duration, tick interval, a movement multiplier, damage per tick, preventing actions) instead of effect lists, with no stacking, saves, or resistance. Warrior and Mage are playable; Rogue and Cleric have no kits yet. The Mage starts with 6 hit points (classes.json), which makes the first fights very hard.
- **Loot is a first slice.** Items are the base definitions only (no rarity or affixes yet, design-loot-and-progression.md 3); there is no weight or grid inventory (a 20-stack bag), no dropping or selling, no vendors, and gold has no use yet. Equipment lives in two places until the inventory owns it: the weapon in the engine, armour in CharacterService. Unknown keys in a save are skipped.
- **Dungeon floors are a first slice.** One theme (the Dungeon Pack's stone), rectangular rooms and L-shaped one-cell corridors, no doors, keys, traps, puzzles, props beyond chests and the trapdoor, boss floors, or minimap. Enemies chase the player anywhere once they have seen them, so rushing through rooms gathers a crowd. Everything on a floor is drawn every frame (no culling), fine at these sizes.
