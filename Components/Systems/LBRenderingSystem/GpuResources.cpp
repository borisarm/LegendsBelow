// GpuResources.cpp - Shared GPU resources of the renderer: per-frame upload memory, the texture
// heap and texture uploads, the built-in textures, and one-off command lists for loading
//
// Internal partition of LB.RenderingSystem. Every pipeline draws with these; device-dependent,
// so they are created with the device and dropped when it is lost.
module;

#include "Direct3D.h"

module LB.RenderingSystem:GpuResources;

import :BitmapFont;
import :Model;
import :DeviceResources;

namespace LB::Systems::Rendering::Detail
{
    using Microsoft::WRL::ComPtr;

    class GpuResources
    {
    public:
        // Distinct images the texture heap can hold (the KayKit sets share one atlas per pack)
        static constexpr std::size_t MaxTextures = 256;
        // The heap's last slot is the shadow map's (ShadowMap); textures stop one short of it
        static constexpr std::size_t ShadowSlot = MaxTextures - 1;

        explicit GpuResources(DeviceResources& device) noexcept : m_device(device) {}

        GpuResources(GpuResources const&) = delete;
        GpuResources& operator= (GpuResources const&) = delete;

        // Create the upload memory and the texture heap, with the built-in textures in it
        void Create()
        {
            auto device = m_device.GetD3DDevice();
            m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device);
            m_textureHeap = std::make_unique<DirectX::DescriptorHeap>(device, MaxTextures);
            m_textures.clear();
            UploadTextures({ &WhiteTexture(), &FontTexture(), &ParticleTexture() });
        }

        // The device is gone: drop everything that lived on it
        void Reset() noexcept
        {
            m_textures.clear();
            m_textureHeap.reset();
            m_graphicsMemory.reset();
        }

        [[nodiscard]] bool IsReady() const noexcept { return m_graphicsMemory && m_textureHeap; }

        // Upload memory for this frame (constants, instance streams), recycled after Commit
        [[nodiscard]] DirectX::GraphicsResource Allocate(std::size_t size, std::size_t alignment = 16)
        {
            return m_graphicsMemory->Allocate(size, alignment);
        }

        // Recycle the frame's upload memory once the GPU is done with it
        void Commit()
        {
            if (m_graphicsMemory)
                m_graphicsMemory->Commit(m_device.GetCommandQueue());
        }

        // Make the texture heap the shader-visible one
        void BindHeap(ID3D12GraphicsCommandList* commandList) const
        {
            ID3D12DescriptorHeap* heaps[] = { m_textureHeap->Heap() };
            commandList->SetDescriptorHeaps(1, heaps);
        }

