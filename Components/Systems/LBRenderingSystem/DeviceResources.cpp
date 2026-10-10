// DeviceResources.cpp - Direct3D 12 device, swap chain, and frame presentation
//
// Internal partition of LB.RenderingSystem: nothing here is visible to consumers of
// the module. Ported from the Microsoft GDK DirectX 12 template (DeviceResources.h/.cpp); the
// 3D world pass draws DirectXTK12 primitives with a lit BasicEffect, and glTF models (textured,
// rigid or skinned) with the instanced model shaders.
module;

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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
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

module LB.RenderingSystem:DeviceResources;

import :BitmapFont;
import :Camera;
import :Font;
import :Model;

namespace LB::Systems::Rendering::Detail
{
    using Microsoft::WRL::ComPtr;

    // Helper class for COM exceptions
    class com_exception : public std::exception
    {
    public:
        com_exception(HRESULT hr) noexcept : result(hr) {}

        const char* what() const noexcept override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
            return s_str;
        }

    private:
        HRESULT result;
    };

    // Helper utility converts D3D API failures into exceptions.
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr);
        }
    }

    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    struct IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;

    protected:
        ~IDeviceNotify() = default;
    };

    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static constexpr unsigned int c_AllowTearing = 0x1;
        static constexpr unsigned int c_EnableHDR    = 0x2;
        static constexpr unsigned int c_ReverseDepth = 0x4;

        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        UINT backBufferCount = 2,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0,
                        unsigned int flags = 0) noexcept(false);
        ~DeviceResources();

        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND window, int width, int height) noexcept;
        bool WindowSizeChanged(int width, int height);
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }
        void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT,
                     D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_RENDER_TARGET);
        void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);
        void WaitForGpu() noexcept;
        void UpdateColorSpace();

        // Device Accessors.
        RECT GetOutputSize() const noexcept { return m_outputSize; }

        // Direct3D Accessors.
        auto                        GetD3DDevice() const noexcept          { return m_d3dDevice.Get(); }
        auto                        GetSwapChain() const noexcept          { return m_swapChain.Get(); }
        auto                        GetDXGIFactory() const noexcept        { return m_dxgiFactory.Get(); }
        HWND                        GetWindow() const noexcept             { return m_window; }
        D3D_FEATURE_LEVEL           GetDeviceFeatureLevel() const noexcept { return m_d3dFeatureLevel; }
        ID3D12Resource*             GetRenderTarget() const noexcept       { return m_renderTargets[m_backBufferIndex].Get(); }
        ID3D12Resource*             GetDepthStencil() const noexcept       { return m_depthStencil.Get(); }
        ID3D12CommandQueue*         GetCommandQueue() const noexcept       { return m_commandQueue.Get(); }
        ID3D12CommandAllocator*     GetCommandAllocator() const noexcept   { return m_commandAllocators[m_backBufferIndex].Get(); }
        auto                        GetCommandList() const noexcept        { return m_commandList.Get(); }
        DXGI_FORMAT                 GetBackBufferFormat() const noexcept   { return m_backBufferFormat; }
        DXGI_FORMAT                 GetDepthBufferFormat() const noexcept  { return m_depthBufferFormat; }
        D3D12_VIEWPORT              GetScreenViewport() const noexcept     { return m_screenViewport; }
        D3D12_RECT                  GetScissorRect() const noexcept        { return m_scissorRect; }
        UINT                        GetCurrentFrameIndex() const noexcept  { return m_backBufferIndex; }
        UINT                        GetBackBufferCount() const noexcept    { return m_backBufferCount; }
        DXGI_COLOR_SPACE_TYPE       GetColorSpace() const noexcept         { return m_colorSpace; }
        unsigned int                GetDeviceOptions() const noexcept      { return m_options; }

        CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const noexcept
        {
            const auto cpuHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, static_cast<INT>(m_backBufferIndex), m_rtvDescriptorSize);
        }
        CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const noexcept
        {
            const auto cpuHandle = m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle);
        }

    private:
        void MoveToNextFrame();
        void GetAdapter(IDXGIAdapter1** ppAdapter);

        static constexpr size_t MAX_BACK_BUFFER_COUNT = 3;

        UINT                                                m_backBufferIndex;

        // Direct3D objects.
        Microsoft::WRL::ComPtr<ID3D12Device>                m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   m_commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue>          m_commandQueue;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_commandAllocators[MAX_BACK_BUFFER_COUNT];

        // Swap chain objects.
        Microsoft::WRL::ComPtr<IDXGIFactory6>               m_dxgiFactory;
        Microsoft::WRL::ComPtr<IDXGISwapChain3>             m_swapChain;
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_renderTargets[MAX_BACK_BUFFER_COUNT];
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_depthStencil;

        // Presentation fence objects.
        Microsoft::WRL::ComPtr<ID3D12Fence>                 m_fence;
        UINT64                                              m_fenceValues[MAX_BACK_BUFFER_COUNT];
        Microsoft::WRL::Wrappers::Event                     m_fenceEvent;

        // Direct3D rendering objects.
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_rtvDescriptorHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_dsvDescriptorHeap;
        UINT                                                m_rtvDescriptorSize;
        D3D12_VIEWPORT                                      m_screenViewport;
        D3D12_RECT                                          m_scissorRect;

        // Direct3D properties.
        DXGI_FORMAT                                         m_backBufferFormat;
        DXGI_FORMAT                                         m_depthBufferFormat;
        UINT                                                m_backBufferCount;
        D3D_FEATURE_LEVEL                                   m_d3dMinFeatureLevel;

        // Cached device properties.
        HWND                                                m_window;
        D3D_FEATURE_LEVEL                                   m_d3dFeatureLevel;
        DWORD                                               m_dxgiFactoryFlags;
        RECT                                                m_outputSize;

        // HDR Support
        DXGI_COLOR_SPACE_TYPE                               m_colorSpace;

        // DeviceResources options (see flags above)
        unsigned int                                        m_options;

        // The IDeviceNotify can be held directly as it owns the DeviceResources.
        IDeviceNotify*                                      m_deviceNotify;
    };

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

    /// <summary>
    /// Thin frame-level wrapper over DeviceResources exposing only plain types,
    /// so RenderingSystem never handles Direct3D types directly.
    /// </summary>
    class GpuRenderer final : public IDeviceNotify
    {
    public:
        using DeviceCallback = std::function<void()>;

        // sRGB render target views over a UNORM swap chain: shaders write linear colors (glTF
        // materials are linear) and the hardware encodes them for display
        GpuRenderer() : m_deviceResources(std::make_unique<DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))
        {
            m_deviceResources->RegisterDeviceNotify(this);
        }

        ~GpuRenderer()
        {
            m_deviceResources->WaitForGpu();
        }

        GpuRenderer(GpuRenderer const&) = delete;
        GpuRenderer& operator= (GpuRenderer const&) = delete;

        // Creates the device and swap chain for the given native window (HWND). Throws on failure.
        void Initialize(void* nativeWindow, int width, int height)
        {
            m_deviceResources->SetWindow(static_cast<HWND>(nativeWindow), width, height);

            m_deviceResources->CreateDeviceResources();
            CreateDeviceDependentResources();

            m_deviceResources->CreateWindowSizeDependentResources();
        }

        // Resets the command list and clears the back buffer, depth buffer, viewport and scissor.
        void BeginFrame(float r, float g, float b, float a)
        {
            m_deviceResources->Prepare();

            auto commandList = m_deviceResources->GetCommandList();

            const auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
            const auto dsvDescriptor = m_deviceResources->GetDepthStencilView();
            const float clearColor[4] = { r, g, b, a };

            commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
            commandList->ClearRenderTargetView(rtvDescriptor, clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            const auto viewport = m_deviceResources->GetScreenViewport();
            const auto scissorRect = m_deviceResources->GetScissorRect();
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissorRect);
        }

        // Camera for the world pass: right-handed look-at and perspective projection, matching
        // IsometricCamera::WorldToScreen. Call after BeginFrame.
        void SetCamera(float eyeX, float eyeY, float eyeZ, float targetX, float targetY, float targetZ,
                       float verticalFov, float nearPlane, float farPlane)
        {
            if (!m_effect) return;

            const auto size = m_deviceResources->GetOutputSize();
            const float width = static_cast<float>(std::max(1l, size.right - size.left));
            const float height = static_cast<float>(std::max(1l, size.bottom - size.top));

            using namespace DirectX;
            const XMVECTOR eye = XMVectorSet(eyeX, eyeY, eyeZ, 1.0f);
            const XMVECTOR target = XMVectorSet(targetX, targetY, targetZ, 1.0f);
            if (XMVector3Equal(eye, target)) return;

            const XMMATRIX view = XMMatrixLookAtRH(eye, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
            const XMMATRIX projection = XMMatrixPerspectiveFovRH(verticalFov, width / height, nearPlane, farPlane);
            m_effect->SetView(view);
            m_effect->SetProjection(projection);
            XMStoreFloat4x4(&m_frameConstants.viewProjection, view * projection);
        }

        // Draw a unit primitive (0 box, 1 cylinder, 2 sphere, 3 cone) scaled, turned around Y,
        // and moved to a world position, lit with the given diffuse color. Call after SetCamera.
        void DrawMesh(int shape, float x, float y, float z, float sizeX, float sizeY, float sizeZ, float yaw,
                      float r, float g, float b, float a, float emissiveR = 0.0f, float emissiveG = 0.0f, float emissiveB = 0.0f)
        {
            if (!m_effect || shape < 0 || shape >= static_cast<int>(std::size(m_shapes)) || !m_shapes[shape]) return;

            using namespace DirectX;
            const XMMATRIX world = XMMatrixScaling(sizeX, sizeY, sizeZ) * XMMatrixRotationY(yaw) * XMMatrixTranslation(x, y, z);

            auto commandList = m_deviceResources->GetCommandList();
            m_effect->SetWorld(world);
            m_effect->SetDiffuseColor(XMVectorSet(r, g, b, 1.0f));
            m_effect->SetEmissiveColor(XMVectorSet(emissiveR, emissiveG, emissiveB, 1.0f));
            m_effect->SetAlpha(a);
            m_effect->Apply(commandList);
            m_shapes[shape]->Draw(commandList);
        }

        // Keep a model and upload its parts and textures to GPU resources; returns its index for
        // DrawModelInstances. The CPU data is kept so the model can be uploaded again after a
        // device loss.
        std::size_t AddModel(std::shared_ptr<const ModelData> model)
        {
            m_models.push_back(GpuModel{ std::move(model), {}, {} });
            if (m_effect)
                UploadModel(m_models.back());
            return m_models.size() - 1;
        }

        // Which pass an instanced model draw belongs to
        enum class ModelPass
        {
            Opaque,        // depth-tested and written, no blending
            DepthOnly,     // faded instances, first: depth only, so each shows its nearest surface
            Transparent    // faded instances, second: premultiplied alpha blend where depth matches
        };

        // Joint matrices (world space) of every skinned model drawn this frame, indexed by the
        // instances' palette offsets. Call after BeginFrame, before BeginModelPass.
        void SetSkinPalette(const Matrix4* matrices, std::size_t count)
        {
            if (!m_graphicsMemory) return;

            // Like the instance stream, each matrix is stored as its first three columns (rows of
            // the transpose); at least one entry so the root SRV always points at valid memory
            const std::size_t entries = std::max<std::size_t>(count, 1);
            auto palette = m_graphicsMemory->Allocate(entries * sizeof(PaletteEntry), 16);
            auto* data = static_cast<PaletteEntry*>(palette.Memory());
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto& m = matrices[i].m;
                for (std::size_t column = 0; column < 3; ++column)
                    data[i].columns[column] = DirectX::XMFLOAT4(m[column], m[4 + column], m[8 + column], m[12 + column]);
            }
            if (count == 0)
                data[0] = PaletteEntry{};
            m_paletteAddress = palette.GpuAddress();
        }

        // Bind the model pipeline's root signature, texture heap, per-frame constants, and the
        // skin palette. Call after SetCamera and SetSkinPalette, before the DrawModelInstances calls.
        void BeginModelPass()
        {
            if (!m_modelRootSignature || !m_graphicsMemory || !m_textureHeap) return;
            if (m_paletteAddress == 0)
                SetSkinPalette(nullptr, 0);

            auto frame = m_graphicsMemory->Allocate(sizeof(FrameConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            std::memcpy(frame.Memory(), &m_frameConstants, sizeof(FrameConstants));

            auto commandList = m_deviceResources->GetCommandList();
            ID3D12DescriptorHeap* heaps[] = { m_textureHeap->Heap() };
            commandList->SetDescriptorHeaps(1, heaps);
            commandList->SetGraphicsRootSignature(m_modelRootSignature.Get());
            commandList->SetGraphicsRootConstantBufferView(RootFrame, frame.GpuAddress());
            commandList->SetGraphicsRootShaderResourceView(RootPalette, m_paletteAddress);
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_boundPipeline = nullptr;
        }

        // Draw every copy of a model with one instanced draw per part. Rigid parts are placed by
        // their part transform (after the copy's part pose, if any) and the copy's world matrix;
        // skinned parts by the copy's joint matrices in the skin palette.
        void DrawModelInstances(std::size_t index, const ModelInstance* instances, std::size_t count, ModelPass pass)
        {
            if (!m_modelRootSignature || !m_graphicsMemory || index >= m_models.size() || count == 0) return;
            const auto& model = m_models[index];
            if (model.parts.size() != model.data->parts.size()) return;

            using namespace DirectX;
            auto commandList = m_deviceResources->GetCommandList();

            const auto toXm = [](const Matrix4& matrix)
            {
                XMFLOAT4X4 stored;
                std::copy(matrix.m.begin(), matrix.m.end(), &stored._11);
                return XMLoadFloat4x4(&stored);
            };

            std::vector<XMMATRIX> worlds;
            worlds.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
                worlds.push_back(toXm(instances[i].world));

            for (std::size_t p = 0; p < model.parts.size(); ++p)
            {
                const auto& part = model.data->parts[p];
                const auto& gpu = model.parts[p];
                const auto& material = model.data->materials[part.materialIndex];
                const bool skinned = part.IsSkinned();

                auto* pipeline = m_modelPipelines[skinned ? 1 : 0][static_cast<int>(pass)].Get();
                if (pipeline != m_boundPipeline)
                {
                    commandList->SetPipelineState(pipeline);
                    m_boundPipeline = pipeline;
                }

                MaterialConstants constants{};
                constants.baseColor = XMFLOAT4(material.baseColor[0], material.baseColor[1], material.baseColor[2], material.baseColor[3]);
                constants.emissive = XMFLOAT4(material.emissive[0], material.emissive[1], material.emissive[2], 0.0f);
                commandList->SetGraphicsRoot32BitConstants(RootMaterial, sizeof(MaterialConstants) / 4, &constants, 0);
                commandList->SetGraphicsRootDescriptorTable(RootTexture, m_textureHeap->GetGpuHandle(model.materialTextures[part.materialIndex]));

                const XMMATRIX local = toXm(part.transform);
                auto buffer = m_graphicsMemory->Allocate(count * sizeof(InstanceData), 16);
                auto* data = static_cast<InstanceData*>(buffer.Memory());
                for (std::size_t i = 0; i < count; ++i)
                {
                    const auto& instance = instances[i];
                    XMMATRIX partToModel = local;
                    if (instance.partPoses)
                    {
                        for (const auto& pose : *instance.partPoses)
                        {
                            if (pose.part == p)
                                partToModel = toXm(pose.transform) * partToModel;
                        }
                    }

                    // Store the first three columns of the row-vector matrix as rows (see the shader)
                    const XMMATRIX world = XMMatrixTranspose(partToModel * worlds[i]);
                    XMStoreFloat4(&data[i].world[0], world.r[0]);
                    XMStoreFloat4(&data[i].world[1], world.r[1]);
                    XMStoreFloat4(&data[i].world[2], world.r[2]);
                    data[i].tint = XMFLOAT4(instance.tint[0], instance.tint[1], instance.tint[2], instance.tint[3]);
                    data[i].paletteOffset = instance.paletteOffset;
                }

                D3D12_VERTEX_BUFFER_VIEW views[3] = { gpu.vertexView };
                UINT viewCount = 0;
                views[viewCount++] = gpu.vertexView;
                if (skinned)
                    views[viewCount++] = gpu.skinView;
                views[viewCount].BufferLocation = buffer.GpuAddress();
                views[viewCount].SizeInBytes = static_cast<UINT>(count * sizeof(InstanceData));
                views[viewCount].StrideInBytes = sizeof(InstanceData);
                ++viewCount;
                commandList->IASetVertexBuffers(0, viewCount, views);
                commandList->IASetIndexBuffer(&gpu.indexView);
                commandList->DrawIndexedInstanced(gpu.indexCount, static_cast<UINT>(count), 0, 0, 0);
            }
        }

        // Keep a font and upload its atlas; it is uploaded again after a device loss
        void AddFont(std::shared_ptr<const FontData> font)
        {
            m_fonts.push_back(std::move(font));
            if (m_textureHeap)
                UploadTextures({ &m_fonts.back()->atlas });
        }

        // Draw the 2D overlay: textured, alpha-blended triangles in pixels over the whole frame
        // (six vertices per quad), one draw per run of the same atlas. Call after the world pass.
        void DrawOverlay(const OverlayVertex* vertices, const std::vector<OverlayBatch>& batches)
        {
            std::size_t count = 0;
            for (const auto& batch : batches)
                count = std::max<std::size_t>(count, batch.first + batch.count);
            if (!m_overlayPipeline || !m_graphicsMemory || !m_textureHeap || count == 0) return;

            auto buffer = m_graphicsMemory->Allocate(count * sizeof(OverlayVertex), 16);
            std::memcpy(buffer.Memory(), vertices, count * sizeof(OverlayVertex));

            const auto size = m_deviceResources->GetOutputSize();
            const float screen[2] = { static_cast<float>(std::max(1l, size.right - size.left)), static_cast<float>(std::max(1l, size.bottom - size.top)) };

            auto commandList = m_deviceResources->GetCommandList();
            ID3D12DescriptorHeap* heaps[] = { m_textureHeap->Heap() };
            commandList->SetDescriptorHeaps(1, heaps);
            commandList->SetGraphicsRootSignature(m_overlayRootSignature.Get());
            commandList->SetGraphicsRoot32BitConstants(0, 2, screen, 0);
            commandList->SetPipelineState(m_overlayPipeline.Get());
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            D3D12_VERTEX_BUFFER_VIEW view{};
            view.BufferLocation = buffer.GpuAddress();
            view.SizeInBytes = static_cast<UINT>(count * sizeof(OverlayVertex));
            view.StrideInBytes = sizeof(OverlayVertex);
            commandList->IASetVertexBuffers(0, 1, &view);
            for (const auto& batch : batches)
            {
                const auto texture = m_textures.find(batch.atlas ? batch.atlas : &FontTexture());
                if (texture == m_textures.end() || batch.count == 0) continue;
                commandList->SetGraphicsRootDescriptorTable(1, m_textureHeap->GetGpuHandle(texture->second.descriptor));
                commandList->DrawInstanced(batch.count, 1, batch.first, 0);
            }
            m_boundPipeline = nullptr;
        }

        // Submits the frame's command list and presents the back buffer.
        void EndFrame()
        {
            m_deviceResources->Present();
            m_paletteAddress = 0;

            // Recycle per-frame constant buffer memory once the GPU is done with it
            if (m_graphicsMemory)
                m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
        }

        // Returns true if the swap chain was resized.
        bool OnWindowSizeChanged(int width, int height)
        {
            return m_deviceResources->WindowSizeChanged(width, height);
        }

        void OnWindowMoved()
        {
            const auto r = m_deviceResources->GetOutputSize();
            m_deviceResources->WindowSizeChanged(r.right, r.bottom);
        }

        void OnDisplayChange()
        {
            m_deviceResources->UpdateColorSpace();
        }

        void WaitForGpu() noexcept
        {
            m_deviceResources->WaitForGpu();
        }

        void SetDeviceCallbacks(DeviceCallback onLost, DeviceCallback onRestored)
        {
            m_onDeviceLost = std::move(onLost);
            m_onDeviceRestored = std::move(onRestored);
        }

        // IDeviceNotify
        void OnDeviceLost() override
        {
            for (auto& model : m_models)
            {
                model.parts.clear();
                model.materialTextures.clear();
            }
            m_textures.clear();
            m_textureHeap.reset();
            for (auto& pipelines : m_modelPipelines)
                for (auto& pipeline : pipelines)
                    pipeline.Reset();
            m_modelRootSignature.Reset();
            m_overlayPipeline.Reset();
            m_overlayRootSignature.Reset();
            m_boundPipeline = nullptr;
            m_paletteAddress = 0;
            for (auto& shape : m_shapes)
                shape.reset();
            m_effect.reset();
            m_graphicsMemory.reset();

            if (m_onDeviceLost)
                m_onDeviceLost();
        }

        void OnDeviceRestored() override
        {
            CreateDeviceDependentResources();

            if (m_onDeviceRestored)
                m_onDeviceRestored();
        }

    private:
        // These are the resources that depend on the device.
        void CreateDeviceDependentResources()
        {
            auto device = m_deviceResources->GetD3DDevice();

            // Check Shader Model 6 support
            D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
            if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
                || (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0))
            {
#ifdef _DEBUG
                OutputDebugStringA("ERROR: Shader Model 6.0 is not supported!\n");
#endif
                throw std::runtime_error("Shader Model 6.0 is not supported!");
            }

            using namespace DirectX;
            m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

            const RenderTargetState renderTargetState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());
            const EffectPipelineStateDescription pipeline(
                &GeometricPrimitive::VertexType::InputLayout,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                CommonStates::CullCounterClockwise,
                renderTargetState);

            m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::PerPixelLighting, pipeline);
            // DirectXTK's default three-light rig, dimmed for a dungeon: the lights add up, and the
            // sRGB target shows linear values brighter than a UNORM one did. Almost no specular, so
            // the flat-colored KayKit materials read as stone and cloth rather than plastic.
            // The model shader uses the same lights (no specular there; almost none here)
            m_effect->EnableDefaultLighting();
            m_effect->SetSpecularColor(XMVectorSet(0.04f, 0.04f, 0.04f, 1.0f));
            ApplyEffectLighting();

            CreateModelPipelines(renderTargetState);
            CreateOverlayPipeline(renderTargetState);

            // Shader-visible texture descriptors; slot 0 is a white texel for untextured materials,
            // slot 1 the overlay's font atlas
            m_textureHeap = std::make_unique<DescriptorHeap>(device, MaxTextures);
            m_textures.clear();
            UploadTextures({ &WhiteTexture(), &FontTexture() });
            for (const auto& font : m_fonts)
                UploadTextures({ &font->atlas });

            // Unit shapes centered on the origin; scaled per draw
            m_shapes[0] = GeometricPrimitive::CreateBox(XMFLOAT3(1.0f, 1.0f, 1.0f));
            m_shapes[1] = GeometricPrimitive::CreateCylinder(1.0f, 1.0f);
            m_shapes[2] = GeometricPrimitive::CreateSphere(1.0f);
            m_shapes[3] = GeometricPrimitive::CreateCone(1.0f, 1.0f);

            // Models added before (or kept across) a device loss
            for (auto& model : m_models)
                UploadModel(model);
        }

    public:
        // Point lights the model shader takes per frame (ModelInstanced.hlsli: MaxPointLights)
        static constexpr std::size_t MaxPointLights = 16;

        // Scale the directional lights and set the ambient light, for the primitives' effect and
        // the model shader alike (e.g. a dark dungeon lit mostly by torches)
        void SetLighting(float directional, float ambientR, float ambientG, float ambientB)
        {
            const FrameConstants defaults{};
            for (int i = 0; i < 3; ++i)
            {
                const auto& base = defaults.lightColor[i];
                m_frameConstants.lightColor[i] = DirectX::XMFLOAT4(base.x * directional, base.y * directional, base.z * directional, 1.0f);
            }
            m_frameConstants.ambientColor = DirectX::XMFLOAT4(ambientR, ambientG, ambientB, 1.0f);
            ApplyEffectLighting();
        }

        // This frame's point lights: x, y, z, radius, r, g, b (color times intensity) each
        void SetPointLights(const float* lights, std::size_t count)
        {
            count = std::min(count, MaxPointLights);
            for (std::size_t i = 0; i < count; ++i)
            {
                const float* light = lights + i * 7;
                m_frameConstants.pointLightPosition[i] = DirectX::XMFLOAT4(light[0], light[1], light[2], light[3]);
                m_frameConstants.pointLightColor[i] = DirectX::XMFLOAT4(light[4], light[5], light[6], 1.0f);
            }
            m_frameConstants.pointLightCount = DirectX::XMFLOAT4(static_cast<float>(count), 0.0f, 0.0f, 0.0f);
        }

    private:
        void ApplyEffectLighting()
        {
            if (!m_effect) return;
            using namespace DirectX;
            m_effect->SetAmbientLightColor(XMLoadFloat4(&m_frameConstants.ambientColor));
            for (int i = 0; i < 3; ++i)
            {
                m_effect->SetLightDirection(i, XMLoadFloat4(&m_frameConstants.lightDirection[i]));
                m_effect->SetLightDiffuseColor(i, XMLoadFloat4(&m_frameConstants.lightColor[i]));
            }
        }

        // Per-frame constants of the model shader (ModelInstanced.hlsli: cbuffer Frame)
        struct FrameConstants
        {
            DirectX::XMFLOAT4X4 viewProjection{};
            // DirectXTK's default three-light rig, dimmed for a dungeon: the lights add up, and the
            // sRGB target shows linear values brighter than a UNORM one did
            DirectX::XMFLOAT4 lightDirection[3] = {
                { -0.5265408f, -0.5735765f, -0.6275069f, 0.0f },
                {  0.7198464f,  0.3420201f,  0.6040227f, 0.0f },
                {  0.4545195f, -0.7660444f,  0.4545195f, 0.0f } };
            DirectX::XMFLOAT4 lightColor[3] = {
                { 0.75f, 0.70f, 0.62f, 1.0f },     // key: warm, torch-like
                { 0.12f, 0.13f, 0.18f, 1.0f },     // fill: cool
                { 0.10f, 0.10f, 0.12f, 1.0f } };   // back
            DirectX::XMFLOAT4 ambientColor{ 0.03f, 0.03f, 0.04f, 1.0f };
            DirectX::XMFLOAT4 pointLightPosition[MaxPointLights]{};
            DirectX::XMFLOAT4 pointLightColor[MaxPointLights]{};
            DirectX::XMFLOAT4 pointLightCount{};
        };

        // Per-part root constants (cbuffer Material)
        struct MaterialConstants
        {
            DirectX::XMFLOAT4 baseColor;
            DirectX::XMFLOAT4 emissive;
        };

        // Per-instance vertex stream of the model shaders (WORLD0..2, COLOR0, PALETTE)
        struct InstanceData
        {
            DirectX::XMFLOAT4 world[3];
            DirectX::XMFLOAT4 tint;
            std::uint32_t paletteOffset;
            std::uint32_t padding[3];
        };

        // One joint matrix of the skin palette (StructuredBuffer<Joint> in the skinned shader)
        struct PaletteEntry
        {
            DirectX::XMFLOAT4 columns[3]{};
        };

        // Root parameters of the model pipelines
        static constexpr UINT RootFrame = 0;       // b0: cbuffer Frame
        static constexpr UINT RootMaterial = 1;    // b1: cbuffer Material (root constants)
        static constexpr UINT RootTexture = 2;     // t0: base color texture (descriptor table)
        static constexpr UINT RootPalette = 3;     // t1: skin palette (root SRV)

        // Distinct images the texture heap can hold (the KayKit sets share one atlas per pack)
        static constexpr std::size_t MaxTextures = 256;

        void CreateModelPipelines(const DirectX::RenderTargetState& renderTargetState)
        {
            using namespace DirectX;
            auto device = m_deviceResources->GetD3DDevice();

            const CD3DX12_DESCRIPTOR_RANGE textureRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            CD3DX12_ROOT_PARAMETER parameters[4];
            parameters[RootFrame].InitAsConstantBufferView(0);
            parameters[RootMaterial].InitAsConstants(sizeof(MaterialConstants) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
            parameters[RootTexture].InitAsDescriptorTable(1, &textureRange, D3D12_SHADER_VISIBILITY_PIXEL);
            parameters[RootPalette].InitAsShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);

            // Trilinear, anisotropic, wrapping (glTF's default sampler)
            CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_ANISOTROPIC);
            sampler.MaxAnisotropy = 8;
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
                IID_PPV_ARGS(m_modelRootSignature.ReleaseAndGetAddressOf())));

            // Rigid parts: vertices (slot 0) and instances (slot 1)
            static constexpr D3D12_INPUT_ELEMENT_DESC rigidElements[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "PALETTE", 0, DXGI_FORMAT_R32_UINT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            };

            // Skinned parts: vertices (slot 0), joints and weights (slot 1), and instances (slot 2)
            static constexpr D3D12_INPUT_ELEMENT_DESC skinnedElements[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
                { "PALETTE", 0, DXGI_FORMAT_R32_UINT, 2, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            };
            static_assert(sizeof(InstanceData) == 80);
            static_assert(sizeof(SkinVertex) == 24);

            std::size_t psSize = 0;
            const auto* ps = LBShaders::ModelInstancedPS(psSize);

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
            desc.pRootSignature = m_modelRootSignature.Get();
            desc.PS = { ps, psSize };
            desc.SampleMask = UINT_MAX;
            desc.RasterizerState = CommonStates::CullCounterClockwise;   // clockwise front faces
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets = 1;
            desc.RTVFormats[0] = renderTargetState.rtvFormats[0];
            desc.DSVFormat = renderTargetState.dsvFormat;
            desc.SampleDesc.Count = 1;

            D3D12_BLEND_DESC depthOnly = CommonStates::Opaque;
            depthOnly.RenderTarget[0].RenderTargetWriteMask = 0;

            for (int skinned = 0; skinned < 2; ++skinned)
            {
                std::size_t vsSize = 0;
                const auto* vs = skinned ? LBShaders::ModelSkinnedVS(vsSize) : LBShaders::ModelInstancedVS(vsSize);
                desc.VS = { vs, vsSize };
                desc.InputLayout = skinned
                    ? D3D12_INPUT_LAYOUT_DESC{ skinnedElements, static_cast<UINT>(std::size(skinnedElements)) }
                    : D3D12_INPUT_LAYOUT_DESC{ rigidElements, static_cast<UINT>(std::size(rigidElements)) };

                const auto create = [&](ModelPass pass, const D3D12_BLEND_DESC& blend, const D3D12_DEPTH_STENCIL_DESC& depth)
                {
                    desc.BlendState = blend;
                    desc.DepthStencilState = depth;
                    ThrowIfFailed(device->CreateGraphicsPipelineState(&desc,
                        IID_PPV_ARGS(m_modelPipelines[skinned][static_cast<int>(pass)].ReleaseAndGetAddressOf())));
                };

                create(ModelPass::Opaque, CommonStates::Opaque, CommonStates::DepthDefault);
                create(ModelPass::DepthOnly, depthOnly, CommonStates::DepthDefault);
                create(ModelPass::Transparent, CommonStates::AlphaBlend, CommonStates::DepthRead);
            }
        }

        // The 2D overlay: screen size constants, the atlas, point sampling, straight alpha blending
        void CreateOverlayPipeline(const DirectX::RenderTargetState& renderTargetState)
        {
            using namespace DirectX;
            auto device = m_deviceResources->GetD3DDevice();

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
                IID_PPV_ARGS(m_overlayRootSignature.ReleaseAndGetAddressOf())));

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
            desc.pRootSignature = m_overlayRootSignature.Get();
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
            ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_overlayPipeline.ReleaseAndGetAddressOf())));
        }

        // GPU copies of a model's parts, in the same order as ModelData::parts
        struct GpuModelPart
        {
            ComPtr<ID3D12Resource> vertexBuffer;
            ComPtr<ID3D12Resource> skinBuffer;     // skinned parts only
            ComPtr<ID3D12Resource> indexBuffer;
            D3D12_VERTEX_BUFFER_VIEW vertexView{};
            D3D12_VERTEX_BUFFER_VIEW skinView{};
            D3D12_INDEX_BUFFER_VIEW indexView{};
            UINT indexCount{0};
        };

        struct GpuModel
        {
            std::shared_ptr<const ModelData> data;
            std::vector<GpuModelPart> parts;
            std::vector<std::size_t> materialTextures;   // texture heap slot per material (0: white)
        };

        // A texture in the heap
        struct GpuTexture
        {
            ComPtr<ID3D12Resource> resource;
            std::size_t descriptor{0};
        };

        static const ModelTexture& WhiteTexture()
        {
            static const ModelTexture white{ { ModelTexture::MipLevel{ 1, 1, { 255, 255, 255, 255 } } } };
            return white;
        }

        // The overlay's font atlas (BitmapFont): one level, sampled with point filtering
        static const ModelTexture& FontTexture()
        {
            static const ModelTexture font{ { ModelTexture::MipLevel{
                static_cast<std::uint32_t>(BitmapFont::AtlasWidth), static_cast<std::uint32_t>(BitmapFont::AtlasHeight), BitmapFont::BuildAtlas() } } };
            return font;
        }

        // ModelVertex must match the input layout shared with GeometricPrimitive
        static_assert(sizeof(ModelVertex) == sizeof(DirectX::VertexPositionNormalTexture));

        // Record work on a one-off command list, run it, and wait for the GPU (models are loaded
        // at startup). ResourceUploadBatch is avoided on purpose: its <future> include breaks
        // `import std` in the units importing this partition (MSVC C1116).
        template<typename Record>
        void RunAndWait(const Record& record)
        {
            auto device = m_deviceResources->GetD3DDevice();
            ComPtr<ID3D12CommandAllocator> allocator;
            ComPtr<ID3D12GraphicsCommandList> commandList;
            ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.GetAddressOf())));
            ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf())));
            record(commandList.Get());
            ThrowIfFailed(commandList->Close());

            auto queue = m_deviceResources->GetCommandQueue();
            ID3D12CommandList* lists[] = { commandList.Get() };
            queue->ExecuteCommandLists(1, lists);

            ComPtr<ID3D12Fence> fence;
            ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
            ThrowIfFailed(queue->Signal(fence.Get(), 1));
            if (fence->GetCompletedValue() < 1)
            {
                Microsoft::WRL::Wrappers::Event done(CreateEventExW(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
                if (!done.IsValid()) throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEventExW");
                ThrowIfFailed(fence->SetEventOnCompletion(1, done.Get()));
                std::ignore = WaitForSingleObjectEx(done.Get(), INFINITE, FALSE);
            }
        }

        // An upload-heap buffer filled through a mapped pointer
        ComPtr<ID3D12Resource> CreateStagingBuffer(UINT64 size, std::uint8_t*& mapped)
        {
            auto device = m_deviceResources->GetD3DDevice();
            ComPtr<ID3D12Resource> staging;
            const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
            const auto desc = CD3DX12_RESOURCE_DESC::Buffer(std::max<UINT64>(size, 1));
            ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(staging.GetAddressOf())));
            const CD3DX12_RANGE noRead(0, 0);
            ThrowIfFailed(staging->Map(0, &noRead, reinterpret_cast<void**>(&mapped)));
            return staging;
        }

        // Upload textures not on the GPU yet (sRGB, every mip level) and give each a heap slot
        void UploadTextures(const std::vector<const ModelTexture*>& textures)
        {
            auto device = m_deviceResources->GetD3DDevice();

            struct Pending { const ModelTexture* texture; ComPtr<ID3D12Resource> resource; ComPtr<ID3D12Resource> staging; std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints; };
            std::vector<Pending> pending;
            for (const auto* texture : textures)
            {
                if (m_textures.contains(texture) || std::ranges::any_of(pending, [&](const Pending& p) { return p.texture == texture; }))
                    continue;
                if (m_textures.size() + pending.size() >= MaxTextures)
                    throw std::runtime_error("too many distinct textures");

                Pending upload{ texture };
                const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, texture->Width(), texture->Height(), 1,
                    static_cast<UINT16>(texture->mips.size()));
                const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
                ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(upload.resource.GetAddressOf())));

                // Staging rows follow the copyable footprint (aligned pitch and offsets)
                const auto mipCount = static_cast<UINT>(texture->mips.size());
                upload.footprints.resize(mipCount);
                std::vector<UINT> rows(mipCount);
                std::vector<UINT64> rowBytes(mipCount);
                UINT64 total = 0;
                device->GetCopyableFootprints(&desc, 0, mipCount, 0, upload.footprints.data(), rows.data(), rowBytes.data(), &total);

                std::uint8_t* mapped = nullptr;
                upload.staging = CreateStagingBuffer(total, mapped);
                for (UINT mip = 0; mip < mipCount; ++mip)
                {
                    const auto& level = texture->mips[mip];
                    const auto& footprint = upload.footprints[mip];
                    for (UINT row = 0; row < rows[mip]; ++row)
                    {
                        std::memcpy(mapped + footprint.Offset + UINT64(row) * footprint.Footprint.RowPitch,
                                    level.pixels.data() + std::size_t{ row } * level.width * 4, static_cast<std::size_t>(rowBytes[mip]));
                    }
                }
                upload.staging->Unmap(0, nullptr);
                pending.push_back(std::move(upload));
            }
            if (pending.empty())
                return;

            RunAndWait([&](ID3D12GraphicsCommandList* commandList)
            {
                std::vector<D3D12_RESOURCE_BARRIER> barriers;
                for (const auto& upload : pending)
                {
                    for (UINT mip = 0; mip < upload.footprints.size(); ++mip)
                    {
                        const CD3DX12_TEXTURE_COPY_LOCATION destination(upload.resource.Get(), mip);
                        const CD3DX12_TEXTURE_COPY_LOCATION source(upload.staging.Get(), upload.footprints[mip]);
                        commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
                    }
                    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(upload.resource.Get(),
                        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
                }
                commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
            });

            for (auto& upload : pending)
            {
                const std::size_t slot = m_textures.size();
                D3D12_SHADER_RESOURCE_VIEW_DESC view = {};
                view.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                view.Texture2D.MipLevels = static_cast<UINT>(upload.texture->mips.size());
                device->CreateShaderResourceView(upload.resource.Get(), &view, m_textureHeap->GetCpuHandle(slot));
                m_textures.emplace(upload.texture, GpuTexture{ std::move(upload.resource), slot });
            }
        }

        // Copy every part into default-heap vertex, skin, and index buffers through one staging
        // buffer, upload the model's textures, and wait for the copies.
        void UploadModel(GpuModel& model)
        {
            auto device = m_deviceResources->GetD3DDevice();

            std::vector<const ModelTexture*> textures;
            for (const auto& texture : model.data->textures)
                textures.push_back(texture.get());
            UploadTextures(textures);

            std::vector<std::size_t> materialTextures;
            for (const auto& material : model.data->materials)
            {
                materialTextures.push_back(material.baseColorTexture
                    ? m_textures.at(model.data->textures[*material.baseColorTexture].get()).descriptor
                    : m_textures.at(&WhiteTexture()).descriptor);
            }

            struct Pending { const void* data; UINT64 size; ID3D12Resource* target; D3D12_RESOURCE_STATES state; UINT64 offset; };
            std::vector<GpuModelPart> parts(model.data->parts.size());
            std::vector<Pending> copies;
            UINT64 stagingSize = 0;

            const auto createBuffer = [&](UINT64 size, ComPtr<ID3D12Resource>& buffer)
            {
                const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
                const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
                ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf())));
            };
            const auto align = [](UINT64 value) { return (value + 255) & ~UINT64(255); };
            const auto queueCopy = [&](const void* data, UINT64 size, ComPtr<ID3D12Resource>& buffer, D3D12_RESOURCE_STATES state)
            {
                createBuffer(size, buffer);
                copies.push_back({ data, size, buffer.Get(), state, stagingSize });
                stagingSize = align(stagingSize + size);
            };

            for (std::size_t i = 0; i < parts.size(); ++i)
            {
                const auto& part = model.data->parts[i];
                auto& gpu = parts[i];
                const UINT64 vertexBytes = part.vertices.size() * sizeof(ModelVertex);
                const UINT64 indexBytes = part.indices.size() * sizeof(std::uint32_t);
                queueCopy(part.vertices.data(), vertexBytes, gpu.vertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
                queueCopy(part.indices.data(), indexBytes, gpu.indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

                gpu.vertexView.BufferLocation = gpu.vertexBuffer->GetGPUVirtualAddress();
                gpu.vertexView.SizeInBytes = static_cast<UINT>(vertexBytes);
                gpu.vertexView.StrideInBytes = sizeof(ModelVertex);
                gpu.indexView.BufferLocation = gpu.indexBuffer->GetGPUVirtualAddress();
                gpu.indexView.SizeInBytes = static_cast<UINT>(indexBytes);
                gpu.indexView.Format = DXGI_FORMAT_R32_UINT;
                gpu.indexCount = static_cast<UINT>(part.indices.size());

                if (part.IsSkinned())
                {
                    const UINT64 skinBytes = part.skin.size() * sizeof(SkinVertex);
                    queueCopy(part.skin.data(), skinBytes, gpu.skinBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
                    gpu.skinView.BufferLocation = gpu.skinBuffer->GetGPUVirtualAddress();
                    gpu.skinView.SizeInBytes = static_cast<UINT>(skinBytes);
                    gpu.skinView.StrideInBytes = sizeof(SkinVertex);
                }
            }

            // Staging buffer in the upload heap holding everything
            std::uint8_t* mapped = nullptr;
            const auto staging = CreateStagingBuffer(stagingSize, mapped);
            for (const auto& copy : copies)
                std::memcpy(mapped + copy.offset, copy.data, static_cast<std::size_t>(copy.size));
            staging->Unmap(0, nullptr);

            // Record the copies and the transitions to their final states
            RunAndWait([&](ID3D12GraphicsCommandList* commandList)
            {
                std::vector<D3D12_RESOURCE_BARRIER> barriers;
                for (const auto& copy : copies)
                {
                    commandList->CopyBufferRegion(copy.target, 0, staging.Get(), copy.offset, copy.size);
                    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(copy.target, D3D12_RESOURCE_STATE_COPY_DEST, copy.state));
                }
                commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
            });

            model.parts = std::move(parts);
            model.materialTextures = std::move(materialTextures);
        }

        std::unique_ptr<DeviceResources> m_deviceResources;

        // Device-dependent DirectXTK12 objects. GraphicsMemory owns the memory the others
        // allocate from, so it is declared first (destroyed last).
        std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;
        std::unique_ptr<DirectX::BasicEffect> m_effect;
        std::unique_ptr<DirectX::GeometricPrimitive> m_shapes[4];

        std::vector<GpuModel> m_models;
        std::vector<std::shared_ptr<const FontData>> m_fonts;
        std::unique_ptr<DirectX::DescriptorHeap> m_textureHeap;
        std::unordered_map<const ModelTexture*, GpuTexture> m_textures;   // CPU textures stay alive with their models

        // Instanced model pipelines: [rigid, skinned][ModelPass]
        FrameConstants m_frameConstants;
        ComPtr<ID3D12RootSignature> m_modelRootSignature;
        ComPtr<ID3D12PipelineState> m_modelPipelines[2][3];
        ComPtr<ID3D12RootSignature> m_overlayRootSignature;
        ComPtr<ID3D12PipelineState> m_overlayPipeline;
        ID3D12PipelineState* m_boundPipeline{nullptr};
        D3D12_GPU_VIRTUAL_ADDRESS m_paletteAddress{0};
        DeviceCallback m_onDeviceLost;
        DeviceCallback m_onDeviceRestored;
    };
}

