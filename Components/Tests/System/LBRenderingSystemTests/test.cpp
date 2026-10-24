#include "pch.h"

import LB.RenderingSystem;
import std;

using namespace LB::Systems::Rendering;

// ============================================================================
// RenderingSystem Initialization Tests
// ============================================================================

TEST(RenderingSystemTests, Initialize_ValidDimensions_Success) {
    RenderingSystem system;

    bool result = system.Initialize(1920, 1080);

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsInitialized());
}

TEST(RenderingSystemTests, Initialize_SetsDimensions) {
    RenderingSystem system;
    system.Initialize(1280, 720);

    int32_t width, height;
    system.GetScreenSize(width, height);

    EXPECT_EQ(width, 1280);
    EXPECT_EQ(height, 720);
}

TEST(RenderingSystemTests, Shutdown_ClearsState) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    system.Shutdown();

    EXPECT_FALSE(system.IsInitialized());
}

// ============================================================================
// Frame Management Tests
// ============================================================================

TEST(RenderingSystemTests, BeginFrame_ClearsQueues) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    SpriteRenderData sprite{};
    sprite.textureId = "test";
    system.DrawSprite(sprite);

    system.BeginFrame();

    // Stats should be reset
    const auto& stats = system.GetStats();
    EXPECT_EQ(stats.drawCalls, 0u);
    EXPECT_EQ(stats.spritesRendered, 0u);
}

TEST(RenderingSystemTests, BeginFrame_BeforeInitialize_NoError) {
    RenderingSystem system;

    system.BeginFrame(); // Should not crash

    SUCCEED();
}

TEST(RenderingSystemTests, EndFrame_RendersQueued) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    system.BeginFrame();
    SpriteRenderData sprite{};
    sprite.textureId = "test";
    system.DrawSprite(sprite);
    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_GT(stats.spritesRendered, 0u);
}

// ============================================================================
// Sprite Rendering Tests
// ============================================================================

TEST(RenderingSystemTests, DrawSprite_ValidSprite_Queued) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    SpriteRenderData sprite{};
    sprite.textureId = "player";
    sprite.position = {100, 200};
    sprite.scale = 2.0f;

    system.DrawSprite(sprite);
    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_EQ(stats.spritesRendered, 1u);
}

TEST(RenderingSystemTests, DrawSprite_MultipleSprites_AllQueued) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    for (int i = 0; i < 10; i++) {
        SpriteRenderData sprite{};
        sprite.textureId = "sprite_" + std::to_string(i);
        system.DrawSprite(sprite);
    }
    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_EQ(stats.spritesRendered, 10u);
}

TEST(RenderingSystemTests, DrawSprite_BeforeInitialize_NoError) {
    RenderingSystem system;

    SpriteRenderData sprite{};
    system.DrawSprite(sprite); // Should not crash

    SUCCEED();
}

TEST(RenderingSystemTests, DrawSprite_WithLayerDepth_SortedCorrectly) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    SpriteRenderData back{};
    back.textureId = "back";
    back.layerDepth = 10;

    SpriteRenderData front{};
    front.textureId = "front";
    front.layerDepth = 1;

    system.DrawSprite(back);
    system.DrawSprite(front);
    system.EndFrame();

    // Sprites should be sorted by layer depth (rendering happens internally)
    SUCCEED();
}

// ============================================================================
// Text Rendering Tests
// ============================================================================

TEST(RenderingSystemTests, DrawText_ValidText_Queued) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    TextRenderData text{};
    text.text = "Hello World";
    text.position = {50, 50};
    text.fontId = "arial";

    system.DrawText(text);
    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_EQ(stats.textRendered, 1u);
}

TEST(RenderingSystemTests, DrawText_MultipleTexts_AllQueued) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    for (int i = 0; i < 5; i++) {
        TextRenderData text{};
        text.text = "Text " + std::to_string(i);
        system.DrawText(text);
    }
    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_EQ(stats.textRendered, 5u);
}

TEST(RenderingSystemTests, DrawText_BeforeInitialize_NoError) {
    RenderingSystem system;

    TextRenderData text{};
    system.DrawText(text); // Should not crash

    SUCCEED();
}

// ============================================================================
// Rectangle Rendering Tests
// ============================================================================

TEST(RenderingSystemTests, DrawRectangle_ValidRect_Queued) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    Rectangle rect{100, 100, 200, 150};
    system.DrawRectangle(rect, Color::Red());
    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_GT(stats.spritesRendered, 0u);
}

TEST(RenderingSystemTests, DrawRectangle_MultipleRects_AllQueued) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    Rectangle rect1{0, 0, 100, 100};
    Rectangle rect2{200, 200, 50, 50};
    system.DrawRectangle(rect1, Color::Blue());
    system.DrawRectangle(rect2, Color::Green());
    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_EQ(stats.spritesRendered, 2u);
}

// ============================================================================
// Clear and Color Tests
// ============================================================================

TEST(RenderingSystemTests, Clear_ValidColor_Success) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    system.Clear(Color::Black());

    // No exception, success
    SUCCEED();
}

TEST(RenderingSystemTests, Clear_BeforeInitialize_NoError) {
    RenderingSystem system;

    system.Clear(Color::White()); // Should not crash

    SUCCEED();
}

TEST(RenderingSystemTests, Color_NamedColors_CorrectValues) {
    EXPECT_EQ(Color::White().r, 1.0f);
    EXPECT_EQ(Color::White().g, 1.0f);
    EXPECT_EQ(Color::White().b, 1.0f);

    EXPECT_EQ(Color::Black().r, 0.0f);
    EXPECT_EQ(Color::Black().g, 0.0f);
    EXPECT_EQ(Color::Black().b, 0.0f);

    EXPECT_EQ(Color::Red().r, 1.0f);
    EXPECT_EQ(Color::Red().g, 0.0f);
    EXPECT_EQ(Color::Red().b, 0.0f);
}

// ============================================================================
// Stats Tests
// ============================================================================

TEST(RenderingSystemTests, GetStats_InitialState_AllZero) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    const auto& stats = system.GetStats();

    EXPECT_EQ(stats.drawCalls, 0u);
    EXPECT_EQ(stats.spritesRendered, 0u);
    EXPECT_EQ(stats.textRendered, 0u);
}

TEST(RenderingSystemTests, GetStats_AfterRendering_UpdatedCorrectly) {
    RenderingSystem system;
    system.Initialize(1920, 1080);
    system.BeginFrame();

    SpriteRenderData sprite{};
    system.DrawSprite(sprite);

    TextRenderData text{};
    system.DrawText(text);

    system.EndFrame();

    const auto& stats = system.GetStats();
    EXPECT_EQ(stats.spritesRendered, 1u);
    EXPECT_EQ(stats.textRendered, 1u);
    EXPECT_GT(stats.drawCalls, 0u);
}

// ============================================================================
// Viewport Tests
// ============================================================================

TEST(RenderingSystemTests, SetViewport_ValidRect_Success) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    Rectangle viewport{0, 0, 1280, 720};
    system.SetViewport(viewport);

    // No exception, success
    SUCCEED();
}

// ============================================================================
// VSync Tests
// ============================================================================

TEST(RenderingSystemTests, SetVSync_Enabled_Success) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    system.SetVSync(true);

    SUCCEED();
}

TEST(RenderingSystemTests, SetVSync_Disabled_Success) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    system.SetVSync(false);

    SUCCEED();
}
// ============================================================================
// Native Window / Headless Mode Tests
// ============================================================================

TEST(RenderingSystemTests, Initialize_NullWindow_RunsHeadless) {
    RenderingSystem system;

    bool result = system.Initialize(nullptr, 1280, 720);

    EXPECT_TRUE(result);
    EXPECT_TRUE(system.IsInitialized());
    EXPECT_FALSE(system.HasGpuDevice());
}

TEST(RenderingSystemTests, Initialize_NullWindow_SetsDimensions) {
    RenderingSystem system;
    system.Initialize(nullptr, 1280, 720);

    int32_t width, height;
    system.GetScreenSize(width, height);

    EXPECT_EQ(width, 1280);
    EXPECT_EQ(height, 720);
}

