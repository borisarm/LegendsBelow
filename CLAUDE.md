# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

LegendsBelow is an isometric, D&D-inspired dungeon crawler (real-time with tactical pause) for Xbox and Windows: C++20 modules, DirectX 12 via DirectXTK12, Microsoft GDK, MSVC/MSBuild only.

## Build and test

Visual Studio 2022 17.14+ (`.slnx` support), the GDK, and vcpkg with VS integration are required. vcpkg dependencies come from the root `vcpkg.json` and install on first build (`Directory.Build.targets` forces the install before C++ module dependency scanning, otherwise a fresh clone fails to find vcpkg headers). On this machine MSBuild is at `C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe`.

```powershell
# Restore GoogleTest NuGet packages for the test projects (packages.config)
msbuild LegendsBelow.slnx -t:restore -p:RestorePackagesConfig=true

# Build everything (only x64 Debug/Release are tested; Profile maps to Release for libraries)
msbuild LegendsBelow.slnx -p:Configuration=Debug -p:Platform=x64 -m

# Build one project (and its references)
msbuild Components/LBEngineCore/LBEngineCore.vcxproj -p:Configuration=Debug -p:Platform=x64

# Run a test project, or a single test, from the repo root
x64\Debug\LBEngineTests.exe
x64\Debug\LBEngineTests.exe --gtest_filter=GameEngineTests.Initialize_Headless_Succeeds
```

`BalanceTests` (a bot playing each class through the first tier) are compiled only when `NDEBUG` is defined: run them from a Release build (`x64\Release\LBEngineTests.exe --gtest_filter=BalanceTests.*`, about half a minute); each prints a `[ BALANCE  ]` line with deaths, potions, level, and minutes.

Every component has its own GoogleTest executable in `x64/<Config>/` (`LBEngineTests`, `LBCombatServiceTests`, `LBModel.Core.Tests`, ...). Engine tests locate the shipped `Assets/Data`, `Assets/Models`, and `Assets/Fonts` by walking up from the current directory, so run them from inside the repo. Test projects use headers and `pch.h`, not modules. The engine tests are split by area (`StepTimerTests.cpp`, `CombatTests.cpp`, `DungeonTests.cpp`, ...); shared setup and helpers are `inline` in `EngineTestHelpers.h`, which every engine test file includes after `pch.h` (put new helpers there, not in an anonymous namespace).

`Scripts/Test.ps1` builds the solution and runs every test executable, failing on any build error, test failure, or AddressSanitizer report (`-Configuration Release`, `-NoBuild`, `-AddressSanitizer`, `-StaticAnalysis`). It is what CI runs (`.github/workflows/build.yml`: Debug, Release, Debug under ASan, and Debug with MSVC `/analyze` on each push and pull request). `-StaticAnalysis` (`-p:LBStaticAnalysis=true`) fails the build on analysis findings; GoogleTest's macros trip C6326/C6262, so those two are off in test projects only. `-AddressSanitizer` rebuilds everything instrumented (`-p:LBAddressSanitizer=true`) and puts the toolset's ASan runtime on `PATH`; run a plain build afterwards to get normal binaries back. GoogleTest here is 1.8.1: flags like `--gtest_brief` do not exist (it prints its help and exits 0).

## Architecture

Layers (see `Documentation/architecture.md`, the source of truth, including known deviations):

```
LegendsBelow (Win32/GDK shell, Main.cpp)   imports LB.Engine only
LBEngineCore (LB.Engine)                   the only place that composes Systems + Services
Systems  (Rendering, Input, Audio, Asset, Persistence)   LB.Core only, no domain knowledge
Services (Character, Combat, Inventory, Progression, Dungeon, AssetPersistence/Content)   LB.Core + domain
Model    (LB.Core, LB.Character, LB.Combat, LB.Inventory, LB.Dungeon)
```

