//
// pch.h
// Header for standard system include files.
//
// The executable is only the Win32/GDK shell; rendering and game code live in the
// LB.Engine and LB.RenderingSystem modules.
//

#pragma once

#include <winsdkver.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <sdkddkver.h>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// GET_X_LPARAM / GET_Y_LPARAM for mouse messages
#include <windowsx.h>

// SHGetKnownFolderPath for the save directory
#include <ShlObj.h>

#include <string>

// D3D12_SDK_VERSION for the Agility SDK exports in Main.cpp
#include <directx/d3d12.h>

// XMVerifyCPUSupport
#include <DirectXMath.h>

#include <algorithm>
#include <cwctype>
#include <memory>
#include <sstream>
#include <tuple>

// Game Runtime
#include <XGameRuntime.h>
