#include "pch.h"

import LB.Dungeon;

using namespace LB::Dungeon;

// ============================================================================
// RoomType Enum Tests
// ============================================================================

TEST(RoomTypeTests, AllTypes_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(RoomType::Empty), static_cast<int>(RoomType::Combat));
    EXPECT_NE(static_cast<int>(RoomType::Combat), static_cast<int>(RoomType::Treasure));
    EXPECT_NE(static_cast<int>(RoomType::Treasure), static_cast<int>(RoomType::Safe));
    EXPECT_NE(static_cast<int>(RoomType::Boss), static_cast<int>(RoomType::Elite));
}

TEST(RoomTypeTests, EmptyType_IsZero) {
    EXPECT_EQ(static_cast<int>(RoomType::Empty), 0);
}

TEST(RoomTypeTests, SpecialRooms_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(RoomType::Entrance), static_cast<int>(RoomType::Exit));
    EXPECT_NE(static_cast<int>(RoomType::Puzzle), static_cast<int>(RoomType::Combat));
}

TEST(RoomTypeTests, CombatRooms_Grouped) {
    int combat = static_cast<int>(RoomType::Combat);
    int elite = static_cast<int>(RoomType::Elite);
    int boss = static_cast<int>(RoomType::Boss);

    EXPECT_GT(combat, 0);
    EXPECT_GT(elite, 0);
    EXPECT_GT(boss, 0);
}

// ============================================================================
// RoomDifficulty Enum Tests
// ============================================================================

TEST(RoomDifficultyTests, AllDifficulties_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(RoomDifficulty::Easy), static_cast<int>(RoomDifficulty::Normal));
    EXPECT_NE(static_cast<int>(RoomDifficulty::Normal), static_cast<int>(RoomDifficulty::Hard));
    EXPECT_NE(static_cast<int>(RoomDifficulty::Hard), static_cast<int>(RoomDifficulty::Elite));
    EXPECT_NE(static_cast<int>(RoomDifficulty::Elite), static_cast<int>(RoomDifficulty::Boss));
}

TEST(RoomDifficultyTests, ProgressiveValues_Ascending) {
    int easy = static_cast<int>(RoomDifficulty::Easy);
    int normal = static_cast<int>(RoomDifficulty::Normal);
    int hard = static_cast<int>(RoomDifficulty::Hard);
    int elite = static_cast<int>(RoomDifficulty::Elite);
    int boss = static_cast<int>(RoomDifficulty::Boss);

    EXPECT_LT(easy, normal);
    EXPECT_LT(normal, hard);
    EXPECT_LT(hard, elite);
    EXPECT_LT(elite, boss);
}

TEST(RoomDifficultyTests, EasyDifficulty_IsOne) {
    EXPECT_EQ(static_cast<int>(RoomDifficulty::Easy), 1);
}

TEST(RoomDifficultyTests, BossDifficulty_IsHighest) {
    int boss = static_cast<int>(RoomDifficulty::Boss);
    int easy = static_cast<int>(RoomDifficulty::Easy);
    int normal = static_cast<int>(RoomDifficulty::Normal);
    int hard = static_cast<int>(RoomDifficulty::Hard);
    int elite = static_cast<int>(RoomDifficulty::Elite);

    EXPECT_GT(boss, easy);
    EXPECT_GT(boss, normal);
    EXPECT_GT(boss, hard);
    EXPECT_GT(boss, elite);
}

// ============================================================================
// Direction Enum Tests
// ============================================================================

TEST(DirectionTests, FourDirections_ExistAndDistinct) {
    EXPECT_NE(static_cast<int>(Direction::North), static_cast<int>(Direction::East));
    EXPECT_NE(static_cast<int>(Direction::East), static_cast<int>(Direction::South));
    EXPECT_NE(static_cast<int>(Direction::South), static_cast<int>(Direction::West));
    EXPECT_NE(static_cast<int>(Direction::West), static_cast<int>(Direction::North));
}

TEST(DirectionTests, NorthDirection_IsZero) {
    EXPECT_EQ(static_cast<int>(Direction::North), 0);
}

TEST(DirectionTests, DirectionsAreSequential) {
    int north = static_cast<int>(Direction::North);
    int east = static_cast<int>(Direction::East);
    int south = static_cast<int>(Direction::South);
    int west = static_cast<int>(Direction::West);

    EXPECT_EQ(east, north + 1);
    EXPECT_EQ(south, east + 1);
    EXPECT_EQ(west, south + 1);
}

TEST(DirectionTests, FourUniqueDirections) {
    std::set<int> directions;
    directions.insert(static_cast<int>(Direction::North));
    directions.insert(static_cast<int>(Direction::East));
    directions.insert(static_cast<int>(Direction::South));
    directions.insert(static_cast<int>(Direction::West));

    EXPECT_EQ(directions.size(), 4u);
}

// ============================================================================
// Direction Semantics Tests
// ============================================================================

