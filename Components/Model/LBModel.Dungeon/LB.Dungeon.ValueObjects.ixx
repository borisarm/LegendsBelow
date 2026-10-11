// LB.Dungeon.ValueObjects.ixx - Dungeon domain value objects
export module LB.Dungeon.ValueObjects;

import LB.Core;
import LB.Character;
import std;

export namespace LB::Dungeon {
    using LB::Character::ItemId;

    /// <summary>
    /// Dungeon generation parameters
    /// </summary>
    struct DungeonGenerationParams {
        int32_t minRooms{5};
        int32_t maxRooms{15};
        int32_t recommendedLevel{1};
        float combatRoomChance{0.6f};
        float treasureRoomChance{0.2f};
        float eliteRoomChance{0.1f};
        bool guaranteeBossRoom{true};
    };

} // namespace LB::Dungeon