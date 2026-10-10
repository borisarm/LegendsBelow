# Changelog

All notable changes to LegendsBelow. Dates are commit dates.

## [Unreleased]

### Added
- Full 3D rendering on DirectXTK12: a depth-tested world pass of lit primitives (`RenderingSystem::DrawMesh` with box, cylinder, sphere, cone) drawn before the 2D overlay; `SetCamera`, `GetQueuedMeshes`, `WorldToScreen`, `RenderStats::meshesRendered`.
- `IsometricCamera` (`LB.RenderingSystem:Camera`): perspective camera at a 40 degree pitch, 90 degree rotation steps with a 0.25 s ease, Close/Medium/Wide zoom with easing, soft follow with a 1.5-tile dead zone, instant refocus (`SnapTo`), screen-to-ground directions, and world-to-screen projection matching the GPU.
- The placeholder arena is drawn in 3D (floor slabs and a ring of walls) with the player as a body, head, and visor facing the walk direction. The camera follows the player and refocuses on spawn and load; Q/E and right stick flicks rotate it, the mouse wheel and right stick up/down zoom, also during the tactical pause.
- Class data file `Assets/Data/classes.json`: starting attributes (game-design.md), weapon adjustments, spell schools, resource, base movement speed, and the level 1-20 progression tables parsed from design-character-system.md. Loaded as `ClassDefinition`, validated, and cross-checked against spells.
- Class weapon adjustment model: every class has an attack adjustment for every weapon category (+3 signature, +1 trained, -1 untrained, -5 restricted; e.g. a sword is +3 for Warriors, +1 Rogues, -1 Mages, -5 Clerics). `CharacterService::GetAttackBonus` = class attack progression + STR (melee) or DEX (bows, crossbows) + adjustment.
- `LB::Character::ClassProgression` domain type shared by content loading and `CharacterService`.
- Attribute growth (classes.json `attributeGrowth`, `primaryAttributes`): per level 1 automatic point alternating between the class's primary attributes and 1 free point the player assigns with `CharacterService::SpendAttributePoint`; primary attributes cap at 25, others at 18. `CharacterProgressionResult::attributePointsGained`, `Character::GetUnspentAttributePoints`, `LB::AttributeType`.
- Class armour adjustment model (classes.json `armorAdjustments`): per class and armour category, scales on the pieces' movement and cast penalties (e.g. Warriors feel heavy armour at x0.5 on movement, Mages pay x2 on movement and casting beyond Light, Clerics cast in Light/Medium/Shield at x0.5). `CharacterService::EquipArmor`/`UnequipArmor`/`GetEquippedArmor`, armour-aware `GetMovementSpeed`, `GetCastTimeMultiplier`; `GameEngine::EquipPlayerArmor`.
- Audio wired into the engine: XAudio2 output device (`LB.AudioSystem:XAudio2Backend`), master volume, suspend/resume with the window, gameplay channels paused during the tactical pause (music keeps playing), volumes restored from and saved to preferences.
- Persistence backed by files: save slots (metadata + key/value `SaveData`) and preferences as JSON in `%LOCALAPPDATA%\LegendsBelow\Saves`, written atomically and discovered at startup. Quick save/load on F5/F9 (game clock and player position).
- Player movement: the engine owns a Warrior (CharacterService) moving in real time with WASD/arrows or the left stick at `ClassBaseSpeed + DEX_mod * 0.1` tiles/s (design-character-system.md 4.2), inside a placeholder test arena; stops while paused.
- Solid rectangle rendering (`RenderingSystem::DrawRectangle` now drawn on the GPU) for the placeholder arena and player.
- Content loading: `LB.Content` definitions for spells, weapons, and armour loaded strictly from `Assets/Data` (all errors reported, nothing loaded on error); `ContentSpellRepository` implements the Proxy spell facade; the build copies the data next to the executable; invalid content stops startup with the error list.
- Domain additions from the design documents: `SpellSchool::Generic`, `EquipmentSlot::Hands`, `DamageType::Acid` and `Psychic`.
- `EngineOptions` (save and content directories) for `GameEngine::Initialize`; `GetInitializationError`.
- Gamepad support through GameInput: `LB.InputSystem:GameInputBackend` reads the most recently used gamepad (radial dead zones, Xbox button mapping); the engine polls it once per simulation step while focused. Start toggles the tactical pause alongside Esc. Without the GameInput runtime the game still starts, without gamepad support.
- `InputSystem`: gamepad button transitions (`IsGamepadButtonPressed`/`Down`, `SetGamepadState`), gamepad action bindings, several bindings per action, `EnableDeviceInput`/`PollDevices`; `GameEngine::OnGamepadState` for hosts and tests.
- Input wired into the engine: the shell forwards keyboard and mouse messages to `GameEngine` (`OnKeyDown`, `OnKeyUp`, `OnMouseMove`, `OnMouseButton`, `OnMouseWheel`); `LB.Engine` re-exports `LB.InputSystem`. Each simulation step reads input, then ends the input frame; losing focus releases all keys and buttons.
- Tactical pause on Esc (design-ui-ux-and-audio.md): the simulation and game clock (`GetGameTimeSeconds`) stop while the loop keeps running; the background dims as placeholder feedback.
- `LB::Systems::Input::Keys` key codes (matching Windows virtual-key codes), mouse setters, `IsMouseButtonPressed`, and `ReleaseAll`.
- `LB.Engine` (LBEngineCore): `GameEngine` runs the Tick → Update → Render loop at a fixed 60 Hz; `StepTimer` ported to `std::chrono` with deterministic `Advance`.
- Direct3D 12 backend in LBRenderingSystem: `DeviceResources` moved from the executable into the internal partition `LB.RenderingSystem:DeviceResources`; `RenderingSystem::Initialize(void* window, …)` creates the device, `EndFrame` clears and presents. A null window runs headless.
- Engine tests (StepTimer, headless GameEngine) replacing the placeholder test; rendering tests for headless mode, resize, and device callbacks.
- Repository tests: `MockSpellRepository` rewritten against the current domain modules, with tests for every `SpellRepository` query and factory, replacing placeholder tests.

