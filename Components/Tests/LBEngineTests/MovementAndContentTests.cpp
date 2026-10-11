// MovementAndContentTests.cpp - LB.Engine tests: player movement, game content, and class data
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// GameEngine Player Movement Tests
// ============================================================================

TEST(GameEngineMovementTests, NoContent_NoPlayer) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600));

    HoldKeysFor(engine, { Keys::FromChar('D') }, 10);

    EXPECT_FALSE(engine.GetPlayerId().has_value());
    EXPECT_EQ(engine.GetRendering().GetQueuedMeshes().size(), ArenaMeshCount());  // arena only
    EXPECT_TRUE(engine.SaveGame(1));
    EXPECT_TRUE(engine.LoadGame(1));
}

TEST(GameEngineMovementTests, Initialize_SpawnsPlayerAtOrigin) {
    GameEngine engine;
    InitializeWithContent(engine);

    ASSERT_TRUE(engine.GetPlayerId().has_value());
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, 0.0f);
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(character.GetClass(), LB::Character::CharacterClass::Warrior);
    EXPECT_EQ(character.GetPosition(), (LB::Position{0, 0}));
}

TEST(GameEngineMovementTests, HoldD_MovesScreenRightAtCharacterSpeed) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    // Isometric: screen-right is a ground diagonal at the default camera yaw
    const float distance = PlayerSpeed(engine) * 30 * static_cast<float>(StepSeconds(engine));
    const auto right = Ground(engine, 1.0f, 0.0f);
    EXPECT_NEAR(engine.GetPlayerPosition().x, right.x * distance, 1e-4);
    EXPECT_NEAR(engine.GetPlayerPosition().y, right.y * distance, 1e-4);
    EXPECT_GT(engine.GetPlayerPosition().x, 0.0f);
    EXPECT_LT(engine.GetPlayerPosition().y, 0.0f);
}

TEST(GameEngineMovementTests, HoldD_PlayerGoesRightOnScreen) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 20);  // stays inside the camera dead zone

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_GT(end.x, start.x + 10.0f);
    EXPECT_NEAR(end.y, start.y, 1.0f);
}

TEST(GameEngineMovementTests, HoldW_MovesUpTheScreen) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('W') }, 20);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_LT(end.y, start.y - 10.0f);
    EXPECT_NEAR(end.x, start.x, 1.0f);
}

TEST(GameEngineMovementTests, ArrowKeys_AlsoMove) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::Left, Keys::Down }, 20);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_LT(end.x, start.x);
    EXPECT_GT(end.y, start.y);
}

TEST(GameEngineMovementTests, Diagonal_NotFasterThanStraight) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D'), Keys::FromChar('S') }, 30);

    const float distance = PlayerSpeed(engine) * 30 * static_cast<float>(StepSeconds(engine));
    EXPECT_NEAR(Travelled(engine), distance, 1e-4);
    const auto direction = Ground(engine, std::sqrt(0.5f), -std::sqrt(0.5f));
    EXPECT_NEAR(engine.GetPlayerPosition().x, direction.x * distance, 1e-4);
    EXPECT_NEAR(engine.GetPlayerPosition().y, direction.y * distance, 1e-4);
}

TEST(GameEngineMovementTests, OppositeKeys_Cancel) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('A'), Keys::FromChar('D') }, 20);

    EXPECT_FLOAT_EQ(Travelled(engine), 0.0f);
}

TEST(GameEngineMovementTests, LeftStick_AnalogSpeed) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, Stick(0.5f, 0.0f));
    RunSteps(engine, 30);

    EXPECT_NEAR(Travelled(engine), 0.5 * PlayerSpeed(engine) * 30 * StepSeconds(engine), 1e-4);
}

