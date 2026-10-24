// LB.RenderingSystem.ixx - Graphics rendering and display system
export module LB.RenderingSystem;

export import :Camera;
export import :Model;
export import :Animation;
export import :BitmapFont;
export import :Font;
import LB.Core;
import std;


/// <summary>
/// Rendering system for LegendsBelow
/// Handles graphics rendering, sprite batching, and visual effects
/// Built on DirectX 12 for Xbox/Windows platforms
/// </summary>
namespace LB::Systems::Rendering::Detail {
    /// <summary>
    /// Decoded images by a hash of their encoded bytes, so models using the same image file share
    /// it; safe from several threads (models are read in parallel, RenderingSystem::PrefetchModels)
    /// </summary>
    class TextureCache {
    public:
        [[nodiscard]] std::shared_ptr<const ModelTexture> Find(std::uint64_t key) const {
            const std::scoped_lock lock(mutex_);
            const auto it = textures_.find(key);
            return it != textures_.end() ? it->second : nullptr;
        }

        // Keep a decoded image; if another thread kept the same one first, that one (shared)
        std::shared_ptr<const ModelTexture> Insert(std::uint64_t key, std::shared_ptr<const ModelTexture> texture) {
            const std::scoped_lock lock(mutex_);
            return textures_.try_emplace(key, std::move(texture)).first->second;
        }

        void Clear() {
            const std::scoped_lock lock(mutex_);
            textures_.clear();
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, std::shared_ptr<const ModelTexture>> textures_;
    };

    /// <summary>
    /// Result of reading a glTF file: the model, or why it could not be read
    /// </summary>
    struct GltfLoadResult {
        ModelData model;
        std::string error;   // empty on success
    };
}

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
        std::int32_t x{0};
        std::int32_t y{0};
        std::int32_t width{0};
        std::int32_t height{0};

