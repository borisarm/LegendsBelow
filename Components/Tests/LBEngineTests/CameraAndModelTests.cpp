// CameraAndModelTests.cpp - LB.Engine tests: the camera, 3D models, wall fading, and draw batching
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// GameEngine Camera Tests
// ============================================================================

TEST(GameEngineCameraTests, Initialize_CameraFocusesPlayerAtDefaults) {
    GameEngine engine;
    InitializeWithContent(engine);

    const auto& camera = engine.GetCamera();
    EXPECT_FLOAT_EQ(camera.GetFocus().x, 0.0f);
    EXPECT_FLOAT_EQ(camera.GetFocus().y, 0.0f);
    EXPECT_FLOAT_EQ(camera.GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees);
    EXPECT_EQ(camera.GetZoom(), ZoomLevel::Medium);
}

TEST(GameEngineCameraTests, KeyE_RotatesClockwise_KeyQ_Back) {
    GameEngine engine;
    InitializeWithContent(engine);

    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees + 90.0f);

    TapKey(engine, Keys::FromChar('Q'));
    RunSteps(engine, 1);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees);
}

TEST(GameEngineCameraTests, Rotation_EasesThenSettles) {
    GameEngine engine;
    InitializeWithContent(engine);

    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    EXPECT_TRUE(engine.GetCamera().IsRotating());

    RunSteps(engine, 30);  // 0.5 s > RotationSeconds
    EXPECT_FALSE(engine.GetCamera().IsRotating());
    EXPECT_FLOAT_EQ(engine.GetCamera().GetYawDegrees(), IsometricCamera::DefaultYawDegrees + 90.0f);
}

TEST(GameEngineCameraTests, AfterRotation_DStillMovesScreenRight) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    RunSteps(engine, 30);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 20);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_GT(end.x, start.x + 10.0f);
    EXPECT_NEAR(end.y, start.y, 1.0f);
}

TEST(GameEngineCameraTests, Rotation_WorksWhilePaused) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::Escape);
    RunSteps(engine, 1);
    ASSERT_TRUE(engine.IsPaused());

    TapKey(engine, Keys::FromChar('Q'));
    RunSteps(engine, 1);

    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees - 90.0f + 360.0f);
}

TEST(GameEngineCameraTests, MouseWheel_Zooms) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnMouseWheel(120);
    RunSteps(engine, 1);
    EXPECT_EQ(engine.GetCamera().GetZoom(), ZoomLevel::Close);

    engine.OnMouseWheel(-120);
    RunSteps(engine, 1);
    engine.OnMouseWheel(-120);
    RunSteps(engine, 1);
    EXPECT_EQ(engine.GetCamera().GetZoom(), ZoomLevel::Wide);

    RunSteps(engine, 60);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetDistance(), IsometricCamera::WideDistance);
}

TEST(GameEngineCameraTests, RightStickFlick_RotatesOncePerFlick) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, RightStick(1.0f, 0.0f));
    RunSteps(engine, 10);  // held: one rotation only
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees + 90.0f);

    engine.OnGamepadState(0, RightStick(0.0f, 0.0f));
    RunSteps(engine, 1);
    engine.OnGamepadState(0, RightStick(1.0f, 0.0f));
    RunSteps(engine, 1);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetTargetYawDegrees(), IsometricCamera::DefaultYawDegrees + 180.0f);
}

TEST(GameEngineCameraTests, RightStickUp_ZoomsIn) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, RightStick(0.0f, 0.9f));
    RunSteps(engine, 1);

    EXPECT_EQ(engine.GetCamera().GetZoom(), ZoomLevel::Close);
}

TEST(GameEngineCameraTests, Camera_FollowsPlayerOutsideDeadZone) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 60);  // ~4 tiles

    const auto focus = engine.GetCamera().GetFocus();
    const auto player = engine.GetPlayerPosition();
    EXPECT_NEAR(std::hypot(player.x - focus.x, player.y - focus.y), IsometricCamera::DeadZoneRadius, 1e-4);
}

TEST(GameEngineCameraTests, Camera_StaysInsideDeadZone) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 10);  // well under 1.5 tiles

    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().y, 0.0f);
}

