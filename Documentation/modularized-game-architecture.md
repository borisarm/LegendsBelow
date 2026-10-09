# Modularized Game Architecture for LegendsBelow

## Executive Summary

This document describes a clear separation of concerns between three key projects:
- **LegendsBelow**: Win32 application shell (module consumer)
- **LBEngineCore**: Game logic orchestration (C++20 module)
- **LBRenderingSystem**: DirectX rendering abstraction (C++20 module)

---

## As Built (2026-10-09)

The migration is implemented. Differences from the proposal below:

- **No Win32 types in module interfaces.** `GameEngine::Initialize` and `RenderingSystem::Initialize` take the window as an opaque `void*` (an `HWND` on Windows). A null window runs headless, which is how the engine and renderer are unit tested.
- **DeviceResources is an internal partition**, `LB.RenderingSystem:DeviceResources` in `DeviceResources.cpp` (MSVC requires internal partitions to use a non-`.ixx` extension and `CompileAsCppModuleInternalPartition`). It also contains `GpuRenderer`, a thin wrapper exposing only plain types. `RenderingSystem` holds it through a pimpl (`GpuBackend`) defined in the implementation unit `RenderingSystem.cpp`, so consumers never see Direct3D types.
- **Frame recording happens in `EndFrame`.** `BeginFrame` resets the queues, `Clear` records the clear color, and `EndFrame` prepares the command list, clears, renders the queues, and presents.
- **StepTimer uses `std::chrono::steady_clock`** instead of `QueryPerformanceCounter`, and adds `Advance(delta, update)` for deterministic stepping. It is exported from `LB.Engine` so it can be tested.
- **The engine defaults to a fixed 60 Hz timestep** (the template defaulted to variable).
- **Not created:** `RenderContext`, `GraphicsTypes`, `SystemManager`, `GameState`, and `WindowsPlatform.h/cpp`. They will be added when there is something to put in them; graphics types stay in `LB.RenderingSystem.ixx`.
- **LBEngineCore references only what it imports** (LBRenderingSystem, LBModel.Core). The other Systems and Services get referenced when they are wired in.
- **vcpkg manifest moved to the repository root** so LBRenderingSystem and the executable share the DirectX headers and the Agility SDK version.
- **Main.cpp is about 310 lines**, most of it the template's `WndProc`. The <200-line target would need the window code split into its own file.

---

## Original State (Before Migration)

### LegendsBelow (Template)
```
LegendsBelow/
├─ Main.cpp          # Win32 entry point, message loop
├─ Game.h/Game.cpp   # DirectX 12 initialization, game loop, rendering
├─ DeviceResources   # DirectX device/swapchain management
└─ StepTimer         # Fixed/variable timestep timing
```

**Problems:**
- Game class directly manages DirectX resources (violates separation of concerns)
- Rendering logic mixed with game logic
- No clear abstraction for systems (input, audio, persistence)
- DirectX implementation details leak into game logic

---

## Proposed Architecture

### 1. **LegendsBelow** - Application Shell (Win32 Module Consumer)

**Responsibility**: Platform entry point and message routing

**Contents:**
```
LegendsBelow/
├─ Main.cpp                    # Win32 entry point (wWinMain)
├─ WindowsPlatform.h/cpp       # Win32 window management, message loop
└─ [imports LB.Engine module]
```

**Code Ownership:**
- `wWinMain()` - Windows entry point
- `WndProc()` - Windows message handling
- Window creation/management
- Platform-specific initialization (XGameRuntime, COM)
- Message translation (WM_* → engine events)

**What Moves OUT:**
- ❌ Game logic → LBEngineCore
- ❌ DirectX initialization → LBRenderingSystem
- ❌ DeviceResources → LBRenderingSystem
- ❌ StepTimer → LBEngineCore

**Example Main.cpp (Simplified):**
```cpp
// Main.cpp - Win32 shell only
#include "pch.h"
import LB.Engine;  // Import the engine module

using namespace LB::Engine;

int WINAPI wWinMain(HINSTANCE hInstance, ...)
{
    // Platform initialization
    XGameRuntimeInitialize();
    CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

    // Create engine (module-based)
    auto engine = std::make_unique<GameEngine>();

    // Create window
    HWND hwnd = CreateGameWindow(hInstance, engine.get());

    // Initialize engine with window handle
    engine->Initialize(hwnd, 800, 600);

    // Message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            engine->Tick();  // Delegate to engine
        }
    }

    engine->Shutdown();
    XGameRuntimeUninitialize();
    return static_cast<int>(msg.wParam);
}
```

