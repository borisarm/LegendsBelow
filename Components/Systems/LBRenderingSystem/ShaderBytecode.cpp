// ShaderBytecode.cpp - compiled shaders, embedded at build time
//
// Not a module unit on purpose: the FXCompile step writes the bytecode headers to $(IntDir)
// after C++ module dependency scanning has run, so this file is excluded from scanning
// (see the project file) and only exposes the bytes through the plain functions declared in
// ShaderBytecode.h.
#include "ShaderBytecode.h"

#include "ModelInstanced_VS.inc"
#include "ModelSkinned_VS.inc"
#include "ModelInstanced_PS.inc"
#include "Overlay_VS.inc"
#include "Overlay_PS.inc"

namespace LBShaders
{
    const unsigned char* ModelInstancedVS(std::size_t& size) noexcept
    {
        size = sizeof(g_ModelInstanced_VS);
        return g_ModelInstanced_VS;
    }

    const unsigned char* ModelSkinnedVS(std::size_t& size) noexcept
    {
        size = sizeof(g_ModelSkinned_VS);
        return g_ModelSkinned_VS;
    }

    const unsigned char* ModelInstancedPS(std::size_t& size) noexcept
    {
        size = sizeof(g_ModelInstanced_PS);
        return g_ModelInstanced_PS;
    }

    const unsigned char* OverlayVS(std::size_t& size) noexcept
    {
        size = sizeof(g_Overlay_VS);
        return g_Overlay_VS;
    }

    const unsigned char* OverlayPS(std::size_t& size) noexcept
    {
        size = sizeof(g_Overlay_PS);
        return g_Overlay_PS;
    }
}