TEST(RenderingSystemTests, HasGpuDevice_HeadlessInitialize_False) {
    RenderingSystem system;
    system.Initialize(1920, 1080);

    EXPECT_FALSE(system.HasGpuDevice());
}

TEST(RenderingSystemTests, OnWindowSizeChanged_UpdatesScreenSize) {
    RenderingSystem system;
    system.Initialize(800, 600);

    system.OnWindowSizeChanged(1024, 768);

    int32_t width, height;
    system.GetScreenSize(width, height);
    EXPECT_EQ(width, 1024);
    EXPECT_EQ(height, 768);
}

TEST(RenderingSystemTests, WindowEvents_Headless_NoError) {
    RenderingSystem system;
    system.Initialize(800, 600);

    system.OnWindowMoved();
    system.OnDisplayChange();

    EXPECT_TRUE(system.IsInitialized());
}

TEST(RenderingSystemTests, SetDeviceCallbacks_Headless_NeverInvoked) {
    RenderingSystem system;
    int calls = 0;
    system.SetDeviceCallbacks([&] { ++calls; }, [&] { ++calls; });
    system.Initialize(800, 600);

    system.BeginFrame();
    system.EndFrame();
    system.Shutdown();

    EXPECT_EQ(calls, 0);
}

TEST(RenderingSystemTests, Shutdown_WithoutInitialize_NoError) {
    RenderingSystem system;

    system.Shutdown();

    EXPECT_FALSE(system.IsInitialized());
}

TEST(RenderingSystemTests, Color_CornflowerBlue_CorrectValues) {
    constexpr Color color = Color::CornflowerBlue();

    EXPECT_NEAR(color.r, 100.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(color.g, 149.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(color.b, 237.0f / 255.0f, 1e-3f);
    EXPECT_EQ(color.a, 1.0f);
}

// ============================================================================
// Solid Rectangle Tests
// ============================================================================

TEST(RenderingSystemTests, DrawRectangle_KeepsExactGeometryAndColor) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();

    system.DrawRectangle(Rectangle{10, 20, 30, 5}, Color::Red(), 3);

    const auto& rects = system.GetQueuedRectangles();
    ASSERT_EQ(rects.size(), 1u);
    EXPECT_EQ(rects[0].rect.x, 10);
    EXPECT_EQ(rects[0].rect.y, 20);
    EXPECT_EQ(rects[0].rect.width, 30);
    EXPECT_EQ(rects[0].rect.height, 5);
    EXPECT_EQ(rects[0].color.r, 1.0f);
    EXPECT_EQ(rects[0].layerDepth, 3);
}

TEST(RenderingSystemTests, BeginFrame_ClearsRectangles) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();
    system.DrawRectangle(Rectangle{0, 0, 10, 10}, Color::Blue());

    system.BeginFrame();

    EXPECT_TRUE(system.GetQueuedRectangles().empty());
}

TEST(RenderingSystemTests, DrawRectangle_BeforeInitialize_NotQueued) {
    RenderingSystem system;

    system.DrawRectangle(Rectangle{0, 0, 10, 10}, Color::Blue());

    EXPECT_TRUE(system.GetQueuedRectangles().empty());
}

TEST(RenderingSystemTests, EndFrame_Headless_RectanglesOffScreen_NoError) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();
    system.DrawRectangle(Rectangle{-50, -50, 20, 20}, Color::Green());
    system.DrawRectangle(Rectangle{790, 590, 100, 100}, Color::Green());

    system.EndFrame();

    EXPECT_EQ(system.GetStats().spritesRendered, 2u);
}

// ============================================================================
// Mesh Queue Tests
// ============================================================================

TEST(RenderingSystemMeshTests, DrawMesh_QueuesUntilNextFrame) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();

    MeshRenderData mesh{};
    mesh.shape = MeshShape::Cylinder;
    mesh.position = { 1.0f, 0.5f, -2.0f };
    system.DrawMesh(mesh);

    ASSERT_EQ(system.GetQueuedMeshes().size(), 1u);
    EXPECT_EQ(system.GetQueuedMeshes()[0].shape, MeshShape::Cylinder);
    EXPECT_FLOAT_EQ(system.GetQueuedMeshes()[0].position.z, -2.0f);

    system.BeginFrame();
    EXPECT_TRUE(system.GetQueuedMeshes().empty());
}

TEST(RenderingSystemMeshTests, DrawMesh_NotInitialized_Ignored) {
    RenderingSystem system;

    system.DrawMesh(MeshRenderData{});

    EXPECT_TRUE(system.GetQueuedMeshes().empty());
}

TEST(RenderingSystemMeshTests, EndFrame_CountsMeshes) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();
    system.DrawMesh(MeshRenderData{});
    system.DrawMesh(MeshRenderData{});

    system.EndFrame();

    EXPECT_EQ(system.GetStats().meshesRendered, 2u);
    EXPECT_GE(system.GetStats().drawCalls, 2u);
}

TEST(RenderingSystemMeshTests, SetCamera_UsedForWorldToScreen) {
    RenderingSystem system;
    system.Initialize(800, 600);
    IsometricCamera camera;
    camera.SnapTo({ 3.0f, 4.0f });

    system.SetCamera(camera.GetView());
    const auto center = system.WorldToScreen({ 3.0f, 0.0f, 4.0f });

    EXPECT_TRUE(center.inFront);
    EXPECT_NEAR(center.x, 400.0f, 0.01f);
    EXPECT_NEAR(center.y, 300.0f, 0.01f);
}

// ============================================================================
// IsometricCamera Tests
// ============================================================================