---

### 2. **LBRenderingSystem** - Graphics Abstraction (C++20 Module)

**Responsibility**: DirectX 12 rendering abstraction and resource management

**Contents:**
```
Components/Systems/LBRenderingSystem/
├─ LB.RenderingSystem.ixx      # Module interface
├─ DeviceResources.ixx         # DirectX device/swapchain management
├─ RenderContext.ixx           # Per-frame rendering context
└─ GraphicsTypes.ixx           # Rendering primitives (Color, Rectangle, etc.)
```

**Module Interface:**
```cpp
// LB.RenderingSystem.ixx
export module LB.RenderingSystem;

import std;

export namespace LB::Systems::Rendering {

    /// <summary>
    /// High-level rendering system (already defined)
    /// </summary>
    class RenderingSystem {
    public:
        bool Initialize(HWND hwnd, int32_t width, int32_t height);
        void Shutdown();

        void BeginFrame();
        void EndFrame();

        void DrawSprite(const SpriteRenderData& sprite);
        void DrawText(const TextRenderData& text);
        void Clear(const Color& color);
        
        // Device management
        void OnDeviceLost();
        void OnDeviceRestored();
        void OnWindowSizeChanged(int32_t width, int32_t height);
    };

    /// <summary>
    /// Low-level DirectX device management (from DeviceResources)
    /// </summary>
    class DeviceResources {
    public:
        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND hwnd, int32_t width, int32_t height);
        
        ID3D12Device* GetD3DDevice() const;
        ID3D12CommandQueue* GetCommandQueue() const;
        ID3D12GraphicsCommandList* GetCommandList() const;
        // ... DirectX interfaces
    };

    /// <summary>
    /// Per-frame rendering context
    /// </summary>
    class RenderContext {
    public:
        void Prepare();
        void Present();
        void Clear(const Color& clearColor);
        
        ID3D12GraphicsCommandList* GetCommandList() const;
    };
}
```

**Code Ownership:**
- ✅ DeviceResources (from LegendsBelow/Common)
- ✅ DirectX 12 device initialization
- ✅ Swapchain management
- ✅ Command list/queue management
- ✅ Viewport/scissor rect management
- ✅ RTV/DSV management
- ✅ Shader compilation and loading
- ✅ Texture/sprite rendering
- ✅ Text rendering (DirectXTK integration)

**What Stays OUT:**
- ❌ Game loop timing (belongs in LBEngineCore)
- ❌ Game state management
- ❌ Input handling
- ❌ Audio management

---

### 3. **LBEngineCore** - Game Engine Orchestration (C++20 Module)

**Responsibility**: Game loop, system coordination, and game state management

**Contents:**
```
Components/LBEngineCore/
├─ LB.Engine.ixx                 # Primary module
├─ GameEngine.ixx                # Main game engine class
├─ StepTimer.ixx                 # Frame timing (from LegendsBelow/Common)
├─ SystemManager.ixx             # Manages all game systems
└─ GameState.ixx                 # Game state machine
```

**Module Interface:**
```cpp
// LB.Engine.ixx
export module LB.Engine;

import LB.Core;
import LB.RenderingSystem;
import LB.InputSystem;
import LB.AudioSystem;
import LB.AssetSystem;
import std;

export namespace LB::Engine {

    /// <summary>
    /// Main game engine - orchestrates all systems
    /// </summary>
    class GameEngine {
    public:
        GameEngine();
        ~GameEngine();

        // Engine lifecycle
        bool Initialize(HWND hwnd, int32_t width, int32_t height);
        void Shutdown();
        void Tick();  // Called by Main.cpp message loop

        // Window events (called from WndProc)
        void OnActivated();
        void OnDeactivated();
        void OnSuspending();
        void OnResuming();
        void OnWindowMoved();
        void OnWindowSizeChanged(int32_t width, int32_t height);

        // Device events (from rendering system)
        void OnDeviceLost();
        void OnDeviceRestored();

        // Configuration
        void GetDefaultWindowSize(int32_t& width, int32_t& height) const;
        void SetFixedTimeStep(bool enabled, double targetSeconds = 1.0 / 60.0);

    private:
        void Update(double deltaTime);
        void Render();

        // System instances
        Systems::Rendering::RenderingSystem m_renderingSystem;
        Systems::Input::InputSystem m_inputSystem;
        Systems::Audio::AudioSystem m_audioSystem;
        Systems::Asset::AssetSystem m_assetSystem;
        Systems::Persistence::PersistenceSystem m_persistenceSystem;

        // Services
        Services::Character::CharacterService m_characterService;
        Services::Combat::CombatService m_combatService;
        Services::Inventory::InventoryService m_inventoryService;
        Services::Progression::ProgressionService m_progressionService;
        Services::Dungeon::DungeonService m_dungeonService;

        // Timing
        StepTimer m_timer;
        bool m_isPaused;
    };

    /// <summary>
    /// Frame timing utility (from StepTimer)
    /// </summary>
    class StepTimer {
    public:
        void Tick(std::function<void()> update);
        void ResetElapsedTime();
        
        void SetFixedTimeStep(bool enabled);
        void SetTargetElapsedSeconds(double target);
        
        uint64_t GetFrameCount() const;
        double GetElapsedSeconds() const;
        double GetTotalSeconds() const;
    };
}
```

