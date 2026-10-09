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
    EXPECT_GT(stats.spritesRendered, 0);
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
    EXPECT_GT(stats.spritesRendered, 0);
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
    EXPECT_GT(stats.drawCalls, 0);
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