namespace {
    float Length(Vector3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
}

TEST(IsometricCameraTests, DefaultView_DistanceAndPitch) {
    IsometricCamera camera;

    const auto view = camera.GetView();
    const Vector3 offset{ view.eye.x - view.target.x, view.eye.y - view.target.y, view.eye.z - view.target.z };

    EXPECT_NEAR(Length(offset), IsometricCamera::MediumDistance, 1e-4);
    const float pitch = std::asin(offset.y / Length(offset)) * 180.0f / std::numbers::pi_v<float>;
    EXPECT_NEAR(pitch, IsometricCamera::PitchDegrees, 1e-3);
    EXPECT_GT(offset.x, 0.0f);  // yaw 45: camera to the south-east of the focus
    EXPECT_GT(offset.z, 0.0f);
}

TEST(IsometricCameraTests, ScreenToGround_DefaultYawIsDiagonal) {
    IsometricCamera camera;

    const auto right = camera.ScreenToGround(1.0f, 0.0f);
    const auto up = camera.ScreenToGround(0.0f, 1.0f);

    EXPECT_NEAR(right.x, std::sqrt(0.5f), 1e-6);
    EXPECT_NEAR(right.y, -std::sqrt(0.5f), 1e-6);
    EXPECT_NEAR(up.x, -std::sqrt(0.5f), 1e-6);
    EXPECT_NEAR(up.y, -std::sqrt(0.5f), 1e-6);
}

TEST(IsometricCameraTests, ScreenToGround_MatchesProjection) {
    IsometricCamera camera;
    const auto view = camera.GetView();

    const auto right = camera.ScreenToGround(1.0f, 0.0f);
    const auto up = camera.ScreenToGround(0.0f, 1.0f);
    const auto onRight = IsometricCamera::WorldToScreen(view, { right.x, 0.0f, right.y }, 800.0f, 600.0f);
    const auto onUp = IsometricCamera::WorldToScreen(view, { up.x, 0.0f, up.y }, 800.0f, 600.0f);

    EXPECT_GT(onRight.x, 400.0f);
    EXPECT_NEAR(onRight.y, 300.0f, 0.5f);
    EXPECT_LT(onUp.y, 300.0f);
    EXPECT_NEAR(onUp.x, 400.0f, 0.01f);
}

TEST(IsometricCameraTests, WorldToScreen_HigherPointsAppearHigher) {
    IsometricCamera camera;
    const auto view = camera.GetView();

    const auto ground = IsometricCamera::WorldToScreen(view, { 0.0f, 0.0f, 0.0f }, 800.0f, 600.0f);
    const auto raised = IsometricCamera::WorldToScreen(view, { 0.0f, 1.0f, 0.0f }, 800.0f, 600.0f);

    EXPECT_LT(raised.y, ground.y);
    EXPECT_NEAR(raised.x, ground.x, 0.01f);
}

TEST(IsometricCameraTests, WorldToScreen_BehindCamera_NotInFront) {
    IsometricCamera camera;
    const auto view = camera.GetView();
    const Vector3 behind{ view.eye.x * 2.0f, view.eye.y * 2.0f, view.eye.z * 2.0f };

    EXPECT_FALSE(IsometricCamera::WorldToScreen(view, behind, 800.0f, 600.0f).inFront);
}

TEST(IsometricCameraTests, Follow_InsideDeadZone_DoesNotMove) {
    IsometricCamera camera;

    camera.Follow({ 1.0f, 0.5f });

    EXPECT_FLOAT_EQ(camera.GetFocus().x, 0.0f);
    EXPECT_FLOAT_EQ(camera.GetFocus().y, 0.0f);
}

TEST(IsometricCameraTests, Follow_OutsideDeadZone_PushesToEdge) {
    IsometricCamera camera;

    camera.Follow({ 4.0f, 0.0f });

    EXPECT_FLOAT_EQ(camera.GetFocus().x, 4.0f - IsometricCamera::DeadZoneRadius);
    EXPECT_FLOAT_EQ(camera.GetFocus().y, 0.0f);
}

TEST(IsometricCameraTests, SnapTo_MovesFocusImmediately) {
    IsometricCamera camera;

    camera.SnapTo({ -5.0f, 7.0f });

    EXPECT_FLOAT_EQ(camera.GetFocus().x, -5.0f);
    EXPECT_FLOAT_EQ(camera.GetFocus().y, 7.0f);
    EXPECT_FLOAT_EQ(camera.GetView().target.z, 7.0f);
}

TEST(IsometricCameraTests, Rotate_ControlsSwitchAtOnce_ViewEases) {
    IsometricCamera camera;

    camera.RotateClockwise();

    EXPECT_FLOAT_EQ(camera.GetTargetYawDegrees(), 135.0f);
    EXPECT_FLOAT_EQ(camera.GetYawDegrees(), 45.0f);
    EXPECT_TRUE(camera.IsRotating());

    camera.Update(IsometricCamera::RotationSeconds * 0.5f);
    EXPECT_NEAR(camera.GetYawDegrees(), 90.0f, 1e-3);  // smoothstep midpoint

    camera.Update(IsometricCamera::RotationSeconds);
    EXPECT_FALSE(camera.IsRotating());
    EXPECT_FLOAT_EQ(camera.GetYawDegrees(), 135.0f);
}

TEST(IsometricCameraTests, Rotate_WrapsWithinFullTurn) {
    IsometricCamera camera;

    camera.RotateCounterClockwise();
    EXPECT_FLOAT_EQ(camera.GetTargetYawDegrees(), 315.0f);
    camera.Update(1.0f);
    EXPECT_FLOAT_EQ(camera.GetYawDegrees(), 315.0f);

    for (int i = 0; i < 4; ++i) camera.RotateClockwise();
    EXPECT_FLOAT_EQ(camera.GetTargetYawDegrees(), 315.0f);
}

TEST(IsometricCameraTests, Rotate_MidEase_ContinuesFromShownYaw) {
    IsometricCamera camera;
    camera.RotateClockwise();
    camera.Update(IsometricCamera::RotationSeconds * 0.5f);
    const float shown = camera.GetYawDegrees();

    camera.RotateClockwise();

    EXPECT_NEAR(camera.GetYawDegrees(), shown, 1e-3);  // no jump
    EXPECT_FLOAT_EQ(camera.GetTargetYawDegrees(), 225.0f);
}

TEST(IsometricCameraTests, Zoom_ClampsAndEases) {
    IsometricCamera camera;

    camera.ZoomIn();
    camera.ZoomIn();
    EXPECT_EQ(camera.GetZoom(), ZoomLevel::Close);
    EXPECT_FLOAT_EQ(camera.GetDistance(), IsometricCamera::MediumDistance);  // not eased yet

    camera.Update(0.05f);
    EXPECT_LT(camera.GetDistance(), IsometricCamera::MediumDistance);
    EXPECT_GT(camera.GetDistance(), IsometricCamera::CloseDistance);

    for (int i = 0; i < 120; ++i) camera.Update(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(camera.GetDistance(), IsometricCamera::CloseDistance);

    camera.ZoomOut();
    camera.ZoomOut();
    camera.ZoomOut();
    EXPECT_EQ(camera.GetZoom(), ZoomLevel::Wide);
}

// ============================================================================
// Model Loading Tests
// ============================================================================

namespace {
    // Assets/Models in the repository, found by walking up from the working directory
    std::filesystem::path ModelsDirectory() {
        for (auto dir = std::filesystem::current_path(); ; dir = dir.parent_path()) {
            if (std::filesystem::exists(dir / "Assets" / "Models" / "wall.glb")) return dir / "Assets" / "Models";
            if (dir == dir.root_path()) return {};
        }
    }

    std::string Base64(const std::vector<std::uint8_t>& bytes) {
        static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        for (std::size_t i = 0; i < bytes.size(); i += 3) {
            const std::uint32_t chunk = (bytes[i] << 16) | ((i + 1 < bytes.size() ? bytes[i + 1] : 0) << 8) | (i + 2 < bytes.size() ? bytes[i + 2] : 0);
            out += table[(chunk >> 18) & 63];
            out += table[(chunk >> 12) & 63];
            out += i + 1 < bytes.size() ? table[(chunk >> 6) & 63] : '=';
            out += i + 2 < bytes.size() ? table[chunk & 63] : '=';
        }
        return out;
    }

    template<typename T>
    void Append(std::vector<std::uint8_t>& bytes, std::initializer_list<T> values) {
        for (T value : values) {
            const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
            bytes.insert(bytes.end(), raw, raw + sizeof(T));
        }
    }

    // One triangle in a node translated by (1, 2, 3), red material; mode 4 = triangles
    std::string TriangleGltf(int mode = 4) {
        std::vector<std::uint8_t> buffer;
        Append<float>(buffer, { 0, 0, 0,  1, 0, 0,  0, 0, 1 });    // positions, 36 bytes
        Append<float>(buffer, { 0, 1, 0,  0, 1, 0,  0, 1, 0 });    // normals, 36 bytes
        Append<std::uint16_t>(buffer, { 0, 2, 1, 0 });             // indices + padding, 8 bytes

        return std::format(R"({{
  "asset": {{ "version": "2.0" }},
  "scene": 0,
  "scenes": [ {{ "nodes": [ 0 ] }} ],
  "nodes": [ {{ "name": "Tri", "mesh": 0, "translation": [ 1, 2, 3 ] }} ],
  "materials": [ {{ "name": "Red", "pbrMetallicRoughness": {{ "baseColorFactor": [ 1, 0, 0, 1 ] }} }} ],
  "meshes": [ {{ "name": "TriMesh", "primitives": [ {{ "attributes": {{ "POSITION": 0, "NORMAL": 1 }}, "indices": 2, "material": 0, "mode": {} }} ] }} ],
  "buffers": [ {{ "byteLength": {}, "uri": "data:application/octet-stream;base64,{}" }} ],
  "bufferViews": [
    {{ "buffer": 0, "byteOffset": 0, "byteLength": 36 }},
    {{ "buffer": 0, "byteOffset": 36, "byteLength": 36 }},
    {{ "buffer": 0, "byteOffset": 72, "byteLength": 6 }}
  ],
  "accessors": [
    {{ "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [ 0, 0, 0 ], "max": [ 1, 0, 1 ] }},
    {{ "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" }},
    {{ "bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR" }}
  ]
}})", mode, buffer.size(), Base64(buffer));
    }

    // A file in a fresh temporary directory, removed with the object
    struct TempFile {
        std::filesystem::path path;

        TempFile(std::string_view name, std::string_view content) {
            const auto dir = std::filesystem::temp_directory_path() / std::format("lb_model_test_{}", std::chrono::steady_clock::now().time_since_epoch().count());
            std::filesystem::create_directories(dir);
            path = dir / name;
            std::ofstream(path, std::ios::binary) << content;
        }

        ~TempFile() {
            std::error_code ec;
            std::filesystem::remove_all(path.parent_path(), ec);
        }
    };
}

TEST(ModelLoadingTests, SyntheticTriangle_ReadsGeometryTransformAndMaterial) {
    RenderingSystem system;
    TempFile file("triangle.gltf", TriangleGltf());

    const auto result = system.LoadModel(file.path);

    ASSERT_TRUE(result.Succeeded()) << result.error;
    const auto* model = system.GetModel(result.id);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->name, "triangle");
    ASSERT_EQ(model->parts.size(), 1u);
    const auto& part = model->parts[0];
    EXPECT_EQ(part.name, "Tri");
    ASSERT_EQ(part.vertices.size(), 3u);
    EXPECT_FLOAT_EQ(part.vertices[1].position.x, 1.0f);
    EXPECT_FLOAT_EQ(part.vertices[2].position.z, 1.0f);
    EXPECT_FLOAT_EQ(part.vertices[0].normal.y, 1.0f);
    EXPECT_EQ(part.indices, (std::vector<std::uint32_t>{ 0, 1, 2 }));  // file order 0 2 1, turned clockwise
    EXPECT_EQ(model->TriangleCount(), 1u);

    // Node translation applies to the part and the bounds
    EXPECT_FLOAT_EQ(part.transform.Translation().x, 1.0f);
    EXPECT_FLOAT_EQ(part.transform.Translation().y, 2.0f);
    EXPECT_FLOAT_EQ(part.transform.Translation().z, 3.0f);
    EXPECT_FLOAT_EQ(model->boundsMin.x, 1.0f);
    EXPECT_FLOAT_EQ(model->boundsMax.x, 2.0f);
    EXPECT_FLOAT_EQ(model->boundsMin.y, 2.0f);
    EXPECT_FLOAT_EQ(model->boundsMax.z, 4.0f);

    ASSERT_EQ(model->materials.size(), 1u);
    EXPECT_EQ(model->materials[0].name, "Red");
    EXPECT_FLOAT_EQ(model->materials[0].baseColor[0], 1.0f);
    EXPECT_FLOAT_EQ(model->materials[0].baseColor[1], 0.0f);
}

TEST(ModelLoadingTests, LinesInsteadOfTriangles_Rejected) {
    RenderingSystem system;
    TempFile file("lines.gltf", TriangleGltf(1));

    const auto result = system.LoadModel(file.path);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_NE(result.error.find("lines.gltf"), std::string::npos) << result.error;
    EXPECT_NE(result.error.find("only triangle lists"), std::string::npos) << result.error;
}

TEST(ModelLoadingTests, MissingFile_ReportsFileName) {
    RenderingSystem system;

    const auto result = system.LoadModel("does_not_exist.glb");

    EXPECT_FALSE(result.Succeeded());
    EXPECT_NE(result.error.find("does_not_exist.glb"), std::string::npos) << result.error;
}

TEST(ModelLoadingTests, InvalidFile_Rejected) {
    RenderingSystem system;
    TempFile file("broken.gltf", "{ this is not json");

    const auto result = system.LoadModel(file.path);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(system.GetModel(1), nullptr);
}

TEST(ModelLoadingTests, KayKitWall_BoundsAndTriangles) {
    const auto dir = ModelsDirectory();
    ASSERT_FALSE(dir.empty()) << "Assets/Models not found";
    RenderingSystem system;

    const auto result = system.LoadModel(dir / "wall.glb");

    ASSERT_TRUE(result.Succeeded()) << result.error;
    const auto* wall = system.GetModel(result.id);
    EXPECT_EQ(wall->name, "wall");
    EXPECT_EQ(wall->TriangleCount(), 5760u);
    EXPECT_NEAR(wall->boundsMin.x, -2.0f, 1e-3);
    EXPECT_NEAR(wall->boundsMax.x, 2.0f, 1e-3);
    EXPECT_NEAR(wall->boundsMin.y, 0.0f, 1e-3);
    EXPECT_NEAR(wall->boundsMax.y, 4.0f, 1e-3);
    EXPECT_NEAR(wall->boundsMax.z, 0.75f, 1e-3);
}

TEST(ModelLoadingTests, KayKitKnight_PartsFollowNodeHierarchy) {
    const auto dir = ModelsDirectory();
    ASSERT_FALSE(dir.empty()) << "Assets/Models not found";
    RenderingSystem system;

    const auto result = system.LoadModel(dir / "character_knight.gltf");

    ASSERT_TRUE(result.Succeeded()) << result.error;
    const auto* knight = system.GetModel(result.id);
    EXPECT_EQ(knight->materials.size(), 9u);
    for (const char* name : { "character_knightBody", "character_knightHead", "character_knightArmLeft",
                              "character_knightArmRight", "character_knightHelmet", "character_knightHelmetVisor" }) {
        EXPECT_NE(knight->FindPart(name), nullptr) << name;
    }

    // Head is a child of the body (identity); the visor is a child of the helmet
    EXPECT_NEAR(knight->FindPart("character_knightHead")->transform.Translation().y, 0.7037f, 1e-3);
    const auto visor = knight->FindPart("character_knightHelmetVisor")->transform.Translation();
    const auto helmet = knight->FindPart("character_knightHelmet")->transform.Translation();
    EXPECT_NEAR(visor.y, helmet.y + 0.3104f, 1e-3);
    EXPECT_NEAR(visor.z, helmet.z + 0.0498f, 1e-3);

    // Stands upright: taller than wide
    EXPECT_GT(knight->boundsMax.y - knight->boundsMin.y, knight->boundsMax.x - knight->boundsMin.x);
}

TEST(ModelLoadingTests, SameFileTwice_SameModel) {
    const auto dir = ModelsDirectory();
    ASSERT_FALSE(dir.empty());
    RenderingSystem system;

    const auto first = system.LoadModel(dir / "pillar.glb");
    const auto second = system.LoadModel(dir / "." / "pillar.glb");

    ASSERT_TRUE(first.Succeeded()) << first.error;
    EXPECT_EQ(first.id, second.id);
    EXPECT_EQ(system.FindModel("pillar"), first.id);
    EXPECT_FALSE(system.FindModel("nothing").has_value());
}

TEST(ModelLoadingTests, AllShippedModels_Load) {
    const auto dir = ModelsDirectory();
    ASSERT_FALSE(dir.empty());
    RenderingSystem system;

    std::size_t loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const auto extension = entry.path().extension();
        if (extension != ".glb" && extension != ".gltf") continue;
        const auto result = system.LoadModel(entry.path());
        EXPECT_TRUE(result.Succeeded()) << result.error;
        ++loaded;
    }
    EXPECT_GE(loaded, 8u);
}