        // The heap slot of an uploaded texture, if it is on the GPU
        [[nodiscard]] std::optional<std::size_t> Slot(const ModelTexture* texture) const
        {
            const auto it = m_textures.find(texture);
            return it != m_textures.end() ? std::optional{ it->second.descriptor } : std::nullopt;
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(std::size_t slot) const
        {
            return m_textureHeap->GetGpuHandle(slot);
        }

        // Where a view in the texture heap is written (the shadow map's, in ShadowSlot)
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(std::size_t slot) const
        {
            return m_textureHeap->GetCpuHandle(slot);
        }

        // A white texel for untextured materials and solid rectangles
        static const ModelTexture& WhiteTexture()
        {
            static const ModelTexture white{ { ModelTexture::MipLevel{ 1, 1, { 255, 255, 255, 255 } } } };
            return white;
        }

        // The particle sprite: a soft round glow (alpha falls off smoothly from the middle)
        static const ModelTexture& ParticleTexture()
        {
            static const ModelTexture sprite = []
            {
                constexpr std::uint32_t size = 64;
                ModelTexture::MipLevel level{ size, size, std::vector<std::uint8_t>(size * size * 4, 255) };
                for (std::uint32_t y = 0; y < size; ++y)
                {
                    for (std::uint32_t x = 0; x < size; ++x)
                    {
                        const float dx = (static_cast<float>(x) + 0.5f) / size * 2.0f - 1.0f;
                        const float dy = (static_cast<float>(y) + 0.5f) / size * 2.0f - 1.0f;
                        const float falloff = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                        level.pixels[(y * size + x) * 4 + 3] = static_cast<std::uint8_t>(falloff * falloff * 255.0f);
                    }
                }
                return ModelTexture{ { std::move(level) } };
            }();
            return sprite;
        }

        // The overlay's pixel font atlas (BitmapFont): one level, sampled with point filtering
        static const ModelTexture& FontTexture()
        {
            static const ModelTexture font{ { ModelTexture::MipLevel{
                static_cast<std::uint32_t>(BitmapFont::AtlasWidth), static_cast<std::uint32_t>(BitmapFont::AtlasHeight), BitmapFont::BuildAtlas() } } };
            return font;
        }

        // Record work on a one-off command list, run it, and wait for the GPU (models are loaded
        // at startup). ResourceUploadBatch is avoided on purpose: its <future> include breaks
        // `import std` in the units importing this partition (MSVC C1116).
        template<typename Record>
        void RunAndWait(const Record& record)
        {
            auto device = m_device.GetD3DDevice();
            ComPtr<ID3D12CommandAllocator> allocator;
            ComPtr<ID3D12GraphicsCommandList> commandList;
            ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.GetAddressOf())));
            ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf())));
            record(commandList.Get());
            ThrowIfFailed(commandList->Close());

            auto queue = m_device.GetCommandQueue();
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

        // An upload-heap buffer of at least `size` bytes, filled by `fill(mapped)` and unmapped
        // again before it is returned
        template<typename Fill>
        ComPtr<ID3D12Resource> CreateStagingBuffer(UINT64 size, const Fill& fill)
        {
            ComPtr<ID3D12Resource> staging;
            const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
            const auto desc = CD3DX12_RESOURCE_DESC::Buffer(std::max<UINT64>(size, 1));
            ThrowIfFailed(m_device.GetD3DDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(staging.GetAddressOf())));

            const CD3DX12_RANGE noRead(0, 0);
            std::uint8_t* mapped = nullptr;
            ThrowIfFailed(staging->Map(0, &noRead, reinterpret_cast<void**>(&mapped)));
            const MappedRange unmap{ staging.Get() };   // unmapped even if filling throws
            fill(mapped);
            return staging;
        }

        // Upload textures not on the GPU yet (sRGB, every mip level) and give each a heap slot
        void UploadTextures(const std::vector<const ModelTexture*>& textures)
        {
            auto device = m_device.GetD3DDevice();

            struct Pending { const ModelTexture* texture; ComPtr<ID3D12Resource> resource; ComPtr<ID3D12Resource> staging; std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints; };
            std::vector<Pending> pending;
            for (const auto* texture : textures)
            {
                if (m_textures.contains(texture) || std::ranges::any_of(pending, [&](const Pending& p) { return p.texture == texture; }))
                    continue;
                if (m_textures.size() + pending.size() >= ShadowSlot)
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

                upload.staging = CreateStagingBuffer(total, [&](std::uint8_t* mapped)
                {
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
                });
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

    private:
        // Unmaps a mapped upload buffer when it goes out of scope
        struct MappedRange
        {
            ID3D12Resource* resource;
            ~MappedRange() { resource->Unmap(0, nullptr); }
        };

        // A texture in the heap
        struct GpuTexture
        {
            ComPtr<ID3D12Resource> resource;
            std::size_t descriptor{0};
        };

        DeviceResources& m_device;

        // GraphicsMemory owns the memory the frame allocates from; declared first, destroyed last
        std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;
        std::unique_ptr<DirectX::DescriptorHeap> m_textureHeap;
        std::unordered_map<const ModelTexture*, GpuTexture> m_textures;   // CPU textures stay alive with their models
    };
}