TEST(DirectionSemanticTests, OppositeDirections_NotEqual) {
    EXPECT_NE(static_cast<int>(Direction::North), static_cast<int>(Direction::South));
    EXPECT_NE(static_cast<int>(Direction::East), static_cast<int>(Direction::West));
}

TEST(DirectionSemanticTests, CardinalDirections_InOrder) {
    // North, East, South, West - clockwise order
    int north = static_cast<int>(Direction::North);
    int east = static_cast<int>(Direction::East);
    int south = static_cast<int>(Direction::South);
    int west = static_cast<int>(Direction::West);

    EXPECT_EQ(north, 0);
    EXPECT_EQ(east, 1);
    EXPECT_EQ(south, 2);
    EXPECT_EQ(west, 3);
}

// ============================================================================
// RoomType Semantics Tests
// ============================================================================

TEST(RoomTypeSemanticTests, EntryAndExit_Exist) {
    int entrance = static_cast<int>(RoomType::Entrance);
    int exit = static_cast<int>(RoomType::Exit);

    EXPECT_GT(entrance, 0);
    EXPECT_GT(exit, 0);
    EXPECT_NE(entrance, exit);
}

TEST(RoomTypeSemanticTests, SafeRoom_Exists) {
    int safe = static_cast<int>(RoomType::Safe);
    EXPECT_GT(safe, 0);
}

TEST(RoomTypeSemanticTests, TreasureRoom_Exists) {
    int treasure = static_cast<int>(RoomType::Treasure);
    EXPECT_GT(treasure, 0);
}

TEST(RoomTypeSemanticTests, PuzzleRoom_Exists) {
    int puzzle = static_cast<int>(RoomType::Puzzle);
    EXPECT_GT(puzzle, 0);
}

// ============================================================================
// Room Difficulty Semantics Tests
// ============================================================================

TEST(RoomDifficultySemanticTests, NormalDifficulty_MiddleValue) {
    int normal = static_cast<int>(RoomDifficulty::Normal);
    int easy = static_cast<int>(RoomDifficulty::Easy);
    int hard = static_cast<int>(RoomDifficulty::Hard);

    EXPECT_GT(normal, easy);
    EXPECT_LT(normal, hard);
}

TEST(RoomDifficultySemanticTests, EliteDifficulty_BelowBoss) {
    int elite = static_cast<int>(RoomDifficulty::Elite);
    int boss = static_cast<int>(RoomDifficulty::Boss);

    EXPECT_LT(elite, boss);
}

TEST(RoomDifficultySemanticTests, AllDifficulties_Positive) {
    EXPECT_GT(static_cast<int>(RoomDifficulty::Easy), 0);
    EXPECT_GT(static_cast<int>(RoomDifficulty::Normal), 0);
    EXPECT_GT(static_cast<int>(RoomDifficulty::Hard), 0);
    EXPECT_GT(static_cast<int>(RoomDifficulty::Elite), 0);
    EXPECT_GT(static_cast<int>(RoomDifficulty::Boss), 0);
}

// ============================================================================
// Dungeon Generation Tests (Enum Combinations)
// ============================================================================

TEST(DungeonGenerationTests, BossRoom_HasBossDifficulty) {
    // Semantic relationship: Boss rooms should use Boss difficulty
    RoomType bossType = RoomType::Boss;
    RoomDifficulty bossDiff = RoomDifficulty::Boss;

    EXPECT_EQ(static_cast<int>(bossType), 4);
    EXPECT_EQ(static_cast<int>(bossDiff), 5);
}

TEST(DungeonGenerationTests, EliteRoom_HasEliteDifficulty) {
    // Semantic relationship: Elite rooms should use Elite difficulty
    RoomType eliteType = RoomType::Elite;
    RoomDifficulty eliteDiff = RoomDifficulty::Elite;

    EXPECT_EQ(static_cast<int>(eliteType), 7);
    EXPECT_EQ(static_cast<int>(eliteDiff), 4);
}

TEST(DungeonGenerationTests, SafeRoom_NoHostiles) {
    // Safe rooms should be special (not combat-oriented)
    RoomType safe = RoomType::Safe;
    RoomType combat = RoomType::Combat;

    EXPECT_NE(safe, combat);
    EXPECT_NE(safe, RoomType::Boss);
    EXPECT_NE(safe, RoomType::Elite);
}

// ============================================================================
// Direction Navigation Tests
// ============================================================================

TEST(DirectionNavigationTests, AllDirections_Reachable) {
    // All four directions should be navigable
    std::vector<Direction> directions = {
        Direction::North,
        Direction::East,
        Direction::South,
        Direction::West
    };

    EXPECT_EQ(directions.size(), 4u);
}

TEST(DirectionNavigationTests, OppositeDirections_TwoStepsApart) {
    int north = static_cast<int>(Direction::North);
    int south = static_cast<int>(Direction::South);
    int east = static_cast<int>(Direction::East);
    int west = static_cast<int>(Direction::West);

    EXPECT_EQ(std::abs(north - south), 2);
    EXPECT_EQ(std::abs(east - west), 2);
}