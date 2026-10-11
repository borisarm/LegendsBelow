// LB.DungeonService.ixx - Dungeon generation and management service using C++20 modules
export module LB.DungeonService;

// Import only required domains
import LB.Core;
import LB.Character;
import LB.Dungeon;
import std;

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
    using LB::Dungeon::DungeonGenerationParams;
    using LB::Dungeon::FloorLayout;
    using LB::Dungeon::FloorGenerationParams;
    using LB::Dungeon::LayoutRoom;
    using LB::Dungeon::CellRect;

    /// <summary>
    /// Core dungeon service
    /// Handles dungeon generation, exploration, and encounters
    /// </summary>
    class DungeonService {
    public:
        /// <summary>
        /// Generate a floor layout (design-dungeon-and-world.md 3.2-3.3), reproducible for a seed:
        /// a critical path of rooms from the entrance to the exit laid out by a random walk on a
        /// grid of room slots, side branches off the rooms between them, rooms of random size in
        /// their slots, and one-cell corridors between connected rooms. Critical rooms between the
        /// entrance and exit are combat rooms; branch rooms are treasure, elite, or safe rooms by
        /// weight.
        /// </summary>
        [[nodiscard]] FloorLayout GenerateFloor(const FloorGenerationParams& params, std::uint32_t seed) const {
            std::mt19937 rng(seed);
            const std::int32_t pathLength = std::max(2, params.criticalPathRooms);
            const std::int32_t slotSize = std::max(params.maxRoomSize + 2, params.slotSize);
            constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> steps{ { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } };

            // Room slots: the critical path as a self-avoiding walk, restarted when it boxes itself in
            std::vector<std::pair<std::int32_t, std::int32_t>> slots;
            std::vector<std::pair<std::int32_t, std::int32_t>> edges;   // room index pairs
            const auto taken = [&](std::pair<std::int32_t, std::int32_t> slot) { return std::ranges::find(slots, slot) != slots.end(); };
            const auto freeNeighbors = [&](std::pair<std::int32_t, std::int32_t> slot) {
                std::vector<std::pair<std::int32_t, std::int32_t>> options;
                for (const auto& [dx, dy] : steps) {
                    const std::pair<std::int32_t, std::int32_t> next{ slot.first + dx, slot.second + dy };
                    if (!taken(next)) options.push_back(next);
                }
                return options;
            };
            const auto pick = [&](const std::vector<std::pair<std::int32_t, std::int32_t>>& options) {
                return options[std::uniform_int_distribution<std::size_t>(0, options.size() - 1)(rng)];
            };
            for (int attempt = 0; attempt < 100 && static_cast<std::int32_t>(slots.size()) < pathLength; ++attempt) {
                slots = { { 0, 0 } };
                edges.clear();
                while (static_cast<std::int32_t>(slots.size()) < pathLength) {
                    const auto options = freeNeighbors(slots.back());
                    if (options.empty()) break;
                    slots.push_back(pick(options));
                    edges.emplace_back(static_cast<std::int32_t>(slots.size()) - 2, static_cast<std::int32_t>(slots.size()) - 1);
                }
            }
            const auto criticalCount = static_cast<std::int32_t>(slots.size());

            // Side branches off the rooms between the entrance and the exit
            std::vector<bool> critical(slots.size(), true);
            std::uniform_real_distribution<float> chance(0.0f, 1.0f);
            for (std::int32_t i = 1; i + 1 < criticalCount; ++i) {
                if (chance(rng) >= params.branchChance) continue;
                const std::int32_t length = std::uniform_int_distribution<std::int32_t>(1, std::max(1, params.maxBranchLength))(rng);
                std::int32_t from = i;
                for (std::int32_t k = 0; k < length; ++k) {
                    const auto options = freeNeighbors(slots[static_cast<std::size_t>(from)]);
                    if (options.empty()) break;
                    slots.push_back(pick(options));
                    critical.push_back(false);
                    edges.emplace_back(from, static_cast<std::int32_t>(slots.size()) - 1);
                    from = static_cast<std::int32_t>(slots.size()) - 1;
                }
            }

            // Slots to cells, with a solid border cell all around
            std::int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
            for (const auto& [x, y] : slots) {
                minX = std::min(minX, x); minY = std::min(minY, y);
                maxX = std::max(maxX, x); maxY = std::max(maxY, y);
            }
            FloorLayout layout;
            layout.width = (maxX - minX + 1) * slotSize + 2;
            layout.height = (maxY - minY + 1) * slotSize + 2;
            layout.cells.assign(static_cast<std::size_t>(layout.width * layout.height), 0);

            // Rooms: a random size, placed at random inside the slot with at least a cell of margin
            const std::int32_t largest = std::max(params.minRoomSize, std::min(params.maxRoomSize, slotSize - 2));
            std::uniform_int_distribution<std::int32_t> size(std::max(1, std::min(params.minRoomSize, largest)), largest);
            const std::int32_t totalWeight = std::max(1, params.treasureWeight + params.eliteWeight + params.safeWeight);
            for (std::size_t i = 0; i < slots.size(); ++i) {
                const std::int32_t width = size(rng);
                const std::int32_t height = size(rng);
                const std::int32_t slotLeft = (slots[i].first - minX) * slotSize + 1;
                const std::int32_t slotTop = (slots[i].second - minY) * slotSize + 1;
                const std::int32_t left = slotLeft + std::uniform_int_distribution<std::int32_t>(1, slotSize - 1 - width)(rng);
                const std::int32_t top = slotTop + std::uniform_int_distribution<std::int32_t>(1, slotSize - 1 - height)(rng);

                LayoutRoom room;
                room.area = CellRect{ left, top, width, height };
                room.onCriticalPath = critical[i];
                if (i == 0) room.type = RoomType::Entrance;
                else if (static_cast<std::int32_t>(i) == criticalCount - 1) room.type = RoomType::Exit;
                else if (critical[i]) room.type = RoomType::Combat;
                else {
                    const std::int32_t roll = std::uniform_int_distribution<std::int32_t>(1, totalWeight)(rng);
                    room.type = roll <= params.treasureWeight ? RoomType::Treasure
                              : roll <= params.treasureWeight + params.eliteWeight ? RoomType::Elite : RoomType::Safe;
                }
                for (std::int32_t y = top; y < top + height; ++y) {
                    for (std::int32_t x = left; x < left + width; ++x) layout.SetFloor(x, y);
                }
                layout.rooms.push_back(std::move(room));
            }
            layout.entranceRoom = 0;
            layout.exitRoom = criticalCount - 1;

            // Corridors: from the first room's center along the slots' shared row (or column) to the
            // second room's center line, then across to it; neighboring slots share a row or a
            // column, so the corridor stays inside the two slots
            for (const auto& [a, b] : edges) {
                auto& from = layout.rooms[static_cast<std::size_t>(a)];
                auto& to = layout.rooms[static_cast<std::size_t>(b)];
                from.connections.push_back(b);
                to.connections.push_back(a);
                const std::int32_t x0 = from.area.CenterX(), y0 = from.area.CenterY();
                const std::int32_t x1 = to.area.CenterX(), y1 = to.area.CenterY();
                const bool horizontal = slots[static_cast<std::size_t>(a)].second == slots[static_cast<std::size_t>(b)].second;
                if (horizontal) {
                    for (std::int32_t x = std::min(x0, x1); x <= std::max(x0, x1); ++x) layout.SetFloor(x, y0);
                    for (std::int32_t y = std::min(y0, y1); y <= std::max(y0, y1); ++y) layout.SetFloor(x1, y);
                }
                else {
                    for (std::int32_t y = std::min(y0, y1); y <= std::max(y0, y1); ++y) layout.SetFloor(x0, y);
                    for (std::int32_t x = std::min(x0, x1); x <= std::max(x0, x1); ++x) layout.SetFloor(x, y1);
                }
            }
            return layout;
        }

        /// <summary>
        /// Generate a new dungeon
        /// </summary>
        [[nodiscard]] Dungeon GenerateDungeon(const LB::String& name, const DungeonGenerationParams& params) {
            Dungeon dungeon{};
            dungeon.dungeonId = generateNextDungeonId();
            dungeon.name = name;
            dungeon.recommendedLevel = params.recommendedLevel;

            // Determine room count
            std::int32_t roomCount = rollRoomCount(params.minRooms, params.maxRooms);
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
        [[nodiscard]] std::optional<Dungeon> GetDungeon(Id dungeonId) const {
            auto it = dungeons_.find(dungeonId);
            if (it == dungeons_.end()) return std::nullopt;
            return it->second;
        }

        /// <summary>
        /// Move to connected room
        /// </summary>
        bool MoveToRoom(Id dungeonId, Direction direction) {
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
            Id newRoomId = currentRoom->connections.at(direction);
            dungeon.currentRoomId = newRoomId;

            // Mark new room as explored
            dungeon.rooms[newRoomId].isExplored = true;

            return true;
        }

        /// <summary>
        /// Mark current room as cleared
        /// </summary>
        bool ClearCurrentRoom(Id dungeonId) {
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
        [[nodiscard]] Encounter GenerateEncounter(RoomDifficulty difficulty, std::int32_t partyLevel) const {
            Encounter encounter{};
            encounter.encounterId = generateNextEncounterId();
            encounter.difficulty = difficulty;
            encounter.recommendedLevel = partyLevel;

            // Determine enemy count based on difficulty
            std::int32_t enemyCount = getEnemyCount(difficulty);

            // Generate enemy IDs (placeholder - would integrate with enemy system)
            for (std::int32_t i = 0; i < enemyCount; ++i) {
                encounter.enemies.push_back(generateNextEnemyId());
            }

            // Calculate challenge rating
            encounter.challengeRating = calculateEncounterCR(difficulty, partyLevel);

            return encounter;
        }

        /// <summary>
        /// Get available directions from current room
        /// </summary>
        [[nodiscard]] std::vector<Direction> GetAvailableDirections(Id dungeonId) const {
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
        [[nodiscard]] std::optional<DungeonRoom> GetRoomInDirection(Id dungeonId, Direction direction) const {
            auto dungeon = GetDungeon(dungeonId);
            if (!dungeon.has_value()) return std::nullopt;

            auto currentRoom = dungeon->GetCurrentRoom();
            if (!currentRoom.has_value()) return std::nullopt;

            if (!currentRoom->HasConnection(direction)) {
                return std::nullopt;
            }

            Id targetRoomId = currentRoom->connections.at(direction);
            return dungeon->GetRoom(targetRoomId);
        }

        /// <summary>
        /// Check if dungeon is complete
        /// </summary>
        [[nodiscard]] bool IsDungeonComplete(Id dungeonId) const {
            auto dungeon = GetDungeon(dungeonId);
            if (!dungeon.has_value()) return false;
            return dungeon->IsComplete();
        }

        /// <summary>
        /// Get dungeon completion percentage
        /// </summary>
        [[nodiscard]] float GetCompletionPercentage(Id dungeonId) const {
            auto dungeon = GetDungeon(dungeonId);
            if (!dungeon.has_value()) return 0.0f;

            std::int32_t totalRooms = dungeon->GetRoomCount();
            if (totalRooms == 0) return 0.0f;

            std::int32_t clearedRooms = dungeon->GetClearedRoomCount();
            return static_cast<float>(clearedRooms) / static_cast<float>(totalRooms);
        }

        /// <summary>
        /// Remove dungeon
        /// </summary>
        bool RemoveDungeon(Id dungeonId) {
            return dungeons_.erase(dungeonId) > 0;
        }

        /// <summary>
        /// Get all dungeons
        /// </summary>
        [[nodiscard]] std::vector<Id> GetAllDungeonIds() const {
            std::vector<Id> ids;
            ids.reserve(dungeons_.size());
            for (const auto& [id, dungeon] : dungeons_) {
                ids.push_back(id);
            }
            return ids;
        }

        /// <summary>
        /// Get dungeon count
        /// </summary>
        [[nodiscard]] std::size_t GetDungeonCount() const {
            return dungeons_.size();
        }

    private:
        std::unordered_map<Id, Dungeon> dungeons_;
        mutable Id nextDungeonId_{1};
        mutable Id nextRoomId_{1};
        mutable Id nextEncounterId_{1};
        mutable Id nextEnemyId_{1000};

        [[nodiscard]] Id generateNextDungeonId() const {
            return nextDungeonId_++;
        }

        [[nodiscard]] Id generateNextRoomId() const {
            return nextRoomId_++;
        }

        [[nodiscard]] Id generateNextEncounterId() const {
            return nextEncounterId_++;
        }

        [[nodiscard]] Id generateNextEnemyId() const {
            return nextEnemyId_++;
        }

        [[nodiscard]] std::int32_t rollRoomCount(std::int32_t min, std::int32_t max) const {
            static thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_int_distribution<std::int32_t> dist(min, max);
            return dist(gen);
        }

        void generateRooms(Dungeon& dungeon, std::int32_t roomCount, const DungeonGenerationParams& params) {
            for (std::int32_t i = 0; i < roomCount; ++i) {
                DungeonRoom room{};
                room.roomId = generateNextRoomId();
                room.name = "Room " + std::to_string(room.roomId);
                room.description = "A dungeon room";
                room.gridPosition = Position(i % 5, i / 5); // Simple grid layout

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
            Id previousRoomId = it->first;
            ++it;

            while (it != dungeon.rooms.end()) {
                Id currentRoomId = it->first;

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

        [[nodiscard]] std::int32_t getEnemyCount(RoomDifficulty difficulty) const {
            switch (difficulty) {
                case RoomDifficulty::Easy: return 1;
                case RoomDifficulty::Normal: return 2;
                case RoomDifficulty::Hard: return 3;
                case RoomDifficulty::Elite: return 3;
                case RoomDifficulty::Boss: return 1;
                default: return 1;
            }
        }

        [[nodiscard]] float calculateEncounterCR(RoomDifficulty difficulty, std::int32_t partyLevel) const {
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

    };

} // namespace LB::Services::Dungeon