TEST(GameEngineCameraTests, LoadGame_SnapsCameraToPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);
    HoldKeysFor(engine, { Keys::FromChar('D') }, 120);
    ASSERT_TRUE(engine.SaveGame(1));
    HoldKeysFor(engine, { Keys::FromChar('A') }, 240);

    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().x, engine.GetPlayerPosition().x);
    EXPECT_FLOAT_EQ(engine.GetCamera().GetFocus().y, engine.GetPlayerPosition().y);
}

TEST(GameEngineCameraTests, Render_UsesCameraView) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    RunSteps(engine, 30);

    const auto expected = engine.GetCamera().GetView();
    const auto& rendered = engine.GetRendering().GetCameraView();
    EXPECT_FLOAT_EQ(rendered.eye.x, expected.eye.x);
    EXPECT_FLOAT_EQ(rendered.eye.y, expected.eye.y);
    EXPECT_FLOAT_EQ(rendered.eye.z, expected.eye.z);
}

// ============================================================================
// GameEngine Model Tests
// ============================================================================

TEST(GameEngineModelTests, Render_DrawsArenaAndPlayerFromModels) {
    GameEngine engine;
    InitializeWithModels(engine);

    engine.Tick(OneSixtieth);

    const auto tiles = static_cast<std::size_t>(GameEngine::ArenaHalfWidth * 2 * GameEngine::ArenaHalfHeight * 2);
    const auto walls = static_cast<std::size_t>(2 * (GameEngine::ArenaHalfWidth + GameEngine::ArenaHalfHeight));  // perimeter / 2-tile segments
    EXPECT_EQ(DrawsOf(engine, "tileBrickB_small").size() + DrawsOf(engine, "tileBrickA_small").size(), tiles);
    EXPECT_GT(DrawsOf(engine, "tileBrickA_small").size(), 0u);
    EXPECT_EQ(DrawsOf(engine, "wall").size(), walls);
    EXPECT_EQ(DrawsOf(engine, "pillar").size(), 4u);
    EXPECT_EQ(DrawsOf(engine, "Knight").size(), 1u);
    EXPECT_TRUE(engine.GetRendering().GetQueuedMeshes().empty());
}

TEST(GameEngineModelTests, FloorTiles_TopAtGroundLevel) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.Tick(OneSixtieth);

    const auto& tile = ModelNamed(engine, "tileBrickB_small");
    for (const auto& draw : DrawsOf(engine, "tileBrickB_small")) {
        EXPECT_NEAR(draw.position.y + tile.boundsMax.y * draw.scale, 0.0f, 1e-5);
        EXPECT_NEAR((tile.boundsMax.x - tile.boundsMin.x) * draw.scale, 1.0f, 0.05f);  // one tile wide
    }
}

TEST(GameEngineModelTests, Walls_InnerFaceOnArenaEdge) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.Tick(OneSixtieth);

    const auto& wall = ModelNamed(engine, "wall");
    const float halfDepth = wall.boundsMax.z * GameEngine::ModelScale;
    for (const auto& draw : DrawsOf(engine, "wall")) {
        EXPECT_NEAR(draw.position.y + wall.boundsMin.y * draw.scale, 0.0f, 1e-5);  // standing on the floor
        if (draw.yawRadians == 0.0f) {
            EXPECT_NEAR(std::abs(draw.position.z) - halfDepth, GameEngine::ArenaHalfHeight, 1e-4);
            EXPECT_LT(std::abs(draw.position.x), GameEngine::ArenaHalfWidth);
        }
        else {
            EXPECT_NEAR(std::abs(draw.position.x) - halfDepth, GameEngine::ArenaHalfWidth, 1e-4);
            EXPECT_LT(std::abs(draw.position.z), GameEngine::ArenaHalfHeight);
        }
    }
}

TEST(GameEngineModelTests, Player_StandsAtPositionFacingWalkDirection) {
    GameEngine engine;
    InitializeWithModels(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 20);

    const auto draws = DrawsOf(engine, "Knight");
    ASSERT_EQ(draws.size(), 1u);
    const auto& knight = ModelNamed(engine, "Knight");
    EXPECT_FLOAT_EQ(draws[0].position.x, engine.GetPlayerPosition().x);
    EXPECT_FLOAT_EQ(draws[0].position.z, engine.GetPlayerPosition().y);
    EXPECT_NEAR(draws[0].position.y + knight.boundsMin.y * draws[0].scale, 0.0f, 1e-5);

    // Facing (sin yaw, cos yaw) is the ground direction of screen-right
    const auto right = Ground(engine, 1.0f, 0.0f);
    EXPECT_NEAR(std::sin(draws[0].yawRadians), right.x, 1e-5);
    EXPECT_NEAR(std::cos(draws[0].yawRadians), right.y, 1e-5);
}

