// LB.RenderingSystem.ixx - Graphics rendering and display system
export module LB.RenderingSystem;

import LB.Core;
import std;

using namespace std;

/// <summary>
/// Rendering system for LegendsBelow
/// Handles graphics rendering, sprite batching, and visual effects
/// Built on DirectX 12 for Xbox/Windows platforms
/// </summary>
export namespace LB::Systems::Rendering {
    using LB::String;
    using LB::Position;

    /// <summary>
    /// Color representation (RGBA)
    /// </summary>
    struct Color {
        float r{1.0f};
        float g{1.0f};
        float b{1.0f};
        float a{1.0f};

        constexpr Color() = default;
        constexpr Color(float r_, float g_, float b_, float a_ = 1.0f)
            : r(r_), g(g_), b(b_), a(a_) {}

        // Common colors
        static constexpr Color White() { return Color{1.0f, 1.0f, 1.0f, 1.0f}; }
        static constexpr Color Black() { return Color{0.0f, 0.0f, 0.0f, 1.0f}; }
        static constexpr Color Red() { return Color{1.0f, 0.0f, 0.0f, 1.0f}; }
        static constexpr Color Green() { return Color{0.0f, 1.0f, 0.0f, 1.0f}; }
        static constexpr Color Blue() { return Color{0.0f, 0.0f, 1.0f, 1.0f}; }
        static constexpr Color Yellow() { return Color{1.0f, 1.0f, 0.0f, 1.0f}; }
        static constexpr Color Magenta() { return Color{1.0f, 0.0f, 1.0f, 1.0f}; }
        static constexpr Color Cyan() { return Color{0.0f, 1.0f, 1.0f, 1.0f}; }
    };

    /// <summary>
    /// Rectangle definition
    /// </summary>
    struct Rectangle {
        int32_t x{0};
        int32_t y{0};
        int32_t width{0};
        int32_t height{0};

        constexpr Rectangle() = default;
        constexpr Rectangle(int32_t x_, int32_t y_, int32_t w, int32_t h)
            : x(x_), y(y_), width(w), height(h) {}
    };

    /// <summary>
    /// Sprite render data
    /// </summary>
    struct SpriteRenderData {
        String textureId;
        Position position{0, 0};
        Rectangle sourceRect{0, 0, 0, 0};
        Color tint{Color::White()};
        float rotation{0.0f};
        float scale{1.0f};
        int32_t layerDepth{0};
    };

    /// <summary>
    /// Text render data
    /// </summary>
    struct TextRenderData {
        String text;
        Position position{0, 0};
        String fontId;
        Color color{Color::White()};
        float scale{1.0f};
        int32_t layerDepth{0};
    };

    /// <summary>
    /// Render statistics
    /// </summary>
    struct RenderStats {
        uint32_t drawCalls{0};
        uint32_t spritesRendered{0};
        uint32_t textRendered{0};
        float frameTime{0.0f};
        float fps{0.0f};
    };

    /// <summary>
    /// Rendering system for graphics output
    /// </summary>
    class RenderingSystem {
    public:
        RenderingSystem() = default;
        ~RenderingSystem() = default;

        /// <summary>
        /// Initialize the rendering system
        /// </summary>
        bool Initialize(int32_t screenWidth, int32_t screenHeight) {
            screenWidth_ = screenWidth;
            screenHeight_ = screenHeight;

            // TODO: Initialize DirectX 12 device and resources
            initialized_ = true;

            return true;
        }

        /// <summary>
        /// Shutdown the rendering system
        /// </summary>
        void Shutdown() {
            // TODO: Release DirectX resources
            initialized_ = false;
        }

        /// <summary>
        /// Begin a new frame
        /// </summary>
        void BeginFrame() {
            if (!initialized_) return;

            // Clear render queues
            spriteQueue_.clear();
            textQueue_.clear();

            // TODO: Begin DirectX frame
            stats_.drawCalls = 0;
            stats_.spritesRendered = 0;
            stats_.textRendered = 0;
        }

        /// <summary>
        /// End frame and present
        /// </summary>
        void EndFrame() {
            if (!initialized_) return;

            // Sort and render all queued items
            RenderSprites();
            RenderText();

            // TODO: Present DirectX frame
        }

        /// <summary>
        /// Queue a sprite for rendering
        /// </summary>
        void DrawSprite(const SpriteRenderData& sprite) {
            if (!initialized_) return;
            spriteQueue_.push_back(sprite);
        }

        /// <summary>
        /// Queue text for rendering
        /// </summary>
        void DrawText(const TextRenderData& text) {
            if (!initialized_) return;
            textQueue_.push_back(text);
        }

        /// <summary>
        /// Draw a filled rectangle
        /// </summary>
        void DrawRectangle(const Rectangle& rect, const Color& color, int32_t layerDepth = 0) {
            if (!initialized_) return;

            SpriteRenderData sprite{};
            sprite.textureId = "pixel"; // 1x1 white texture
            sprite.position = Position{rect.x, rect.y};
            sprite.sourceRect = Rectangle{0, 0, 1, 1};
            sprite.tint = color;
            sprite.scale = static_cast<float>(std::max(rect.width, rect.height));
            sprite.layerDepth = layerDepth;

            spriteQueue_.push_back(sprite);
        }

        /// <summary>
        /// Clear the screen with a color
        /// </summary>
        void Clear(const Color& color) {
            if (!initialized_) return;
            clearColor_ = color;
            // TODO: Clear DirectX render target
        }

        /// <summary>
        /// Get screen dimensions
        /// </summary>
        void GetScreenSize(int32_t& width, int32_t& height) const {
            width = screenWidth_;
            height = screenHeight_;
        }

        /// <summary>
        /// Get render statistics
        /// </summary>
        const RenderStats& GetStats() const {
            return stats_;
        }

        /// <summary>
        /// Set viewport
        /// </summary>
        void SetViewport(const Rectangle& viewport) {
            viewport_ = viewport;
            // TODO: Update DirectX viewport
        }

        /// <summary>
        /// Enable/disable VSync
        /// </summary>
        void SetVSync(bool enabled) {
            vsyncEnabled_ = enabled;
        }

        /// <summary>
        /// Check if system is initialized
        /// </summary>
        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized_;
        }

    private:
        bool initialized_{false};
        int32_t screenWidth_{1920};
        int32_t screenHeight_{1080};
        Color clearColor_{Color::Black()};
        Rectangle viewport_{0, 0, 1920, 1080};
        bool vsyncEnabled_{true};

        std::vector<SpriteRenderData> spriteQueue_;
        std::vector<TextRenderData> textQueue_;
        RenderStats stats_{};

        void RenderSprites() {
            // Sort by layer depth
            std::sort(spriteQueue_.begin(), spriteQueue_.end(),
                [](const SpriteRenderData& a, const SpriteRenderData& b) {
                    return a.layerDepth < b.layerDepth;
                });

            // TODO: Batch and render sprites
            stats_.spritesRendered = static_cast<uint32_t>(spriteQueue_.size());
            stats_.drawCalls += stats_.spritesRendered > 0 ? 1 : 0;
        }

        void RenderText() {
            // Sort by layer depth
            std::sort(textQueue_.begin(), textQueue_.end(),
                [](const TextRenderData& a, const TextRenderData& b) {
                    return a.layerDepth < b.layerDepth;
                });

            // TODO: Render text
            stats_.textRendered = static_cast<uint32_t>(textQueue_.size());
            stats_.drawCalls += stats_.textRendered > 0 ? 1 : 0;
        }
    };

} // namespace LB::Systems::Rendering