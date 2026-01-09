# LegendsBelow – Technical Design (DirectX 12)

## 1. Purpose

Defines the high-level technical architecture for the custom engine using DirectX 12:
- Rendering pipeline
- Resource management
- Integration with existing `DeviceResources` / DirectXTK patterns
- Performance targets

This document is descriptive; engine code lives in C++ files like `Game.cpp`, `DeviceResources.cpp`, etc.

---

## 2. Rendering Architecture

### 2.1 Renderer Overview

- API: DirectX 12.
- Platform focus: Xbox Series consoles & Windows 10+.
- Rendering style: isometric scene with 2D/3D hybrid (tile geometry + sprites + UI).

Core components:
- `DeviceResources` – encapsulates device, swap chain, command queues, etc.
- `Renderer` (or equivalent) – owns frame graph, render passes, and resource bindings.
- `Game` – issues high-level draw requests and manages scene state.

### 2.2 Swap Chain and Frame Buffers

- Double or triple buffering (to be tuned).
- Command queue for graphics, optional separate copy queue.
- Back buffers cleared with a dim ambient color.

### 2.3 Render Passes (Baseline)

1. **Depth/Prepass (optional)** – if using 3D walls/props with depth-heavy scenes.
2. **Opaque Geometry** – floor tiles, walls, static props.
3. **Characters and Dynamic Objects** – player, enemies, projectiles.
4. **Transparent/Particles** – spell effects, environment FX.
5. **UI/HUD** – drawn last in screen space.

Each pass has its own root signature and PSO configuration or shares a generalized one where possible.

---

## 3. Scene Representation

### 3.1 Isometric World

- World stored as a tile grid with optional height information.
- Each tile:
  - Base type (floor/wall/void).
  - Theme/material (crypt stone, cave rock, etc.).
  - Flags: walkable, occluder, hazard, cover.

### 3.2 Entities

- ECS or component-based system (simplified):
  - Transform (position, rotation, scale).
  - Renderable (mesh/sprite ID, material).
  - Collision/Navigation (bounds, flags).

Characters and props register themselves with the renderer each frame or maintain a persistent handle.

---

## 4. Resources and Asset Management

### 4.1 Textures and Sprites

- Use DirectXTK helpers where appropriate (`CreateWICTextureFromFile`, spritebatch-style drawing).
- Pack UI and small assets into atlases to reduce bindings.

### 4.2 Geometry

- Floor/walls: tile meshes instanced or batched.
- Characters: either 2D sprites or low-poly 3D meshes rendered from isometric camera.

### 4.3 Shaders

- HLSL files compiled to bytecode at build time.
- Shader types:
  - Tile/geometry shaders.
  - Character shaders.
  - Particle shaders.
  - UI shaders.

---

## 5. Command Recording and Batching

### 5.1 Command Lists

- One or more command lists per frame:
  - Geometry & characters.
  - Particles.
  - UI.

Workflow per frame:
- Reset command allocator & command list.
- Record commands for each pass.
- Close and execute on graphics queue.

### 5.2 Batching

- Batch tiles by material and draw them via instancing.
- Group characters using shared shaders and textures.
- Reduce state changes (PSO and root signature switches).

---

## 6. Performance Targets

### 6.1 Frame Rate

- Target: 60 FPS locked on Xbox Series X|S and modern PCs.
- Minimum: 30 FPS on lower-spec hardware (configurable).

### 6.2 Budgets (Initial Targets)

- Draw calls: keep under a few thousand per frame.
- Triangle count: relatively low due to isometric perspective; emphasis on efficient sprites.
- CPU time per frame: under 8 ms for game and render logic combined on target hardware.

---

## 7. Data-Driven Content

### 7.1 Configuration Files

- Use JSON/XML for:
  - Items
  - Enemies
  - Spells
  - Dungeon templates

### 7.2 Hot Reload (Debug Only)

- Engine should support reloading data files without restart for iteration speed:
  - Detect file changes.
  - Reload relevant assets safely between frames.

---

## 8. Integration with Game Loop

### 8.1 Game Update/Render Separation

- `Game::Update` – handles simulation (input, AI, physics, gameplay logic).
- `Game::Render` – issues draw calls via renderer and DirectX 12.

### 8.2 StepTimer

- Use existing `StepTimer` for fixed or variable-step update.
- Adjust logic if adding tactical pause to avoid time accumulation while paused.

---

## 9. Platform Considerations

### 9.1 Xbox

- Follow Xbox performance guidelines:
  - Avoid blocking operations on main thread.
  - Manage ESRAM/Memory considerations where applicable (Series consoles).

### 9.2 Windows

- Support windowed and fullscreen modes.
- Handle device lost/reset via `DeviceResources` patterns.

---

## 10. Future Extensions

- Post-processing pipeline for advanced effects.
- Dynamic shadows for moving lights.
- More advanced material system (normal maps, PBR-style lighting) if needed.