        constexpr Rectangle() = default;
        constexpr Rectangle(std::int32_t x_, std::int32_t y_, std::int32_t w, std::int32_t h)
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
        std::int32_t layerDepth{0};
    };

    /// <summary>
    /// Solid rectangle render data (screen space, pixels)
    /// </summary>
    struct RectangleRenderData {
        Rectangle rect{};
        Color color{Color::White()};
        std::int32_t layerDepth{0};
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
        Color emissive{Color::Black()};        // light the shape gives off itself (flames)
    };

    /// <summary>
    /// A point light in the world (a torch): lights models within its radius, fading smoothly to
    /// nothing at the edge
    /// </summary>
    struct PointLight {
        Vector3 position{};
        Color color{1.0f, 0.6f, 0.3f, 1.0f};   // linear
        float radius{5.0f};                    // tiles
        float intensity{1.0f};
    };

    /// <summary>
    /// A particle: a soft round sprite facing the camera, hidden by nearer geometry. Glowing ones
    /// (fire, sparks) add their light to what is behind them; the others (dust, smoke) blend over it.
    /// </summary>
    struct ParticleRenderData {
        Vector3 position{};
        float size{0.1f};                       // tiles across
        Color color{1.0f, 0.6f, 0.2f, 1.0f};    // linear; alpha: brightness (glowing) or opacity
        bool glowing{true};
    };

    /// <summary>
    /// A soft dark disc lying on the ground (a shadow under a character): darkest in the middle,
    /// fading to nothing at its edge
    /// </summary>
    struct ShadowRenderData {
        Vector3 position{};                     // its center, just above the floor
        float size{0.9f};                       // tiles across
        float opacity{0.45f};                   // how dark its middle is
    };

    /// <summary>
    /// The light everything shares: directional lights scaled (1: the default rig), ambient, and
    /// how dark the key light's shadows are on models (0: no shadows, 1: none of it gets there)
    /// </summary>
    struct SceneLighting {
        float directional{1.0f};
        Color ambient{0.03f, 0.03f, 0.04f, 1.0f};   // linear
        float shadows{0.8f};
    };

    /// <summary>
    /// A loaded model placed in the world: transform (if any), then scale, a turn around Y, and a move
    /// </summary>
    struct ModelRenderData {
        ModelId model{InvalidModelId};
        Vector3 position{};                    // where the model's origin goes
        float yawRadians{0.0f};                // rotation around the vertical axis (0: model +Z faces +Z)
        float scale{1.0f};                     // uniform
        Color tint{Color::White()};            // multiplies the material colors
        std::optional<Matrix4> transform;      // model space, before the placement (e.g. a sword in a hand)
        AnimationState animation;              // skinned models: the pose (no clip: rest pose)
        std::vector<PartPose> partPoses;       // rigid models: parts posed around their origin
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
    /// Outcome of RenderingSystem::LoadAnimations: the file's clips, in file order
    /// </summary>
    struct AnimationLoadResult {
        std::vector<AnimationId> clips;
        String error;                          // why loading failed (empty on success)

        [[nodiscard]] bool Succeeded() const noexcept { return error.empty() && !clips.empty(); }
    };

    /// <summary>
    /// A line of overlay text (UTF-8), drawn after the world. Capital letters are 7 x `scale` pixels
    /// tall in every font, so the built-in pixel font (each font pixel `scale` screen pixels) and
    /// loaded TrueType fonts line up.
    /// </summary>
    struct TextRenderData {
        String text;
        Position position{0, 0};               // top-left corner (top of the capitals), pixels
        String fontId;                         // a loaded font's name; empty: the default font; "pixel": the pixel font
        Color color{Color::White()};           // linear, straight alpha
        float scale{1.0f};
        std::int32_t layerDepth{0};                 // overlay order, shared with rectangles
        bool outline{false};                   // a dark outline, for text over the world
    };

    /// <summary>
    /// A loaded UI image (1-based; 0 is none)
    /// </summary>
    using ImageId = std::uint32_t;
    inline constexpr ImageId InvalidImageId = 0;

    /// <summary>
    /// Outcome of RenderingSystem::LoadImage
    /// </summary>
    struct ImageLoadResult {
        ImageId id{InvalidImageId};
        String error;                          // why loading failed (empty on success)
        [[nodiscard]] bool Succeeded() const noexcept { return id != InvalidImageId; }
    };

    /// <summary>
    /// A UI image drawn into a rectangle (UI units), multiplied by a tint (white images take its
    /// color). With `nineSlice` > 0 it is a frame: corners of that many image pixels keep their
    /// proportions (drawn `corner` UI units wide), edges and the middle stretch. `mirrored` flips it
    /// left to right.
    /// </summary>
    struct ImageRenderData {
        ImageId image{InvalidImageId};
        Rectangle rect{};
        Color color{Color::White()};
        std::int32_t layerDepth{0};
        float nineSlice{0.0f};
        float corner{0.0f};
        bool mirrored{false};
    };

    /// <summary>
    /// Outcome of RenderingSystem::LoadFont
    /// </summary>
    struct FontLoadResult {
        FontId id{PixelFontId};
        String error;                          // why loading failed (empty on success)

        [[nodiscard]] bool Succeeded() const noexcept { return id != PixelFontId; }
    };

    /// <summary>
    /// Render statistics
    /// </summary>
    struct RenderStats {
        std::uint32_t drawCalls{0};
        std::uint32_t meshesRendered{0};
        std::uint32_t modelsRendered{0};
        std::uint32_t spritesRendered{0};
        std::uint32_t textRendered{0};
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
        bool Initialize(std::int32_t screenWidth, std::int32_t screenHeight) {
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
        bool Initialize(void* nativeWindow, std::int32_t screenWidth, std::int32_t screenHeight);

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
        void OnWindowSizeChanged(std::int32_t width, std::int32_t height);

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
            imageQueue_.clear();
            meshQueue_.clear();
            modelQueue_.clear();
            lightQueue_.clear();
            particleQueue_.clear();
            shadowQueue_.clear();

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
        /// Queue a point light for this frame. Models are lit by the MaxPointLights lights nearest
        /// to the camera's target.
        /// </summary>
        void DrawLight(const PointLight& light) {
            if (!initialized_) return;
            lightQueue_.push_back(light);
        }

        const std::vector<PointLight>& GetQueuedLights() const {
            return lightQueue_;
        }

        /// <summary>
        /// Queue a particle for this frame (drawn after the models: dust first, then glowing ones;
        /// one draw each)
        /// </summary>
        void DrawParticle(const ParticleRenderData& particle) {
            if (!initialized_) return;
            particleQueue_.push_back(particle);
        }

        const std::vector<ParticleRenderData>& GetQueuedParticles() const {
            return particleQueue_;
        }

        /// <summary>
        /// Queue a shadow on the ground for this frame (drawn after the solid models, under the
        /// faded walls and the particles; one draw)
        /// </summary>
        void DrawShadow(const ShadowRenderData& shadow) {
            if (!initialized_) return;
            shadowQueue_.push_back(shadow);
        }

        const std::vector<ShadowRenderData>& GetQueuedShadows() const {
            return shadowQueue_;
        }

        static constexpr std::size_t MaxPointLights = 16;

        /// <summary>
        /// Set the scene's directional and ambient light (kept until changed)
        /// </summary>
        void SetLighting(const SceneLighting& lighting) {
            lighting_ = lighting;
        }

        [[nodiscard]] const SceneLighting& GetLighting() const noexcept { return lighting_; }

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
        /// Read and decode models ahead, on worker threads (the files, their meshes, skins, and
        /// images): a later LoadModel of one of them only uploads it. Models loaded or read ahead
        /// already are skipped. Returns how many were read.
        /// </summary>
        std::size_t PrefetchModels(std::span<const std::filesystem::path> paths);

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
        /// Load every animation clip of a glTF file (e.g. a KayKit Rig_Medium animation set). Clips
        /// drive joints by name, so they play on any skinned model with the same joint names.
        /// Loading the same file twice returns the same clips.
        /// </summary>
        AnimationLoadResult LoadAnimations(const std::filesystem::path& path, std::string_view prefix = {});

        /// <summary>
        /// A loaded clip by handle, or null
        /// </summary>
        [[nodiscard]] const AnimationClip* GetAnimation(AnimationId id) const noexcept {
            return id != InvalidAnimationId && id <= animations_.size() ? animations_[id - 1].get() : nullptr;
        }

        /// <summary>
        /// A loaded clip by name (the first one loaded with that name)
        /// </summary>
        [[nodiscard]] std::optional<AnimationId> FindAnimation(std::string_view name) const noexcept {
            for (std::size_t i = 0; i < animations_.size(); ++i) {
                if (animations_[i]->name == name) return static_cast<AnimationId>(i + 1);
            }
            return std::nullopt;
        }

        /// <summary>
        /// Model-space transform of a skinned model's joint in a pose (e.g. "handslot.r", to put a
        /// weapon in the hand). Null for unknown models or joints.
        /// </summary>
        [[nodiscard]] std::optional<Matrix4> GetJointTransform(ModelId model, std::string_view joint, const AnimationState& animation) const;

        /// <summary>
        /// Queue a loaded model for the world pass (unknown handles are ignored). Copies of the
        /// same model are drawn together, one instanced draw per part; a tint alpha below 1 draws
        /// the copy see-through (after the opaque world, showing only its nearest surface).
        /// Skinned models are posed by their animation state.
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
        /// <summary>
        /// The floor point under a screen pixel (the mouse), for this frame's camera
        /// </summary>
        [[nodiscard]] std::optional<Vector3> ScreenToFloor(float x, float y) const noexcept {
            return IsometricCamera::ScreenToFloor(cameraView_, x, y, static_cast<float>(screenWidth_), static_cast<float>(screenHeight_));
        }

        [[nodiscard]] ScreenPoint WorldToScreen(Vector3 point) const noexcept {
            return IsometricCamera::WorldToScreen(cameraView_, point, static_cast<float>(screenWidth_), static_cast<float>(screenHeight_));
        }

        /// <summary>
        /// Where a world point is drawn, in UI units (for 2D drawn over the world: health bars,
        /// damage numbers)
        /// </summary>
        [[nodiscard]] ScreenPoint WorldToUi(Vector3 point) const noexcept {
            auto screen = WorldToScreen(point);
            screen.x = (screen.x - UiOffsetX()) / uiScale_;
            screen.y = (screen.y - UiOffsetY()) / uiScale_;
            return screen;
        }

        /// <summary>
        /// Queue overlay text (drawn with the rectangles, back to front by layer)
        /// </summary>
        void DrawText(const TextRenderData& text) {
            if (!initialized_) return;
            textQueue_.push_back(text);
        }

        /// <summary>
        /// Text queued this frame (in submission order)
        /// </summary>
        const std::vector<TextRenderData>& GetQueuedText() const {
            return textQueue_;
        }

        /// <summary>
        /// Size of a line of text in pixels at a scale, in a font (empty: the default font)
        /// </summary>
        [[nodiscard]] TextSize MeasureText(std::string_view text, float scale, std::string_view font = {}) const;

        /// <summary>
        /// Load a TrueType font (.ttf), baked to a distance field atlas at FontBakeSize. The first
        /// font loaded becomes the default for text. Works headless too (layout and measuring);
        /// loading the same file twice returns the same font.
        /// </summary>
        FontLoadResult LoadFont(const std::filesystem::path& path);

        /// <summary>
        /// Load a UI image (PNG), kept for DrawImage; the same file again returns the same id
        /// </summary>
        ImageLoadResult LoadImage(const std::filesystem::path& path);

        [[nodiscard]] const ModelTexture* GetImage(ImageId id) const noexcept {
            return id != InvalidImageId && id <= images_.size() ? images_[id - 1].get() : nullptr;
        }

        /// <summary>
        /// Queue a UI image for this frame's overlay (ordered with rectangles and text by layer)
        /// </summary>
        void DrawImage(const ImageRenderData& image) {
            if (!initialized_ || !GetImage(image.image)) return;
            imageQueue_.push_back(image);
        }

        [[nodiscard]] const std::vector<ImageRenderData>& GetQueuedImages() const noexcept { return imageQueue_; }

        /// <summary>
        /// A loaded font by handle, or null (the pixel font has no FontData)
        /// </summary>
        [[nodiscard]] const FontData* GetFont(FontId id) const noexcept {
            return id != PixelFontId && id <= fonts_.size() ? fonts_[id - 1].get() : nullptr;
        }

        /// <summary>
        /// A loaded font by name (file name without extension)
        /// </summary>
        [[nodiscard]] std::optional<FontId> FindFont(std::string_view name) const noexcept {
            for (std::size_t i = 0; i < fonts_.size(); ++i) {
                if (fonts_[i]->name == name) return static_cast<FontId>(i + 1);
            }
            return std::nullopt;
        }

        // Glyphs are baked at this pixel height; larger text stays sharp through the distance field
        static constexpr float FontBakeSize = 48.0f;

        /// <summary>
        /// Draw a filled screen-space rectangle on the 2D overlay (above the 3D world; alpha blends)
        /// </summary>
        void DrawRectangle(const Rectangle& rect, const Color& color, std::int32_t layerDepth = 0) {
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
        void GetScreenSize(std::int32_t& width, std::int32_t& height) const {
            width = screenWidth_;
            height = screenHeight_;
        }

        /// <summary>
        /// The 2D layer's scale: rectangles and text are given in UI units, drawn at this many
        /// pixels each (the game sets it so its layout grows with the window)
        /// </summary>
        void SetUiScale(float scale) noexcept { uiScale_ = std::clamp(scale, 0.25f, 8.0f); }

        /// <summary>
        /// The share of the screen the 2D layout keeps to, centered (televisions crop their edges:
        /// 0.9 keeps a twentieth of the screen clear on each side); 1: the whole screen
        /// </summary>
        void SetUiSafeArea(float fraction) noexcept { uiSafeArea_ = std::clamp(fraction, MinUiSafeArea, 1.0f); }
        [[nodiscard]] float GetUiSafeArea() const noexcept { return uiSafeArea_; }
        static constexpr float MinUiSafeArea = 0.8f;

        /// <summary>
        /// Where UI unit (0, 0) is drawn, in pixels: the safe area's margin
        /// </summary>
        [[nodiscard]] float UiOffsetX() const noexcept { return static_cast<float>(screenWidth_) * (1.0f - uiSafeArea_) * 0.5f; }
        [[nodiscard]] float UiOffsetY() const noexcept { return static_cast<float>(screenHeight_) * (1.0f - uiSafeArea_) * 0.5f; }
        [[nodiscard]] float GetUiScale() const noexcept { return uiScale_; }

        /// <summary>
        /// The window's size in UI units (what 2D layout should fill)
        /// </summary>
        void GetUiSize(std::int32_t& width, std::int32_t& height) const {
            width = static_cast<std::int32_t>(std::lround(static_cast<float>(screenWidth_) * uiSafeArea_ / uiScale_));
            height = static_cast<std::int32_t>(std::lround(static_cast<float>(screenHeight_) * uiSafeArea_ / uiScale_));
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
        std::int32_t screenWidth_{1920};
        std::int32_t screenHeight_{1080};
        float uiScale_{1.0f};
        float uiSafeArea_{1.0f};
        Color clearColor_{Color::Black()};
        Rectangle viewport_{0, 0, 1920, 1080};
        bool vsyncEnabled_{true};

        std::vector<SpriteRenderData> spriteQueue_;
        std::vector<TextRenderData> textQueue_;
        std::vector<RectangleRenderData> rectangleQueue_;
        std::vector<MeshRenderData> meshQueue_;
        std::vector<ModelRenderData> modelQueue_;
        std::vector<PointLight> lightQueue_;
        std::vector<ParticleRenderData> particleQueue_;
        std::vector<ShadowRenderData> shadowQueue_;
        SceneLighting lighting_{};
        std::vector<std::vector<ModelInstance>> opaqueInstances_;   // per model, rebuilt each frame
        std::vector<std::vector<ModelInstance>> fadedInstances_;
        std::vector<ModelInstance> shadowCasters_;          // one model's copies near the shadow pass's focus (reused)
        std::vector<std::shared_ptr<const ModelData>> models_;   // ModelId - 1
        std::vector<std::filesystem::path> modelPaths_;          // canonical source of each model
        std::vector<std::shared_ptr<const AnimationClip>> animations_;   // AnimationId - 1
        std::vector<std::pair<std::filesystem::path, std::vector<AnimationId>>> animationFiles_;
        Detail::TextureCache textureCache_;                      // by image content
        std::map<std::filesystem::path, Detail::GltfLoadResult> prefetched_;   // read ahead, by canonical path
        std::vector<Matrix4> skinPalette_;                       // this frame's joint matrices, world space
        std::vector<std::shared_ptr<const FontData>> fonts_;     // FontId - 1
        std::vector<std::shared_ptr<const ModelTexture>> images_;   // ImageId - 1
        std::vector<std::filesystem::path> imagePaths_;
        std::vector<ImageRenderData> imageQueue_;
        std::vector<std::filesystem::path> fontPaths_;

        // The font a text item names: null for the pixel font (also when no font is loaded)
        [[nodiscard]] const FontData* ResolveFont(std::string_view name) const noexcept {
            if (name == "pixel") return nullptr;
            if (!name.empty()) {
                if (const auto id = FindFont(name)) return GetFont(*id);
            }
            return fonts_.empty() ? nullptr : fonts_.front().get();
        }

        // Pose of a skinned model in world space: one matrix per joint appended to skinPalette_
        std::uint32_t AppendSkinPalette(const ModelData& model, const AnimationState& animation, const Matrix4& world);
        CameraView cameraView_{};
        RenderStats stats_{};

        void RenderSprites() {
            // Sort by layer depth
            std::sort(spriteQueue_.begin(), spriteQueue_.end(),
                [](const SpriteRenderData& a, const SpriteRenderData& b) {
                    return a.layerDepth < b.layerDepth;
                });

            // Textured sprites are not drawn yet; rectangles queue a sprite each and are drawn by the overlay
            stats_.spritesRendered = static_cast<std::uint32_t>(spriteQueue_.size());
        }

        void RenderText() {
            // Sort by layer depth
            std::sort(textQueue_.begin(), textQueue_.end(),
                [](const TextRenderData& a, const TextRenderData& b) {
                    return a.layerDepth < b.layerDepth;
                });

            stats_.textRendered = static_cast<std::uint32_t>(textQueue_.size());

            // The overlay (rectangles, text, and sprites) is one draw
            stats_.drawCalls += !spriteQueue_.empty() || !textQueue_.empty() ? 1 : 0;
        }
    };

} // namespace LB::Systems::Rendering

// Not exported: shared with the implementation units
namespace LB::Systems::Rendering::Detail {
    /// <summary>
    /// Result of reading the animations of a glTF file
    /// </summary>
    struct GltfAnimationsResult {
        std::vector<AnimationClip> clips;
        std::string error;   // empty on success
    };

    /// <summary>
    /// Result of baking a TrueType font
    /// </summary>
    struct TrueTypeLoadResult {
        FontData font;
        std::string error;   // empty on success
    };

    /// <summary>
    /// Bake a .ttf file to a distance field atlas (FontLoader.cpp, stb_truetype)
    /// </summary>
    TrueTypeLoadResult LoadTrueTypeFont(const std::filesystem::path& path, float bakeSize);

    /// <summary>
    /// Read a PNG (or other stb_image format) as a one-level UI image (GltfLoader.cpp)
    /// </summary>
    std::shared_ptr<const ModelTexture> LoadImageFile(const std::filesystem::path& path, std::string& error);

    /// <summary>
    /// Read a .gltf or .glb file (GltfLoader.cpp, fastgltf)
    /// </summary>
    GltfLoadResult LoadGltfModel(const std::filesystem::path& path, TextureCache& textureCache);

    /// <summary>
    /// Read the animations of a .gltf or .glb file (GltfLoader.cpp, fastgltf)
    /// </summary>
    GltfAnimationsResult LoadGltfAnimations(const std::filesystem::path& path);
}