TEST(ModelLoadingTests, DrawModel_QueuesKnownModelsOnly) {
    RenderingSystem system;
    system.Initialize(800, 600);
    TempFile file("triangle.gltf", TriangleGltf());
    const auto id = system.LoadModel(file.path).id;
    system.BeginFrame();

    system.DrawModel(ModelRenderData{ id, { 1.0f, 0.0f, 2.0f }, 0.5f, 2.0f });
    system.DrawModel(ModelRenderData{ 42 });
    system.DrawModel(ModelRenderData{ InvalidModelId });

    ASSERT_EQ(system.GetQueuedModels().size(), 1u);
    EXPECT_EQ(system.GetQueuedModels()[0].model, id);
    EXPECT_FLOAT_EQ(system.GetQueuedModels()[0].scale, 2.0f);

    system.EndFrame();
    EXPECT_EQ(system.GetStats().modelsRendered, 1u);
    EXPECT_EQ(system.GetStats().drawCalls, 1u);  // one part

    system.BeginFrame();
    EXPECT_TRUE(system.GetQueuedModels().empty());
}

TEST(ModelLoadingTests, Shutdown_ForgetsModels) {
    RenderingSystem system;
    system.Initialize(800, 600);
    TempFile file("triangle.gltf", TriangleGltf());
    const auto id = system.LoadModel(file.path).id;

    system.Shutdown();

    EXPECT_EQ(system.GetModel(id), nullptr);
}

// ============================================================================
// Matrix and Quaternion Tests
// ============================================================================

