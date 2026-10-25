# LegendsBelow – Architecture

This is the single source of truth for the software architecture: how the code is organized now, the rules that govern dependencies, and the target design still to be built.

Related documents:
- `modularized-game-architecture.md`: migration plan for the Win32 shell → `LB.Engine` → `LB.RenderingSystem` split
- `microsoft-proxy-implementation.md`: repository abstraction via Microsoft Proxy
- `asset-preprocesing-strategy-instructions.md`: JSON → binary asset pipeline
- `design-technical-directx12.md`: rendering design
- `.github/copilot-instructions.md`: coding conventions, module rules, and common build errors

**Last Updated**: 2026-10-18

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

`LB.Core` provides `String`, `Id`, `Position`, `Dice`, `Attributes`, and `Entity`. Its parts are partitions of `LB.Core`, so `import LB.Core` is the only way to get them.

### Services – `Components/Services/`

| Project | Module | Namespace | Status |
|---------|--------|-----------|--------|
| LBCharacterService | `LB.CharacterService` | `LB::Services::Character` | Implemented and tested; data-driven class rules (`ClassRules` from classes.json): HP/MP from the progression tables (design 4.2 formulas), movement speed, and `GetAttackBonus` with per-class weapon adjustments; a class without rules cannot be created |
| LBCombatService | `LB.CombatService` | `LB::Services::Combat` | Implemented and tested; `ResolveWeaponAttack` (d20 vs AC, dice, crit range and multiplier) and `ApplyDefenses` (flat physical armour, then resistance fractions) resolve the arena's melee attacks with a seedable RNG |
| LBInventoryService | `LB.InventoryService` | `LB::Services::Inventory` | Implemented and tested |
| LBProgressionService | `LB.ProgressionService` (+5 sub-modules) | `LB::Services::Progression` | Implemented and tested |
| LBDungeonService | `LB.DungeonService` | `LB::Services::Dungeon` | Implemented and tested; `GenerateFloor` (seeded floor layouts: critical path, branches, rooms, corridors) is what the game uses; the older room-graph `GenerateDungeon` is unused |
| LBAssetPersistenceService | `LB.Repositories`, `LB.Content`, `LB.ContentRepositories` | `LB::Repositories`, `LB::Content` | Content definitions (spells, weapons, armour, classes, enemies, allies, consumables, loot tables, statuses, rarities, affixes, lore, objectives) loaded strictly from `Assets/Data` JSON (nlohmann-json); spell repository facade (Microsoft Proxy 4) with a content-backed implementation |

### Systems – `Components/Systems/`

