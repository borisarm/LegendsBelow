# Changelog

All notable changes to LegendsBelow. Dates are commit dates.

## [Unreleased]

### Changed
- Moved design and architecture documents from `.github/` to `Documentation/`; `copilot-instructions.md` stays in `.github/` so GitHub Copilot picks it up.
- Merged `architecture-current.md` and `design-architecture.md` into `Documentation/architecture.md`, which now reflects the C++20 module layout and actual implementation status.
- Updated `microsoft-proxy-implementation.md` to document the Proxy 4 facade that is actually in use.
- Converted design documents from Windows-1252 to UTF-8 and restored characters lost in earlier conversions.
- Solution: the Documentation folder now points at `Documentation/`; fixed the `/Components/MOdel/` folder name.
- Rewrote the README and this changelog to match the current state of the code.

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