**Code Ownership:**
- ✅ Game loop orchestration (Tick → Update → Render)
- ✅ StepTimer (from LegendsBelow/Common)
- ✅ System initialization and coordination
- ✅ Service initialization and coordination
- ✅ Game state management
- ✅ Window event translation to game events
- ✅ Frame timing (fixed/variable timestep)

**What Stays OUT:**
- ❌ Win32 message loop (belongs in LegendsBelow)
- ❌ Window creation (belongs in LegendsBelow)
- ❌ DirectX device management (belongs in LBRenderingSystem)

---

## Component Responsibilities Matrix

| Component | Win32 Entry | Message Loop | Window Mgmt | DirectX Init | Game Loop | Systems | Services | Game Logic |
|-----------|-------------|--------------|-------------|--------------|-----------|---------|----------|------------|
| **LegendsBelow** | ✅ | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **LBEngineCore** | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ |
| **LBRenderingSystem** | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |

---

## Migration Plan

### Phase 1: Move DeviceResources to LBRenderingSystem
1. Copy `LegendsBelow/DeviceResources.h/cpp` → `LBRenderingSystem/DeviceResources.ixx`
2. Convert to C++20 module
3. Integrate with existing `RenderingSystem` class
4. Update `LBRenderingSystem.ixx` to export `DeviceResources`

### Phase 2: Move StepTimer to LBEngineCore
1. Copy `LegendsBelow/StepTimer.h` → `LBEngineCore/StepTimer.ixx`
2. Convert to C++20 module
3. Export from `LB.Engine` module

### Phase 3: Create GameEngine in LBEngineCore
1. Create `LBEngineCore/GameEngine.ixx`
2. Move game logic from `LegendsBelow/Game.cpp` → `GameEngine`
3. Initialize all systems (rendering, input, audio, etc.)
4. Implement Update/Render loop

### Phase 4: Simplify LegendsBelow to Shell
1. Keep only `Main.cpp` with Win32 boilerplate
2. Add `import LB.Engine;`
3. Create `GameEngine` instance
4. Delegate to `engine->Tick()` in message loop

### Phase 5: Update Project References
```xml
<!-- LegendsBelow.vcxproj -->
<ItemGroup>
  <ProjectReference Include="..\Components\LBEngineCore\LBEngineCore.vcxproj" />
</ItemGroup>

<!-- LBEngineCore.vcxproj -->
<ItemGroup>
  <ProjectReference Include="..\Systems\LBRenderingSystem\LBRenderingSystem.vcxproj" />
  <ProjectReference Include="..\Systems\LBInputSystem\LBInputSystem.vcxproj" />
  <ProjectReference Include="..\Systems\LBAudioSystem\LBAudioSystem.vcxproj" />
  <!-- ... other systems -->
</ItemGroup>
```

---

## Module Dependency Graph

```
LegendsBelow (Win32 app)
    └─ imports LB.Engine
                ├─ imports LB.RenderingSystem
                ├─ imports LB.InputSystem
                ├─ imports LB.AudioSystem
                ├─ imports LB.AssetSystem
                ├─ imports LB.PersistenceSystem
                ├─ imports LB.CharacterService
                ├─ imports LB.CombatService
                ├─ imports LB.InventoryService
                ├─ imports LB.ProgressionService
                └─ imports LB.DungeonService
                        ├─ imports LB.Core
                        ├─ imports LB.Character
                        ├─ imports LB.Combat
                        ├─ imports LB.Inventory
                        └─ imports LB.Dungeon
```

