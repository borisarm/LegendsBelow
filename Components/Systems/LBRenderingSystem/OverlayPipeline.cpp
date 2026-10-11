// OverlayPipeline.cpp - The 2D overlay: textured, alpha-blended triangles in pixels over the frame
// (rectangles, pixel font text, distance field text)
//
// Internal partition of LB.RenderingSystem (Overlay*.hlsl).
module;

#include "Direct3D.h"

module LB.RenderingSystem:OverlayPipeline;

import :Model;
import :DeviceResources;
import :GpuResources;

namespace LB::Systems::Rendering::Detail
{
    using Microsoft::WRL::ComPtr;

    /// <summary>
    /// One vertex of the 2D overlay (Overlay.hlsli): pixels from the top-left corner, atlas
    /// coordinates, and a linear color with straight alpha
    /// </summary>
    struct OverlayVertex
    {
        float x, y;
        float u, v;
        float r, g, b, a;
        float mode;      // 0: atlas texel (pixel font, rectangles); 1: distance field text; 2: with outline
        float outline;   // distance field units the outline reaches past the edge
    };

    /// <summary>
    /// A run of overlay vertices sampling one atlas (null: the built-in pixel font atlas)
    /// </summary>
    struct OverlayBatch
    {
        const ModelTexture* atlas;
        std::uint32_t first;
        std::uint32_t count;
    };

    class OverlayPipeline
    {
    public:
        // Screen size constants, the atlas, point and linear sampling, straight alpha blending
        void Create(ID3D12Device* device, const DirectX::RenderTargetState& renderTargetState)
        {
            using namespace DirectX;
            const CD3DX12_DESCRIPTOR_RANGE atlasRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            CD3DX12_ROOT_PARAMETER parameters[2];
            parameters[0].InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
            parameters[1].InitAsDescriptorTable(1, &atlasRange, D3D12_SHADER_VISIBILITY_PIXEL);
            // s0 point (pixel font, rectangles), s1 linear (distance fields)
            CD3DX12_STATIC_SAMPLER_DESC samplers[2];
            samplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_POINT,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
            samplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
            for (auto& sampler : samplers)
                sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            const CD3DX12_ROOT_SIGNATURE_DESC rootDesc(static_cast<UINT>(std::size(parameters)), parameters, 2, samplers,
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
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };
            static_assert(sizeof(OverlayVertex) == 40);

            std::size_t vsSize = 0;
            std::size_t psSize = 0;
            const auto* vs = LBShaders::OverlayVS(vsSize);
            const auto* ps = LBShaders::OverlayPS(psSize);

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
            desc.pRootSignature = m_rootSignature.Get();
            desc.VS = { vs, vsSize };
            desc.PS = { ps, psSize };
            desc.BlendState = CommonStates::NonPremultiplied;
            desc.SampleMask = UINT_MAX;
            desc.RasterizerState = CommonStates::CullNone;
            desc.DepthStencilState = CommonStates::DepthNone;
            desc.InputLayout = { elements, static_cast<UINT>(std::size(elements)) };
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets = 1;
            desc.RTVFormats[0] = renderTargetState.rtvFormats[0];
            desc.DSVFormat = renderTargetState.dsvFormat;
            desc.SampleDesc.Count = 1;
            ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_pipeline.ReleaseAndGetAddressOf())));
        }

        void Reset() noexcept
        {
            m_pipeline.Reset();
            m_rootSignature.Reset();
        }

        // Six vertices per quad, one draw per run of the same atlas. Call after the world pass.
        void Draw(ID3D12GraphicsCommandList* commandList, GpuResources& resources, float screenWidth, float screenHeight,
                  const OverlayVertex* vertices, const std::vector<OverlayBatch>& batches)
        {
            std::size_t count = 0;
            for (const auto& batch : batches)
                count = std::max<std::size_t>(count, batch.first + batch.count);
            if (!m_pipeline || !resources.IsReady() || count == 0) return;

            auto buffer = resources.Allocate(count * sizeof(OverlayVertex));
            std::memcpy(buffer.Memory(), vertices, count * sizeof(OverlayVertex));
            const float screen[2] = { screenWidth, screenHeight };

            resources.BindHeap(commandList);
            commandList->SetGraphicsRootSignature(m_rootSignature.Get());
            commandList->SetGraphicsRoot32BitConstants(0, 2, screen, 0);
            commandList->SetPipelineState(m_pipeline.Get());
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            D3D12_VERTEX_BUFFER_VIEW view{};
            view.BufferLocation = buffer.GpuAddress();
            view.SizeInBytes = static_cast<UINT>(count * sizeof(OverlayVertex));
            view.StrideInBytes = sizeof(OverlayVertex);
            commandList->IASetVertexBuffers(0, 1, &view);
            for (const auto& batch : batches)
            {
                const auto slot = resources.Slot(batch.atlas ? batch.atlas : &GpuResources::FontTexture());
                if (!slot || batch.count == 0) continue;
                commandList->SetGraphicsRootDescriptorTable(1, resources.GpuHandle(*slot));
                commandList->DrawInstanced(batch.count, 1, batch.first, 0);
            }
        }

    private:
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipeline;
    };
}