TEST(GameEngineMovementTests, LeftStickUp_MovesUpTheScreen) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.OnGamepadState(0, Stick(0.0f, 1.0f));
    RunSteps(engine, 10);

    const auto start = OnScreen(engine, 0.0f, 0.0f);
    const auto end = OnScreen(engine, engine.GetPlayerPosition().x, engine.GetPlayerPosition().y);
    EXPECT_LT(end.y, start.y);
}

TEST(GameEngineMovementTests, Paused_DoesNotMove) {
    GameEngine engine;
    InitializeWithContent(engine);
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    EXPECT_FLOAT_EQ(Travelled(engine), 0.0f);
}

TEST(GameEngineMovementTests, ArenaEdge_StopsPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 60 * 10);  // far longer than needed to cross

    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, GameEngine::ArenaHalfWidth - GameEngine::PlayerSizeTiles * 0.5f);
}

TEST(GameEngineMovementTests, DomainTilePosition_FollowsPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 45);  // ~2.7 tiles at Warrior speed

    const auto expectedTileX = static_cast<std::int32_t>(std::floor(engine.GetPlayerPosition().x));
    const auto expectedTileY = static_cast<std::int32_t>(std::floor(engine.GetPlayerPosition().y));
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_GE(expectedTileX, 1);
    EXPECT_LE(expectedTileY, -2);
    EXPECT_EQ(character.GetPosition(), (LB::Position{expectedTileX, expectedTileY}));
}

TEST(GameEngineMovementTests, SaveLoad_RestoresPlayerPosition) {
    GameEngine engine;
    InitializeWithContent(engine);
    HoldKeysFor(engine, { Keys::FromChar('D'), Keys::FromChar('W') }, 20);
    const auto saved = engine.GetPlayerPosition();
    ASSERT_TRUE(engine.SaveGame(1));

    HoldKeysFor(engine, { Keys::FromChar('A') }, 40);
    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, saved.x);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().y, saved.y);
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(character.GetPosition().x, static_cast<std::int32_t>(std::floor(saved.x)));
}

TEST(GameEngineMovementTests, Render_DrawsArenaAndCenteredPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.Tick(OneSixtieth);

    const auto& meshes = engine.GetRendering().GetQueuedMeshes();
    ASSERT_EQ(meshes.size(), ArenaMeshCount() + PlayerMeshCount);

    // The player's body stands on the floor at the player's position...
    const auto& body = meshes[ArenaMeshCount()];
    EXPECT_EQ(body.shape, LB::Systems::Rendering::MeshShape::Cylinder);
    EXPECT_FLOAT_EQ(body.position.x, 0.0f);
    EXPECT_FLOAT_EQ(body.position.z, 0.0f);
    EXPECT_FLOAT_EQ(body.position.y - body.size.y * 0.5f, 0.0f);

    // ...in the middle of the 800x600 window, where the camera looks
    const auto feet = OnScreen(engine, 0.0f, 0.0f);
    ASSERT_TRUE(feet.inFront);
    EXPECT_NEAR(feet.x, 400.0f, 0.01f);
    EXPECT_NEAR(feet.y, 300.0f, 0.01f);
}

TEST(GameEngineMovementTests, Render_FloorTopAtGroundAndWallsAroundArena) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);
    engine.Tick(OneSixtieth);

    std::size_t walls = 0;
    for (const auto& mesh : engine.GetRendering().GetQueuedMeshes()) {
        const float top = mesh.position.y + mesh.size.y * 0.5f;
        if (top > 0.5f) {
            ++walls;
            const bool outsideX = std::abs(mesh.position.x) > GameEngine::ArenaHalfWidth;
            const bool outsideY = std::abs(mesh.position.z) > GameEngine::ArenaHalfHeight;
            EXPECT_TRUE(outsideX || outsideY) << mesh.position.x << ", " << mesh.position.z;
            EXPECT_FLOAT_EQ(top, GameEngine::WallHeight);
        }
        else {
            EXPECT_FLOAT_EQ(top, 0.0f);  // floor
        }
    }
    const auto perimeter = static_cast<std::size_t>((GameEngine::ArenaHalfWidth * 2 + 2) * 2 + GameEngine::ArenaHalfHeight * 2 * 2);
    EXPECT_EQ(walls, perimeter);
}