namespace {
    void ExpectNear(Vector3 actual, Vector3 expected, float tolerance = 1e-4f) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }

    void ExpectIdentity(const Matrix4& matrix, float tolerance = 1e-3f) {
        const Matrix4 identity;
        for (std::size_t i = 0; i < 16; ++i) EXPECT_NEAR(matrix.m[i], identity.m[i], tolerance) << "element " << i;
    }
}

TEST(MatrixTests, Multiply_AppliesLeftThenRight) {
    const auto moveThenTurn = Matrix4::Translation({ 1.0f, 0.0f, 0.0f }) * Matrix4::RotationY(std::numbers::pi_v<float> * 0.5f);

    // (1, 0, 0) turned a quarter around Y lands on -Z (DirectXMath convention)
    ExpectNear(moveThenTurn.TransformPoint({}), { 0.0f, 0.0f, -1.0f });
}

TEST(MatrixTests, RotationY_QuarterTurn_TurnsPlusZToPlusX) {
    ExpectNear(Matrix4::RotationY(std::numbers::pi_v<float> * 0.5f).TransformDirection({ 0.0f, 0.0f, 1.0f }), { 1.0f, 0.0f, 0.0f });
}

TEST(MatrixTests, FromTranslationRotationScale_ScalesRotatesThenMoves) {
    const auto q = Quaternion::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, std::numbers::pi_v<float> * 0.5f);
    const auto trs = Matrix4::FromTranslationRotationScale({ 5.0f, 0.0f, 0.0f }, q, { 2.0f, 2.0f, 2.0f });

    ExpectNear(trs.TransformPoint({ 0.0f, 0.0f, 1.0f }), { 7.0f, 0.0f, 0.0f });
}

TEST(MatrixTests, Placement_MatchesScaleYawMove) {
    const auto placement = Matrix4::Placement({ 1.0f, 2.0f, 3.0f }, std::numbers::pi_v<float>, 0.5f);

    ExpectNear(placement.TransformPoint({ 0.0f, 0.0f, 2.0f }), { 1.0f, 2.0f, 2.0f });
}

TEST(QuaternionTests, Slerp_HalfwayBetweenTurns) {
    const Vector3 up{ 0.0f, 1.0f, 0.0f };
    const auto none = Quaternion::FromAxisAngle(up, 0.0f);
    const auto quarter = Quaternion::FromAxisAngle(up, std::numbers::pi_v<float> * 0.5f);

    const auto half = Quaternion::Slerp(none, quarter, 0.5f);
    const auto eighth = Quaternion::FromAxisAngle(up, std::numbers::pi_v<float> * 0.25f);
    EXPECT_NEAR(half.y, eighth.y, 1e-5f);
    EXPECT_NEAR(half.w, eighth.w, 1e-5f);
}

TEST(QuaternionTests, Slerp_TakesShortestPath) {
    const Vector3 up{ 0.0f, 1.0f, 0.0f };
    const auto a = Quaternion::FromAxisAngle(up, 0.1f);
    auto b = Quaternion::FromAxisAngle(up, 0.3f);
    b = { -b.x, -b.y, -b.z, -b.w };   // same rotation, opposite sign

    const auto mid = Quaternion::Slerp(a, b, 0.5f);
    const auto expected = Quaternion::FromAxisAngle(up, 0.2f);
    EXPECT_NEAR(std::abs(mid.y), expected.y, 1e-5f);
    EXPECT_NEAR(std::abs(mid.w), expected.w, 1e-5f);
}

// ============================================================================
// Animation Sampling Tests (synthetic two-joint skeleton)
// ============================================================================

namespace {
    // A root joint and a child one unit above it
    Skeleton TwoJointSkeleton() {
        Skeleton skeleton;
        Joint root;
        root.name = "root";
        Joint child;
        child.name = "child";
        child.parent = 0;
        child.translation = { 0.0f, 1.0f, 0.0f };
        child.inverseBind = Matrix4::Translation({ 0.0f, -1.0f, 0.0f });
        skeleton.joints = { root, child };
        skeleton.evaluationOrder = { 0, 1 };
        return skeleton;
    }

    // The root slides from x = 0 to x = 2 over two seconds
    AnimationClip SlideClip() {
        AnimationClip clip;
        clip.name = "Slide";
        clip.durationSeconds = 2.0f;
        clip.channels.push_back(AnimationChannel{ "root", AnimationPath::Translation, { 0.0f, 2.0f }, { 0.0f, 0.0f, 0.0f,  2.0f, 0.0f, 0.0f } });
        clip.channels.push_back(AnimationChannel{ "not_in_skeleton", AnimationPath::Translation, { 0.0f }, { 9.0f, 9.0f, 9.0f } });
        return clip;
    }
}

TEST(AnimationSamplingTests, RestPose_SkinMatricesAreIdentity) {
    const auto skeleton = TwoJointSkeleton();

    const auto skin = ComputeSkinMatrices(skeleton, ComputeJointTransforms(skeleton, RestPose(skeleton)));

    ASSERT_EQ(skin.size(), 2u);
    ExpectIdentity(skin[0]);
    ExpectIdentity(skin[1]);
}

TEST(AnimationSamplingTests, SampleClip_InterpolatesAndChildFollowsParent) {
    const auto skeleton = TwoJointSkeleton();

    const auto pose = SampleClip(skeleton, SlideClip(), 0.5f, true);
    const auto joints = ComputeJointTransforms(skeleton, pose);

    ExpectNear(joints[0].Translation(), { 0.5f, 0.0f, 0.0f });
    ExpectNear(joints[1].Translation(), { 0.5f, 1.0f, 0.0f });   // child keeps its rest offset
}

TEST(AnimationSamplingTests, SampleClip_LoopingWrapsTime) {
    const auto skeleton = TwoJointSkeleton();

    const auto pose = SampleClip(skeleton, SlideClip(), 2.5f, true);

    EXPECT_NEAR(pose[0].translation.x, 0.5f, 1e-5f);
}

TEST(AnimationSamplingTests, SampleClip_NotLoopingHoldsLastFrame) {
    const auto skeleton = TwoJointSkeleton();

    const auto pose = SampleClip(skeleton, SlideClip(), 5.0f, false);

    EXPECT_NEAR(pose[0].translation.x, 2.0f, 1e-5f);
}

TEST(AnimationSamplingTests, BlendPoses_Midway) {
    const auto skeleton = TwoJointSkeleton();
    const auto start = SampleClip(skeleton, SlideClip(), 0.0f, false);
    const auto end = SampleClip(skeleton, SlideClip(), 2.0f, false);

    const auto mixed = BlendPoses(start, end, 0.25f);

    EXPECT_NEAR(mixed[0].translation.x, 0.5f, 1e-5f);
}

TEST(AnimationSamplingTests, PoseFor_UnknownClip_RestPose) {
    const auto skeleton = TwoJointSkeleton();
    AnimationState state;
    state.current.clip = 7;

    const auto pose = PoseFor(skeleton, state, [](AnimationId) -> const AnimationClip* { return nullptr; });

    EXPECT_FLOAT_EQ(pose[1].translation.y, 1.0f);
}

TEST(AnimationSamplingTests, PoseFor_CrossFadesFromPreviousClip) {
    const auto skeleton = TwoJointSkeleton();
    const auto slide = SlideClip();
    AnimationState state;
    state.previous = { 1, 2.0f, false };   // root at x = 2
    state.current = { 2, 0.0f, false };    // a clip that is not found: rest pose, x = 0
    state.blend = 0.75f;

    const auto pose = PoseFor(skeleton, state, [&](AnimationId id) { return id == 1 ? &slide : nullptr; });

    EXPECT_NEAR(pose[0].translation.x, 0.5f, 1e-5f);
}

// ============================================================================
// Skinned Model and Animation Loading Tests (KayKit Skeletons)
// ============================================================================

