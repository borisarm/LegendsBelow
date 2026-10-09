// RenderingSystem.cpp - LB.RenderingSystem implementation unit (Direct3D 12 backend wiring)
module LB.RenderingSystem;

import :DeviceResources;
import std;

namespace LB::Systems::Rendering {

    struct RenderingSystem::GpuBackend {
        Detail::GpuRenderer renderer;
    };

    RenderingSystem::RenderingSystem() = default;

    RenderingSystem::~RenderingSystem() {
        Shutdown();
    }

    bool RenderingSystem::Initialize(void* nativeWindow, int32_t screenWidth, int32_t screenHeight) {
        if (nativeWindow == nullptr) {
            return Initialize(screenWidth, screenHeight);
        }

        try {
            auto backend = std::make_unique<GpuBackend>();
            backend->renderer.SetDeviceCallbacks(
                [this] { if (onDeviceLost_) onDeviceLost_(); },
                [this] { if (onDeviceRestored_) onDeviceRestored_(); });
            backend->renderer.Initialize(nativeWindow, screenWidth, screenHeight);
            gpu_ = std::move(backend);
        }
        catch (const std::exception&) {
            gpu_.reset();
            return false;
        }

        return Initialize(screenWidth, screenHeight);
    }

    void RenderingSystem::Shutdown() {
        // Destroying the renderer waits for the GPU before releasing resources
        gpu_.reset();
        initialized_ = false;
    }

    void RenderingSystem::EndFrame() {
        if (!initialized_) return;

        if (gpu_) {
            gpu_->renderer.BeginFrame(clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a);

            // Solid rectangles, back to front; clipped to the back buffer
            auto rectangles = rectangleQueue_;
            std::ranges::stable_sort(rectangles, {}, &RectangleRenderData::layerDepth);
            for (const auto& item : rectangles) {
                const int32_t left = std::clamp(item.rect.x, 0, screenWidth_);
                const int32_t top = std::clamp(item.rect.y, 0, screenHeight_);
                const int32_t right = std::clamp(item.rect.x + item.rect.width, 0, screenWidth_);
                const int32_t bottom = std::clamp(item.rect.y + item.rect.height, 0, screenHeight_);
                if (right <= left || bottom <= top) continue;

                gpu_->renderer.FillRectangle(left, top, right, bottom, item.color.r, item.color.g, item.color.b, item.color.a);
            }
        }

        // Sort and render all queued items
        RenderSprites();
        RenderText();

        if (gpu_) {
            gpu_->renderer.EndFrame();
        }
    }

    void RenderingSystem::OnWindowSizeChanged(int32_t width, int32_t height) {
        screenWidth_ = width;
        screenHeight_ = height;
        viewport_ = Rectangle{0, 0, width, height};

        if (gpu_) {
            gpu_->renderer.OnWindowSizeChanged(width, height);
        }
    }

    void RenderingSystem::OnWindowMoved() {
        if (gpu_) {
            gpu_->renderer.OnWindowMoved();
        }
    }

    void RenderingSystem::OnDisplayChange() {
        if (gpu_) {
            gpu_->renderer.OnDisplayChange();
        }
    }

    void RenderingSystem::SetDeviceCallbacks(DeviceCallback onDeviceLost, DeviceCallback onDeviceRestored) {
        onDeviceLost_ = std::move(onDeviceLost);
        onDeviceRestored_ = std::move(onDeviceRestored);
    }

} // namespace LB::Systems::Rendering
