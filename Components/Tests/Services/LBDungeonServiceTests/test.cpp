#include "pch.h"

import LB.DungeonService;
import std;

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
// ============================================================================
// Floor Layout Generation Tests
// ============================================================================

namespace {
    // Floor cells reachable from a room's center (4-neighbors)
    std::int32_t ReachableFloorCells(const FloorLayout& layout, std::int32_t room) {
        const auto& area = layout.rooms[static_cast<std::size_t>(room)].area;
        std::vector<bool> seen(layout.cells.size(), false);
        std::vector<std::pair<std::int32_t, std::int32_t>> frontier{ { area.CenterX(), area.CenterY() } };
        seen[static_cast<std::size_t>(area.CenterY() * layout.width + area.CenterX())] = true;
        for (std::size_t head = 0; head < frontier.size(); ++head) {
            const auto [x, y] = frontier[head];
            for (const auto& [dx, dy] : { std::pair{ 1, 0 }, std::pair{ -1, 0 }, std::pair{ 0, 1 }, std::pair{ 0, -1 } }) {
                if (!layout.IsFloor(x + dx, y + dy)) continue;
                const auto index = static_cast<std::size_t>((y + dy) * layout.width + x + dx);
                if (seen[index]) continue;
                seen[index] = true;
                frontier.emplace_back(x + dx, y + dy);
            }
        }
        return static_cast<std::int32_t>(frontier.size());
    }

    bool Overlap(const CellRect& a, const CellRect& b) {
        return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
    }
}

TEST(FloorGenerationTests, SameSeed_SameFloor_OtherSeed_Different) {
    DungeonService service;
    const FloorGenerationParams params{};

    const auto a = service.GenerateFloor(params, 42);
    const auto b = service.GenerateFloor(params, 42);
    const auto c = service.GenerateFloor(params, 43);

    EXPECT_EQ(a.cells, b.cells);
    EXPECT_EQ(a.rooms.size(), b.rooms.size());
    EXPECT_TRUE(a.cells != c.cells || a.width != c.width || a.height != c.height);
}

TEST(FloorGenerationTests, ManySeeds_ValidFloors) {
    DungeonService service;
    FloorGenerationParams params{};
    params.criticalPathRooms = 7;

    for (std::uint32_t seed = 1; seed <= 200; ++seed) {
        const auto layout = service.GenerateFloor(params, seed);
        SCOPED_TRACE(seed);

        // The critical path, its ends, and the rooms' types
        ASSERT_GE(layout.rooms.size(), 7u);
        EXPECT_EQ(layout.rooms[static_cast<std::size_t>(layout.entranceRoom)].type, RoomType::Entrance);
        EXPECT_EQ(layout.rooms[static_cast<std::size_t>(layout.exitRoom)].type, RoomType::Exit);
        EXPECT_EQ(layout.exitRoom, 6);
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            const auto& room = layout.rooms[i];
            if (room.onCriticalPath) continue;
            EXPECT_TRUE(room.type == RoomType::Treasure || room.type == RoomType::Elite || room.type == RoomType::Safe);
        }

        // Rooms inside the border, apart from each other, made of floor, and connected both ways
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            const auto& area = layout.rooms[i].area;
            EXPECT_GE(area.x, 1);
            EXPECT_GE(area.y, 1);
            EXPECT_LE(area.x + area.width, layout.width - 1);
            EXPECT_LE(area.y + area.height, layout.height - 1);
            EXPECT_TRUE(layout.IsFloor(area.CenterX(), area.CenterY()));
            for (std::size_t j = i + 1; j < layout.rooms.size(); ++j) EXPECT_FALSE(Overlap(area, layout.rooms[j].area));
            for (const auto other : layout.rooms[i].connections) {
                const auto& back = layout.rooms[static_cast<std::size_t>(other)].connections;
                EXPECT_NE(std::ranges::find(back, static_cast<std::int32_t>(i)), back.end());
            }
        }

        // The border is solid and every floor cell is reachable from the entrance
        for (std::int32_t x = 0; x < layout.width; ++x) {
            EXPECT_FALSE(layout.IsFloor(x, 0));
            EXPECT_FALSE(layout.IsFloor(x, layout.height - 1));
        }
        for (std::int32_t y = 0; y < layout.height; ++y) {
            EXPECT_FALSE(layout.IsFloor(0, y));
            EXPECT_FALSE(layout.IsFloor(layout.width - 1, y));
        }
        EXPECT_EQ(ReachableFloorCells(layout, layout.entranceRoom), layout.FloorCellCount());
    }
}

TEST(FloorGenerationTests, Branches_AddSideRooms) {
    DungeonService service;
    FloorGenerationParams none{};
    none.branchChance = 0.0f;
    FloorGenerationParams always{};
    always.branchChance = 1.0f;
    always.maxBranchLength = 1;

    const auto straight = service.GenerateFloor(none, 7);
    const auto branched = service.GenerateFloor(always, 7);

    EXPECT_EQ(straight.rooms.size(), static_cast<std::size_t>(none.criticalPathRooms));
    EXPECT_GT(branched.rooms.size(), straight.rooms.size());
    EXPECT_LE(branched.rooms.size(), static_cast<std::size_t>(2 * always.criticalPathRooms - 2));
}

TEST(FloorGenerationTests, RoomSizes_WithinTheParams) {
    DungeonService service;
    FloorGenerationParams params{};
    params.minRoomSize = 4;
    params.maxRoomSize = 5;

    const auto layout = service.GenerateFloor(params, 99);

    for (const auto& room : layout.rooms) {
        EXPECT_GE(room.area.width, 4);
        EXPECT_LE(room.area.width, 5);
        EXPECT_GE(room.area.height, 4);
        EXPECT_LE(room.area.height, 5);
    }
}

TEST(FloorGenerationTests, SingleRoom_IsTheWholeFloor) {
    const auto layout = FloorLayout::SingleRoom(10, 7);

    EXPECT_EQ(layout.width, 12);
    EXPECT_EQ(layout.height, 9);
    EXPECT_EQ(layout.FloorCellCount(), 70);
    EXPECT_EQ(layout.RoomAt(1, 1), 0);
    EXPECT_EQ(layout.RoomAt(0, 0), -1);
    EXPECT_FALSE(layout.IsFloor(-1, 3));
}
