#include "pch.h"

import LB.RenderingSystem;

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
    EXPECT_EQ(stats.drawCalls, 0);
    EXPECT_EQ(stats.spritesRendered, 0);
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
    EXPECT_EQ(stats.spritesRendered, 1);
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
    EXPECT_EQ(stats.spritesRendered, 10);
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
    EXPECT_EQ(stats.textRendered, 1);
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
    EXPECT_EQ(stats.textRendered, 5);
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
    EXPECT_EQ(stats.spritesRendered, 2);
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

    EXPECT_EQ(stats.drawCalls, 0);
    EXPECT_EQ(stats.spritesRendered, 0);
    EXPECT_EQ(stats.textRendered, 0);
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
    EXPECT_EQ(stats.spritesRendered, 1);
    EXPECT_EQ(stats.textRendered, 1);
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
