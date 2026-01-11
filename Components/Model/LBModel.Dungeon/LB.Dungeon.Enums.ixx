// LB.Dungeon.Enums.ixx - Dungeon domain enumerations
export module LB.Dungeon.Enums;

import std;

export namespace LB::Dungeon {

    /// <summary>
    /// Room type defining purpose and contents
    /// </summary>
    enum class RoomType : uint32_t {
        Empty = 0,
        Combat = 1,
        Treasure = 2,
        Safe = 3,
        Boss = 4,
        Entrance = 5,
        Exit = 6,
        Elite = 7,
        Puzzle = 8
    };

    /// <summary>
    /// Room difficulty level
    /// </summary>
    enum class RoomDifficulty : uint32_t {
        Easy = 1,
        Normal = 2,
        Hard = 3,
        Elite = 4,
        Boss = 5
    };

    /// <summary>
    /// Direction for room connections
    /// </summary>
    enum class Direction : uint32_t {
        North = 0,
        East = 1,
        South = 2,
        West = 3
    };

} // namespace LB::Dungeon