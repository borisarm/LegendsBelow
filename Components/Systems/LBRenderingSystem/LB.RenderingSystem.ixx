// LB.RenderingSystem.ixx - Graphics rendering and display system
export module LB.RenderingSystem;

export import :Camera;
export import :Model;
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
        static constexpr Color CornflowerBlue() { return Color{0.392156899f, 0.584313750f, 0.929411829f, 1.0f}; }
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
    /// Solid rectangle render data (screen space, pixels)
    /// </summary>
    struct RectangleRenderData {
        Rectangle rect{};
        Color color{Color::White()};
        int32_t layerDepth{0};
    };

    /// <summary>
    /// Built-in 3D shapes, each of unit size centered on the origin (placeholders until models)
    /// </summary>
    enum class MeshShape {
        Box,
        Cylinder,   // upright (axis along Y)
        Sphere,
        Cone        // upright, tip up
    };

    /// <summary>
    /// A lit, solid-colored 3D shape in world space
    /// </summary>
    struct MeshRenderData {
        MeshShape shape{MeshShape::Box};
        Vector3 position{};                    // center of the shape
        Vector3 size{1.0f, 1.0f, 1.0f};        // full extents along X, Y, Z
        float yawRadians{0.0f};                // rotation around the vertical axis
        Color color{Color::White()};
    };

    /// <summary>
    /// A loaded model placed in the world
    /// </summary>
    struct ModelRenderData {
        ModelId model{InvalidModelId};
        Vector3 position{};                    // where the model's origin goes
        float yawRadians{0.0f};                // rotation around the vertical axis (0: model +Z faces +Z)
        float scale{1.0f};                     // uniform
        Color tint{Color::White()};            // multiplies the material colors
    };

    /// <summary>
    /// Outcome of RenderingSystem::LoadModel
    /// </summary>
    struct ModelLoadResult {
        ModelId id{InvalidModelId};
        String error;                          // why loading failed (empty on success)

        [[nodiscard]] bool Succeeded() const noexcept { return id != InvalidModelId; }
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
        uint32_t meshesRendered{0};
        uint32_t modelsRendered{0};
        uint32_t spritesRendered{0};
        uint32_t textRendered{0};
        float frameTime{0.0f};
        float fps{0.0f};
    };

    /// <summary>
    /// Rendering system for graphics output.
    /// Runs headless (queues and statistics only) unless initialized with a native window,
    /// in which case frames are cleared and presented through a Direct3D 12 device.
    /// </summary>
    class RenderingSystem {
    public:
        using DeviceCallback = std::function<void()>;

        RenderingSystem();
        ~RenderingSystem();

        RenderingSystem(const RenderingSystem&) = delete;
        RenderingSystem& operator=(const RenderingSystem&) = delete;

        /// <summary>
        /// Initialize the rendering system without a GPU device (headless)
        /// </summary>
        bool Initialize(int32_t screenWidth, int32_t screenHeight) {
            screenWidth_ = screenWidth;
            screenHeight_ = screenHeight;
            viewport_ = Rectangle{0, 0, screenWidth, screenHeight};

            initialized_ = true;

            return true;
        }

        /// <summary>
        /// Initialize the rendering system with a Direct3D 12 device presenting to the given
        /// native window (an HWND on Windows). A null window falls back to headless mode.
        /// Returns false if the device could not be created.
        /// </summary>
        bool Initialize(void* nativeWindow, int32_t screenWidth, int32_t screenHeight);

        /// <summary>
        /// Shutdown the rendering system, waiting for the GPU and releasing the device
        /// </summary>
        void Shutdown();

        /// <summary>
        /// True when frames are presented through a GPU device (false when headless)
        /// </summary>
        [[nodiscard]] bool HasGpuDevice() const noexcept {
            return gpu_ != nullptr;
        }

        /// <summary>
        /// Notify the renderer that the output window was resized
        /// </summary>
        void OnWindowSizeChanged(int32_t width, int32_t height);

        /// <summary>
        /// Notify the renderer that the output window moved (may change display/HDR state)
        /// </summary>
        void OnWindowMoved();

        /// <summary>
        /// Notify the renderer that display settings changed
        /// </summary>
        void OnDisplayChange();

        /// <summary>
        /// Callbacks invoked when the GPU device is lost and after it has been recreated
        /// </summary>
        void SetDeviceCallbacks(DeviceCallback onDeviceLost, DeviceCallback onDeviceRestored);

        /// <summary>
        /// Begin a new frame
        /// </summary>
        void BeginFrame() {
            if (!initialized_) return;

            // Clear render queues
            spriteQueue_.clear();
            textQueue_.clear();
            rectangleQueue_.clear();
            meshQueue_.clear();
            modelQueue_.clear();

            stats_.drawCalls = 0;
            stats_.meshesRendered = 0;
            stats_.modelsRendered = 0;
            stats_.spritesRendered = 0;
            stats_.textRendered = 0;
        }

        /// <summary>
        /// End frame: render all queued items and present
        /// </summary>
        void EndFrame();

        /// <summary>
        /// Queue a sprite for rendering
        /// </summary>
        void DrawSprite(const SpriteRenderData& sprite) {
            if (!initialized_) return;
            spriteQueue_.push_back(sprite);
        }

        /// <summary>
        /// Set the camera the 3D world is drawn through this frame
        /// </summary>
        void SetCamera(const CameraView& view) {
            cameraView_ = view;
        }

        [[nodiscard]] const CameraView& GetCameraView() const noexcept {
            return cameraView_;
        }

        /// <summary>
        /// Queue a 3D shape for the world pass (depth-tested, drawn before the 2D overlay)
        /// </summary>
        void DrawMesh(const MeshRenderData& mesh) {
            if (!initialized_) return;
            meshQueue_.push_back(mesh);
        }

        /// <summary>
        /// Meshes queued this frame (in submission order)
        /// </summary>
        const std::vector<MeshRenderData>& GetQueuedMeshes() const {
            return meshQueue_;
        }

        /// <summary>
        /// Load a glTF 2.0 model (.gltf or .glb) and, with a GPU device, upload it. Works headless
        /// too, so model data can be inspected without a device. Loading the same file twice
        /// returns the same model.
        /// </summary>
        ModelLoadResult LoadModel(const std::filesystem::path& path);

        /// <summary>
        /// A loaded model by handle, or null
        /// </summary>
        [[nodiscard]] const ModelData* GetModel(ModelId id) const noexcept {
            return id != InvalidModelId && id <= models_.size() ? models_[id - 1].get() : nullptr;
        }

        /// <summary>
        /// A loaded model by name (file name without extension)
        /// </summary>
        [[nodiscard]] std::optional<ModelId> FindModel(std::string_view name) const noexcept {
            for (std::size_t i = 0; i < models_.size(); ++i) {
                if (models_[i]->name == name) return static_cast<ModelId>(i + 1);
            }
            return std::nullopt;
        }

        /// <summary>
        /// Queue a loaded model for the world pass (unknown handles are ignored)
        /// </summary>
        void DrawModel(const ModelRenderData& model) {
            if (!initialized_ || !GetModel(model.model)) return;
            modelQueue_.push_back(model);
        }

        /// <summary>
        /// Models queued this frame (in submission order)
        /// </summary>
        const std::vector<ModelRenderData>& GetQueuedModels() const {
            return modelQueue_;
        }

        /// <summary>
        /// Project a world point to the screen through the current camera
        /// </summary>
        [[nodiscard]] ScreenPoint WorldToScreen(Vector3 point) const noexcept {
            return IsometricCamera::WorldToScreen(cameraView_, point, static_cast<float>(screenWidth_), static_cast<float>(screenHeight_));
        }

        /// <summary>
        /// Queue text for rendering
        /// </summary>
        void DrawText(const TextRenderData& text) {
            if (!initialized_) return;
            textQueue_.push_back(text);
        }

        /// <summary>
        /// Draw a filled screen-space rectangle on the 2D overlay (above the 3D world)
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

            rectangleQueue_.push_back(RectangleRenderData{rect, color, layerDepth});
        }

        /// <summary>
        /// Rectangles queued this frame (in submission order)
        /// </summary>
        const std::vector<RectangleRenderData>& GetQueuedRectangles() const {
            return rectangleQueue_;
        }

        /// <summary>
        /// Clear the screen with a color
        /// </summary>
        void Clear(const Color& color) {
            if (!initialized_) return;
            // Applied to the back buffer when the frame is recorded in EndFrame
            clearColor_ = color;
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
        // Direct3D 12 backend, defined in RenderingSystem.cpp; null when headless
        struct GpuBackend;
        std::unique_ptr<GpuBackend> gpu_;
        DeviceCallback onDeviceLost_;
        DeviceCallback onDeviceRestored_;

        bool initialized_{false};
        int32_t screenWidth_{1920};
        int32_t screenHeight_{1080};
        Color clearColor_{Color::Black()};
        Rectangle viewport_{0, 0, 1920, 1080};
        bool vsyncEnabled_{true};

        std::vector<SpriteRenderData> spriteQueue_;
        std::vector<TextRenderData> textQueue_;
        std::vector<RectangleRenderData> rectangleQueue_;
        std::vector<MeshRenderData> meshQueue_;
        std::vector<ModelRenderData> modelQueue_;
        std::vector<std::shared_ptr<const ModelData>> models_;   // ModelId - 1
        std::vector<std::filesystem::path> modelPaths_;          // canonical source of each model
        CameraView cameraView_{};
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

// Not exported: shared with the implementation units
namespace LB::Systems::Rendering::Detail {
    /// <summary>
    /// Result of reading a glTF file: the model, or why it could not be read
    /// </summary>
    struct GltfLoadResult {
        ModelData model;
        std::string error;   // empty on success
    };

    /// <summary>
    /// Read a .gltf or .glb file (GltfLoader.cpp, fastgltf)
    /// </summary>
    GltfLoadResult LoadGltfModel(const std::filesystem::path& path);
}