// ---------------------------------------------------------------------------
// DeviceResources implementation
// ---------------------------------------------------------------------------

namespace
{
    inline DXGI_FORMAT NoSRGB(DXGI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:   return DXGI_FORMAT_B8G8R8X8_UNORM;
        default:                                return fmt;
        }
    }

    inline long ComputeIntersectionArea(
        long ax1, long ay1, long ax2, long ay2,
        long bx1, long by1, long bx2, long by2) noexcept
    {
        return std::max(0l, std::min(ax2, bx2) - std::max(ax1, bx1)) * std::max(0l, std::min(ay2, by2) - std::max(ay1, by1));
    }
}

namespace LB::Systems::Rendering::Detail
{
// Constructor for DeviceResources.
DeviceResources::DeviceResources(
    DXGI_FORMAT backBufferFormat,
    DXGI_FORMAT depthBufferFormat,
    UINT backBufferCount,
    D3D_FEATURE_LEVEL minFeatureLevel,
    unsigned int flags) noexcept(false) :
        m_backBufferIndex(0),
        m_fenceValues{},
        m_rtvDescriptorSize(0),
        m_screenViewport{},
        m_scissorRect{},
        m_backBufferFormat(backBufferFormat),
        m_depthBufferFormat(depthBufferFormat),
        m_backBufferCount(backBufferCount),
        m_d3dMinFeatureLevel(minFeatureLevel),
        m_window(nullptr),
        m_d3dFeatureLevel(D3D_FEATURE_LEVEL_11_0),
        m_dxgiFactoryFlags(0),
        m_outputSize{0, 0, 1, 1},
        m_colorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),
        m_options(flags),
        m_deviceNotify(nullptr)
{
    if (backBufferCount < 2 || backBufferCount > MAX_BACK_BUFFER_COUNT)
    {
        throw std::out_of_range("invalid backBufferCount");
    }

    if (minFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        throw std::out_of_range("minFeatureLevel too low");
    }
}

