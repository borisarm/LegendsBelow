#include "pch.h"

import LB.DungeonService;

using namespace LB::Services::Dungeon;


// ============================================================================
// Dungeon Generation Tests
// ============================================================================

TEST(DungeonServiceTests, GenerateDungeon_ValidParams_Success) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 5;
    params.maxRooms = 10;
    params.recommendedLevel = 1;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    EXPECT_GT(dungeon.dungeonId, 0);
    EXPECT_EQ(dungeon.name, "Test Dungeon");
    EXPECT_EQ(dungeon.recommendedLevel, 1);
    EXPECT_GE(dungeon.GetRoomCount(), 5);
    EXPECT_LE(dungeon.GetRoomCount(), 10);
}

TEST(DungeonServiceTests, GenerateDungeon_HasEntranceRoom_Success) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    EXPECT_GT(dungeon.entranceRoomId, 0);
    auto entrance = dungeon.GetRoom(dungeon.entranceRoomId);
    ASSERT_TRUE(entrance.has_value());
    EXPECT_EQ(entrance->type, RoomType::Entrance);
}

TEST(DungeonServiceTests, GenerateDungeon_WithBossRoom_HasBoss) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.guaranteeBossRoom = true;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    EXPECT_GT(dungeon.exitRoomId, 0);
    auto exitRoom = dungeon.GetRoom(dungeon.exitRoomId);
    ASSERT_TRUE(exitRoom.has_value());
    EXPECT_EQ(exitRoom->type, RoomType::Boss);
}

TEST(DungeonServiceTests, GenerateDungeon_MinRooms_GeneratesMinimum) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 5;
    params.maxRooms = 5; // Same as min

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    EXPECT_EQ(dungeon.GetRoomCount(), 5);
}

TEST(DungeonServiceTests, GenerateDungeon_StartsAtEntrance_CurrentRoomIsEntrance) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    EXPECT_EQ(dungeon.currentRoomId, dungeon.entranceRoomId);
}

// ============================================================================
// Room Connection Tests
// ============================================================================

TEST(DungeonServiceTests, GenerateDungeon_RoomsConnected_HasConnections) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 3;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    // Check that rooms have connections
    int32_t connectedRooms = 0;
    for (const auto& [id, room] : dungeon.rooms) {
        if (room.GetConnectionCount() > 0) {
            connectedRooms++;
        }
    }

    EXPECT_GT(connectedRooms, 0);
}

TEST(DungeonServiceTests, MoveToRoom_ValidDirection_Success) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 3;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    auto startRoom = dungeon.GetCurrentRoom();
    ASSERT_TRUE(startRoom.has_value());

    // Get available directions
    auto directions = service.GetAvailableDirections(dungeon.dungeonId);
    if (!directions.empty()) {
        bool moved = service.MoveToRoom(dungeon.dungeonId, directions[0]);
        EXPECT_TRUE(moved);

        // Verify we're in a different room
        auto updatedDungeon = service.GetDungeon(dungeon.dungeonId);
        EXPECT_NE(updatedDungeon->currentRoomId, startRoom->roomId);
    }
}

TEST(DungeonServiceTests, MoveToRoom_InvalidDirection_Fails) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    // Try to move in a direction that doesn't exist
    // Since rooms are linearly connected, North should not exist
    bool moved = service.MoveToRoom(dungeon.dungeonId, Direction::North);

    EXPECT_FALSE(moved);
}

TEST(DungeonServiceTests, MoveToRoom_MarksRoomExplored_Success) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 3;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    auto directions = service.GetAvailableDirections(dungeon.dungeonId);

    if (!directions.empty()) {
        service.MoveToRoom(dungeon.dungeonId, directions[0]);

        auto updatedDungeon = service.GetDungeon(dungeon.dungeonId);
        auto currentRoom = updatedDungeon->GetCurrentRoom();
        ASSERT_TRUE(currentRoom.has_value());
        EXPECT_TRUE(currentRoom->isExplored);
    }
}

// ============================================================================
// Room Clearing Tests
// ============================================================================

TEST(DungeonServiceTests, ClearCurrentRoom_ValidRoom_Success) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    bool cleared = service.ClearCurrentRoom(dungeon.dungeonId);

    EXPECT_TRUE(cleared);
    auto updatedDungeon = service.GetDungeon(dungeon.dungeonId);
    auto currentRoom = updatedDungeon->GetCurrentRoom();
    ASSERT_TRUE(currentRoom.has_value());
    EXPECT_TRUE(currentRoom->isCleared);
}

TEST(DungeonServiceTests, ClearCurrentRoom_InvalidDungeon_Fails) {
    DungeonService service;

    bool cleared = service.ClearCurrentRoom(999);

    EXPECT_FALSE(cleared);
}

// ============================================================================
// Encounter Generation Tests
// ============================================================================