TEST(SkinnedModelTests, SkeletonMinion_LoadsSkeletonTextureAndGlow) {
    const auto dir = ModelsDirectory();
    ASSERT_FALSE(dir.empty());
    RenderingSystem system;

    const auto result = system.LoadModel(dir / "Skeleton_Minion.glb");
    ASSERT_TRUE(result.Succeeded()) << result.error;
    const auto& minion = *system.GetModel(result.id);

    EXPECT_TRUE(minion.IsSkinned());
    EXPECT_EQ(minion.skeleton.joints.size(), 23u);   // KayKit Rig_Medium
    EXPECT_TRUE(minion.skeleton.FindJoint("handslot.r").has_value());
    EXPECT_TRUE(minion.skeleton.FindJoint("head").has_value());
    for (const auto& part : minion.parts) {
        EXPECT_TRUE(part.IsSkinned()) << part.name;
        EXPECT_EQ(part.skin.size(), part.vertices.size()) << part.name;
    }

    // One 1024x1024 atlas with a full mip chain, used by the bone material
    ASSERT_EQ(minion.textures.size(), 1u);
    EXPECT_EQ(minion.textures[0]->Width(), 1024u);
    EXPECT_EQ(minion.textures[0]->mips.size(), 11u);
    EXPECT_EQ(minion.textures[0]->mips.back().width, 1u);
    const auto& bone = minion.materials[minion.parts[0].materialIndex];
    EXPECT_EQ(bone.baseColorTexture, std::optional<std::uint32_t>(0));

    // The eyes glow
    const auto glow = std::ranges::find(minion.materials, std::string("Glow"), &ModelMaterial::name);
    ASSERT_NE(glow, minion.materials.end());
    EXPECT_GT(glow->emissive[1], 0.5f);

    // Bind pose stands on the origin, about 2.2 units tall
    EXPECT_NEAR(minion.boundsMin.y, 0.0f, 0.01f);
    EXPECT_NEAR(minion.boundsMax.y, 2.17f, 0.05f);
}

TEST(SkinnedModelTests, RestPose_SkinMatricesAreIdentity) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;
    const auto& minion = *system.GetModel(system.LoadModel(dir / "Skeleton_Minion.glb").id);

    const auto skin = ComputeSkinMatrices(minion.skeleton, ComputeJointTransforms(minion.skeleton, RestPose(minion.skeleton)));

    for (const auto& matrix : skin) ExpectIdentity(matrix);
}

TEST(SkinnedModelTests, SkeletonWarrior_HelmetRidesOnTheHeadJoint) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;
    const auto result = system.LoadModel(dir / "Skeleton_Warrior.glb");
    ASSERT_TRUE(result.Succeeded()) << result.error;
    const auto& warrior = *system.GetModel(result.id);

    // A rigid mesh under the head bone becomes a part skinned entirely to it, in model space
    const auto* helmet = warrior.FindPart("Skeleton_Warrior_Helmet");
    ASSERT_NE(helmet, nullptr);
    const auto head = *warrior.skeleton.FindJoint("head");
    ASSERT_TRUE(helmet->IsSkinned());
    for (const auto& vertex : helmet->skin) {
        EXPECT_EQ(vertex.joints[0], head);
        EXPECT_FLOAT_EQ(vertex.weights[0], 1.0f);
    }
    float lowest = 100.0f;
    for (const auto& vertex : helmet->vertices) lowest = std::min(lowest, vertex.position.y);
    EXPECT_GT(lowest, 1.0f);   // on the head, not at the feet
}

TEST(SkinnedModelTests, SameImage_SharedBetweenModels) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;

    const auto blade = system.LoadModel(dir / "Skeleton_Blade.gltf");
    const auto axe = system.LoadModel(dir / "Skeleton_Axe.gltf");
    ASSERT_TRUE(blade.Succeeded()) << blade.error;
    ASSERT_TRUE(axe.Succeeded()) << axe.error;

    EXPECT_FALSE(system.GetModel(blade.id)->IsSkinned());
    ASSERT_EQ(system.GetModel(blade.id)->textures.size(), 1u);
    EXPECT_EQ(system.GetModel(blade.id)->textures[0], system.GetModel(axe.id)->textures[0]);
}

TEST(AnimationLoadingTests, RigMediumGeneral_LoadsNamedClips) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;

    const auto result = system.LoadAnimations(dir / "Animations" / "Rig_Medium_General.glb");
    ASSERT_TRUE(result.Succeeded()) << result.error;

    EXPECT_EQ(result.clips.size(), 15u);
    for (std::string_view name : { "Idle_A", "Hit_A", "Death_A", "Spawn_Ground", "Throw" }) {
        const auto id = system.FindAnimation(name);
        ASSERT_TRUE(id.has_value()) << name;
        EXPECT_GT(system.GetAnimation(*id)->durationSeconds, 0.0f) << name;
        EXPECT_FALSE(system.GetAnimation(*id)->channels.empty()) << name;
    }
    EXPECT_FALSE(system.FindAnimation("Walking_A").has_value());   // in the movement set
}

TEST(AnimationLoadingTests, SameFileTwice_SameClips) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;

    const auto first = system.LoadAnimations(dir / "Animations" / "Rig_Medium_MovementBasic.glb");
    const auto second = system.LoadAnimations(dir / "Animations" / "." / "Rig_Medium_MovementBasic.glb");

    ASSERT_TRUE(first.Succeeded()) << first.error;
    EXPECT_EQ(first.clips, second.clips);
}

TEST(AnimationLoadingTests, FileWithoutAnimations_ReportsFileName) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;

    const auto result = system.LoadAnimations(dir / "wall.glb");

    EXPECT_FALSE(result.Succeeded());
    EXPECT_NE(result.error.find("wall.glb: the file has no animations"), std::string::npos) << result.error;
}

TEST(AnimationLoadingTests, GetJointTransform_HandMovesWithTheWalk) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;
    const auto minion = system.LoadModel(dir / "Skeleton_Minion.glb").id;
    system.LoadAnimations(dir / "Animations" / "Rig_Medium_MovementBasic.glb");
    const auto walk = *system.FindAnimation("Walking_A");

    AnimationState early;
    early.current = { walk, 0.0f, true };
    AnimationState later;
    later.current = { walk, 0.4f, true };
    const auto a = system.GetJointTransform(minion, "handslot.r", early);
    const auto b = system.GetJointTransform(minion, "handslot.r", later);

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    const auto pa = a->Translation();
    const auto pb = b->Translation();
    EXPECT_GT(std::abs(pa.x - pb.x) + std::abs(pa.y - pb.y) + std::abs(pa.z - pb.z), 0.01f);
    EXPECT_FALSE(system.GetJointTransform(minion, "tail", early).has_value());
    EXPECT_FALSE(system.GetJointTransform(InvalidModelId, "handslot.r", early).has_value());
}

TEST(AnimationLoadingTests, DrawModel_KeepsAnimationAndTransform) {
    const auto dir = ModelsDirectory();
    RenderingSystem system;
    system.Initialize(800, 600);
    const auto minion = system.LoadModel(dir / "Skeleton_Minion.glb").id;

    ModelRenderData draw{ minion, { 1.0f, 0.0f, 2.0f } };
    draw.animation.current = { 3, 0.5f, false };
    draw.transform = Matrix4::Translation({ 0.0f, 1.0f, 0.0f });
    system.BeginFrame();
    system.DrawModel(draw);

    ASSERT_EQ(system.GetQueuedModels().size(), 1u);
    EXPECT_EQ(system.GetQueuedModels()[0].animation.current.clip, 3u);
    ASSERT_TRUE(system.GetQueuedModels()[0].transform.has_value());
    system.EndFrame();   // headless: no posing, but batching still counts the parts
    EXPECT_EQ(system.GetStats().drawCalls, system.GetModel(minion)->parts.size());
}

// ============================================================================
// Bitmap Font and Text Tests
// ============================================================================

TEST(BitmapFontTests, Measure_FixedWidthTimesScale) {
    const auto size = BitmapFont::Measure("12", 3.0f);

    EXPECT_FLOAT_EQ(size.width, (2 * BitmapFont::Advance - 1) * 3.0f);
    EXPECT_FLOAT_EQ(size.height, BitmapFont::GlyphHeight * 3.0f);
    EXPECT_FLOAT_EQ(BitmapFont::Measure("", 3.0f).width, 0.0f);
    EXPECT_FLOAT_EQ(RenderingSystem{}.MeasureText("MISS", 2.0f).width, BitmapFont::Measure("MISS", 2.0f).width);
}