TEST(GameEngineModelTests, Paused_DimsModels) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.Tick(OneSixtieth);
    const float normal = engine.GetRendering().GetQueuedModels().front().tint.r;

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    EXPECT_LT(engine.GetRendering().GetQueuedModels().front().tint.r, normal * 0.5f);
}

TEST(GameEngineModelTests, MissingModels_StopStartupListingFiles) {
    GameEngine engine;
    const auto empty = std::filesystem::temp_directory_path() / "lb_no_models_here";
    std::filesystem::create_directories(empty);

    const bool ok = engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .modelDirectory = empty.string() });

    EXPECT_FALSE(ok);
    const auto& error = engine.GetInitializationError();
    // Arena (4), the knight, the mage, the rogue, the cleric, and the loot sword (5), dungeon props (4), loot props (10),
    // the potion and the arrow and bolt bundles (3), the merchant, the shrine and its candles (3),
    // the tiers' floors and walls, props, and traps (18),
    // weapons and the shield in the heroes' hands and their arrows and bolts (11),
    // enemy bodies, weapons and shots (13), animation sets (5 medium, 3 large)
    EXPECT_NE(error.find("79 problems"), std::string::npos) << error;
    EXPECT_NE(error.find("wall.glb"), std::string::npos) << error;
    EXPECT_NE(error.find("Knight.glb"), std::string::npos) << error;
    EXPECT_NE(error.find("Skeleton_Minion.glb"), std::string::npos) << error;
    EXPECT_NE(error.find("Rig_Medium_General.glb"), std::string::npos) << error;
    std::filesystem::remove_all(empty);
}

TEST(GameEngineModelTests, NoModelDirectory_FallsBackToPrimitives) {
    GameEngine engine;
    InitializeWithContent(engine);
    engine.Tick(OneSixtieth);

    EXPECT_TRUE(engine.GetRendering().GetQueuedModels().empty());
    EXPECT_FALSE(engine.GetRendering().GetQueuedMeshes().empty());
}

// ============================================================================
// GameEngine Wall Fade and Batching Tests
// ============================================================================

TEST(GameEngineWallFadeTests, PlayerInTheMiddle_NoWallFades) {
    GameEngine engine;
    InitializeWithModels(engine);

    RunSteps(engine, 30);

    const auto opacities = engine.GetWallOpacities();
    EXPECT_EQ(opacities.size(), static_cast<std::size_t>(2 * (GameEngine::ArenaHalfWidth + GameEngine::ArenaHalfHeight)) + 4);
    EXPECT_EQ(CountFaded(engine), 0u);
}

TEST(GameEngineWallFadeTests, PlayerBehindNearWall_ThatWallFades) {
    GameEngine engine;
    InitializeWithModels(engine);

    WalkToNearEdge(engine);
    RunSteps(engine, 30);   // longer than WallFadeSeconds

    // At the default yaw the camera is south-east: the faded pieces are on those sides only
    std::size_t faded = 0;
    const auto& draws = engine.GetRendering().GetQueuedModels();
    for (const auto& draw : draws) {
        const auto& name = engine.GetRendering().GetModel(draw.model)->name;
        if ((name == "wall" || name == "pillar") && draw.tint.a < 1.0f) {
            ++faded;
            EXPECT_NEAR(draw.tint.a, GameEngine::FadedWallAlpha, 1e-5);
            EXPECT_TRUE(draw.position.z > GameEngine::ArenaHalfHeight || draw.position.x > GameEngine::ArenaHalfWidth)
                << draw.position.x << ", " << draw.position.z;
        }
    }
    EXPECT_GE(faded, 1u);
    EXPECT_LE(faded, 4u);   // only the pieces in the sight lines, not the whole side
}