- **Shell**: `LegendsBelow/Main.cpp` owns the window and message loop and forwards input/window events to `LB::Engine::GameEngine`. No game code goes here.
- **Engine** (`Components/LBEngineCore`): `LB.Engine.ixx` re-exports `LB.InputSystem`, `LB.RenderingSystem`, and partitions (`:World` collision/LOS/flow field, `:Enemies` enemy data, `:EnemyBehavior` Tier 1 AI and movement (ranged enemies kite), `:Projectiles` arrows, bolts, and spell missiles in flight, `:Statuses` status effects (slow, root, burn) on a character, `:Waves` wave rules and spawn points, `:Encounter` the roster, its projectiles, summoners and bosses, and attack resolution, `:Loot` (loot rolls, rarity and affixes: `ItemRolls`), `:Particles`/`:Effects` (simulation / `EffectsSystem` recipes), `:Walls`, `:Torches`, `:Decor` (tier looks, props as round `WorldMap` obstacles, spike traps), `:Minimap`, `:CombatText`, `:Hud`, `:Sounds` (`SoundBank` recipes per `GameSound`), `:Music` (`MusicDirector` crossfading the calm and combat layers), `:Menus` (title, pause, options, credits pages), `:CharacterScreen`, `:GameEngine`). `GameEngine` is declared in `GameEngine.ixx`; its members are implemented across `module LB.Engine;` implementation units split by concern: `EngineSetup.cpp` (init, content/models/fonts, save/load), `Simulation.cpp` (one fixed step), `WorldView.cpp` (render queues), `DungeonFloor.cpp`, `PlayerProgress.cpp`, `Effects.cpp`, `GameSounds.cpp` (loading the sounds; `PlaySound(GameSound, at)` places a sound in the world), `GameMenus.cpp` (menu input and actions). `StartRun()` builds a fresh run (Initialize, New Game, Quit to Title). All per-run state lives in `GameEngine::Session session_`; `Shutdown` replaces it wholesale, so new run state belongs in `Session` (loaded model handles in `Assets`). Self-contained features go in their own partition owning their state (like `TorchSet`, `Minimap`, `Hud` drawn from a `HudModel` snapshot), with `GameEngine` only coordinating; public tunables stay reachable as `GameEngine::` aliases.
- **Loop**: `StepTimer` drives Tick → Update (fixed 60 Hz) → Render. Each step reads input and then calls `InputSystem::Update()`, which ends the input frame (clears transitions); query input before it. The tactical pause stops the simulation and game clock but not the loop (camera, fire, sparkles keep animating).
- **Headless**: `Initialize(nullptr, ...)` runs without a device; tests drive the engine this way with `EngineOptions` (`contentDirectory`, `modelDirectory`, `fontDirectory`, `soundDirectory`, `musicDirectory`, `uiDirectory` (menu frames), `titleScreen` (off in tests), `playerClass` (Warrior or Mage), `spawnEnemies`, `randomSeed`, `dungeon`); tests check sounds through `GetAudio().TakePlayedClips()`. Without `dungeon` the game is the single-room test arena with enemy waves.
- **Rendering** (`Components/Systems/LBRenderingSystem`): a draw-command API (`DrawModel`, `DrawMesh`, `DrawParticle`, `DrawLight`, `DrawRectangle`, `DrawText`, `DrawImage` with nine-slice frames) queued per frame and inspectable via `GetQueued*` in tests. 2D (rectangles, text) is in UI units: lay it out with `GetUiSize` and `WorldToUi`, never `GetScreenSize`/`WorldToScreen` (those are pixels); the engine sets the UI scale each frame so the 800 x 600 layout grows with the window. D3D12 and DirectXTK12 types stay inside internal partitions (`.cpp` files that include `Direct3D.h` in their global module fragment): `:DeviceResources` (GDK device/swap chain template), `:GpuResources` (upload memory, texture heap, built-in textures, one-off command lists), `:ModelPipeline`, `:PrimitivePipeline`, `:OverlayPipeline`, `:ParticlePipeline`, and `:GpuRenderer`, the façade `RenderingSystem.cpp` drives. A new GPU feature gets its own pipeline partition composed by `GpuRenderer`. GameInput types stay inside `LB.InputSystem:GameInputBackend`. Native handles cross module boundaries as opaque `void*`. HLSL in `Shaders/` is compiled at build time by FXCompile.
- **Content** is data-driven: `Assets/Data/*.json` loaded strictly by `LB.Content` (`ContentLoader.cpp`) at startup; any error stops startup with the full list. The build copies `Assets` next to the executable. Models are KayKit glTF (CC0) loaded with fastgltf.
- Repository abstractions use Microsoft Proxy (duck-typed facades), not virtual inheritance.

## Conventions

The detailed rules and an error-to-fix table are in `.github/copilot-instructions.md`. The ones that most often break the build:

- Every project compiles at `/W4` with warnings as errors (set once in `Directory.Build.targets`, after the projects' own settings). Headers included with angle brackets are external and compiled without warnings; ours are not. Unused parameters of placeholder APIs get `[[maybe_unused]]`; gtest comparisons against unsigned values need unsigned literals (`EXPECT_EQ(v.size(), 3u)`). `-p:LBWarningsAsErrors=false` reports without failing.

- Everything except LBGameEditor and test projects is a C++20 module (`.ixx`, module names `LB.Xxx`). Module projects have no precompiled headers.
- Partition files: `export module LB.Engine:Name;`, import siblings as `import :Name;`, re-export from the primary interface. Internal (non-exported) partitions must be `.cpp` files with `<CompileAs>CompileAsCppModuleInternalPartition</CompileAs>` (else C7622). New files must be added to the `.vcxproj` (and `.filters`).
- In module code write `std::int32_t`, `std::size_t`, `std::string`: `import std` only guarantees names in `std`, and `using namespace std;` is not used.
- Import `LB.Core` for the core types (`String`, `Id`, `Position`, `Entity`, ...); its parts are partitions (`LB.Core:Common`, `:ValueObjects`, `:Entity`) and cannot be imported alone. There is no `LB::Domain` namespace; use `LB::Character`, `LB::Combat`, `LB::Inventory`, `LB::Dungeon`. Never redefine domain types in services; `using` them.
- Projects importing module libraries must match their `SDLCheck`, `FloatingPointModel`, and `ControlFlowGuard` settings (else C5050). Test projects reference both the component and `LBModel.Core`.
- Gameplay rules follow `Documentation/design-*.md`; cite the section in comments for non-obvious formulas, and keep tunables in data files.
- File headers are a one-line `// File.ixx - what it is` comment; types and public members get `/// <summary>` doc comments.
- User-visible changes are recorded in `CHANGELOG.md` under `[Unreleased]` (detailed, naming the new APIs), and `README-LegendsBelow.md` / `Documentation/architecture.md` status tables are kept current.
