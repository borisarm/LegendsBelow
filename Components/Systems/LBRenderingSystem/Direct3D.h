// Direct3D.h - The Win32, Direct3D 12, DXGI, and DirectXTK12 headers the renderer's internal
// partitions include in their global module fragments (one list, the same everywhere)
#pragma once

#include <winsdkver.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <sdkddkver.h>

#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wrl/client.h>
#include <wrl/event.h>

#include <directx/dxgiformat.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxguids/dxguids.h>

#include <dxgi1_6.h>

// DirectX Tool Kit for DX12 (static, see triplets/): use the same DirectX-Headers d3d12.h as above
#define USING_DIRECTX_HEADERS
#include <DirectXMath.h>
#include <directxtk12/CommonStates.h>
#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/Effects.h>
#include <directxtk12/EffectPipelineStateDescription.h>
#include <directxtk12/GeometricPrimitive.h>
#include <directxtk12/GraphicsMemory.h>
#include <directxtk12/RenderTargetState.h>
#include <directxtk12/VertexTypes.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <unordered_map>

// Keep the library self-contained so any consumer (game, tests) links without extra setup.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// Compiled shaders (ShaderBytecode.cpp, a plain C++ file including the FXCompile output)
#include "ShaderBytecode.h"

#pragma warning(disable : 4061)