TEST(GameEngineWallFadeTests, Fade_EasesInAndBackOut) {
    GameEngine engine;
    InitializeWithModels(engine);
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    ASSERT_GE(CountFaded(engine), 1u);

    // Walk back to the middle: the walls come back
    HoldKeysFor(engine, { Keys::FromChar('W') }, 60 * 3);
    RunSteps(engine, 30);
    EXPECT_EQ(CountFaded(engine), 0u);
}

TEST(GameEngineWallFadeTests, Fade_TakesWallFadeSeconds) {
    GameEngine engine;
    InitializeWithModels(engine);
    ASSERT_TRUE(engine.SaveGame(1));           // player in the middle
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    const auto faded = engine.GetWallOpacities();
    const auto wall = std::ranges::find_if(faded, [](float opacity) { return opacity < 1.0f; }) - faded.begin();
    ASSERT_LT(static_cast<std::size_t>(wall), faded.size());
    EXPECT_FLOAT_EQ(faded[wall], GameEngine::FadedWallAlpha);

    ASSERT_TRUE(engine.LoadGame(1));           // back in the middle at once: nothing hides the player
    RunSteps(engine, 1);
    const float step = static_cast<float>(StepSeconds(engine)) / GameEngine::WallFadeSeconds * (1.0f - GameEngine::FadedWallAlpha);
    EXPECT_NEAR(engine.GetWallOpacities()[wall], GameEngine::FadedWallAlpha + step, 1e-5);

    RunSteps(engine, 12);                      // 0.2 s in all
    EXPECT_FLOAT_EQ(engine.GetWallOpacities()[wall], 1.0f);
}

TEST(GameEngineWallFadeTests, CameraRotationWhilePaused_ChangesWhichWallsFade) {
    GameEngine engine;
    InitializeWithModels(engine);
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    const auto before = engine.GetWallOpacities();

    TapKey(engine, Keys::Escape);
    RunSteps(engine, 1);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 1);
    TapKey(engine, Keys::FromChar('E'));
    RunSteps(engine, 60);                  // half a turn: the camera is now on the opposite side

    EXPECT_TRUE(engine.IsPaused());
    EXPECT_NE(engine.GetWallOpacities(), before);
    EXPECT_EQ(CountFaded(engine), 0u);     // the player stands with their back to the far wall
}

TEST(GameEngineBatchingTests, Arena_FewDrawCalls) {
    GameEngine engine;
    InitializeWithModels(engine);

    engine.Tick(OneSixtieth);

    // One instanced draw per part of each model: floor tiles (2 models), wall, pillar, knight, sword
    std::size_t expected = 0;
    for (std::string_view name : { "tileBrickB_small", "tileBrickA_small", "wall", "pillar", "Knight", "sword_1handed" }) {
        expected += ModelNamed(engine, name).parts.size();
    }
    expected += 1;                         // the HUD's overlay batch
    const auto& stats = engine.GetRendering().GetStats();
    EXPECT_EQ(stats.drawCalls, expected);
    EXPECT_LT(stats.drawCalls, 35u);       // was one draw per part per copy: several hundred
    EXPECT_GT(stats.modelsRendered, 300u);  // every tile and wall piece still drawn
}

TEST(GameEngineBatchingTests, FadedWalls_TwoExtraPassesPerPart) {
    GameEngine engine;
    InitializeWithModels(engine);
    WalkToNearEdge(engine);
    RunSteps(engine, 30);
    ASSERT_GE(CountFaded(engine), 1u);

    const auto& rendering = engine.GetRendering();
    const auto wallParts = ModelNamed(engine, "wall").parts.size();
    std::size_t expected = 0;
    for (std::string_view name : { "tileBrickB_small", "tileBrickA_small", "wall", "pillar", "Knight", "sword_1handed" }) {
        expected += ModelNamed(engine, name).parts.size();
    }
    bool pillarFaded = false;
    for (const auto& draw : rendering.GetQueuedModels()) {
        if (rendering.GetModel(draw.model)->name == "pillar" && draw.tint.a < 1.0f) pillarFaded = true;
    }
    expected += 1;                         // the HUD's overlay batch
    expected += 2 * wallParts;
    if (pillarFaded) expected += 2 * ModelNamed(engine, "pillar").parts.size();
    EXPECT_EQ(rendering.GetStats().drawCalls, expected);
}