### Changed
- `CharacterService` uses data-driven class rules: HP/MP come from the class progression tables (MaxHP = table HP + CON_mod x level; MaxMP = table MP + INT/WIS_mod x level for Mages/Clerics), replacing the hard-coded formulas. Characters can only be created for classes whose rules are configured; the engine configures them from classes.json and runs without a player when there is no content.
- Removed the hard-coded class attribute bonus on creation (classes.json starting attributes are the class template) and the fixed per-level attribute gains. Armour proficiency lists replaced by the armour adjustment model; armour.json `allowedClasses` is no longer cross-checked against them.
- Movement is screen-relative through the camera: W/stick up walks away from the camera whatever its rotation. The background is a dim ambient color instead of cornflower blue.
- `LB.Engine` re-exports `LB.RenderingSystem`. DirectXTK12 added to `vcpkg.json` (used as a DLL); test projects that link the renderer copy `DirectXTK12.dll` next to them (`LBDeployDirectXTK12`).
- LegendsBelow is now a Win32/GDK shell: `Main.cpp` imports `LB.Engine` and forwards window events; `Game`, `DeviceResources`, and `StepTimer` removed from the executable.
- `vcpkg.json` moved to the repository root so all projects share one manifest.
- LBEngineCore references only the projects it imports (LBRenderingSystem, LBModel.Core).
- Executable compiler flags aligned with the module libraries (`/fp:precise`, `/sdl`; libraries gain `/guard:cf` in Release) and C++20 in all configurations.
- Release|x64 no longer expects a nonexistent `pch.h` in LBModel.Core, LBRenderingSystem, and LBEngineCore.

### Fixed
- Flaky combat tests (about 4% failure rate): `CombatService` takes an optional dice seed and keeps its RNG per instance; the tests use a fixed seed.
- Input edge detection no longer loses taps shorter than a frame: presses and releases are recorded as transitions instead of comparing current and previous key states. Mouse actions are now edge-triggered like keyboard actions.
- LBAssetPersistenceService builds: `proxy` added to the vcpkg manifest, included via the global module fragment instead of a header unit.
- Fresh clones build on the first try: `Directory.Build.targets` runs the vcpkg install before C++ module dependency scanning.
- Test projects build in Release|x64 (C++20 was only set for Debug); tests and libraries now share `/sdl` and, in Release, `/guard:cf`, removing the C5050 module-environment warnings.

### Removed
- Empty placeholder modules: `GameState`, `SystemManager` (LBEngineCore), `RenderContext`, `GraphicsTypes` (LBRenderingSystem), and `LBAssetPersistenceService` (LBAssetPersistenceService).

## 2026-10-09

### Changed
- Moved design and architecture documents from `.github/` to `Documentation/`; `copilot-instructions.md` stays in `.github/` so GitHub Copilot picks it up.
- Merged `architecture-current.md` and `design-architecture.md` into `Documentation/architecture.md`, which now reflects the C++20 module layout and actual implementation status.
- Updated `microsoft-proxy-implementation.md` to document the Proxy 4 facade that is actually in use.
- Converted design documents from Windows-1252 to UTF-8 and restored characters lost in earlier conversions.
- Solution: the Documentation folder now points at `Documentation/`; fixed the `/Components/MOdel/` folder name.
- Rewrote the README and this changelog to match the current state of the code.

### Removed
- Legacy `Components/LBModel` and `Components/Tests/LBModelTests`, superseded by `Components/Model/*`.
- `vcpkg_installed/` output from version control (now ignored).

## 2026-01-12

### Changed
- Split the services layer away from the old `LB.Domain` types onto the per-domain modules.
- Converted systems to C++20 modules and removed PCH usage from module projects.
- Laid out the modular game architecture (`LB.Engine`, rendering split); see `Documentation/modularized-game-architecture.md`.

### Added
- Unit tests for all Model, Service, and System components.
- Expanded `copilot-instructions.md` with module rules and common build errors.

## 2026-01-11

### Changed
- Split the domain model into separate C++20 module projects: `LBModel.Core`, `.Character`, `.Combat`, `.Inventory`, `.Dungeon`.
- Moved Progression Service DTOs into their own modules.

## 2026-01-10

### Added
- C++20 module services with tests: Character, Combat, Progression, Inventory.
- Module partitions for entities (Character, Spell, Weapon, Armor, StatusEffect).

### Changed
- Repository interfaces now use Microsoft Proxy 4.0 type erasure (`LB.Repositories`).
- `LBModel` became module-ready.

## 2026-01-09

### Added
- Initial project from the Microsoft GDK DirectX 12 template.
- D&D-inspired domain model (characters, items, spells, weapons, armour) with unit tests.
- Solution structure for Services, Systems, Tools, and GoogleTest test projects.
- Repository interfaces and asset persistence scaffolding.
- Game content: `spells.json`, `weapons.json`, `armour.json`.