// Destructor for DeviceResources.
DeviceResources::~DeviceResources()
{
    // Ensure that the GPU is no longer referencing resources that are about to be destroyed.
    WaitForGpu();
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DeviceResources::CreateDeviceResources()
{
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    //
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            m_dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
        }
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));

    // Determines whether tearing support is available for fullscreen borderless windows.
    if (m_options & c_AllowTearing)
    {
        BOOL allowTearing = FALSE;
        HRESULT hr = m_dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        if (FAILED(hr) || !allowTearing)
        {
            m_options &= ~c_AllowTearing;
#ifdef _DEBUG
            OutputDebugStringA("WARNING: Variable refresh rate displays not supported");
#endif
        }
    }

    ComPtr<IDXGIAdapter1> adapter;
    GetAdapter(adapter.GetAddressOf());

    // Create the DX12 API device object.
    HRESULT hr = D3D12CreateDevice(
        adapter.Get(),
        m_d3dMinFeatureLevel,
        IID_PPV_ARGS(m_d3dDevice.ReleaseAndGetAddressOf())
        );
    ThrowIfFailed(hr);

    m_d3dDevice->SetName(L"DeviceResources");

#ifndef NDEBUG
    // Configure debug device (if active).
    ComPtr<ID3D12InfoQueue> d3dInfoQueue;
    if (SUCCEEDED(m_d3dDevice.As(&d3dInfoQueue)))
    {
#ifdef _DEBUG
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
        D3D12_MESSAGE_ID hide[] =
        {
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            // Workarounds for debug layer issues on hybrid-graphics systems
            D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
        };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
        filter.DenyList.pIDList = hide;
        d3dInfoQueue->AddStorageFilterEntries(&filter);
    }
