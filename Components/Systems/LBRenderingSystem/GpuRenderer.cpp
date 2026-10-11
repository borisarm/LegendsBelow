// GpuRenderer.cpp - The frame-level façade over the GPU: owns the device and the shared
// resources, composes the pipelines (models, primitives, particles, overlay), and handles the
// frame lifecycle, the camera, and device loss
//
// Internal partition of LB.RenderingSystem. It exposes only plain types, so RenderingSystem never
// handles Direct3D types; each pipeline owns its own root signature and pipeline states.
module;

#include "Direct3D.h"

module LB.RenderingSystem:GpuRenderer;

import :Model;
import :Font;
import :DeviceResources;
import :GpuResources;
import :ModelPipeline;
import :PrimitivePipeline;
import :OverlayPipeline;
import :ParticlePipeline;

namespace LB::Systems::Rendering::Detail
{
    class GpuRenderer final : public IDeviceNotify
    {
    public:
        using DeviceCallback = std::function<void()>;
        using ModelPass = Detail::ModelPass;
        static constexpr std::size_t MaxPointLights = ModelPipeline::MaxPointLights;

        // sRGB render target views over a UNORM swap chain: shaders write linear colors (glTF
        // materials are linear) and the hardware encodes them for display
        GpuRenderer()
            : m_device(std::make_unique<DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)),
              m_resources(*m_device),
              m_models(*m_device, m_resources)
        {
            m_device->RegisterDeviceNotify(this);
            m_models.SetLights(m_lights);
        }

        ~GpuRenderer()
        {
            m_device->WaitForGpu();
        }

        GpuRenderer(GpuRenderer const&) = delete;
        GpuRenderer& operator= (GpuRenderer const&) = delete;

        // Creates the device and swap chain for the given native window (HWND). Throws on failure.
        void Initialize(void* nativeWindow, int width, int height)
        {
            m_device->SetWindow(static_cast<HWND>(nativeWindow), width, height);
            m_device->CreateDeviceResources();
            CreateDeviceDependentResources();
            m_device->CreateWindowSizeDependentResources();
        }

        // Resets the command list and clears the back buffer, depth buffer, viewport and scissor.
        void BeginFrame(float r, float g, float b, float a)
        {
            m_device->Prepare();
            auto commandList = m_device->GetCommandList();

            const auto rtvDescriptor = m_device->GetRenderTargetView();
            const auto dsvDescriptor = m_device->GetDepthStencilView();
            const float clearColor[4] = { r, g, b, a };
            commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
            commandList->ClearRenderTargetView(rtvDescriptor, clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            const auto viewport = m_device->GetScreenViewport();
            const auto scissorRect = m_device->GetScissorRect();
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissorRect);
        }

