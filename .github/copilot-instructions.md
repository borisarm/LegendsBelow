# GitHub Copilot Instructions for LegendsBelow

## Project Overview
LegendsBelow is a DirectX game project built with C++ for Xbox/Windows platforms.

## Code Style Guidelines
- Follow existing C++ coding conventions in the codebase
- Use DirectX Toolkit (DirectXTK) patterns and best practices
- Maintain consistent indentation and formatting with existing files
- Use meaningful variable and function names

## Project Structure
- **Common**: Shared utility classes (StepTimer, DeviceResources)
- **Assets**: Game assets (images, icons, manifests)
- Main game logic in `Game.cpp` and `Game.h`

## Best Practices
- Keep DirectX resource management patterns consistent with DeviceResources
- Follow the existing game loop structure in Game class
- Use precompiled headers (pch.h) for commonly included headers
- Maintain Xbox compatibility when adding features

## Suggested Patterns
- Use RAII for resource management
- Prefer smart pointers (ComPtr) for DirectX COM objects
- Follow the existing Update/Render separation pattern
- Use the StepTimer for game timing logic

## Architectural Guidelines
- Favor data-driven design for gameplay systems (characters, items, abilities, dungeons) using simple JSON or equivalent.
- Keep engine/technical concerns (rendering, input, audio) separated from gameplay logic (stats, combat, AI).
- Model core concepts (character, stats, status effects, abilities) as small, composable types instead of deep inheritance trees.
- Isolate combat and stat calculation logic into testable, side-effect-light functions or modules.
- Avoid coupling UI directly to gameplay data; use view models or thin adapters where possible.

## Coding Design Guidelines
- Follow the patterns and terminology defined in `.github/design-*.md` documents when adding gameplay features.
- Assume C++20 as the baseline language standard and use its features where they improve clarity and safety (e.g., `enum class`, `std::optional`, ranges, `std::span`), while remaining consistent with existing code.
- Prefer clear, explicit functions for gameplay rules (e.g., damage, saves, XP) over inlined magic numbers.
- Keep configuration constants and tables (XP curve, class growth, loot tables) in data files rather than hard-coded.
- When adding new systems, start with simple structs and free functions; introduce classes only when lifetime or polymorphism require it.
- Add brief comments for non-obvious game rules or formulas, especially where behavior matches the design documents.