TEST(DungeonServiceTests, GenerateEncounter_EasyDifficulty_OneEnemy) {
    DungeonService service;

    auto encounter = service.GenerateEncounter(RoomDifficulty::Easy, 1);

    EXPECT_GT(encounter.encounterId, 0);
    EXPECT_EQ(encounter.difficulty, RoomDifficulty::Easy);
    EXPECT_EQ(encounter.GetEnemyCount(), 1);
}

TEST(DungeonServiceTests, GenerateEncounter_NormalDifficulty_MultipleEnemies) {
    DungeonService service;

    auto encounter = service.GenerateEncounter(RoomDifficulty::Normal, 5);

    EXPECT_EQ(encounter.difficulty, RoomDifficulty::Normal);
    EXPECT_GE(encounter.GetEnemyCount(), 2);
}

TEST(DungeonServiceTests, GenerateEncounter_BossDifficulty_HighCR) {
    DungeonService service;

    auto encounter = service.GenerateEncounter(RoomDifficulty::Boss, 5);

    EXPECT_EQ(encounter.difficulty, RoomDifficulty::Boss);
    EXPECT_GT(encounter.challengeRating, 5.0f); // Boss CR should be higher than party level
}

TEST(DungeonServiceTests, GenerateEncounter_DifferentLevels_ScalesCR) {
    DungeonService service;

    auto level1 = service.GenerateEncounter(RoomDifficulty::Normal, 1);
    auto level10 = service.GenerateEncounter(RoomDifficulty::Normal, 10);

    EXPECT_LT(level1.challengeRating, level10.challengeRating);
}

// ============================================================================
// Loot Generation Tests
// ============================================================================

TEST(DungeonServiceTests, GenerateLoot_TreasureRoom_MultipleLoot) {
    DungeonService service;

    auto loot = service.GenerateLoot(RoomType::Treasure, 1);

    EXPECT_GT(loot.size(), size_t(0));
}

TEST(DungeonServiceTests, GenerateLoot_BossRoom_MoreLoot) {
    DungeonService service;

    auto treasureLoot = service.GenerateLoot(RoomType::Treasure, 1);
    auto bossLoot = service.GenerateLoot(RoomType::Boss, 1);

    EXPECT_GE(bossLoot.size(), treasureLoot.size());
}

TEST(DungeonServiceTests, GenerateLoot_EmptyRoom_NoLoot) {
    DungeonService service;

    auto loot = service.GenerateLoot(RoomType::Empty, 1);

    EXPECT_EQ(loot.size(), size_t(0));
}

TEST(DungeonServiceTests, LootDrop_ShouldDrop_RespectsProbability) {
    LootDrop guaranteedDrop{};
    guaranteedDrop.dropChance = 1.0f;

    LootDrop neverDrop{};
    neverDrop.dropChance = 0.0f;

    EXPECT_TRUE(guaranteedDrop.ShouldDrop());
    EXPECT_FALSE(neverDrop.ShouldDrop());
}

TEST(DungeonServiceTests, LootDrop_RollQuantity_WithinRange) {
    LootDrop drop{};
    drop.minQuantity = 1;
    drop.maxQuantity = 5;

    for (int i = 0; i < 10; ++i) {
        int32_t quantity = drop.RollQuantity();
        EXPECT_GE(quantity, 1);
        EXPECT_LE(quantity, 5);
    }
}

// ============================================================================
// Navigation Tests
// ============================================================================

TEST(DungeonServiceTests, GetAvailableDirections_ReturnsConnections) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 3;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    auto directions = service.GetAvailableDirections(dungeon.dungeonId);

    // Entrance should have at least one connection
    EXPECT_GT(directions.size(), size_t(0));
}

TEST(DungeonServiceTests, GetRoomInDirection_ValidDirection_ReturnsRoom) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 3;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    auto directions = service.GetAvailableDirections(dungeon.dungeonId);

    if (!directions.empty()) {
        auto nextRoom = service.GetRoomInDirection(dungeon.dungeonId, directions[0]);
        EXPECT_TRUE(nextRoom.has_value());
    }
}

TEST(DungeonServiceTests, GetRoomInDirection_InvalidDirection_ReturnsNullopt) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    auto nextRoom = service.GetRoomInDirection(dungeon.dungeonId, Direction::North);

    EXPECT_FALSE(nextRoom.has_value());
}

// ============================================================================
// Completion Tests
// ============================================================================

TEST(DungeonServiceTests, IsDungeonComplete_NewDungeon_ReturnsFalse) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    EXPECT_FALSE(service.IsDungeonComplete(dungeon.dungeonId));
}

TEST(DungeonServiceTests, GetCompletionPercentage_NewDungeon_ReturnsZero) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    float completion = service.GetCompletionPercentage(dungeon.dungeonId);

    EXPECT_FLOAT_EQ(completion, 0.0f);
}

