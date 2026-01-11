// LB.Dungeon.Entities.ixx - Dungeon domain entities
export module LB.Dungeon.Entities;

import LB.Core;
import LB.Character;
import LB.Dungeon.Enums;
import std;

export namespace LB::Dungeon {
    using LB::Character::CharacterId;
    using LB::Character::ItemId;
    using RoomId = LB::Id;
    using DungeonId = LB::Id;
    using EncounterId = LB::Id;

    /// <summary>
    /// Dungeon room with connections and contents
    /// </summary>
    struct DungeonRoom {
        RoomId roomId;
        LB::String name;
        LB::String description;
        RoomType type{RoomType::Empty};
        RoomDifficulty difficulty{RoomDifficulty::Normal};
        LB::Position gridPosition{0, 0};

        // Connections to other rooms
        std::unordered_map<Direction, RoomId> connections;

        // Contents
        std::vector<CharacterId> enemies;
        std::vector<ItemId> loot;
        bool isExplored{false};
        bool isCleared{false};

        [[nodiscard]] bool HasConnection(Direction dir) const {
            return connections.contains(dir);
        }

        [[nodiscard]] int32_t GetConnectionCount() const {
            return static_cast<int32_t>(connections.size());
        }

        [[nodiscard]] bool IsCombatRoom() const {
            return type == RoomType::Combat || type == RoomType::Elite || type == RoomType::Boss;
        }

        [[nodiscard]] bool HasEnemies() const {
            return !enemies.empty();
        }

        [[nodiscard]] bool HasLoot() const {
            return !loot.empty();
        }
    };

    /// <summary>
    /// Encounter configuration for combat rooms
    /// </summary>
    struct Encounter {
        EncounterId encounterId;
        RoomDifficulty difficulty;
        int32_t recommendedLevel;
        std::vector<CharacterId> enemies;
        float challengeRating;

        [[nodiscard]] int32_t GetEnemyCount() const {
            return static_cast<int32_t>(enemies.size());
        }
    };

    /// <summary>
    /// Complete dungeon with all rooms and connections
    /// </summary>
    struct Dungeon {
        DungeonId dungeonId;
        LB::String name;
        int32_t recommendedLevel;
        int32_t depth; // Number of rooms

        std::unordered_map<RoomId, DungeonRoom> rooms;
        RoomId entranceRoomId{0};
        RoomId exitRoomId{0};
        RoomId currentRoomId{0};

        [[nodiscard]] int32_t GetRoomCount() const {
            return static_cast<int32_t>(rooms.size());
        }

        [[nodiscard]] std::optional<DungeonRoom> GetRoom(RoomId roomId) const {
            auto it = rooms.find(roomId);
            if (it == rooms.end()) return std::nullopt;
            return it->second;
        }

        [[nodiscard]] std::optional<DungeonRoom> GetCurrentRoom() const {
            return GetRoom(currentRoomId);
        }

        [[nodiscard]] int32_t GetExploredRoomCount() const {
            int32_t count = 0;
            for (const auto& [id, room] : rooms) {
                if (room.isExplored) count++;
            }
            return count;
        }

        [[nodiscard]] int32_t GetClearedRoomCount() const {
            int32_t count = 0;
            for (const auto& [id, room] : rooms) {
                if (room.isCleared) count++;
            }
            return count;
        }

        [[nodiscard]] bool IsComplete() const {
            auto exitRoom = GetRoom(exitRoomId);
            return currentRoomId == exitRoomId && exitRoom.has_value() && exitRoom->isCleared;
        }
    };

} // namespace LB::Dungeon