TEST(BitmapFontTests, Glyphs_DigitOneIsAColumn) {
    // "1": the stem is the middle column on every row but the last (the base spans three)
    for (int y = 0; y < BitmapFont::GlyphHeight; ++y) EXPECT_TRUE(BitmapFont::Pixel('1', 2, y)) << y;
    EXPECT_FALSE(BitmapFont::Pixel('1', 0, 3));
    EXPECT_FALSE(BitmapFont::Pixel(' ', 2, 3));
    EXPECT_FALSE(BitmapFont::Pixel('1', 5, 0));   // outside the glyph
}

TEST(BitmapFontTests, LowerCaseAndUnknown_MapToCapitalsAndQuestionMark) {
    EXPECT_EQ(BitmapFont::GlyphIndex('m'), BitmapFont::GlyphIndex('M'));
    EXPECT_EQ(BitmapFont::GlyphIndex('~'), BitmapFont::GlyphIndex('?'));
    EXPECT_EQ(BitmapFont::GlyphIndex('\n'), BitmapFont::GlyphIndex('?'));
}

TEST(BitmapFontTests, Atlas_HasGlyphsInAlphaAndAWhiteStrip) {
    const auto atlas = BitmapFont::BuildAtlas();
    ASSERT_EQ(atlas.size(), static_cast<std::size_t>(BitmapFont::AtlasWidth * BitmapFont::AtlasHeight * 4));

    const auto alpha = [&](int x, int y) { return atlas[static_cast<std::size_t>((y * BitmapFont::AtlasWidth + x) * 4 + 3)]; };
    const auto [x, y] = BitmapFont::GlyphOrigin('1');
    EXPECT_EQ(alpha(x + 2, y), 255);
    EXPECT_EQ(alpha(x, y + 3), 0);
    EXPECT_EQ(alpha(BitmapFont::AtlasWidth / 2, BitmapFont::WhiteTop + 2), 255);
}

TEST(RenderingSystemTests, DrawText_KeepsOutlineAndClearsNextFrame) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();

    system.DrawText(TextRenderData{ "12!", { 10, 20 }, {}, Color::Yellow(), 4.0f, 20, true });

    ASSERT_EQ(system.GetQueuedText().size(), 1u);
    EXPECT_TRUE(system.GetQueuedText()[0].outline);
    EXPECT_EQ(system.GetQueuedText()[0].text, "12!");
    system.EndFrame();
    EXPECT_EQ(system.GetStats().textRendered, 1u);
    EXPECT_EQ(system.GetStats().drawCalls, 1u);   // the overlay is one draw

    system.BeginFrame();
    EXPECT_TRUE(system.GetQueuedText().empty());
}

TEST(RenderingSystemTests, RectanglesAndText_OneOverlayDraw) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();

    system.DrawRectangle(Rectangle{ 0, 0, 10, 10 }, Color::Red());
    system.DrawRectangle(Rectangle{ 0, 0, 10, 10 }, Color::Blue());
    system.DrawText(TextRenderData{ "HP" });
    system.EndFrame();

    EXPECT_EQ(system.GetStats().drawCalls, 1u);
}

// ============================================================================
// TrueType Font Tests (Assets/Fonts)
// ============================================================================

namespace {
    std::filesystem::path FontsDirectory() {
        const auto models = ModelsDirectory();
        return models.empty() ? std::filesystem::path{} : models.parent_path() / "Fonts";
    }
}

TEST(FontTests, MedievalSharp_BakesGlyphsAndMetrics) {
    RenderingSystem system;
    const auto result = system.LoadFont(FontsDirectory() / "MedievalSharp-Regular.ttf");
    ASSERT_TRUE(result.Succeeded()) << result.error;
    const auto& font = *system.GetFont(result.id);

    EXPECT_EQ(font.name, "MedievalSharp-Regular");
    EXPECT_EQ(system.FindFont("MedievalSharp-Regular"), std::optional<FontId>(result.id));
    EXPECT_GT(font.capHeight, RenderingSystem::FontBakeSize * 0.4f);
    EXPECT_LT(font.capHeight, RenderingSystem::FontBakeSize);
    for (char32_t c : { U'0', U'9', U'A', U'z', U'!', char32_t{ 0xF1 }, char32_t{ 0xE9 } }) {   // n and e with accents
        ASSERT_NE(font.Find(c), nullptr) << static_cast<std::uint32_t>(c);
        EXPECT_GT(font.Find(c)->advance, 0.0f);
    }
    EXPECT_EQ(font.Find(U' ')->width, 0.0f);   // no quad for spaces
    EXPECT_EQ(font.atlas.Width(), 512u);
    EXPECT_TRUE(std::has_single_bit(font.atlas.Height()));
}

TEST(FontTests, Glyph_DistanceFieldInAlpha) {
    RenderingSystem system;
    const auto& font = *system.GetFont(system.LoadFont(FontsDirectory() / "MedievalSharp-Regular.ttf").id);
    const auto& glyph = *font.Find(U'I');
    const auto& pixels = font.atlas.mips[0].pixels;
    const auto alpha = [&](float x, float y) {
        return pixels[static_cast<std::size_t>((static_cast<int>(y) * static_cast<int>(font.atlas.Width()) + static_cast<int>(x)) * 4 + 3)];
    };

    // Inside the stroke above the edge value, the padding corner below it
    EXPECT_GT(alpha(glyph.atlasX + glyph.width * 0.5f, glyph.atlasY + glyph.height * 0.5f), 128);
    EXPECT_LT(alpha(glyph.atlasX + 1.0f, glyph.atlasY + 1.0f), 128);
}

TEST(FontTests, MeasureText_UsesTheDefaultFont) {
    RenderingSystem system;
    const auto pixel = system.MeasureText("WAVE 12", 3.0f);
    ASSERT_TRUE(system.LoadFont(FontsDirectory() / "MedievalSharp-Regular.ttf").Succeeded());

    const auto ttf = system.MeasureText("WAVE 12", 3.0f);
    EXPECT_FLOAT_EQ(ttf.height, BitmapFont::GlyphHeight * 3.0f);   // capitals match the pixel font
    EXPECT_GT(ttf.width, 0.0f);
    EXPECT_NE(ttf.width, pixel.width);
    EXPECT_FLOAT_EQ(system.MeasureText("WAVE 12", 3.0f, "pixel").width, pixel.width);
    EXPECT_NEAR(system.MeasureText("WAVE 12", 6.0f).width, ttf.width * 2.0f, 1e-3f);
    EXPECT_GT(system.MeasureText("MMMM", 3.0f).width, system.MeasureText("iiii", 3.0f).width);   // proportional
}

TEST(FontTests, SameFileTwice_SameFont_MissingFile_Error) {
    RenderingSystem system;
    const auto first = system.LoadFont(FontsDirectory() / "MedievalSharp-Regular.ttf");
    const auto second = system.LoadFont(FontsDirectory() / "." / "MedievalSharp-Regular.ttf");
    EXPECT_EQ(first.id, second.id);

    const auto missing = system.LoadFont(FontsDirectory() / "Nope.ttf");
    EXPECT_FALSE(missing.Succeeded());
    EXPECT_NE(missing.error.find("Nope.ttf: file not found"), std::string::npos) << missing.error;

    const auto notAFont = system.LoadFont(ModelsDirectory() / "wall.glb");
    EXPECT_FALSE(notAFont.Succeeded());
}

TEST(FontTests, DecodeUtf8_AccentsAndInvalidBytes) {
    EXPECT_TRUE(DecodeUtf8("A\xC3\xB1") == (std::u32string{ U'A', char32_t{ 0xF1 } }));    // A, n with tilde
    EXPECT_TRUE(DecodeUtf8("\xE2\x82\xAC") == std::u32string(1, char32_t{ 0x20AC }));    // euro sign
    EXPECT_TRUE(DecodeUtf8("\xFF") == std::u32string(U"?"));
    EXPECT_TRUE(DecodeUtf8("\xC3") == std::u32string(U"?"));                                      // cut short
}

// ============================================================================
// Lighting Tests
// ============================================================================

TEST(LightingTests, DrawLight_QueuedForOneFrame) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();

    system.DrawLight(PointLight{ { 1.0f, 1.5f, 2.0f }, Color{ 1.0f, 0.5f, 0.2f }, 6.0f, 1.5f });

    ASSERT_EQ(system.GetQueuedLights().size(), 1u);
    EXPECT_FLOAT_EQ(system.GetQueuedLights()[0].radius, 6.0f);
    system.EndFrame();
    system.BeginFrame();
    EXPECT_TRUE(system.GetQueuedLights().empty());
}