#endif

    // Determine maximum supported feature level for this device
    static const D3D_FEATURE_LEVEL s_featureLevels[] =
    {
#if defined(NTDDI_WIN10_FE) || defined(USING_D3D12_AGILITY_SDK)
        D3D_FEATURE_LEVEL_12_2,
#endif
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
    {
        static_cast<UINT>(std::size(s_featureLevels)), s_featureLevels, D3D_FEATURE_LEVEL_11_0
    };

    hr = m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
    if (SUCCEEDED(hr))
    {
        m_d3dFeatureLevel = featLevels.MaxSupportedFeatureLevel;
    }
    else
    {
        m_d3dFeatureLevel = m_d3dMinFeatureLevel;
    }

    // Create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.ReleaseAndGetAddressOf())));

    m_commandQueue->SetName(L"DeviceResources");

    // Create descriptor heaps for render target views and depth stencil views.
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
    rtvDescriptorHeapDesc.NumDescriptors = m_backBufferCount;
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(m_rtvDescriptorHeap.ReleaseAndGetAddressOf())));

    m_rtvDescriptorHeap->SetName(L"DeviceResources");

    m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
        dsvDescriptorHeapDesc.NumDescriptors = 1;
        dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

        ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(m_dsvDescriptorHeap.ReleaseAndGetAddressOf())));

        m_dsvDescriptorHeap->SetName(L"DeviceResources");
    }

    // Create a command allocator for each back buffer that will be rendered to.
    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocators[n].ReleaseAndGetAddressOf())));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", n);
        m_commandAllocators[n]->SetName(name);
    }

    // Create a command list for recording graphics commands.
    ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
    ThrowIfFailed(m_commandList->Close());

    m_commandList->SetName(L"DeviceResources");

    // Create a fence for tracking GPU execution progress.
    ThrowIfFailed(m_d3dDevice->CreateFence(m_fenceValues[m_backBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));
    m_fenceValues[m_backBufferIndex]++;

    m_fence->SetName(L"DeviceResources");

    m_fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
    if (!m_fenceEvent.IsValid())
    {
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEventEx");
    }
}