| Project | Module | Namespace | Status |
|---------|--------|-----------|--------|
| LBRenderingSystem | `LB.RenderingSystem` | `LB::Systems::Rendering` | Draw-command API with a Direct3D 12 backend: a depth-tested world pass of lit DirectXTK12 primitives (`DrawMesh`: box, cylinder, sphere, cone) through the camera set with `SetCamera`, then a 2D overlay of rectangles, text, and images (`DrawRectangle`, `DrawText`, `DrawImage` with nine-slice frames from `LoadImage`), alpha-blended, one draw per atlas, in UI units (`GetUiSize`, `WorldToUi`, a UI scale and safe area set by the engine). Text uses TrueType fonts (`LoadFont`: stb_truetype in `FontLoader.cpp` bakes a signed distance field atlas, `:Font` lays out UTF-8 lines with kerning; the shader antialiases and outlines) or the built-in pixel font of `:BitmapFont`. `:Camera` exports `IsometricCamera` (pure math: 40 degree pitch, 90 degree rotation steps with easing, three zoom levels, dead-zone follow, `WorldToScreen`, `ScreenToGround`). glTF 2.0 models (`LoadModel`, `DrawModel`): read with fastgltf in `GltfLoader.cpp` into CPU `ModelData` (`:Model`: one part per mesh primitive with its node transform, base-color materials with an optional texture (stb_image, sRGB, mip chain, shared between models by image content) and an emissive color, one skin, bounds), uploaded to default-heap buffers and a shader-visible texture heap; winding turned to clockwise at load. Skinned models are posed by `ModelRenderData::animation` (clips from `LoadAnimations`, matched to joints by name, sampled and cross-faded on the CPU in `:Animation`); `GetJointTransform` places held items, `transform` attaches them, and `partPoses` pose rigid parts. Models are batched per frame: every copy of a model is one instanced draw per part with custom Shader Model 6.0 shaders (`Shaders/ModelInstanced_*.hlsl`, `ModelSkinned_VS.hlsl`, compiled by FXCompile into `ShaderBytecode.cpp`, the scene's directional lights plus up to 16 point lights from `DrawLight`, per pixel); before the main pass every model is drawn into the key light's 2048 x 2048 shadow map (`:ShadowMap`, `ModelPass::Shadow`) around the camera's focus, filtered with nine comparisons; skinned copies read their joint matrices from a per-frame palette (root SRV). A tint alpha below 1 draws a copy see-through after the opaque world: a depth-only pass, then a premultiplied-alpha pass, so each faded copy shows only its nearest surface. Particles (`DrawParticle`) and soft ground shadows (`DrawShadow`) go through `:ParticlePipeline`. sRGB render target (linear shading). Headless when no window is given (models and animations still load, for tests) |
| LBInputSystem | `LB.InputSystem` | `LB::Systems::Input` | Keyboard/mouse pushed by the host; gamepads read through GameInput (internal partition `:GameInputBackend`) with radial dead zones; transition-based edge detection (no lost taps); actions with multiple bindings |
| LBAudioSystem | `LB.AudioSystem` | `LB::Systems::Audio` | Ogg Vorbis, MP3, and WAV clips (stb_vorbis, dr_mp3, dr_wav; `AudioDecoders.cpp`) played through XAudio2 (internal partition `:XAudio2Backend`: a stereo submix per channel, a source voice per sound with volume and constant-power pan; COM initialized by the backend itself); channel volumes, pause, suspend/resume; sources end with their clip, tracked by time so headless runs behave the same; at most 48 at once (the oldest one-shot makes room) |
| LBAssetSystem | `LB.AssetSystem` | `LB::Systems::Asset` | Asset registry/lifecycle; no file I/O yet |
| LBPersistenceSystem | `LB.PersistenceSystem` | `LB::Systems::Persistence` | Save slots (metadata + key/value `SaveData`) and preferences as JSON files, written atomically; existing saves discovered at startup; in-memory when no directory is given |

### Engine and application

| Project | Status |
|---------|--------|
| `Components/LBEngineCore` | `LB.Engine`: `GameEngine` (`GameEngine.ixx`, its tunables in `:EngineTunables`) owns the systems and one run (`Session`, replaced wholesale on `Shutdown`; `StartRun` builds a fresh one) and coordinates the parts; fixed 60 Hz loop (`:StepTimer`) driving Rendering, Input, Audio, and Persistence; loads content, models, fonts, sounds, music, and menu art at startup (`EngineOptions`; headless with `Initialize(nullptr, ...)` for tests). The game: a title menu (`:Menus`: new game with a class choice, continue, three save slots, options, remappable controls, the codex, credits), then a run down generated dungeon floors (`:World` places a DungeonService layout on a two-tile cell grid for collision, sight lines, the enemies' flow field, and `FindPath`; without `EngineOptions::dungeon`, the one-room test arena with waves, `:Waves`) in four tiers of three floors (crypts, caves, ruins, the Archive; `:Decor` looks, props, and spike traps), each ending in a boss and a resting place (a merchant, companions for hire, a shrine). Combat: `:Encounter` owns the roster (each tier's creatures from enemies.json, Tier 1 AI and ranks in `:EnemyBehavior`, summoners, bosses with phases and telegraphed attacks, champions), its projectiles (`:Projectiles`), and attack resolution through CombatService; `:Statuses` holds slows, roots, burns, and stuns; `:Allies` (`AllyParty`) the companions. Progression: kills, levels, and paths are published on the run's `EventBus` (`:GameEvents`) and handled in `SubscribeToEvents`; `:Loot` rolls items with rarity, affixes, levels, and legendary powers (`ItemRolls`); `:Features` holds locked gates, secret walls, and candle puzzles; `:Objectives` each floor's objective and the merchant's bounties; `:Legacy` the records across runs, their boons, and the codex. Presentation: `WallSet` (`:Walls`, fading walls in the sight lines), `TorchSet` (`:Torches`), `Minimap`, `EffectsSystem` (`:Effects` recipes over `:Particles`), `CombatTextLog` (`:CombatText`), `Hud` (from a `HudModel` snapshot), `:Ceremony` (banners, `Fanfare`, `PathChoice`), `:CharacterScreen`, `SoundBank` (`:Sounds`), `MusicDirector` (`:Music`), `:Animator`. Implementation units by concern: `EngineSetup.cpp` (init, content, save/load), `Simulation.cpp` (one step), `WorldView.cpp` (render queues), `DungeonFloor.cpp`, `PlayerProgress.cpp`, `CharacterScreens.cpp`, `Effects.cpp`, `GameSounds.cpp`, `GameMenus.cpp`. The tactical pause (Tab) stops the simulation and takes orders; Esc/Start opens the pause menu; F5/F9 quick save and load; the run autosaves. `LB.Engine` re-exports `LB.InputSystem` and `LB.RenderingSystem`. No Win32 dependency. The Asset system is not wired in yet |
| `LegendsBelow` | Win32/GDK shell only: `Main.cpp` creates the window, runs the message loop, and forwards events to `LB::Engine::GameEngine` |

The native window crosses module boundaries only as an opaque `void*`. Direct3D types stay inside LBRenderingSystem's internal partitions (`:DeviceResources` the device and swap chain, `:GpuResources` upload memory and textures, `:ModelPipeline`, `:ShadowMap` (the key light's shadow map, drawn by the model pipeline's shadow pass before the main pass), `:PrimitivePipeline`, `:OverlayPipeline`, `:ParticlePipeline`, and `:GpuRenderer` composing them; all `.cpp` files including `Direct3D.h` in their global module fragments), and GameInput types inside LBInputSystem's `LB.InputSystem:GameInputBackend` (`GameInputBackend.cpp`); consumers cannot import either. `gameinput.lib` is a static loader merged into `LBInputSystem.lib`, so the game starts without the GameInput runtime (it then runs without gamepad support). DirectXTK12 is used only inside those partitions and fastgltf only inside `GltfLoader.cpp` (an implementation unit nobody imports: textual standard headers in an imported partition break `import std` with MSVC C1116, which is also why model uploads avoid DirectXTK12's `ResourceUploadBatch`). Both are linked statically and merged into `LBRenderingSystem.lib`, so consumers need no extra libraries or DLLs.

All projects share one vcpkg manifest at the repository root (`vcpkg.json`). `vcpkg-configuration.json` adds the overlay triplets in `triplets/`: the stock `x64-windows`/`arm64-windows` (dynamic libraries, dynamic CRT) except that DirectXTK12, fastgltf, and simdjson are built as static libraries (fastgltf and simdjson with `/guard:cf /guard:ehcont`, which the game's Release link requires). Editing a triplet does not re-run the build's vcpkg install by itself; touch `vcpkg-configuration.json`; LegendsBelow, LBRenderingSystem, and LBAssetPersistenceService enable manifest mode. `Directory.Build.targets` makes vcpkg install before C++ module dependency scanning, so a fresh clone builds on the first try.

### Tools

| Project | Purpose | Status |
|---------|---------|--------|
| `Tools/LBAssetProcessor` | Build-time JSON validation and binary packing | Empty project |
| `Tools/LBGameEditor` | WinUI content editor | Template only |

### Tests – `Components/Tests/`

One GoogleTest project per component under `Model/`, `Services/`, and `System/`, plus `LBEngineTests` (StepTimer and headless GameEngine). Test packages come from NuGet `packages.config`; restore with `msbuild LegendsBelow.slnx -t:restore -p:RestorePackagesConfig=true` or Visual Studio's package restore. Coverage targets: domain >90%, services >85%, systems >75%.

### Models – `Assets/Models/`

KayKit models by Kay Lousberg (CC0, the `LICENSE-KayKit*.txt` files): the Dungeon Pack's floors, walls, props, chests, and items; the Skeletons; the Adventurers heroes and their weapons; Mystery Monthly creatures (orcs, werewolf, witch, vampire, tiefling, black knight, golems, the Archive's creatures, the Abomination); and the Character Animations sets in `Animations/` (Rig_Medium and Rig_Large). glTF (`.gltf` + `.bin`, or `.glb`), textured with each set's atlas PNG. The Dungeon Pack is built on a 2-unit grid, so the engine draws it at `ModelScale` 0.5 (a small tile is one world tile, a wall segment two). The build copies the folder to `<exe dir>/Assets/Models`; a missing or invalid model stops startup with every problem listed.

### Data – `Assets/Data/`

`spells.json` (55 spells and abilities), `weapons.json` (40, in four tiers), `armour.json` (55, in four tiers), `classes.json` (4: starting attributes, primary attributes and growth, weapon and armour adjustments, spell schools, movement speed, level 1-20 progression from design-character-system.md), `enemies.json` (20, four of them bosses: stat blocks, attacks, resistances, movement, models, voices, loot table), `allies.json` (3 companions), `consumables.json` (4: the potion, arrows, bolts, the key), `loot.json` (9 tables, the merchant's stock among them), `statuses.json` (10), `rarities.json` (5), `affixes.json` (23), `lore.json` (16 codex pages), `objectives.json` (objective and bounty rewards, champions, relics). Loaded at startup by `LB::Content::ContentDatabase` from `<exe dir>/Assets/Data` (the build copies each file there, listed one by one in `LegendsBelow.vcxproj`). Invalid content stops startup with every error listed; `ShippedContentTests` validate the files in the test run. Cross-file rule: class-restricted spells must be in one of the class's schools.

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
- [x] Five classes: the Rogue (sneak attacks, Evasion, Shadowstep), the Cleric (Smite, heals, cures, Turn Undead), and the Necromancer (curses, life drain)
- [x] Bosses: the Necromancer at the end of each tier (summons, a second phase), the golem brute on Rig_Large, groups that notice the player together
- [x] The tactical pause and orders (move, attack, cast; carried out when the pause ends)
- [x] Loot rarity and affixes (rarities.json, affixes.json, rolled items), item comparison, ammunition
- [x] Resting floors after each boss: a merchant (buying and selling for gold) and a shrine
- [x] Dungeon variety: a look per tier (crypts, caves, ruins), props that bodies walk around, spike traps (`:Decor`)
- [x] Character growth: the Warrior's stamina abilities, the Rogue's level-3 and level-5 abilities, a path at level 5
- [x] Group tactics (tanks lead, shooters keep behind), telegraphed boss attacks, a boss per tier (the Golem Warlord)
- [x] A balance check: `BalanceTests` plays each class through all four tiers, the final floor to the victory, and New Game+1's first tier (Release builds), growing as a player would; `PerformanceTests` holds a step to a frame-time budget (a crowded floor, the final fight)
- [x] The inventory, dungeon, and progression services are composed by the engine; progression comes from the data: `LBProgressionService` is configured from progression.json (the experience curve), classes.json (tables, growth), and spells.json (the abilities by level), and `CharacterService` levels by the same curve
- Inject repositories over `GetContent()` (the services read the content database directly)
- [x] Gamepad input through GameInput: `LB.InputSystem:GameInputBackend` (`GameInputBackend.cpp`) reads the most recently used gamepad; the engine enables it only with a real window and polls once per simulation step while focused
- [x] Game states: the title menu, a run, the pause menu, the death screen (the resting places stand in for a town)
- [x] Cross-part events: `LB::EventBus` (LB.Core, queued and typed); the engine publishes kills, levels, and paths (`:GameEvents`) for progression and the fanfare
- [x] Equipment in one place (the inventory's slots), path finding for orders, drawing only what the camera can see (`ViewArea`)
- [x] Allies (`:Allies`): companions hired from the merchant who fight beside the hero, targeted by enemies, healed by the Cleric; two at most, the Acolyte healer, orders (hold, attack), gear, statuses and traps
- [x] Feel: ground shadows (`DrawShadow`), hit-stop and camera shake, a death screen with the run's summary, each tier's ambience and footsteps
- [x] Level and path banners and the path cards (`:Ceremony`); dropping items
- [x] Autosave (a new game, each floor, quitting, closing, suspending); Continue loads the newest save
- [x] Locked gates and keys, secret walls, candle puzzles, greater chests (`:Features`; `WorldMap::SetBlocked`)
- [x] Each tier its inhabitants (`GameEngine::Rosters`: the caves' and ruins' creatures, the Frost Golem), remappable controls, the key light's shadow map (`:ShadowMap`), knockback, each tier's air, animated gates, walls, and chest lids
- [x] Item levels, attribute and hit point affixes (`CharacterService::SetGear`), legendary powers, item icons
- [x] Save slots and the legacy: records across runs, boons for new heroes (`:Legacy`, the profile)
- [x] A fourth tier (the Archive: its look, creatures, boss, gear), the codex (lore.json, `LoreBook`, kept with the legacy), creatures' voices, playtest launches (`--class --depth --level`), Xbox readiness (pausing on interruption, the screen margin, no Quit on a console, `PackageLayout.ps1` checks and packages)
- [x] The final floor (`FinalDepth`: `DungeonService::FinalFloor`, the Red Library) and its boss, the First Archivist: three phases, echoes of the earlier tiers (`EnemyDefinition::summons`), a second telegraphed attack (`phaseThreeSlam`), guttering torches (`TorchSet::Gutter`)
- [x] New Game+: `DescendAgain`, `Session::cycle` (saved as `floor.cycle`), `ChallengeDepth`, `--cycle`
- [x] Victory: `RunWonEvent`, the way up (`IsWayUpOpen`), the victory page (`MenuPage::Victory`), the codex's last page, victories in the legacy and the Laurels boon
- [x] Floor objectives and the merchant's bounties (`:Objectives`, objectives.json); each tier its gear (`ForTier`); coverage in CI (OpenCppCoverage); the tunables in `:EngineTunables`, the character screen in `CharacterScreens.cpp`
- [x] Smart loot (`ForClass`), respec at the merchant (`Retrain`, `ForgetPath`), spell crits, interrupts, and armour-slowed casts, opportunity attacks, enemies flanking and focusing fire, New Game+ mutators (`mutators.json`)
- [x] The load (`CarriedWeight`, `GetWeightPenalty`), levers and waystones (`:Features`, `FeatureActivatedEvent`), the spellbook (`Spellbook`, `CycleSpell`) and utility spells (`SpellDefinition::effect`: blink, buffs, cleansing, arcane sight, mage hand, ice wall)

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
- **Release|Win32 / ARM64 configurations** of the component projects are untested; build Debug|x64 or Release|x64.
- **Placeholder store identity.** `MicrosoftGame.config` carries a placeholder Title ID and MSA App ID; a package passes submission validation otherwise, but needs the real ones from Partner Center.
- **New Game+ is a first slice.** A run is won on the final floor (depth 14, `FinalDepth`; design-dungeon-and-world.md 7.4), and Descend Again takes the same hero down a new Below, a cycle stronger: `ChallengeDepth` (depth + FinalDepth per cycle) scales enemies, traps, and finds through the existing depth rules, and gear stops at the fourth tier, so a hero grows through rarity and item levels only. Each cycle adds one mutator (`mutators.json`, `GameEngine::GetMutators`: enemy hit points, armour, attack, damage, life steal, more enemies, gold), drawn from the run's seed and saved (`floor.mutators`); they are modifiers on the existing rules, not new behaviours. The balance bot plays only New Game+1's first tier (`BalanceTests`). Floors past 14 (tools only) go on in the Archive's look.
- **The Necromancer owns the Shadow school.** Its eight spells (Raise Dead; Hex Bolt, the witch's too; Weakening Curse, Curse of Frailty, and Doom as curses on a target, `SpellDefinition::effect` "Curse"; Life Drain and Devour Essence healing the caster by `lifestealFraction`; Shadow Grasp) are `requiredClass` Necromancer. Raise Dead summons (`Summon`): a skeleton that fights as a companion for 30 seconds (two at most, apart from the two companions; never saved).
- **Class rules are data-driven.** Attribute growth is guided with a free share: each level gives 1 automatic point to the class's primary attributes (alternating; a capped primary passes it on) and 1 free point the player assigns (`SpendAttributePoint`). Primary attributes cap at 25, others at 18 (`attributeGrowth` in classes.json). Respec (game-design.md) is at the merchant: retraining gives back the points the player assigned (`CharacterService::GetAssignedPoints`, `Retrain`; saved as `player.assigned`) for 15 gold a level, and forgetting the path (the full respec) for 60 a level (`GameEngine::Retrain`, `ForgetPath`).
- **Armour is by class, and its penalties are class-scaled.** A class wears what `armour.json`'s `allowedClasses` lists (the Mage cloth only: 16 of the 55 pieces; drops lean toward what the hero can wear, `GameEngine::ForClass`); each piece's `movementPenalty` and `castPenalty` are multiplied by the class's `armorAdjustments` for its category (0.5 signature, 1 trained, 1.5 untrained, 2 restricted). Speed = (base + DEX_mod x 0.1) x (1 - sum of scaled movement penalties); cast time x (1 + sum of scaled cast penalties), `GetCastTimeMultiplier`, which slows the hero's casts (`PlayerCasting::castSeconds`). The load slows too (design-armour-system.md 4.2: `GetWeightPenalty`, past half of 8 x STR).
- **Rendering.** Real shadows come from the key light only (point lights cast the soft ground shadows: particle quads stretched away from the nearest torch); there is no normal mapping and no ceiling. Skinning poses are computed on the CPU every frame (the GPU blends them); animation clips only support linear keyframes. Model textures are block compressed when loaded (BC1, or BC3 with alpha, with CPU-built mips; 15 MiB in all), not in an asset pipeline: UI images and fonts stay RGBA8. Instance data is rebuilt every frame in upload memory. Models are read and decoded on worker threads at startup (`RenderingSystem::PrefetchModels`) and uploaded on the main thread. Floors, walls, torches, and props outside the view frustum's footprint on the floor (the screen's corners projected down, widened for tall walls) are not drawn (`GameEngine::GetViewArea`); there is no occlusion culling, and characters are not culled.
- **No /GL in LBRenderingSystem Release.** As in LBAssetPersistenceService, whole-program optimization corrupts this library's module debug info (LNK4020), so it is off and `/guard:ehcont` is set at compile time.
- **Combat.** Enemies are stat blocks in `:Encounter`, not `Character` entities. Walking out of an awake melee enemy's reach draws its blow (`Encounter::CheckOpportunity`, design-combat-system.md 3.2: once every 3 seconds per enemy, not from one mid-swing; dashes and Shadowstep leave freely); the hero never makes opportunity attacks. Group tactics: ranks (fighters behind their tank, shooters behind the front line), enemies flanking (+2 to hit with another enemy threatening the target's far side, `IsFlanking`), and shooters focusing the most wounded target in range (`PlayerTarget::health`). The hero's flank and sneak attacks count. Each boss has a second phase and one telegraphed attack (a ring on the ground). Crossbows are drawn with a quarter turn in the hand (`PistolGripModels`): the KayKit shooting clips hold the fist like a pistol grip.
- **Spells.** Missiles, bursts (around the caster or at a target), targeted strikes, heals, dashes, and Shadowstep can be cast (`SpellDefinition::IsCastable`); so can the utility effects (`SpellDefinition::effect`: Blink, Buff, Cleanse, RevealSecrets, RemoteInteract, IceWall, Curse, Portal, Summon); Featherfall is a Buff against spike traps, since nothing falls. The Portal leads only to the last resting place above (there is no town), and the floor left is rebuilt from what a save keeps of it (cleared rooms, gates, puzzles) plus what lay on the ground: enemies of a room fought but not cleared rise again, as after loading. The bar takes any castable spell of the class (`Spellbook`). Spells do not roll to hit; a damaging cast is critical on a natural 20 (5%, x1.5, `SpellDefinition::canCrit`), a blow of a fifth of the hero's hit points interrupts a cast (4.3, `GameEngine::InterruptCast`), and there is no recovery time after one. Status effects are a flat subset of design-character-system.md 6.1 (duration, tick interval, a movement multiplier, damage per tick, preventing actions) instead of effect lists, with no stacking, saves, or resistance. Turn Undead holds undead helpless (Charmed) rather than making them flee.
- **Inventory.** The bag holds 20 stacks (`BagCapacity`), and what is carried weighs (5.2); there is no grid inventory (design-loot-and-progression.md 5.1). The merchant sells a fixed share of a stock table and buys at a quarter of the price. Unknown keys in a save are skipped.
- **Dungeon floors.** Rectangular rooms and L-shaped one-cell corridors; ordinary doorways have no doors; levers open some treasure gates and a pair of waystones joins the entrance and the room before the exit (design-dungeon-and-world.md 6.3), and from depth 7 a crank may swing a bridge across a chasm in such a corridor instead (a gate of another look: the chasm is a blocked cell with no floor laid); there are no moving platforms, falls, or levers that change the layout. Enemies chase the hero anywhere once they have seen them, so rushing through rooms gathers a crowd.
- **Allies.** Two companions at most (`AllyParty::MaxAllies`), hired at resting places. They go down at 0 hit points and get up once no enemy has been awake near them for 3 seconds; they never die for good.