TEST(LightingTests, SetLighting_KeptUntilChanged) {
    RenderingSystem system;
    system.Initialize(800, 600);
    EXPECT_FLOAT_EQ(system.GetLighting().directional, 1.0f);

    system.SetLighting(SceneLighting{ 0.3f, Color{ 0.01f, 0.01f, 0.02f } });
    system.BeginFrame();
    system.EndFrame();

    EXPECT_FLOAT_EQ(system.GetLighting().directional, 0.3f);
    EXPECT_FLOAT_EQ(system.GetLighting().ambient.b, 0.02f);
}

TEST(LightingTests, DrawLight_BeforeInitialize_Ignored) {
    RenderingSystem system;
    system.DrawLight(PointLight{});

    EXPECT_TRUE(system.GetQueuedLights().empty());
}

TEST(LightingTests, DrawParticle_QueuedAndOneDraw) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();

    for (int i = 0; i < 50; ++i) system.DrawParticle(ParticleRenderData{ { 0.0f, 1.0f, 0.0f }, 0.1f });

    EXPECT_EQ(system.GetQueuedParticles().size(), 50u);
    system.EndFrame();
    EXPECT_EQ(system.GetStats().drawCalls, 1u);
    system.BeginFrame();
    EXPECT_TRUE(system.GetQueuedParticles().empty());
}

TEST(LightingTests, DustAndGlowingParticles_OneDrawEach) {
    RenderingSystem system;
    system.Initialize(800, 600);
    system.BeginFrame();

    for (int i = 0; i < 10; ++i) system.DrawParticle(ParticleRenderData{ { 0.0f, 1.0f, 0.0f }, 0.1f });
    for (int i = 0; i < 10; ++i) system.DrawParticle(ParticleRenderData{ { 0.0f, 0.1f, 0.0f }, 0.3f, Color{ 0.5f, 0.5f, 0.5f, 0.4f }, false });
    system.EndFrame();

    EXPECT_EQ(system.GetStats().drawCalls, 2u);
}

// ============================================================================
// UI Scale Tests
// ============================================================================

TEST(UiScaleTests, UiSize_IsTheScreenInUiUnits) {
    RenderingSystem system;
    system.Initialize(1600, 1200);

    system.SetUiScale(2.0f);

    std::int32_t width = 0;
    std::int32_t height = 0;
    system.GetUiSize(width, height);
    EXPECT_EQ(width, 800);
    EXPECT_EQ(height, 600);
    EXPECT_FLOAT_EQ(system.GetUiScale(), 2.0f);
}

TEST(UiScaleTests, WorldToUi_IsWorldToScreenScaledDown) {
    RenderingSystem system;
    system.Initialize(1600, 1200);
    system.SetUiScale(2.0f);
    const Vector3 point{ 1.0f, 0.5f, 2.0f };

    const auto screen = system.WorldToScreen(point);
    const auto ui = system.WorldToUi(point);

    EXPECT_FLOAT_EQ(ui.x, screen.x / 2.0f);
    EXPECT_FLOAT_EQ(ui.y, screen.y / 2.0f);
}

TEST(UiScaleTests, Scale_Clamped) {
    RenderingSystem system;
    system.SetUiScale(0.0f);
    EXPECT_FLOAT_EQ(system.GetUiScale(), 0.25f);
    system.SetUiScale(100.0f);
    EXPECT_FLOAT_EQ(system.GetUiScale(), 8.0f);
}

// ============================================================================
// UI Image Tests
// ============================================================================

namespace {
    std::filesystem::path ShippedUi(const char* file) {
        for (auto directory = std::filesystem::current_path(); !directory.empty(); directory = directory.parent_path()) {
            const auto candidate = directory / "Assets" / "UI" / file;
            if (std::filesystem::exists(candidate)) return candidate;
            if (directory == directory.parent_path()) break;
        }
        return {};
    }
}

TEST(UiImageTests, LoadImage_ShippedPng_OnceById) {
    RenderingSystem system;
    system.Initialize(800, 600);
    const auto path = ShippedUi("panel-border-014.png");
    ASSERT_FALSE(path.empty());

    const auto first = system.LoadImage(path);
    const auto again = system.LoadImage(path);

    ASSERT_TRUE(first.Succeeded()) << first.error;
    EXPECT_EQ(first.id, again.id);
    ASSERT_NE(system.GetImage(first.id), nullptr);
    EXPECT_EQ(system.GetImage(first.id)->Width(), 48u);
    EXPECT_EQ(system.GetImage(first.id)->Height(), 48u);
}

TEST(UiImageTests, LoadImage_MissingOrInvalid_Reported) {
    RenderingSystem system;
    system.Initialize(800, 600);

    EXPECT_FALSE(system.LoadImage("Z:/no/such/image.png").Succeeded());
    const auto notImage = system.LoadImage(ShippedUi("LICENSE-Kenney-FantasyUIBorders.txt"));
    EXPECT_FALSE(notImage.Succeeded());
    EXPECT_FALSE(notImage.error.empty());
}

TEST(UiImageTests, DrawImage_QueuedForTheFrame_UnknownImageIgnored) {
    RenderingSystem system;
    system.Initialize(800, 600);
    const auto image = system.LoadImage(ShippedUi("panel-border-008.png")).id;
    system.BeginFrame();

    system.DrawImage(ImageRenderData{ image, { 10, 10, 200, 40 }, Color::White(), 5, 16.0f, 12.0f });
    system.DrawImage(ImageRenderData{ 999, { 0, 0, 10, 10 } });

    ASSERT_EQ(system.GetQueuedImages().size(), 1u);
    EXPECT_FLOAT_EQ(system.GetQueuedImages()[0].nineSlice, 16.0f);
    system.EndFrame();
    system.BeginFrame();
    EXPECT_TRUE(system.GetQueuedImages().empty());
}

// Models read ahead on worker threads (PrefetchModels) load as those read one by one: the same
// meshes, the same ids in the order loaded, images shared between models
TEST(ModelLoadingTests, Prefetched_LoadAsReadOneByOne) {
    const auto dir = ModelsDirectory();
    ASSERT_FALSE(dir.empty()) << "Assets/Models not found";
    const std::vector<std::filesystem::path> files{ dir / "wall.glb", dir / "character_knight.gltf", dir / "tileBrickA_small.glb",
                                                    dir / "wall_cracked.gltf", dir / "barrel_large.gltf" };
    RenderingSystem plain;
    RenderingSystem ahead;
    EXPECT_EQ(ahead.PrefetchModels(files), files.size());
    EXPECT_EQ(ahead.PrefetchModels(files), 0u);   // read ahead already
    for (const auto& file : files) {
        const auto a = plain.LoadModel(file);
        const auto b = ahead.LoadModel(file);
        ASSERT_TRUE(a.Succeeded() && b.Succeeded()) << a.error << b.error;
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(plain.GetModel(a.id)->TriangleCount(), ahead.GetModel(b.id)->TriangleCount()) << file;
        EXPECT_EQ(plain.GetModel(a.id)->textures.size(), ahead.GetModel(b.id)->textures.size()) << file;
    }
    EXPECT_EQ(ahead.PrefetchModels(files), 0u);   // loaded: nothing to read
    // The dungeon pieces share their atlas
    const auto* wall = ahead.GetModel(ahead.LoadModel(files[3]).id);
    const auto* barrel = ahead.GetModel(ahead.LoadModel(files[4]).id);
    ASSERT_FALSE(wall->textures.empty());
    ASSERT_FALSE(barrel->textures.empty());
    EXPECT_EQ(wall->textures.front(), barrel->textures.front());   // one decoded image, read on two threads

    // A file that cannot be read fails at its load, as it would have
    RenderingSystem broken;
    EXPECT_EQ(broken.PrefetchModels(std::vector<std::filesystem::path>{ dir / "does_not_exist.glb" }), 1u);
    EXPECT_FALSE(broken.LoadModel(dir / "does_not_exist.glb").Succeeded());
}