// These resources need to be recreated every time the window size is changed.
void DeviceResources::CreateWindowSizeDependentResources()
{
    if (!m_window)
    {
        throw std::logic_error("Call SetWindow with a valid Win32 window handle");
    }

    // Wait until all previous GPU work is complete.
    WaitForGpu();

    // Release resources that are tied to the swap chain and update fence values.
    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        m_renderTargets[n].Reset();
        m_fenceValues[n] = m_fenceValues[m_backBufferIndex];
    }

    // Determine the render target size in pixels.
    const UINT backBufferWidth = std::max<UINT>(static_cast<UINT>(m_outputSize.right - m_outputSize.left), 1u);
    const UINT backBufferHeight = std::max<UINT>(static_cast<UINT>(m_outputSize.bottom - m_outputSize.top), 1u);
    const DXGI_FORMAT backBufferFormat = NoSRGB(m_backBufferFormat);

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        // If the swap chain already exists, resize it.
        HRESULT hr = m_swapChain->ResizeBuffers(
            m_backBufferCount,
            backBufferWidth,
            backBufferHeight,
            backBufferFormat,
            (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
            );

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
#ifdef _DEBUG
            char buff[64] = {};
            sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr));
            OutputDebugStringA(buff);
#endif
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            HandleDeviceLost();

            // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
            // and correctly set up the new device.
            return;
        }
        else
        {
            ThrowIfFailed(hr);
        }
    }
    else
    {
        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = m_backBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = (m_options & c_AllowTearing) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        // Create a swap chain for the window.
        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            m_window,
            &swapChainDesc,
            &fsSwapChainDesc,
            nullptr,
            swapChain.GetAddressOf()
            ));

        ThrowIfFailed(swapChain.As(&m_swapChain));

        // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
        ThrowIfFailed(m_dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // Handle color space settings for HDR
    UpdateColorSpace();

    // Obtain the back buffers for this window which will be the final render targets
    // and create render target views for each of them.
    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(m_renderTargets[n].GetAddressOf())));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", n);
        m_renderTargets[n]->SetName(name);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_backBufferFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        const auto cpuHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(cpuHandle, static_cast<INT>(n), m_rtvDescriptorSize);
        m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), &rtvDesc, rtvDescriptor);
    }

    // Reset the index to the current back buffer.
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
    {
        // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
        // on this surface.
        const CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            m_depthBufferFormat,
            backBufferWidth,
            backBufferHeight,
            1, // Use a single array entry.
            1  // Use a single mipmap level.
            );
        depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        const CD3DX12_CLEAR_VALUE depthOptimizedClearValue(m_depthBufferFormat, (m_options & c_ReverseDepth) ? 0.0f : 1.0f, 0u);

        ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
            &depthHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(m_depthStencil.ReleaseAndGetAddressOf())
            ));

        m_depthStencil->SetName(L"Depth stencil");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = m_depthBufferFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        const auto cpuHandle = m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_d3dDevice->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, cpuHandle);
    }

    // Set the 3D rendering viewport and scissor rectangle to target the entire window.
    m_screenViewport.TopLeftX = m_screenViewport.TopLeftY = 0.f;
    m_screenViewport.Width = static_cast<float>(backBufferWidth);
    m_screenViewport.Height = static_cast<float>(backBufferHeight);
    m_screenViewport.MinDepth = D3D12_MIN_DEPTH;
    m_screenViewport.MaxDepth = D3D12_MAX_DEPTH;

    m_scissorRect.left = m_scissorRect.top = 0;
    m_scissorRect.right = static_cast<LONG>(backBufferWidth);
    m_scissorRect.bottom = static_cast<LONG>(backBufferHeight);
}

