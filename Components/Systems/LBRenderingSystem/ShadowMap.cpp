// ShadowMap.cpp - The key light's shadow map: a depth texture the opaque world is drawn into from
// the light before the main pass, and the light's view and projection over what the camera sees
//
// Internal partition of LB.RenderingSystem. The model pipeline draws into it (ModelPass::Shadow)
// and its pixel shader samples it (ModelInstanced.hlsli: ShadowMap) through the texture heap's
// last slot.
module;

#include "Direct3D.h"

module LB.RenderingSystem:ShadowMap;

import :DeviceResources;
import :GpuResources;

namespace LB::Systems::Rendering::Detail
{
    using Microsoft::WRL::ComPtr;

    class ShadowMap
    {
    public:
        static constexpr UINT Size = 2048;                  // texels a side
        static constexpr float Extent = 36.0f;              // tiles a side, around what the camera looks at
        static constexpr float Distance = 40.0f;            // tiles from the light's eye to that point
        static constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D32_FLOAT;

        ShadowMap(DeviceResources& device, GpuResources& resources) noexcept : m_device(device), m_resources(resources) {}

        ShadowMap(ShadowMap const&) = delete;
        ShadowMap& operator= (ShadowMap const&) = delete;

        // The depth texture (typeless: written as depth, read as R32), its depth view, and its
        // shader view in the texture heap's shadow slot. Call after GpuResources::Create.
        void Create()
        {
            auto device = m_device.GetD3DDevice();
            const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, Size, Size, 1, 1, 1, 0,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
            const CD3DX12_CLEAR_VALUE clear(DepthFormat, 1.0f, 0);
            ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear, IID_PPV_ARGS(m_texture.ReleaseAndGetAddressOf())));

            m_dsvHeap = std::make_unique<DirectX::DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1);
            D3D12_DEPTH_STENCIL_VIEW_DESC depthView = {};
            depthView.Format = DepthFormat;
            depthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            device->CreateDepthStencilView(m_texture.Get(), &depthView, m_dsvHeap->GetFirstCpuHandle());

            D3D12_SHADER_RESOURCE_VIEW_DESC shaderView = {};
            shaderView.Format = DXGI_FORMAT_R32_FLOAT;
            shaderView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            shaderView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            shaderView.Texture2D.MipLevels = 1;
            device->CreateShaderResourceView(m_texture.Get(), &shaderView, m_resources.CpuHandle(GpuResources::ShadowSlot));
        }

        void Reset() noexcept
        {
            m_texture.Reset();
            m_dsvHeap.reset();
        }

        [[nodiscard]] bool IsReady() const noexcept { return m_texture != nullptr; }

        // The light's view and projection: orthographic, Extent tiles a side around `focus`, seen
        // along `direction` (where the light travels). The focus snaps to whole texels in the
        // light's view so the shadows' edges do not crawl as the camera follows the hero.
        [[nodiscard]] static DirectX::XMFLOAT4X4 LightViewProjection(const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT4& direction)
        {
            using namespace DirectX;
            const XMVECTOR travel = XMVector3Normalize(XMVectorSet(direction.x, direction.y, direction.z, 0.0f));
            const XMVECTOR up = std::abs(direction.y) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            const XMMATRIX orientation = XMMatrixLookToRH(XMVectorZero(), travel, up);

            // The focus in the light's view, snapped to the texel grid, and back to the world
            const float texel = Extent / static_cast<float>(Size);
            XMVECTOR seen = XMVector3TransformCoord(XMLoadFloat3(&focus), orientation);
            seen = XMVectorSetX(seen, std::floor(XMVectorGetX(seen) / texel) * texel);
            seen = XMVectorSetY(seen, std::floor(XMVectorGetY(seen) / texel) * texel);
            const XMVECTOR snapped = XMVector3TransformCoord(seen, XMMatrixInverse(nullptr, orientation));

            const XMVECTOR eye = XMVectorSubtract(snapped, XMVectorScale(travel, Distance));
            const XMMATRIX view = XMMatrixLookToRH(eye, travel, up);
            const XMMATRIX projection = XMMatrixOrthographicRH(Extent, Extent, 1.0f, Distance * 2.0f);
            XMFLOAT4X4 result;
            XMStoreFloat4x4(&result, view * projection);
            return result;
        }

        // Into the shadow map: depth only, cleared, its own viewport. EndPass gives it back to
        // the pixel shaders.
        void BeginPass(ID3D12GraphicsCommandList* commandList)
        {
            const auto toDepth = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            commandList->ResourceBarrier(1, &toDepth);
            const auto dsv = m_dsvHeap->GetFirstCpuHandle();
            commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
            commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(Size), static_cast<float>(Size), 0.0f, 1.0f };
            const D3D12_RECT scissor{ 0, 0, static_cast<LONG>(Size), static_cast<LONG>(Size) };
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissor);
        }

        void EndPass(ID3D12GraphicsCommandList* commandList)
        {
            const auto toShader = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            commandList->ResourceBarrier(1, &toShader);
        }

    private:
        DeviceResources& m_device;
        GpuResources& m_resources;
        ComPtr<ID3D12Resource> m_texture;
        std::unique_ptr<DirectX::DescriptorHeap> m_dsvHeap;
    };
}
