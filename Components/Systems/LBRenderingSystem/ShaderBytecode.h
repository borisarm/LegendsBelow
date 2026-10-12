// ShaderBytecode.h - compiled shaders embedded by ShaderBytecode.cpp (FXCompile output)
#pragma once

#include <cstddef>

namespace LBShaders
{
    const unsigned char* ModelInstancedVS(std::size_t& size) noexcept;
    const unsigned char* ModelSkinnedVS(std::size_t& size) noexcept;
    const unsigned char* ModelInstancedPS(std::size_t& size) noexcept;
    const unsigned char* OverlayVS(std::size_t& size) noexcept;
    const unsigned char* OverlayPS(std::size_t& size) noexcept;
    const unsigned char* ParticleVS(std::size_t& size) noexcept;
    const unsigned char* ParticlePS(std::size_t& size) noexcept;
    const unsigned char* ParticleAlphaPS(std::size_t& size) noexcept;
    const unsigned char* ParticleShadowPS(std::size_t& size) noexcept;
}
