// ModelPipeline.cpp - The glTF model pipeline: GPU copies of models, the per-frame constants and
// lights of the model shaders, the skin palette, and instanced draws (rigid and skinned parts)
//
// Internal partition of LB.RenderingSystem (ModelInstanced*.hlsl).
module;

#include "Direct3D.h"

module LB.RenderingSystem:ModelPipeline;

import :Model;
import :DeviceResources;
import :GpuResources;

namespace LB::Systems::Rendering::Detail
{
    using Microsoft::WRL::ComPtr;

    // Which pass an instanced model draw belongs to
    enum class ModelPass
    {
        Opaque,        // depth-tested and written, no blending
        DepthOnly,     // faded instances, first: depth only, so each shows its nearest surface
        Transparent,   // faded instances, second: premultiplied alpha blend where depth matches
        Shadow         // into the key light's shadow map: depth only, both faces, biased
    };
    inline constexpr int ModelPassCount = 4;

    // The scene's directional lights and ambient light, shared by the model shader and the
    // primitives' effect
    struct SceneLights
    {
        // DirectXTK's default three-light rig, dimmed for a dungeon: the lights add up, and the
        // sRGB target shows linear values brighter than a UNORM one did
        DirectX::XMFLOAT4 direction[3] = {
            { -0.5265408f, -0.5735765f, -0.6275069f, 0.0f },
            {  0.7198464f,  0.3420201f,  0.6040227f, 0.0f },
            {  0.4545195f, -0.7660444f,  0.4545195f, 0.0f } };
        DirectX::XMFLOAT4 color[3] = {
            { 0.75f, 0.70f, 0.62f, 1.0f },     // key: warm, torch-like
            { 0.12f, 0.13f, 0.18f, 1.0f },     // fill: cool
            { 0.10f, 0.10f, 0.12f, 1.0f } };   // back
        DirectX::XMFLOAT4 ambient{ 0.03f, 0.03f, 0.04f, 1.0f };
    };

    class ModelPipeline
    {
    public:
        // Point lights the model shader takes per frame (ModelInstanced.hlsli: MaxPointLights)
        static constexpr std::size_t MaxPointLights = 16;

        ModelPipeline(DeviceResources& device, GpuResources& resources) noexcept : m_device(device), m_resources(resources) {}

        ModelPipeline(ModelPipeline const&) = delete;
        ModelPipeline& operator= (ModelPipeline const&) = delete;

        // Root signature, the [rigid, skinned] x ModelPass pipelines, and every model kept so far
        void Create(const DirectX::RenderTargetState& renderTargetState)
        {
            CreatePipelines(renderTargetState);
            for (auto& model : m_models)
                Upload(model);
        }

        // The device is gone; models keep their CPU data to be uploaded again
        void Reset() noexcept
        {
            for (auto& model : m_models)
            {
                model.parts.clear();
                model.materialTextures.clear();
            }
            for (auto& pipelines : m_pipelines)
                for (auto& pipeline : pipelines)
                    pipeline.Reset();
            m_rootSignature.Reset();
            m_boundPipeline = nullptr;
            m_paletteAddress = 0;
        }

        // Keep a model and upload it if the device is there; returns its index for Draw
        std::size_t AddModel(std::shared_ptr<const ModelData> model)
        {
            m_models.push_back(GpuModel{ std::move(model), {}, {} });
            if (m_rootSignature)
                Upload(m_models.back());
            return m_models.size() - 1;
        }

        void SetViewProjection(const DirectX::XMFLOAT4X4& viewProjection) noexcept { m_frame.viewProjection = viewProjection; }
        [[nodiscard]] const DirectX::XMFLOAT4X4& ViewProjection() const noexcept { return m_frame.viewProjection; }

        void SetLights(const SceneLights& lights) noexcept
        {
            for (int i = 0; i < 3; ++i)
            {
                m_frame.lightDirection[i] = lights.direction[i];
                m_frame.lightColor[i] = lights.color[i];
            }
            m_frame.ambientColor = lights.ambient;
        }

        // The key light's view and projection, for the shadow pass and for sampling its shadow
        // map; strength 0 turns the shadows off
        void SetShadow(const DirectX::XMFLOAT4X4& lightViewProjection, float strength, float texel) noexcept
        {
            m_frame.shadowViewProjection = lightViewProjection;
            m_frame.shadowParams = DirectX::XMFLOAT4(strength > 0.0f ? 1.0f : 0.0f, texel, strength, 0.0f);
        }

        // This frame's point lights: x, y, z, radius, r, g, b (color times intensity) each
        void SetPointLights(const float* lights, std::size_t count) noexcept
        {
            count = std::min(count, MaxPointLights);
            for (std::size_t i = 0; i < count; ++i)
            {
                const float* light = lights + i * 7;
                m_frame.pointLightPosition[i] = DirectX::XMFLOAT4(light[0], light[1], light[2], light[3]);
                m_frame.pointLightColor[i] = DirectX::XMFLOAT4(light[4], light[5], light[6], 1.0f);
            }
            m_frame.pointLightCount = DirectX::XMFLOAT4(static_cast<float>(count), 0.0f, 0.0f, 0.0f);
        }

