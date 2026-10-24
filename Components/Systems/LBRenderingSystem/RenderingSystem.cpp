// RenderingSystem.cpp - LB.RenderingSystem implementation unit (Direct3D 12 backend wiring)
module LB.RenderingSystem;

import :GpuRenderer;
import :ModelPipeline;
import :OverlayPipeline;
import std;

namespace LB::Systems::Rendering {

    namespace {
        // Overlay vertices batched by the atlas they sample (null: the built-in atlas)
        struct OverlayBuilder {
            std::vector<Detail::OverlayVertex> vertices;
            std::vector<Detail::OverlayBatch> batches;
            float uiScale{1.0f};   // UI units to pixels: applied to every corner, so all 2D scales together
            float offsetX{0.0f};   // then moved into the safe area
            float offsetY{0.0f};

            // Two triangles covering a screen rectangle; atlas coordinates in pixels
            void Quad(const ModelTexture* atlas, float atlasWidth, float atlasHeight,
                      float left, float top, float right, float bottom, float u0, float v0, float u1, float v1,
                      const Color& color, float mode = 0.0f, float outline = 0.0f) {
                if (batches.empty() || batches.back().atlas != atlas) {
                    batches.push_back({ atlas, static_cast<std::uint32_t>(vertices.size()), 0 });
                }
                u0 /= atlasWidth; u1 /= atlasWidth;
                v0 /= atlasHeight; v1 /= atlasHeight;
                const auto vertex = [&](float x, float y, float u, float v) {
                    return Detail::OverlayVertex{ x * uiScale + offsetX, y * uiScale + offsetY, u, v, color.r, color.g, color.b, color.a, mode, outline };
                };
                vertices.insert(vertices.end(), {
                    vertex(left, top, u0, v0), vertex(right, top, u1, v0), vertex(left, bottom, u0, v1),
                    vertex(right, top, u1, v0), vertex(right, bottom, u1, v1), vertex(left, bottom, u0, v1) });
                batches.back().count += 6;
            }

            // A UI image into a rectangle, or a nine-slice frame: the corners keep their shape, the
            // edges and the middle stretch between them
            void Image(const ModelTexture& texture, const ImageRenderData& image) {
                const float w = static_cast<float>(texture.Width());
                const float h = static_cast<float>(texture.Height());
                const float left = static_cast<float>(image.rect.x);
                const float top = static_cast<float>(image.rect.y);
                const float right = left + static_cast<float>(image.rect.width);
                const float bottom = top + static_cast<float>(image.rect.height);
                // Mirroring swaps the u range
                const auto quad = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
                    if (image.mirrored) {
                        u0 = w - u0;
                        u1 = w - u1;
                    }
                    Quad(&texture, w, h, x0, y0, x1, y1, u0, v0, u1, v1, image.color);
                };
                if (image.nineSlice <= 0.0f) {
                    quad(left, top, right, bottom, 0.0f, 0.0f, w, h);
                    return;
                }
                const float s = image.nineSlice;
                const float c = std::min({ image.corner > 0.0f ? image.corner : s, (right - left) * 0.5f, (bottom - top) * 0.5f });
                const float xs[4] = { left, left + c, right - c, right };
                const float ys[4] = { top, top + c, bottom - c, bottom };
                const float us[4] = { 0.0f, s, w - s, w };
                const float vs[4] = { 0.0f, s, h - s, h };
                for (int row = 0; row < 3; ++row) {
                    for (int column = 0; column < 3; ++column) {
                        quad(xs[column], ys[row], xs[column + 1], ys[row + 1], us[column], vs[row], us[column + 1], vs[row + 1]);
                    }
                }
            }

            // A solid rectangle samples the built-in atlas's white strip
            void Rect(const Rectangle& rect, const Color& color) {
                constexpr float u = BitmapFont::AtlasWidth * 0.5f;
                constexpr float v = BitmapFont::WhiteTop + 4.0f;
                Quad(nullptr, BitmapFont::AtlasWidth, BitmapFont::AtlasHeight, static_cast<float>(rect.x), static_cast<float>(rect.y),
                     static_cast<float>(rect.x + rect.width), static_cast<float>(rect.y + rect.height), u, v, u, v, color);
            }

