// LB.DungeonService.ixx - Dungeon generation and management service using C++20 modules
export module LB.DungeonService;

// Import only required domains
import LB.Core;
import LB.Character;
import LB.Dungeon;
import std;
using namespace std;

/// <summary>
/// Dungeon service for LegendsBelow
/// Provides procedural dungeon generation, encounter management, and exploration mechanics
/// Integrates with combat and inventory services for encounters and loot
/// </summary>
export namespace LB::Services::Dungeon {
    // Re-export domain types for service convenience
    using LB::Dungeon::RoomType;
    using LB::Dungeon::RoomDifficulty;
    using LB::Dungeon::Direction;
    using LB::Dungeon::DungeonRoom;
    using LB::Dungeon::Dungeon;
    using LB::Dungeon::Encounter;
    using LB::Dungeon::LootDrop;
    using LB::Dungeon::DungeonGenerationParams;

    /// <summary>
    /// Core dungeon service
    /// Handles dungeon generation, exploration, and encounters
    /// </summary>
    class DungeonService {
    public:
        /// <summary>
        /// Generate a new dungeon
        /// </summary>
        [[nodiscard]] Dungeon GenerateDungeon(const LB::String& name, const DungeonGenerationParams& params) {
            Dungeon dungeon{};
            dungeon.dungeonId = generateNextDungeonId();
            dungeon.name = name;
            dungeon.recommendedLevel = params.recommendedLevel;

            // Determine room count
            int32_t roomCount = rollRoomCount(params.minRooms, params.maxRooms);
            dungeon.depth = roomCount;

            // Generate rooms
            generateRooms(dungeon, roomCount, params);

            // Connect rooms
            connectRooms(dungeon);

            // Set entrance and current room
            if (!dungeon.rooms.empty()) {
                dungeon.entranceRoomId = dungeon.rooms.begin()->first;
                dungeon.currentRoomId = dungeon.entranceRoomId;
                dungeon.rooms[dungeon.entranceRoomId].type = RoomType::Entrance;
            }

            // Store dungeon
            dungeons_[dungeon.dungeonId] = dungeon;

            return dungeon;
        }

        /// <summary>
        /// Get dungeon by ID
        /// </summary>
        [[nodiscard]] std::optional<Dungeon> GetDungeon(LB::Domain::Id dungeonId) const {
            auto it = dungeons_.find(dungeonId);
            if (it == dungeons_.end()) return std::nullopt;
            return it->second;
        }

        /// <summary>
        /// Move to connected room
        /// </summary>
        bool MoveToRoom(LB::Domain::Id dungeonId, Direction direction) {
            auto it = dungeons_.find(dungeonId);
            if (it == dungeons_.end()) return false;

            auto& dungeon = it->second;
            auto currentRoom = dungeon.GetCurrentRoom();
            if (!currentRoom.has_value()) return false;

            // Check if connection exists
            if (!currentRoom->HasConnection(direction)) {
                return false;
            }

            // Move to new room
            LB::Domain::Id newRoomId = currentRoom->connections.at(direction);
            dungeon.currentRoomId = newRoomId;

            // Mark new room as explored
            dungeon.rooms[newRoomId].isExplored = true;

            return true;
        }

        /// <summary>
        /// Mark current room as cleared
        /// </summary>
        bool ClearCurrentRoom(LB::Domain::Id dungeonId) {
            auto it = dungeons_.find(dungeonId);
            if (it == dungeons_.end()) return false;

            auto& dungeon = it->second;
            auto currentRoom = dungeon.GetCurrentRoom();
            if (!currentRoom.has_value()) return false;

            dungeon.rooms[dungeon.currentRoomId].isCleared = true;
            return true;
        }

        /// <summary>
        /// Generate encounter for a combat room
        /// </summary>
        [[nodiscard]] Encounter GenerateEncounter(RoomDifficulty difficulty, int32_t partyLevel) const {
            Encounter encounter{};
            encounter.encounterId = generateNextEncounterId();
            encounter.difficulty = difficulty;
            encounter.recommendedLevel = partyLevel;

            // Determine enemy count based on difficulty
            int32_t enemyCount = getEnemyCount(difficulty);

            // Generate enemy IDs (placeholder - would integrate with enemy system)
            for (int32_t i = 0; i < enemyCount; ++i) {
                encounter.enemies.push_back(generateNextEnemyId());
            }

            // Calculate challenge rating
            encounter.challengeRating = calculateEncounterCR(difficulty, partyLevel);

            return encounter;
        }