        // Joint matrices (world space) of every skinned model drawn this frame, indexed by the
        // instances' palette offsets. Call before BeginPass.
        void SetSkinPalette(const Matrix4* matrices, std::size_t count)
        {
            if (!m_resources.IsReady()) return;

            // Like the instance stream, each matrix is stored as its first three columns (rows of
            // the transpose); at least one entry so the root SRV always points at valid memory
            const std::size_t entries = std::max<std::size_t>(count, 1);
            auto palette = m_resources.Allocate(entries * sizeof(PaletteEntry));
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

        // Bind the root signature, texture heap, per-frame constants, the shadow map, and the
        // skin palette. The shadow pass sees the world from the key light instead of the camera
        // (and leaves the shadow map out: it is being written).
        void BeginPass(ID3D12GraphicsCommandList* commandList, bool shadowPass = false)
        {
            if (!m_rootSignature || !m_resources.IsReady()) return;
            if (m_paletteAddress == 0)
                SetSkinPalette(nullptr, 0);

            auto frame = m_resources.Allocate(sizeof(FrameConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            auto constants = m_frame;
            if (shadowPass)
                constants.viewProjection = m_frame.shadowViewProjection;
            std::memcpy(frame.Memory(), &constants, sizeof(FrameConstants));

            m_resources.BindHeap(commandList);
            commandList->SetGraphicsRootSignature(m_rootSignature.Get());
            commandList->SetGraphicsRootConstantBufferView(RootFrame, frame.GpuAddress());
            commandList->SetGraphicsRootShaderResourceView(RootPalette, m_paletteAddress);
            commandList->SetGraphicsRootDescriptorTable(RootShadowMap, m_resources.GpuHandle(shadowPass ? 0 : GpuResources::ShadowSlot));
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_boundPipeline = nullptr;
        }

        // Draw every copy of a model with one instanced draw per part. Rigid parts are placed by
        // their part transform (after the copy's part pose, if any) and the copy's world matrix;
        // skinned parts by the copy's joint matrices in the skin palette.
        void Draw(ID3D12GraphicsCommandList* commandList, std::size_t index, const ModelInstance* instances, std::size_t count, ModelPass pass)
        {
            if (!m_rootSignature || !m_resources.IsReady() || index >= m_models.size() || count == 0) return;
            const auto& model = m_models[index];
            if (model.parts.size() != model.data->parts.size()) return;

            using namespace DirectX;
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

                auto* pipeline = m_pipelines[skinned ? 1 : 0][static_cast<int>(pass)].Get();
                if (pipeline != m_boundPipeline)
                {
                    commandList->SetPipelineState(pipeline);
                    m_boundPipeline = pipeline;
                }

                MaterialConstants constants{};
                constants.baseColor = XMFLOAT4(material.baseColor[0], material.baseColor[1], material.baseColor[2], material.baseColor[3]);
                constants.emissive = XMFLOAT4(material.emissive[0], material.emissive[1], material.emissive[2], 0.0f);
                commandList->SetGraphicsRoot32BitConstants(RootMaterial, sizeof(MaterialConstants) / 4, &constants, 0);
                commandList->SetGraphicsRootDescriptorTable(RootTexture, m_resources.GpuHandle(model.materialTextures[part.materialIndex]));

                const XMMATRIX local = toXm(part.transform);
                auto buffer = m_resources.Allocate(count * sizeof(InstanceData));
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

                D3D12_VERTEX_BUFFER_VIEW views[3] = {};
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

        // Another pipeline was bound: the next model draw binds its own again
        void InvalidateBinding() noexcept { m_boundPipeline = nullptr; }

        // The skin palette is per frame
        void EndFrame() noexcept { m_paletteAddress = 0; }

    private:
        // Per-frame constants of the model shader (ModelInstanced.hlsli: cbuffer Frame)
        struct FrameConstants
        {
            DirectX::XMFLOAT4X4 viewProjection{};
            DirectX::XMFLOAT4 lightDirection[3]{};
            DirectX::XMFLOAT4 lightColor[3]{};
            DirectX::XMFLOAT4 ambientColor{};
            DirectX::XMFLOAT4 pointLightPosition[MaxPointLights]{};
            DirectX::XMFLOAT4 pointLightColor[MaxPointLights]{};
            DirectX::XMFLOAT4 pointLightCount{};
            DirectX::XMFLOAT4X4 shadowViewProjection{};
            DirectX::XMFLOAT4 shadowParams{};
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

        // Root parameters
        static constexpr UINT RootFrame = 0;       // b0: cbuffer Frame
        static constexpr UINT RootMaterial = 1;    // b1: cbuffer Material (root constants)
        static constexpr UINT RootTexture = 2;     // t0: base color texture (descriptor table)
        static constexpr UINT RootPalette = 3;     // t1: skin palette (root SRV)
        static constexpr UINT RootShadowMap = 4;   // t2: the key light's shadow map (descriptor table)

        // ModelVertex must match the input layout shared with GeometricPrimitive
        static_assert(sizeof(ModelVertex) == sizeof(DirectX::VertexPositionNormalTexture));

        void CreatePipelines(const DirectX::RenderTargetState& renderTargetState)
        {
            using namespace DirectX;
            auto device = m_device.GetD3DDevice();

            const CD3DX12_DESCRIPTOR_RANGE textureRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            const CD3DX12_DESCRIPTOR_RANGE shadowRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
            CD3DX12_ROOT_PARAMETER parameters[5]{};
            parameters[RootFrame].InitAsConstantBufferView(0);
            parameters[RootMaterial].InitAsConstants(sizeof(MaterialConstants) / 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
            parameters[RootTexture].InitAsDescriptorTable(1, &textureRange, D3D12_SHADER_VISIBILITY_PIXEL);
            parameters[RootPalette].InitAsShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
            parameters[RootShadowMap].InitAsDescriptorTable(1, &shadowRange, D3D12_SHADER_VISIBILITY_PIXEL);

            // Trilinear, anisotropic, wrapping (glTF's default sampler)
            CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_ANISOTROPIC);
            sampler.MaxAnisotropy = 8;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // The shadow map's: comparisons (lit where the surface is no farther than the depth
            // there), bilinear for soft edges; beyond the map, lit
            CD3DX12_STATIC_SAMPLER_DESC shadowSampler(1, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
                D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
                0.0f, 1, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);
            shadowSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            const CD3DX12_STATIC_SAMPLER_DESC samplers[] = { sampler, shadowSampler };

            const CD3DX12_ROOT_SIGNATURE_DESC rootDesc(static_cast<UINT>(std::size(parameters)), parameters,
                static_cast<UINT>(std::size(samplers)), samplers,
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf()));
            ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));

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
            desc.pRootSignature = m_rootSignature.Get();
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
                        IID_PPV_ARGS(m_pipelines[skinned][static_cast<int>(pass)].ReleaseAndGetAddressOf())));
                };