// This method is called when the Win32 window is created (or re-created).
void DeviceResources::SetWindow(HWND window, int width, int height) noexcept
{
    m_window = window;

    m_outputSize.left = m_outputSize.top = 0;
    m_outputSize.right = static_cast<long>(width);
    m_outputSize.bottom = static_cast<long>(height);
}

// This method is called when the Win32 window changes size.
bool DeviceResources::WindowSizeChanged(int width, int height)
{
    if (!m_window)
        return false;

    RECT newRc;
    newRc.left = newRc.top = 0;
    newRc.right = static_cast<long>(width);
    newRc.bottom = static_cast<long>(height);
    if (newRc.right == m_outputSize.right && newRc.bottom == m_outputSize.bottom)
    {
        // Handle color space settings for HDR
        UpdateColorSpace();

        return false;
    }

    m_outputSize = newRc;
    CreateWindowSizeDependentResources();
    return true;
}

// Recreate all device resources and set them back to the current state.
void DeviceResources::HandleDeviceLost()
{
    if (m_deviceNotify)
    {
        m_deviceNotify->OnDeviceLost();
    }

    for (UINT n = 0; n < m_backBufferCount; n++)
    {
        m_commandAllocators[n].Reset();
        m_renderTargets[n].Reset();
    }

    m_depthStencil.Reset();
    m_commandQueue.Reset();
    m_commandList.Reset();
    m_fence.Reset();
    m_rtvDescriptorHeap.Reset();
    m_dsvDescriptorHeap.Reset();
    m_swapChain.Reset();
    m_d3dDevice.Reset();
    m_dxgiFactory.Reset();

#ifdef _DEBUG
    {
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
#endif

    CreateDeviceResources();
    CreateWindowSizeDependentResources();

    if (m_deviceNotify)
    {
        m_deviceNotify->OnDeviceRestored();
    }
}

// Prepare the command list and render target for rendering.
void DeviceResources::Prepare(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
    // Reset command list and allocator.
    ThrowIfFailed(m_commandAllocators[m_backBufferIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_backBufferIndex].Get(), nullptr));

    if (beforeState != afterState)
    {
        // Transition the render target into the correct state to allow for drawing into it.
        const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_backBufferIndex].Get(),
            beforeState, afterState);
        m_commandList->ResourceBarrier(1, &barrier);
    }
}