        /// <summary>
        /// Generate loot for a room
        /// </summary>
        [[nodiscard]] std::vector<LootDrop> GenerateLoot(RoomType roomType, int32_t dungeonLevel) const {
            std::vector<LootDrop> loot;

            int32_t lootCount = getLootCount(roomType);

            for (int32_t i = 0; i < lootCount; ++i) {
                LootDrop drop{};
                drop.itemId = generateNextItemId();
                drop.itemName = "Item_" + std::to_string(drop.itemId);
                drop.dropChance = getLootDropChance(roomType);
                drop.minQuantity = 1;
                drop.maxQuantity = getMaxLootQuantity(roomType);

                loot.push_back(drop);
            }

            return loot;
        }

        /// <summary>
        /// Get available directions from current room
        /// </summary>
        [[nodiscard]] std::vector<Direction> GetAvailableDirections(LB::Domain::Id dungeonId) const {
            std::vector<Direction> directions;

            auto dungeon = GetDungeon(dungeonId);
            if (!dungeon.has_value()) return directions;

            auto currentRoom = dungeon->GetCurrentRoom();
            if (!currentRoom.has_value()) return directions;

            for (const auto& [dir, roomId] : currentRoom->connections) {
                directions.push_back(dir);
            }

            return directions;
        }

        /// <summary>
        /// Get room in specific direction
        /// </summary>
        [[nodiscard]] std::optional<DungeonRoom> GetRoomInDirection(LB::Domain::Id dungeonId, Direction direction) const {
            auto dungeon = GetDungeon(dungeonId);
            if (!dungeon.has_value()) return std::nullopt;

            auto currentRoom = dungeon->GetCurrentRoom();
            if (!currentRoom.has_value()) return std::nullopt;

            if (!currentRoom->HasConnection(direction)) {
                return std::nullopt;
            }

            LB::Domain::Id targetRoomId = currentRoom->connections.at(direction);
            return dungeon->GetRoom(targetRoomId);
        }

        /// <summary>
        /// Check if dungeon is complete
        /// </summary>
        [[nodiscard]] bool IsDungeonComplete(LB::Domain::Id dungeonId) const {
            auto dungeon = GetDungeon(dungeonId);
            if (!dungeon.has_value()) return false;
            return dungeon->IsComplete();
        }

        /// <summary>
        /// Get dungeon completion percentage
        /// </summary>
        [[nodiscard]] float GetCompletionPercentage(LB::Domain::Id dungeonId) const {
            auto dungeon = GetDungeon(dungeonId);
            if (!dungeon.has_value()) return 0.0f;

            int32_t totalRooms = dungeon->GetRoomCount();
            if (totalRooms == 0) return 0.0f;

            int32_t clearedRooms = dungeon->GetClearedRoomCount();
            return static_cast<float>(clearedRooms) / static_cast<float>(totalRooms);
        }

        /// <summary>
        /// Remove dungeon
        /// </summary>
        bool RemoveDungeon(LB::Domain::Id dungeonId) {
            return dungeons_.erase(dungeonId) > 0;
        }

        /// <summary>
        /// Get all dungeons
        /// </summary>
        [[nodiscard]] std::vector<LB::Domain::Id> GetAllDungeonIds() const {
            std::vector<LB::Domain::Id> ids;
            ids.reserve(dungeons_.size());
            for (const auto& [id, dungeon] : dungeons_) {
                ids.push_back(id);
            }
            return ids;
        }

        /// <summary>
        /// Get dungeon count
        /// </summary>
        [[nodiscard]] size_t GetDungeonCount() const {
            return dungeons_.size();
        }

    private:
        std::unordered_map<LB::Domain::Id, Dungeon> dungeons_;
        mutable LB::Domain::Id nextDungeonId_{1};
        mutable LB::Domain::Id nextRoomId_{1};
        mutable LB::Domain::Id nextEncounterId_{1};
        mutable LB::Domain::Id nextEnemyId_{1000};
        mutable LB::Domain::Id nextItemId_{2000};

        [[nodiscard]] LB::Domain::Id generateNextDungeonId() const {
            return nextDungeonId_++;
        }

        [[nodiscard]] LB::Domain::Id generateNextRoomId() const {
            return nextRoomId_++;
        }

        [[nodiscard]] LB::Domain::Id generateNextEncounterId() const {
            return nextEncounterId_++;
        }

        [[nodiscard]] LB::Domain::Id generateNextEnemyId() const {
            return nextEnemyId_++;
        }

        [[nodiscard]] LB::Domain::Id generateNextItemId() const {
            return nextItemId_++;
        }