TEST(DungeonServiceTests, GetCompletionPercentage_ClearedRoom_IncreasesPercentage) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 2;
    params.maxRooms = 2;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    service.ClearCurrentRoom(dungeon.dungeonId);

    float completion = service.GetCompletionPercentage(dungeon.dungeonId);

    EXPECT_GT(completion, 0.0f);
    EXPECT_LE(completion, 1.0f);
}

// ============================================================================
// Dungeon Management Tests
// ============================================================================

TEST(DungeonServiceTests, GetDungeon_ExistingDungeon_ReturnsCorrect) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    auto retrieved = service.GetDungeon(dungeon.dungeonId);

    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->dungeonId, dungeon.dungeonId);
    EXPECT_EQ(retrieved->name, "Test Dungeon");
}

TEST(DungeonServiceTests, GetDungeon_NonExistent_ReturnsNullopt) {
    DungeonService service;

    auto retrieved = service.GetDungeon(999);

    EXPECT_FALSE(retrieved.has_value());
}

TEST(DungeonServiceTests, RemoveDungeon_ExistingDungeon_Success) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    bool removed = service.RemoveDungeon(dungeon.dungeonId);

    EXPECT_TRUE(removed);
    EXPECT_FALSE(service.GetDungeon(dungeon.dungeonId).has_value());
}

TEST(DungeonServiceTests, RemoveDungeon_NonExistent_ReturnsFalse) {
    DungeonService service;

    bool removed = service.RemoveDungeon(999);

    EXPECT_FALSE(removed);
}

TEST(DungeonServiceTests, GetAllDungeonIds_MultipleDungeons_ReturnsAll) {
    DungeonService service;
    DungeonGenerationParams params{};

    std::ignore = service.GenerateDungeon("Dungeon1", params);
    std::ignore = service.GenerateDungeon("Dungeon2", params);
    std::ignore = service.GenerateDungeon("Dungeon3", params);

    auto ids = service.GetAllDungeonIds();

    EXPECT_EQ(ids.size(), size_t(3));
}

TEST(DungeonServiceTests, GetDungeonCount_TracksCount) {
    DungeonService service;
    DungeonGenerationParams params{};

    EXPECT_EQ(service.GetDungeonCount(), size_t(0));

    std::ignore = service.GenerateDungeon("Dungeon1", params);
    EXPECT_EQ(service.GetDungeonCount(), size_t(1));

    std::ignore = service.GenerateDungeon("Dungeon2", params);
    EXPECT_EQ(service.GetDungeonCount(), size_t(2));
}

// ============================================================================
// Room Property Tests
// ============================================================================

TEST(DungeonServiceTests, DungeonRoom_IsCombatRoom_DetectsCombatTypes) {
    DungeonRoom combatRoom{};
    combatRoom.type = RoomType::Combat;

    DungeonRoom bossRoom{};
    bossRoom.type = RoomType::Boss;

    DungeonRoom safeRoom{};
    safeRoom.type = RoomType::Safe;

    EXPECT_TRUE(combatRoom.IsCombatRoom());
    EXPECT_TRUE(bossRoom.IsCombatRoom());
    EXPECT_FALSE(safeRoom.IsCombatRoom());
}

TEST(DungeonServiceTests, DungeonRoom_HasConnection_DetectsConnections) {
    DungeonRoom room{};
    room.connections[Direction::North] = 2;

    EXPECT_TRUE(room.HasConnection(Direction::North));
    EXPECT_FALSE(room.HasConnection(Direction::South));
}

TEST(DungeonServiceTests, DungeonRoom_GetConnectionCount_ReturnsCorrectCount) {
    DungeonRoom room{};
    room.connections[Direction::North] = 2;
    room.connections[Direction::East] = 3;

    EXPECT_EQ(room.GetConnectionCount(), 2);
}

// ============================================================================
// Dungeon Query Tests
// ============================================================================

TEST(DungeonServiceTests, Dungeon_GetExploredRoomCount_CountsExplored) {
    DungeonService service;
    DungeonGenerationParams params{};
    params.minRooms = 3;

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);

    // Move to explore rooms
    auto directions = service.GetAvailableDirections(dungeon.dungeonId);
    if (!directions.empty()) {
        service.MoveToRoom(dungeon.dungeonId, directions[0]);
    }

    auto updated = service.GetDungeon(dungeon.dungeonId);
    EXPECT_GE(updated->GetExploredRoomCount(), 1);
}

TEST(DungeonServiceTests, Dungeon_GetClearedRoomCount_CountsCleared) {
    DungeonService service;
    DungeonGenerationParams params{};

    auto dungeon = service.GenerateDungeon("Test Dungeon", params);
    service.ClearCurrentRoom(dungeon.dungeonId);

    auto updated = service.GetDungeon(dungeon.dungeonId);
    EXPECT_EQ(updated->GetClearedRoomCount(), 1);
}