TEST(GameEngineMovementTests, Shutdown_RemovesPlayer) {
    GameEngine engine;
    InitializeWithContent(engine);

    engine.Shutdown();

    EXPECT_FALSE(engine.GetPlayerId().has_value());
    EXPECT_EQ(engine.GetCharacters().GetCharacterCount(), 0u);
}

// ============================================================================
// GameEngine Content Tests
// ============================================================================

TEST(GameEngineContentTests, NoContentDirectory_StartsWithEmptyContent) {
    GameEngine engine;

    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600));

    ASSERT_NE(engine.GetContent(), nullptr);
    EXPECT_TRUE(engine.GetContent()->IsEmpty());
    EXPECT_TRUE(engine.GetInitializationError().empty());
}

TEST(GameEngineContentTests, ShippedContent_Loads) {
    const auto dir = FindShippedContent();
    ASSERT_TRUE(dir.has_value());
    GameEngine engine;

    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = *dir })) << engine.GetInitializationError();

    EXPECT_EQ(engine.GetContent()->GetSpells().size(), 45u);
    EXPECT_EQ(engine.GetContent()->GetWeapons().size(), 12u);
    EXPECT_EQ(engine.GetContent()->GetArmour().size(), 15u);
    EXPECT_NE(engine.GetContent()->FindWeapon("weapon_sword_rusted_shortsword_t1"), nullptr);
}

TEST(GameEngineContentTests, InvalidContent_FailsWithAllErrors) {
    TempSaveDir dir;
    std::filesystem::create_directories(dir.path);
    std::ofstream(dir.path / "spells.json") << R"([{"id":"spell_a","name":"A","school":"Nope","level":1,"categories":["Damage"],"manaCost":5,"castTimeSeconds":1.0}])";
    std::ofstream(dir.path / "weapons.json") << "[{";
    std::ofstream(dir.path / "armour.json") << "[]";
    std::ofstream(dir.path / "classes.json") << "[]";
    std::ofstream(dir.path / "enemies.json") << "[]";
    std::ofstream(dir.path / "consumables.json") << "[]";
    std::ofstream(dir.path / "loot.json") << "[]";
    GameEngine engine;

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = dir.Utf8() }));

    const auto& error = engine.GetInitializationError();
    EXPECT_NE(error.find("invalid (2 problems)"), std::string::npos) << error;
    EXPECT_NE(error.find("'school' has unknown value 'Nope'"), std::string::npos) << error;
    EXPECT_NE(error.find("weapons.json: not valid JSON"), std::string::npos) << error;
    EXPECT_FALSE(engine.IsInitialized());
}

TEST(GameEngineContentTests, MissingContentDirectory_Fails) {
    GameEngine engine;

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = "Z:/no/such/content" }));

    EXPECT_NE(engine.GetInitializationError().find("file not found"), std::string::npos);
}

// ============================================================================
// GameEngine Class Data Tests
// ============================================================================

TEST(GameEngineClassDataTests, ShippedContent_ConfiguresPlayerFromClassData) {
    const auto dir = FindShippedContent();
    ASSERT_TRUE(dir.has_value());
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = *dir })) << engine.GetInitializationError();

    const auto* warrior = engine.GetContent()->FindClass(LB::Character::CharacterClass::Warrior);
    ASSERT_NE(warrior, nullptr);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    const float expected = warrior->baseMovementSpeed + static_cast<float>(player.GetAttributes().GetDexterityModifier()) * 0.1f;

    EXPECT_EQ(engine.GetContent()->GetClasses().size(), 4u);
    EXPECT_FLOAT_EQ(*engine.GetCharacters().GetMovementSpeed(*engine.GetPlayerId()), expected);
}

