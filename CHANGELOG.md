# Changelog

All notable changes to LegendsBelow. Dates are commit dates.

## [Unreleased]

### Added
- Audio wired into the engine: XAudio2 output device (`LB.AudioSystem:XAudio2Backend`), master volume, suspend/resume with the window, gameplay channels paused during the tactical pause (music keeps playing), volumes restored from and saved to preferences.
- Persistence backed by files: save slots (metadata + key/value `SaveData`) and preferences as JSON in `%LOCALAPPDATA%\LegendsBelow\Saves`, written atomically and discovered at startup. Quick save/load on F5/F9 (game clock and player position).
- Player movement: the engine owns a Warrior (CharacterService) moving in real time with WASD/arrows or the left stick at `ClassBaseSpeed + DEX_mod * 0.1` tiles/s (design-character-system.md 4.2), inside a placeholder test arena; stops while paused.
- Solid rectangle rendering (`RenderingSystem::DrawRectangle` now drawn on the GPU) for the placeholder arena and player.
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
