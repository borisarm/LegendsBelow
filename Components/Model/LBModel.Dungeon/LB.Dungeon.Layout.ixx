// LB.Dungeon.Layout.ixx - A dungeon floor as a grid of cells: rooms joined by corridors
export module LB.Dungeon.Layout;

import LB.Core;
import LB.Dungeon.Enums;
import std;

export namespace LB::Dungeon {

    /// <summary>
    /// A rectangle of cells
    /// </summary>
    struct CellRect {
        std::int32_t x{0};
        std::int32_t y{0};
        std::int32_t width{0};
        std::int32_t height{0};

        [[nodiscard]] bool Contains(std::int32_t cx, std::int32_t cy) const noexcept {
            return cx >= x && cx < x + width && cy >= y && cy < y + height;
        }

        [[nodiscard]] std::int32_t CenterX() const noexcept { return x + width / 2; }
        [[nodiscard]] std::int32_t CenterY() const noexcept { return y + height / 2; }
    };

    /// <summary>
    /// A room of a floor: its purpose, the cells it covers, and the rooms its corridors lead to
    /// </summary>
    struct LayoutRoom {
        RoomType type{RoomType::Empty};
        CellRect area;
        bool onCriticalPath{false};          // between the entrance and the exit
        std::vector<std::int32_t> connections;    // indices of connected rooms
    };

    /// <summary>
    /// A dungeon floor (design-dungeon-and-world.md 2.1): a grid of floor and solid cells, the rooms
    /// carved into it, and the entrance and exit. Every floor cell is reachable from every other;
    /// the outermost ring of cells is always solid.
    /// </summary>
    struct FloorLayout {
        std::int32_t width{0};                    // cells
        std::int32_t height{0};
        std::vector<std::uint8_t> cells;     // row-major, 1: floor
        std::vector<LayoutRoom> rooms;       // the entrance first
        std::int32_t entranceRoom{0};
        std::int32_t exitRoom{0};

        [[nodiscard]] bool InBounds(std::int32_t x, std::int32_t y) const noexcept {
            return x >= 0 && y >= 0 && x < width && y < height;
        }

        [[nodiscard]] bool IsFloor(std::int32_t x, std::int32_t y) const noexcept {
            return InBounds(x, y) && cells[static_cast<std::size_t>(y * width + x)] != 0;
        }

        void SetFloor(std::int32_t x, std::int32_t y) {
            if (InBounds(x, y)) cells[static_cast<std::size_t>(y * width + x)] = 1;
        }

        /// <summary>
        /// The room covering a cell, or -1 (corridors, walls)
        /// </summary>
        [[nodiscard]] std::int32_t RoomAt(std::int32_t x, std::int32_t y) const noexcept {
            for (std::size_t i = 0; i < rooms.size(); ++i) {
                if (rooms[i].area.Contains(x, y)) return static_cast<std::int32_t>(i);
            }
            return -1;
        }

        [[nodiscard]] std::int32_t FloorCellCount() const noexcept {
            return static_cast<std::int32_t>(std::ranges::count(cells, std::uint8_t{1}));
        }

        /// <summary>
        /// One rectangular room of floor cells inside a ring of solid cells
        /// </summary>
        [[nodiscard]] static FloorLayout SingleRoom(std::int32_t roomWidth, std::int32_t roomHeight) {
            FloorLayout layout;
            layout.width = roomWidth + 2;
            layout.height = roomHeight + 2;
            layout.cells.assign(static_cast<std::size_t>(layout.width * layout.height), 0);
            for (std::int32_t y = 1; y <= roomHeight; ++y) {
                for (std::int32_t x = 1; x <= roomWidth; ++x) layout.SetFloor(x, y);
            }
            layout.rooms.push_back(LayoutRoom{ RoomType::Entrance, CellRect{ 1, 1, roomWidth, roomHeight }, true, {} });
            return layout;
        }
    };

    /// <summary>
    /// How a floor is generated (design-dungeon-and-world.md 3.2-3.3)
    /// </summary>
    struct FloorGenerationParams {
        std::int32_t criticalPathRooms{6};        // entrance to exit, both included
        float branchChance{0.5f};            // per critical room between entrance and exit
        std::int32_t maxBranchLength{2};          // rooms in one side branch
        std::int32_t slotSize{10};                // cells per room slot (rooms sit in a grid of slots)
        std::int32_t minRoomSize{3};              // cells
        std::int32_t maxRoomSize{6};
        // Branch room types, by weight (critical rooms between entrance and exit are combat rooms)
        std::int32_t treasureWeight{4};
        std::int32_t eliteWeight{3};
        std::int32_t safeWeight{2};
        std::int32_t puzzleWeight{0};           // a puzzle room (design-dungeon-and-world.md 2.2): candles to light
    };

} // namespace LB::Dungeon