// Present the contents of the swap chain to the screen.
void DeviceResources::Present(D3D12_RESOURCE_STATES beforeState)
{
    if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
    {
        // Transition the render target to the state that allows it to be presented to the display.
        const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_backBufferIndex].Get(),
            beforeState, D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &barrier);
    }

    // Send the command list off to the GPU for processing.
    ThrowIfFailed(m_commandList->Close());
    m_commandQueue->ExecuteCommandLists(1, CommandListCast(m_commandList.GetAddressOf()));

    HRESULT hr;
    if (m_options & c_AllowTearing)
    {
        // Recommended to always use tearing if supported when using a sync interval of 0.
        // Note this will fail if in true 'fullscreen' mode.
        hr = m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    else
    {
        // The first argument instructs DXGI to block until VSync, putting the application
        // to sleep until the next VSync. This ensures we don't waste any cycles rendering
        // frames that will never be displayed to the screen.
        hr = m_swapChain->Present(1, 0);
    }

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
        char buff[64] = {};
        sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n",
            static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr));
        OutputDebugStringA(buff);
#endif
        HandleDeviceLost();
    }
    else
    {
        ThrowIfFailed(hr);

        MoveToNextFrame();

        if (!m_dxgiFactory->IsCurrent())
        {
            UpdateColorSpace();
        }
    }
}