---

## Benefits of This Architecture

### 1. **Clear Separation of Concerns**
- LegendsBelow: Platform shell
- LBEngineCore: Game logic orchestration
- LBRenderingSystem: Graphics abstraction

### 2. **Testability**
- LBEngineCore can be unit tested without Win32 dependencies
- LBRenderingSystem can be tested with mock devices
- Systems can be tested in isolation

### 3. **Reusability**
- LBEngineCore could run on different platforms (SDL, console SDKs)
- LBRenderingSystem abstracts DirectX 12 details
- Services are platform-agnostic

### 4. **Maintainability**
- Changes to Win32 code don't affect engine
- Changes to DirectX code don't affect game logic
- Each module has a single, well-defined responsibility

### 5. **Module Encapsulation**
- DirectX types (`ID3D12Device*`) stay in LBRenderingSystem
- Win32 types (`HWND`, `MSG`) stay in LegendsBelow
- Game types flow through clean interfaces

---

## Example: Rendering Flow

```cpp
// LegendsBelow/Main.cpp
engine->Tick();

// LBEngineCore/GameEngine.cpp
void GameEngine::Tick() {
    m_timer.Tick([this]() {
        Update(m_timer.GetElapsedSeconds());
    });
    Render();
}

void GameEngine::Render() {
    m_renderingSystem.BeginFrame();
    m_renderingSystem.Clear(Color::CornflowerBlue());
    
    // Render game entities
    for (auto& sprite : m_gameState.GetVisibleSprites()) {
        m_renderingSystem.DrawSprite(sprite);
    }
    
    m_renderingSystem.EndFrame();
}

// LBRenderingSystem/RenderingSystem.ixx
void RenderingSystem::BeginFrame() {
    m_deviceResources->Prepare();
    // DirectX 12 command list preparation
}

void RenderingSystem::EndFrame() {
    m_deviceResources->Present();
    // DirectX 12 present
}
```

---

## Decision Points

### Should DeviceResources be public or internal to RenderingSystem?

**Recommendation**: Keep it internal (information hiding)

```cpp
// ✅ Preferred - DeviceResources is implementation detail
class RenderingSystem {
private:
    std::unique_ptr<DeviceResources> m_deviceResources;
};

// ❌ Avoid - Leaking DirectX types
class RenderingSystem {
public:
    DeviceResources* GetDeviceResources();  // Exposes DirectX internals
};
```

### Should StepTimer be part of LB.Engine or separate?

**Recommendation**: Part of LB.Engine (not exported separately)

StepTimer is an implementation detail of GameEngine. External consumers don't need it.

---

## File Move Checklist

### From LegendsBelow → LBRenderingSystem:
- [x] `DeviceResources.h/cpp` → `DeviceResources.cpp` (internal partition)
- [x] DirectX initialization code from `Game.cpp`
- [x] `Clear()`, `CreateDeviceDependentResources()`, `CreateWindowSizeDependentResources()`

### From LegendsBelow → LBEngineCore:
- [x] `StepTimer.h` → `StepTimer.ixx`
- [x] `Game.h/cpp` → `GameEngine.ixx` (refactored)
- [x] `Update()`, `Render()` logic
- [ ] System initialization code (only RenderingSystem so far)

### Stays in LegendsBelow:
- [x] `Main.cpp` (Win32 entry point)
- [x] `WndProc()` (Windows message handling)
- [x] Window creation code
- [x] Platform initialization (XGameRuntime, COM)

---

## Success Criteria

- [ ] LegendsBelow has <200 lines of code (Win32 shell only); about 310 today, see As Built
- [x] LBEngineCore has no Win32 dependencies
- [x] LBRenderingSystem has no game logic
- [ ] All projects compile as C++20 modules (LegendsBelow's `Main.cpp` imports `LB.Engine` but still uses `pch.h` for Win32/GDK headers)
- [x] Separation of concerns is clear and maintainable
- [x] Module boundaries prevent unwanted dependencies

---

## Next Steps

1. Wire Input, Audio, Asset, and Persistence systems into `GameEngine`
2. Implement sprite/text rendering behind `RenderingSystem::EndFrame`
3. Inject Services and repositories
4. Optionally move window management out of `Main.cpp` into `WindowsPlatform.cpp`

---

**Document Status**: Implemented (see As Built).  
**Last Updated**: 2026-10-09  
**Author**: GitHub Copilot (based on project analysis)