            // Pixel font: one quad per visible glyph; an outline draws dark copies one font pixel around it first
            void PixelText(const TextRenderData& text) {
                const float scale = std::max(0.25f, text.scale);
                const auto glyphs = [&](float dx, float dy, const Color& color) {
                    float x = static_cast<float>(text.position.x) + dx;
                    const float y = static_cast<float>(text.position.y) + dy;
                    for (const char c : text.text) {
                        if (c != ' ') {
                            const auto [gx, gy] = BitmapFont::GlyphOrigin(c);
                            Quad(nullptr, BitmapFont::AtlasWidth, BitmapFont::AtlasHeight,
                                 x, y, x + BitmapFont::GlyphWidth * scale, y + BitmapFont::GlyphHeight * scale,
                                 static_cast<float>(gx), static_cast<float>(gy),
                                 static_cast<float>(gx + BitmapFont::GlyphWidth), static_cast<float>(gy + BitmapFont::GlyphHeight), color);
                        }
                        x += BitmapFont::Advance * scale;
                    }
                };

                if (text.outline) {
                    const Color dark{ 0.0f, 0.0f, 0.0f, text.color.a };
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox != 0 || oy != 0) glyphs(static_cast<float>(ox) * scale, static_cast<float>(oy) * scale, dark);
                        }
                    }
                }
                glyphs(0.0f, 0.0f, text.color);
            }

            // TrueType font: distance field glyphs on the baseline under the capitals' top edge; the
            // shader draws the outline (about an eighth of the capital height, at least a pixel)
            void FontText(const FontData& font, const TextRenderData& text) {
                const float capital = BitmapFont::GlyphHeight * std::max(0.25f, text.scale);
                const float scale = font.ScaleFor(capital);
                const float atlasWidth = static_cast<float>(font.atlas.Width());
                const float atlasHeight = static_cast<float>(font.atlas.Height());
                const float baseline = static_cast<float>(text.position.y) + capital;

                // Distance field units: 0.5 on the outline, 0.5 / spread per bake pixel
                const float outlinePixels = std::max(1.0f, capital * 0.12f);
                const float outline = text.outline ? std::min(0.45f, outlinePixels / scale * 0.5f / font.spread) : 0.0f;
                const float mode = text.outline ? 2.0f : 1.0f;

                const auto codepoints = DecodeUtf8(text.text);
                float pen = static_cast<float>(text.position.x);
                for (std::size_t i = 0; i < codepoints.size(); ++i) {
                    const auto* glyph = font.Find(codepoints[i]);
                    if (!glyph) continue;
                    if (glyph->width > 0.0f) {
                        const float left = pen + glyph->offsetX * scale;
                        const float top = baseline + glyph->offsetY * scale;
                        Quad(&font.atlas, atlasWidth, atlasHeight, left, top, left + glyph->width * scale, top + glyph->height * scale,
                             glyph->atlasX, glyph->atlasY, glyph->atlasX + glyph->width, glyph->atlasY + glyph->height,
                             text.color, mode, outline);
                    }
                    pen += glyph->advance * scale;
                    if (i + 1 < codepoints.size()) pen += font.Kerning(codepoints[i], codepoints[i + 1]) * scale;
                }
            }
        };
    }

    struct RenderingSystem::GpuBackend {
        Detail::GpuRenderer renderer;
    };

    RenderingSystem::RenderingSystem() = default;

    RenderingSystem::~RenderingSystem() {
        Shutdown();
    }

    bool RenderingSystem::Initialize(void* nativeWindow, std::int32_t screenWidth, std::int32_t screenHeight) {
        if (nativeWindow == nullptr) {
            return Initialize(screenWidth, screenHeight);
        }

        try {
            auto backend = std::make_unique<GpuBackend>();
            backend->renderer.SetDeviceCallbacks(
                [this] { if (onDeviceLost_) onDeviceLost_(); },
                [this] { if (onDeviceRestored_) onDeviceRestored_(); });
            backend->renderer.Initialize(nativeWindow, screenWidth, screenHeight);
            for (const auto& model : models_) {
                backend->renderer.AddModel(model);  // loaded before the device existed
            }
            for (const auto& font : fonts_) {
                backend->renderer.AddFont(font);
            }
            for (const auto& image : images_) {
                backend->renderer.AddImage(image);
            }
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
        models_.clear();
        modelPaths_.clear();
        animations_.clear();
        animationFiles_.clear();
        textureCache_.Clear();
        prefetched_.clear();
        fonts_.clear();
        fontPaths_.clear();
        initialized_ = false;
    }

    void RenderingSystem::EndFrame() {
        if (!initialized_) return;

        // Batch models: every copy of a model is one instanced draw per part. Faded copies
        // (tint alpha < 1) are drawn after the opaque world, in two passes (depth, then blend).
        // Skinned copies are posed on the CPU: their joint matrices go into the frame's palette.
        opaqueInstances_.assign(models_.size(), {});
        fadedInstances_.assign(models_.size(), {});
        skinPalette_.clear();
        for (const auto& model : modelQueue_) {
            const auto& data = *models_[model.model - 1];
            ModelInstance instance{};
            instance.world = Matrix4::Placement(model.position, model.yawRadians, model.scale);
            if (model.transform) instance.world = *model.transform * instance.world;
            instance.tint = { model.tint.r, model.tint.g, model.tint.b, model.tint.a };
            instance.partPoses = model.partPoses.empty() ? nullptr : &model.partPoses;
            if (gpu_ && data.IsSkinned()) {
                instance.paletteOffset = AppendSkinPalette(data, model.animation, instance.world);
            }

            auto& group = model.tint.a < 1.0f ? fadedInstances_ : opaqueInstances_;
            group[model.model - 1].push_back(instance);
        }

        if (gpu_) {
            gpu_->renderer.BeginFrame(clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a);

            // World pass: lit 3D shapes and models through the camera, depth-tested
            if (!meshQueue_.empty() || !modelQueue_.empty() || !particleQueue_.empty() || !shadowQueue_.empty()) {
                const auto& view = cameraView_;
                gpu_->renderer.SetCamera(view.eye.x, view.eye.y, view.eye.z, view.target.x, view.target.y, view.target.z,
                                         view.verticalFovRadians, view.nearPlane, view.farPlane);
                gpu_->renderer.SetLighting(lighting_.directional, lighting_.ambient.r, lighting_.ambient.g, lighting_.ambient.b);

                // The point lights nearest to what the camera looks at
                auto lights = lightQueue_;
                const auto away = [&](const PointLight& light) {
                    const float dx = light.position.x - view.target.x;
                    const float dz = light.position.z - view.target.z;
                    return dx * dx + dz * dz;
                };
                std::ranges::sort(lights, {}, away);
                std::vector<float> packed;
                for (std::size_t i = 0; i < lights.size() && i < MaxPointLights; ++i) {
                    const auto& light = lights[i];
                    packed.insert(packed.end(), { light.position.x, light.position.y, light.position.z, light.radius,
                                                  light.color.r * light.intensity, light.color.g * light.intensity, light.color.b * light.intensity });
                }
                gpu_->renderer.SetPointLights(packed.data(), packed.size() / 7);
                for (const auto& mesh : meshQueue_) {
                    gpu_->renderer.DrawMesh(static_cast<int>(mesh.shape),
                        mesh.position.x, mesh.position.y, mesh.position.z,
                        mesh.size.x, mesh.size.y, mesh.size.z, mesh.yawRadians,
                        mesh.color.r, mesh.color.g, mesh.color.b, mesh.color.a,
                        mesh.emissive.r, mesh.emissive.g, mesh.emissive.b);
                }

                using Pass = Detail::GpuRenderer::ModelPass;
                gpu_->renderer.SetSkinPalette(skinPalette_.data(), skinPalette_.size());
                const auto drawGroups = [this](const std::vector<std::vector<ModelInstance>>& groups, Pass pass) {
                    for (std::size_t i = 0; i < groups.size(); ++i) {
                        gpu_->renderer.DrawModelInstances(i, groups[i].data(), groups[i].size(), pass);
                    }
                };

                // The key light's shadows: every model near enough to throw one into the map (the
                // faded walls too) drawn from the light first
                if (gpu_->renderer.BeginShadowPass(view.target.x, view.target.y, view.target.z, lighting_.shadows)) {
                    constexpr float reach = Detail::GpuRenderer::ShadowReach;
                    for (const auto* groups : { &opaqueInstances_, &fadedInstances_ }) {
                        for (std::size_t i = 0; i < groups->size(); ++i) {
                            shadowCasters_.clear();
                            for (const auto& instance : (*groups)[i]) {
                                if (std::abs(instance.world.m[12] - view.target.x) <= reach && std::abs(instance.world.m[14] - view.target.z) <= reach) {
                                    shadowCasters_.push_back(instance);
                                }
                            }
                            gpu_->renderer.DrawModelInstances(i, shadowCasters_.data(), shadowCasters_.size(), Pass::Shadow);
                        }
                    }
                    gpu_->renderer.EndShadowPass();
                    stats_.drawCalls += 1;
                }

                gpu_->renderer.BeginModelPass();
                drawGroups(opaqueInstances_, Pass::Opaque);

                drawGroups(fadedInstances_, Pass::DepthOnly);
                drawGroups(fadedInstances_, Pass::Transparent);

                // Shadows on the floor, under everything else blended (the faded walls, written to
                // depth, hide those behind them)
                if (!shadowQueue_.empty()) {
                    std::vector<float> discs;
                    discs.reserve(shadowQueue_.size() * 8);
                    for (const auto& shadow : shadowQueue_) {
                        discs.insert(discs.end(), { shadow.position.x, shadow.position.y, shadow.position.z, shadow.size, 0.0f, 0.0f, 0.0f, shadow.opacity });
                    }
                    gpu_->renderer.DrawGroundShadows(discs.data(), discs.size() / 8);
                }

                // Particles over everything already drawn (depth-tested): dust blended first, then the
                // glowing ones adding their light on top
                for (const bool glowing : { false, true }) {
                    std::vector<float> sprites;
                    for (const auto& particle : particleQueue_) {
                        if (particle.glowing != glowing) continue;
                        sprites.insert(sprites.end(), { particle.position.x, particle.position.y, particle.position.z, particle.size,
                                                      particle.color.r, particle.color.g, particle.color.b, particle.color.a });
                    }
                    gpu_->renderer.DrawParticles(sprites.data(), sprites.size() / 8, glowing);
                }
            }

            // 2D overlay: rectangles and text back to front by layer (submission order within a
            // layer, rectangles first), alpha-blended in one draw
            enum class Kind { Rectangle, Image, Text };
            struct OverlayItem { std::int32_t layer; Kind kind; std::size_t index; };
            std::vector<OverlayItem> items;
            items.reserve(rectangleQueue_.size() + imageQueue_.size() + textQueue_.size());
            for (std::size_t i = 0; i < rectangleQueue_.size(); ++i) items.push_back({ rectangleQueue_[i].layerDepth, Kind::Rectangle, i });
            for (std::size_t i = 0; i < imageQueue_.size(); ++i) items.push_back({ imageQueue_[i].layerDepth, Kind::Image, i });
            for (std::size_t i = 0; i < textQueue_.size(); ++i) items.push_back({ textQueue_[i].layerDepth, Kind::Text, i });
            std::ranges::stable_sort(items, {}, &OverlayItem::layer);

            OverlayBuilder overlay;
            overlay.uiScale = uiScale_;
            overlay.offsetX = UiOffsetX();
            overlay.offsetY = UiOffsetY();
            for (const auto& item : items) {
                if (item.kind == Kind::Rectangle) {
                    overlay.Rect(rectangleQueue_[item.index].rect, rectangleQueue_[item.index].color);
                    continue;
                }
                if (item.kind == Kind::Image) {
                    const auto& image = imageQueue_[item.index];
                    overlay.Image(*images_[image.image - 1], image);
                    continue;
                }
                const auto& text = textQueue_[item.index];
                if (const auto* font = ResolveFont(text.fontId)) overlay.FontText(*font, text);
                else overlay.PixelText(text);
            }
            gpu_->renderer.DrawOverlay(overlay.vertices.data(), overlay.batches);
        }

        stats_.meshesRendered = static_cast<std::uint32_t>(meshQueue_.size());
        stats_.drawCalls += std::ranges::any_of(particleQueue_, &ParticleRenderData::glowing) ? 1 : 0;
        stats_.drawCalls += std::ranges::any_of(particleQueue_, [](const ParticleRenderData& p) { return !p.glowing; }) ? 1 : 0;
        stats_.drawCalls += shadowQueue_.empty() ? 0 : 1;
        stats_.drawCalls += stats_.meshesRendered;
        stats_.modelsRendered = static_cast<std::uint32_t>(modelQueue_.size());
        for (std::size_t i = 0; i < models_.size(); ++i) {
            const auto parts = static_cast<std::uint32_t>(models_[i]->parts.size());
            if (!opaqueInstances_[i].empty()) stats_.drawCalls += parts;
            if (!fadedInstances_[i].empty()) stats_.drawCalls += 2 * parts;   // depth, then blend
        }

        // Sort and render all queued items
        RenderSprites();
        RenderText();

        if (gpu_) {
            gpu_->renderer.EndFrame();
        }
    }

    ModelLoadResult RenderingSystem::LoadModel(const std::filesystem::path& path) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);
        if (ec) canonical = path;

        for (std::size_t i = 0; i < modelPaths_.size(); ++i) {
            if (modelPaths_[i] == canonical) return ModelLoadResult{ static_cast<ModelId>(i + 1), {} };
        }

        // Read ahead (PrefetchModels), or now
        Detail::GltfLoadResult loaded;
        if (const auto ahead = prefetched_.find(canonical); ahead != prefetched_.end()) {
            loaded = std::move(ahead->second);
            prefetched_.erase(ahead);
        }
        else {
            loaded = Detail::LoadGltfModel(path, textureCache_);
        }
        if (!loaded.error.empty()) {
            return ModelLoadResult{ InvalidModelId, std::move(loaded.error) };
        }

        auto model = std::make_shared<const ModelData>(std::move(loaded.model));
        if (gpu_) {
            try {
                gpu_->renderer.AddModel(model);
            }
            catch (const std::exception& e) {
                return ModelLoadResult{ InvalidModelId, std::format("{}: GPU upload failed ({})", path.filename().string(), e.what()) };
            }
        }

        models_.push_back(std::move(model));
        modelPaths_.push_back(std::move(canonical));
        return ModelLoadResult{ static_cast<ModelId>(models_.size()), {} };
    }

    std::size_t RenderingSystem::PrefetchModels(std::span<const std::filesystem::path> paths) {
        // What is still to read (not loaded, not read ahead, each once)
        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> work;   // (path, canonical)
        for (const auto& path : paths) {
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(path, ec);
            if (ec) canonical = path;
            if (std::ranges::find(modelPaths_, canonical) != modelPaths_.end() || prefetched_.contains(canonical)) continue;
            if (std::ranges::any_of(work, [&](const auto& item) { return item.second == canonical; })) continue;
            work.emplace_back(path, std::move(canonical));
        }
        if (work.empty()) return 0;

        // A worker per core (the main thread waits): each takes the next file in turn
        std::vector<Detail::GltfLoadResult> results(work.size());
        std::atomic<std::size_t> next{0};
        const auto workers = std::clamp<std::size_t>(std::thread::hardware_concurrency(), 1, std::min<std::size_t>(work.size(), 16));
        {
            std::vector<std::jthread> threads;
            threads.reserve(workers);
            for (std::size_t w = 0; w < workers; ++w) {
                threads.emplace_back([&] {
                    for (std::size_t i = next++; i < work.size(); i = next++) {
                        try {
                            results[i] = Detail::LoadGltfModel(work[i].first, textureCache_);
                        }
                        catch (const std::exception& e) {
                            results[i].error = std::format("{}: {}", work[i].first.filename().string(), e.what());
                        }
                    }
                });
            }
        }   // the threads join here
        for (std::size_t i = 0; i < work.size(); ++i) prefetched_.emplace(std::move(work[i].second), std::move(results[i]));
        return work.size();
    }

    ImageLoadResult RenderingSystem::LoadImage(const std::filesystem::path& path) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);
        if (ec) canonical = path;
        for (std::size_t i = 0; i < imagePaths_.size(); ++i) {
            if (imagePaths_[i] == canonical) return ImageLoadResult{ static_cast<ImageId>(i + 1), {} };
        }

        std::string error;
        auto image = Detail::LoadImageFile(path, error);
        if (!image) return ImageLoadResult{ InvalidImageId, std::move(error) };
        if (gpu_) {
            try {
                gpu_->renderer.AddImage(image);
            }
            catch (const std::exception& e) {
                return ImageLoadResult{ InvalidImageId, std::format("{}: GPU upload failed ({})", path.filename().string(), e.what()) };
            }
        }
        images_.push_back(std::move(image));
        imagePaths_.push_back(std::move(canonical));
        return ImageLoadResult{ static_cast<ImageId>(images_.size()), {} };
    }

    FontLoadResult RenderingSystem::LoadFont(const std::filesystem::path& path) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);
        if (ec) canonical = path;

        for (std::size_t i = 0; i < fontPaths_.size(); ++i) {
            if (fontPaths_[i] == canonical) return FontLoadResult{ static_cast<FontId>(i + 1), {} };
        }

        auto loaded = Detail::LoadTrueTypeFont(path, FontBakeSize);
        if (!loaded.error.empty()) {
            return FontLoadResult{ PixelFontId, std::move(loaded.error) };
        }

        auto font = std::make_shared<const FontData>(std::move(loaded.font));
        if (gpu_) {
            try {
                gpu_->renderer.AddFont(font);
            }
            catch (const std::exception& e) {
                return FontLoadResult{ PixelFontId, std::format("{}: GPU upload failed ({})", path.filename().string(), e.what()) };
            }
        }

        fonts_.push_back(std::move(font));
        fontPaths_.push_back(std::move(canonical));
        return FontLoadResult{ static_cast<FontId>(fonts_.size()), {} };
    }

    TextSize RenderingSystem::MeasureText(std::string_view text, float scale, std::string_view font) const {
        if (const auto* data = ResolveFont(font)) return MeasureLine(*data, text, BitmapFont::GlyphHeight * scale);
        return BitmapFont::Measure(text, scale);
    }

    // Clips are named by the prefix and their name in the file (a rig's sets share clip names:
    // "Large/Idle_A" beside "Idle_A")
    AnimationLoadResult RenderingSystem::LoadAnimations(const std::filesystem::path& path, std::string_view prefix) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);
        if (ec) canonical = path;

        for (const auto& [file, clips] : animationFiles_) {
            if (file == canonical) return AnimationLoadResult{ clips, {} };
        }

        auto loaded = Detail::LoadGltfAnimations(path);
        if (!loaded.error.empty()) {
            return AnimationLoadResult{ {}, std::move(loaded.error) };
        }

        AnimationLoadResult result;
        for (auto& clip : loaded.clips) {
            clip.name = std::string(prefix) + clip.name;
            animations_.push_back(std::make_shared<const AnimationClip>(std::move(clip)));
            result.clips.push_back(static_cast<AnimationId>(animations_.size()));
        }
        animationFiles_.emplace_back(std::move(canonical), result.clips);
        return result;
    }

    std::optional<Matrix4> RenderingSystem::GetJointTransform(ModelId model, std::string_view joint, const AnimationState& animation) const {
        const auto* data = GetModel(model);
        if (!data) return std::nullopt;
        const auto index = data->skeleton.FindJoint(joint);
        if (!index) return std::nullopt;

        const auto pose = PoseFor(data->skeleton, animation, [this](AnimationId id) { return GetAnimation(id); });
        return ComputeJointTransforms(data->skeleton, pose)[*index];
    }

    std::uint32_t RenderingSystem::AppendSkinPalette(const ModelData& model, const AnimationState& animation, const Matrix4& world) {
        const auto offset = static_cast<std::uint32_t>(skinPalette_.size());
        const auto pose = PoseFor(model.skeleton, animation, [this](AnimationId id) { return GetAnimation(id); });
        for (const auto& skin : ComputeSkinMatrices(model.skeleton, ComputeJointTransforms(model.skeleton, pose))) {
            skinPalette_.push_back(skin * world);
        }
        return offset;
    }

    void RenderingSystem::OnWindowSizeChanged(std::int32_t width, std::int32_t height) {
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