        // Camera for the world pass: right-handed look-at and perspective projection, matching
        // IsometricCamera::WorldToScreen. Call after BeginFrame.
        void SetCamera(float eyeX, float eyeY, float eyeZ, float targetX, float targetY, float targetZ,
                       float verticalFov, float nearPlane, float farPlane)
        {
            if (!m_primitives.IsReady()) return;

            const auto size = m_device->GetOutputSize();
            const float width = static_cast<float>(std::max(1l, size.right - size.left));
            const float height = static_cast<float>(std::max(1l, size.bottom - size.top));

            using namespace DirectX;
            const XMVECTOR eye = XMVectorSet(eyeX, eyeY, eyeZ, 1.0f);
            const XMVECTOR target = XMVectorSet(targetX, targetY, targetZ, 1.0f);
            if (XMVector3Equal(eye, target)) return;

            const XMMATRIX view = XMMatrixLookAtRH(eye, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
            const XMMATRIX projection = XMMatrixPerspectiveFovRH(verticalFov, width / height, nearPlane, farPlane);
            m_primitives.SetCamera(view, projection);
            XMFLOAT4X4 viewProjection;
            XMStoreFloat4x4(&viewProjection, view * projection);
            m_models.SetViewProjection(viewProjection);

            // Screen right and up in the world, for camera-facing particles
            const XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, eye));
            const XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
            XMStoreFloat4(&m_cameraRight, right);
            XMStoreFloat4(&m_cameraUp, XMVector3Cross(right, forward));
        }

        // Scale the directional lights and set the ambient light, for the primitives and the
        // model shader alike (e.g. a dark dungeon lit mostly by torches)
        void SetLighting(float directional, float ambientR, float ambientG, float ambientB)
        {
            const SceneLights defaults{};
            for (int i = 0; i < 3; ++i)
            {
                const auto& base = defaults.color[i];
                m_lights.color[i] = DirectX::XMFLOAT4(base.x * directional, base.y * directional, base.z * directional, 1.0f);
            }
            m_lights.ambient = DirectX::XMFLOAT4(ambientR, ambientG, ambientB, 1.0f);
            m_models.SetLights(m_lights);
            m_primitives.SetLights(m_lights);
        }

        // This frame's point lights: x, y, z, radius, r, g, b (color times intensity) each
        void SetPointLights(const float* lights, std::size_t count) { m_models.SetPointLights(lights, count); }

        // Draw a unit primitive (0 box, 1 cylinder, 2 sphere, 3 cone) scaled, turned around Y,
        // and moved to a world position, lit with the given diffuse color. Call after SetCamera.
        void DrawMesh(int shape, float x, float y, float z, float sizeX, float sizeY, float sizeZ, float yaw,
                      float r, float g, float b, float a, float emissiveR = 0.0f, float emissiveG = 0.0f, float emissiveB = 0.0f)
        {
            using namespace DirectX;
            const XMMATRIX world = XMMatrixScaling(sizeX, sizeY, sizeZ) * XMMatrixRotationY(yaw) * XMMatrixTranslation(x, y, z);
            m_primitives.Draw(m_device->GetCommandList(), shape, world, XMFLOAT4(r, g, b, a), XMFLOAT3(emissiveR, emissiveG, emissiveB));
            m_models.InvalidateBinding();
        }

        // Keep a model and upload its parts and textures; returns its index for
        // DrawModelInstances. The CPU data is kept so it can be uploaded again after a device loss.
        std::size_t AddModel(std::shared_ptr<const ModelData> model) { return m_models.AddModel(std::move(model)); }

        // Joint matrices of every skinned model drawn this frame. Call before BeginModelPass.
        void SetSkinPalette(const Matrix4* matrices, std::size_t count) { m_models.SetSkinPalette(matrices, count); }

        // Bind the model pipeline. Call after SetCamera and SetSkinPalette.
        void BeginModelPass() { m_models.BeginPass(m_device->GetCommandList()); }

        void DrawModelInstances(std::size_t index, const ModelInstance* instances, std::size_t count, ModelPass pass)
        {
            m_models.Draw(m_device->GetCommandList(), index, instances, count, pass);
        }

        // Keep a font and upload its atlas; it is uploaded again after a device loss
        void AddFont(std::shared_ptr<const FontData> font)
        {
            m_fonts.push_back(std::move(font));
            if (m_resources.IsReady())
                m_resources.UploadTextures({ &m_fonts.back()->atlas });
        }

        // Keep a UI image and upload it; it is uploaded again after a device loss
        void AddImage(std::shared_ptr<const ModelTexture> image)
        {
            m_images.push_back(std::move(image));
            if (m_resources.IsReady())
                m_resources.UploadTextures({ m_images.back().get() });
        }

        // Particles (x, y, z, size, r, g, b, a each), glowing (added) or dust (blended). Call
        // after the models.
        void DrawParticles(const float* particles, std::size_t count, bool additive)
        {
            m_particles.Draw(m_device->GetCommandList(), m_resources, m_models.ViewProjection(), m_cameraRight, m_cameraUp, particles, count, additive);
            m_models.InvalidateBinding();
        }

        // The 2D overlay over the whole frame. Call after the world pass.
        void DrawOverlay(const OverlayVertex* vertices, const std::vector<OverlayBatch>& batches)
        {
            const auto size = m_device->GetOutputSize();
            m_overlay.Draw(m_device->GetCommandList(), m_resources,
                static_cast<float>(std::max(1l, size.right - size.left)), static_cast<float>(std::max(1l, size.bottom - size.top)), vertices, batches);
            m_models.InvalidateBinding();
        }

        // Submits the frame's command list and presents the back buffer.
        void EndFrame()
        {
            m_device->Present();
            m_models.EndFrame();
            m_resources.Commit();
        }

        // Returns true if the swap chain was resized.
        bool OnWindowSizeChanged(int width, int height) { return m_device->WindowSizeChanged(width, height); }

        void OnWindowMoved()
        {
            const auto r = m_device->GetOutputSize();
            m_device->WindowSizeChanged(r.right, r.bottom);
        }

        void OnDisplayChange() { m_device->UpdateColorSpace(); }

        void WaitForGpu() noexcept { m_device->WaitForGpu(); }

        void SetDeviceCallbacks(DeviceCallback onLost, DeviceCallback onRestored)
        {
            m_onDeviceLost = std::move(onLost);
            m_onDeviceRestored = std::move(onRestored);
        }

        // IDeviceNotify: everything on the device goes, the effect before the memory it uses
        void OnDeviceLost() override
        {
            m_models.Reset();
            m_overlay.Reset();
            m_particles.Reset();
            m_primitives.Reset();
            m_resources.Reset();
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
        void CreateDeviceDependentResources()
        {
            auto device = m_device->GetD3DDevice();

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

            // Memory and textures first: the pipelines' resources come from them
            m_resources.Create();
            for (const auto& font : m_fonts)
                m_resources.UploadTextures({ &font->atlas });
            for (const auto& image : m_images)
                m_resources.UploadTextures({ image.get() });

            const DirectX::RenderTargetState renderTargetState(m_device->GetBackBufferFormat(), m_device->GetDepthBufferFormat());
            m_primitives.Create(device, renderTargetState, m_lights);
            m_overlay.Create(device, renderTargetState);
            m_particles.Create(device, renderTargetState);
            m_models.Create(renderTargetState);   // and uploads the models kept across a device loss
        }

        // Declaration order is destruction order in reverse: the pipelines go before the shared
        // resources (whose GraphicsMemory backs the effect), and those before the device
        std::unique_ptr<DeviceResources> m_device;
        GpuResources m_resources;
        ModelPipeline m_models;
        PrimitivePipeline m_primitives;
        OverlayPipeline m_overlay;
        ParticlePipeline m_particles;

        SceneLights m_lights;
        std::vector<std::shared_ptr<const FontData>> m_fonts;
        std::vector<std::shared_ptr<const ModelTexture>> m_images;
        DirectX::XMFLOAT4 m_cameraRight{ 1.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 m_cameraUp{ 0.0f, 1.0f, 0.0f, 0.0f };
        DeviceCallback m_onDeviceLost;
        DeviceCallback m_onDeviceRestored;
    };
}