// Wait for pending GPU work to complete.
void DeviceResources::WaitForGpu() noexcept
{
    if (m_commandQueue && m_fence && m_fenceEvent.IsValid())
    {
        // Schedule a Signal command in the GPU queue.
        const UINT64 fenceValue = m_fenceValues[m_backBufferIndex];
        if (SUCCEEDED(m_commandQueue->Signal(m_fence.Get(), fenceValue)))
        {
            // Wait until the Signal has been processed.
            if (SUCCEEDED(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent.Get())))
            {
                std::ignore = WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);

                // Increment the fence value for the current frame.
                m_fenceValues[m_backBufferIndex]++;
            }
        }
    }
}

// Prepare to render the next frame.
void DeviceResources::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_backBufferIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the back buffer index.
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_backBufferIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent.Get()));
        std::ignore = WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_backBufferIndex] = currentFenceValue + 1;
}

// This method acquires the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, try WARP. Otherwise throw an exception.
void DeviceResources::GetAdapter(IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0;
        SUCCEEDED(m_dxgiFactory->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
        adapterIndex++)
    {
        DXGI_ADAPTER_DESC1 desc;
        ThrowIfFailed(adapter->GetDesc1(&desc));

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), m_d3dMinFeatureLevel, __uuidof(ID3D12Device), nullptr)))
        {
        #ifdef _DEBUG
            wchar_t buff[256] = {};
            swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
            OutputDebugStringW(buff);
        #endif
            break;
        }
    }

#if !defined(NDEBUG)
    if (!adapter)
    {
        // Try WARP12 instead
        if (FAILED(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
        {
            throw std::runtime_error("WARP12 not available. Enable the 'Graphics Tools' optional feature");
        }

        OutputDebugStringA("Direct3D Adapter - WARP12\n");
    }
#endif

    if (!adapter)
    {
        throw std::runtime_error("No Direct3D 12 device found");
    }

    *ppAdapter = adapter.Detach();
}

// Sets the color space for the swap chain in order to handle HDR output.
void DeviceResources::UpdateColorSpace()
{
    if (!m_dxgiFactory)
        return;

    if (!m_dxgiFactory->IsCurrent())
    {
        // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
        ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));
    }

    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    bool isDisplayHDR10 = false;

    if (m_swapChain)
    {
        // To detect HDR support, we will need to check the color space in the primary
        // DXGI output associated with the app at this point in time
        // (using window/display intersection).

        // Get the retangle bounds of the app window.
        RECT windowBounds;
        if (!GetWindowRect(m_window, &windowBounds))
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "GetWindowRect");

        const long ax1 = windowBounds.left;
        const long ay1 = windowBounds.top;
        const long ax2 = windowBounds.right;
        const long ay2 = windowBounds.bottom;

        ComPtr<IDXGIOutput> bestOutput;
        long bestIntersectArea = -1;

        ComPtr<IDXGIAdapter> adapter;
        for (UINT adapterIndex = 0;
            SUCCEEDED(m_dxgiFactory->EnumAdapters(adapterIndex, adapter.ReleaseAndGetAddressOf()));
            ++adapterIndex)
        {
            ComPtr<IDXGIOutput> output;
            for (UINT outputIndex = 0;
                SUCCEEDED(adapter->EnumOutputs(outputIndex, output.ReleaseAndGetAddressOf()));
                ++outputIndex)
            {
                // Get the rectangle bounds of current output.
                DXGI_OUTPUT_DESC desc;
                ThrowIfFailed(output->GetDesc(&desc));
                const auto& r = desc.DesktopCoordinates;

                // Compute the intersection
                const long intersectArea = ComputeIntersectionArea(ax1, ay1, ax2, ay2, r.left, r.top, r.right, r.bottom);
                if (intersectArea > bestIntersectArea)
                {
                    bestOutput.Swap(output);
                    bestIntersectArea = intersectArea;
                }
            }
        }

        if (bestOutput)
        {
            ComPtr<IDXGIOutput6> output6;
            if (SUCCEEDED(bestOutput.As(&output6)))
            {
                DXGI_OUTPUT_DESC1 desc;
                ThrowIfFailed(output6->GetDesc1(&desc));

                if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                {
                    // Display output is HDR10.
                    isDisplayHDR10 = true;
                }
            }
        }
    }

    if ((m_options & c_EnableHDR) && isDisplayHDR10)
    {
        switch (m_backBufferFormat)
        {
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            // The application creates the HDR10 signal.
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            break;

        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            // The system creates the HDR10 signal; application uses linear values.
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            break;

        default:
            break;
        }
    }

    m_colorSpace = colorSpace;

    UINT colorSpaceSupport = 0;
    if (m_swapChain
        && SUCCEEDED(m_swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport))
        && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
    {
        ThrowIfFailed(m_swapChain->SetColorSpace1(colorSpace));
    }
}
}