TEST(GameEngineClassDataTests, EditedClassSpeed_ChangesPlayerMovement) {
    const auto shipped = FindShippedContent();
    ASSERT_TRUE(shipped.has_value());
    TempSaveDir dir;
    std::filesystem::create_directories(dir.path);
    const std::filesystem::path source(std::u8string(shipped->begin(), shipped->end()));
    for (const char* file : { "spells.json", "weapons.json", "armour.json", "classes.json", "enemies.json", "consumables.json", "loot.json" }) {
        std::filesystem::copy_file(source / file, dir.path / file);
    }

    // Data-only change: Warrior base speed 4.0 -> 8.0
    std::string classes;
    {
        std::ifstream in(dir.path / "classes.json", std::ios::binary);
        classes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const auto at = classes.find("\"baseMovementSpeed\": 4.0");
    ASSERT_NE(at, std::string::npos);
    classes.replace(at, std::string("\"baseMovementSpeed\": 4.0").size(), "\"baseMovementSpeed\": 8.0");
    std::ofstream(dir.path / "classes.json", std::ios::binary | std::ios::trunc) << classes;

    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = dir.Utf8(), .spawnEnemies = false })) << engine.GetInitializationError();
    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);

    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    const float speed = 8.0f + static_cast<float>(player.GetAttributes().GetDexterityModifier()) * 0.1f;
    EXPECT_NEAR(Travelled(engine), speed * 30 * StepSeconds(engine), 1e-4);
}

// ============================================================================
// GameEngine Class Growth and Armour Tests
// ============================================================================

TEST(GameEngineArmorTests, Player_HasExactClassStartingAttributes) {
    GameEngine engine;
    InitializeWithContent(engine);

    const auto* warrior = engine.GetContent()->FindClass(LB::Character::CharacterClass::Warrior);
    const auto& player = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    EXPECT_EQ(player.GetAttributes().strength, warrior->startingAttributes.strength);       // 16, no hidden bonus
    EXPECT_EQ(player.GetAttributes().constitution, warrior->startingAttributes.constitution);
    EXPECT_EQ(*engine.GetCharacters().GetAttributeCap(*engine.GetPlayerId(), LB::AttributeType::Strength), 25);
    EXPECT_EQ(*engine.GetCharacters().GetAttributeCap(*engine.GetPlayerId(), LB::AttributeType::Intelligence), 18);
}

TEST(GameEngineArmorTests, HeavyBreastplate_SlowsWarriorByScaledPenalty) {
    GameEngine engine;
    InitializeWithContent(engine);
    const float unarmored = PlayerSpeed(engine);

    ASSERT_TRUE(engine.EquipPlayerArmor("armor_chest_rusted_breastplate_t1"));  // Heavy, 15% movement, 5% cast

    // Warriors handle heavy armour at 0.5x: 7.5% slower
    EXPECT_FLOAT_EQ(PlayerSpeed(engine), unarmored * (1.0f - 0.15f * 0.5f));
    EXPECT_FLOAT_EQ(*engine.GetCharacters().GetCastTimeMultiplier(*engine.GetPlayerId()), 1.0f + 0.05f);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 30);
    EXPECT_NEAR(Travelled(engine), unarmored * (1.0f - 0.075f) * 30 * StepSeconds(engine), 1e-4);
}

TEST(GameEngineArmorTests, EquipPlayerArmor_UnknownKey_Fails) {
    GameEngine engine;
    InitializeWithContent(engine);

    EXPECT_FALSE(engine.EquipPlayerArmor("armor_does_not_exist"));
}

TEST(GameEngineArmorTests, EquipPlayerArmor_WithoutPlayer_Fails) {
    GameEngine engine;
    engine.Initialize(nullptr, 800, 600);

    EXPECT_FALSE(engine.EquipPlayerArmor("armor_chest_rusted_breastplate_t1"));
}