                create(ModelPass::Opaque, CommonStates::Opaque, CommonStates::DepthDefault);
                create(ModelPass::DepthOnly, depthOnly, CommonStates::DepthDefault);
                create(ModelPass::Transparent, CommonStates::AlphaBlend, CommonStates::DepthRead);

                // The shadow pass: no pixel shader or color target, the shadow map's depth format,
                // both faces (thin walls and open meshes still cast), and a slope bias against acne
                auto shadow = desc;
                shadow.PS = {};
                shadow.NumRenderTargets = 0;
                shadow.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
                shadow.DSVFormat = DXGI_FORMAT_D32_FLOAT;
                shadow.BlendState = depthOnly;
                shadow.DepthStencilState = CommonStates::DepthDefault;
                shadow.RasterizerState = CommonStates::CullNone;
                shadow.RasterizerState.DepthBias = 64;
                shadow.RasterizerState.SlopeScaledDepthBias = 2.0f;
                shadow.RasterizerState.DepthBiasClamp = 0.01f;
                ThrowIfFailed(device->CreateGraphicsPipelineState(&shadow,
                    IID_PPV_ARGS(m_pipelines[skinned][static_cast<int>(ModelPass::Shadow)].ReleaseAndGetAddressOf())));
            }
        }

        // Copy every part into default-heap vertex, skin, and index buffers through one staging
        // buffer, upload the model's textures, and wait for the copies.
        void Upload(GpuModel& model)
        {
            auto device = m_device.GetD3DDevice();

            std::vector<const ModelTexture*> textures;
            for (const auto& texture : model.data->textures)
                textures.push_back(texture.get());
            m_resources.UploadTextures(textures);

            std::vector<std::size_t> materialTextures;
            for (const auto& material : model.data->materials)
            {
                const auto* texture = material.baseColorTexture ? model.data->textures[*material.baseColorTexture].get() : &GpuResources::WhiteTexture();
                materialTextures.push_back(m_resources.Slot(texture).value());
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

            // One staging buffer in the upload heap holding everything
            const auto staging = m_resources.CreateStagingBuffer(stagingSize, [&](std::uint8_t* mapped)
            {
                for (const auto& copy : copies)
                    std::memcpy(mapped + copy.offset, copy.data, static_cast<std::size_t>(copy.size));
            });

            // Record the copies and the transitions to their final states
            m_resources.RunAndWait([&](ID3D12GraphicsCommandList* commandList)
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

        DeviceResources& m_device;
        GpuResources& m_resources;
        std::vector<GpuModel> m_models;
        FrameConstants m_frame;
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelines[2][ModelPassCount];   // [rigid, skinned][ModelPass]
        ID3D12PipelineState* m_boundPipeline{nullptr};
        D3D12_GPU_VIRTUAL_ADDRESS m_paletteAddress{0};
    };
}
