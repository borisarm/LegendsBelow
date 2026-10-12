// ParticlePipeline.cpp - Camera-facing particle sprites: glowing ones added to the frame, dust
// blended over it; and shadows, the same quads laid flat on the ground. Depth-tested against the
// world without writing depth
//
// Internal partition of LB.RenderingSystem (Particle*.hlsl).
module;

#include "Direct3D.h"

module LB.RenderingSystem:ParticlePipeline;

import :DeviceResources;
import :GpuResources;

namespace LB::Systems::Rendering::Detail
{
    using Microsoft::WRL::ComPtr;

    class ParticlePipeline
    {
    public:
        // How a draw's quads land on the frame
        enum class Blend { Additive, Alpha, Shadow };

        // Floats per particle in Draw's input: x, y, z, size, r, g, b, a
        static constexpr std::size_t ParticleFloats = 8;

        // Frame constants, the sprite, and two blend modes over the same shaders' inputs
        void Create(ID3D12Device* device, const DirectX::RenderTargetState& renderTargetState)
        {
            using namespace DirectX;
            const CD3DX12_DESCRIPTOR_RANGE spriteRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            CD3DX12_ROOT_PARAMETER parameters[2]{};
            parameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
            parameters[1].InitAsDescriptorTable(1, &spriteRange, D3D12_SHADER_VISIBILITY_PIXEL);
            CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            const CD3DX12_ROOT_SIGNATURE_DESC rootDesc(static_cast<UINT>(std::size(parameters)), parameters, 1, &sampler,
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf()));
            ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));

            static constexpr D3D12_INPUT_ELEMENT_DESC elements[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            };

            std::size_t vsSize = 0;
            std::size_t psSize = 0;
            const auto* vs = LBShaders::ParticleVS(vsSize);
            const auto* ps = LBShaders::ParticlePS(psSize);

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
            desc.pRootSignature = m_rootSignature.Get();
            desc.VS = { vs, vsSize };
            desc.PS = { ps, psSize };
            desc.BlendState = CommonStates::Additive;
            desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;   // the shader premultiplies
            desc.SampleMask = UINT_MAX;
            desc.RasterizerState = CommonStates::CullNone;
            desc.DepthStencilState = CommonStates::DepthRead;
            desc.InputLayout = { elements, static_cast<UINT>(std::size(elements)) };
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets = 1;
            desc.RTVFormats[0] = renderTargetState.rtvFormats[0];
            desc.DSVFormat = renderTargetState.dsvFormat;
            desc.SampleDesc.Count = 1;
            ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_additive.ReleaseAndGetAddressOf())));

            // Dust and smoke: straight alpha blending
            const auto* alphaPs = LBShaders::ParticleAlphaPS(psSize);
            desc.PS = { alphaPs, psSize };
            desc.BlendState = CommonStates::NonPremultiplied;
            ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_alpha.ReleaseAndGetAddressOf())));

            // Shadows: an even dark middle and a soft rim of their own (not the sprite's)
            const auto* shadowPs = LBShaders::ParticleShadowPS(psSize);
            desc.PS = { shadowPs, psSize };
            ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_shadow.ReleaseAndGetAddressOf())));
        }

        void Reset() noexcept
        {
            m_additive.Reset();
            m_alpha.Reset();
            m_shadow.Reset();
            m_rootSignature.Reset();
        }

        // Particles as camera-facing quads, facing along the camera's right and up vectors. Call
        // after the models.
        void Draw(ID3D12GraphicsCommandList* commandList, GpuResources& resources, const DirectX::XMFLOAT4X4& viewProjection,
                  DirectX::XMFLOAT4 cameraRight, DirectX::XMFLOAT4 cameraUp, const float* particles, std::size_t count, Blend blend)
        {
            auto* pipeline = blend == Blend::Additive ? m_additive.Get() : blend == Blend::Alpha ? m_alpha.Get() : m_shadow.Get();
            const auto sprite = resources.Slot(&GpuResources::ParticleTexture());
            if (!pipeline || !resources.IsReady() || !sprite || count == 0) return;

            struct ParticleFrame
            {
                DirectX::XMFLOAT4X4 viewProjection;
                DirectX::XMFLOAT4 right;
                DirectX::XMFLOAT4 up;
            };
            auto frame = resources.Allocate(sizeof(ParticleFrame), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            *static_cast<ParticleFrame*>(frame.Memory()) = ParticleFrame{ viewProjection, cameraRight, cameraUp };

            const std::size_t bytes = count * ParticleFloats * sizeof(float);
            auto instances = resources.Allocate(bytes);
            std::memcpy(instances.Memory(), particles, bytes);

            resources.BindHeap(commandList);
            commandList->SetGraphicsRootSignature(m_rootSignature.Get());
            commandList->SetGraphicsRootConstantBufferView(0, frame.GpuAddress());
            commandList->SetGraphicsRootDescriptorTable(1, resources.GpuHandle(*sprite));
            commandList->SetPipelineState(pipeline);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            D3D12_VERTEX_BUFFER_VIEW view{};
            view.BufferLocation = instances.GpuAddress();
            view.SizeInBytes = static_cast<UINT>(bytes);
            view.StrideInBytes = ParticleFloats * sizeof(float);
            commandList->IASetVertexBuffers(0, 1, &view);
            commandList->DrawInstanced(6, static_cast<UINT>(count), 0, 0);
        }

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_additive;
        ComPtr<ID3D12PipelineState> m_alpha;
        ComPtr<ID3D12PipelineState> m_shadow;
    };
}