        [[nodiscard]] int32_t rollRoomCount(int32_t min, int32_t max) const {
            static thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_int_distribution<int32_t> dist(min, max);
            return dist(gen);
        }

        void generateRooms(Dungeon& dungeon, int32_t roomCount, const DungeonGenerationParams& params) {
            for (int32_t i = 0; i < roomCount; ++i) {
                DungeonRoom room{};
                room.roomId = generateNextRoomId();
                room.name = "Room " + std::to_string(room.roomId);
                room.description = "A dungeon room";
                room.gridPosition = LB::Domain::Position(i % 5, i / 5); // Simple grid layout

                // Assign room type
                if (i == roomCount - 1 && params.guaranteeBossRoom) {
                    room.type = RoomType::Boss;
                    room.difficulty = RoomDifficulty::Boss;
                    dungeon.exitRoomId = room.roomId;
                } else {
                    room.type = rollRoomType(params);
                    room.difficulty = rollRoomDifficulty(room.type);
                }

                dungeon.rooms[room.roomId] = room;
            }
        }

        void connectRooms(Dungeon& dungeon) {
            if (dungeon.rooms.empty()) return;

            // Simple linear connection for now
            auto it = dungeon.rooms.begin();
            LB::Domain::Id previousRoomId = it->first;
            ++it;

            while (it != dungeon.rooms.end()) {
                LB::Domain::Id currentRoomId = it->first;

                // Connect previous to current
                dungeon.rooms[previousRoomId].connections[Direction::East] = currentRoomId;
                dungeon.rooms[currentRoomId].connections[Direction::West] = previousRoomId;

                previousRoomId = currentRoomId;
                ++it;
            }
        }

        [[nodiscard]] RoomType rollRoomType(const DungeonGenerationParams& params) const {
            static thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            float roll = dist(gen);

            if (roll < params.combatRoomChance) {
                return RoomType::Combat;
            } else if (roll < params.combatRoomChance + params.treasureRoomChance) {
                return RoomType::Treasure;
            } else if (roll < params.combatRoomChance + params.treasureRoomChance + params.eliteRoomChance) {
                return RoomType::Elite;
            } else {
                return RoomType::Safe;
            }
        }

        [[nodiscard]] RoomDifficulty rollRoomDifficulty(RoomType type) const {
            switch (type) {
                case RoomType::Combat:
                    return RoomDifficulty::Normal;
                case RoomType::Elite:
                    return RoomDifficulty::Elite;
                case RoomType::Boss:
                    return RoomDifficulty::Boss;
                default:
                    return RoomDifficulty::Easy;
            }
        }

        [[nodiscard]] int32_t getEnemyCount(RoomDifficulty difficulty) const {
            switch (difficulty) {
                case RoomDifficulty::Easy: return 1;
                case RoomDifficulty::Normal: return 2;
                case RoomDifficulty::Hard: return 3;
                case RoomDifficulty::Elite: return 3;
                case RoomDifficulty::Boss: return 1;
                default: return 1;
            }
        }

        [[nodiscard]] float calculateEncounterCR(RoomDifficulty difficulty, int32_t partyLevel) const {
            float baseCR = static_cast<float>(partyLevel);

            switch (difficulty) {
                case RoomDifficulty::Easy: return baseCR * 0.5f;
                case RoomDifficulty::Normal: return baseCR;
                case RoomDifficulty::Hard: return baseCR * 1.5f;
                case RoomDifficulty::Elite: return baseCR * 2.0f;
                case RoomDifficulty::Boss: return baseCR * 3.0f;
                default: return baseCR;
            }
        }

        [[nodiscard]] int32_t getLootCount(RoomType roomType) const {
            switch (roomType) {
                case RoomType::Treasure: return 3;
                case RoomType::Boss: return 5;
                case RoomType::Elite: return 2;
                case RoomType::Combat: return 1;
                default: return 0;
            }
        }

        [[nodiscard]] float getLootDropChance(RoomType roomType) const {
            switch (roomType) {
                case RoomType::Treasure: return 1.0f;
                case RoomType::Boss: return 1.0f;
                case RoomType::Elite: return 0.8f;
                case RoomType::Combat: return 0.5f;
                default: return 0.0f;
            }
        }

        [[nodiscard]] int32_t getMaxLootQuantity(RoomType roomType) const {
            switch (roomType) {
                case RoomType::Treasure: return 5;
                case RoomType::Boss: return 3;
                case RoomType::Elite: return 2;
                default: return 1;
            }
        }
    };

} // namespace LB::Services::Dungeon