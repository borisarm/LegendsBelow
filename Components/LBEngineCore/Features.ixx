// Features.ixx - What a floor holds beyond fights (design-dungeon-and-world.md 4.3, 6.3, 8.3): a
// treasure room behind a locked gate (its key in a room on the way), a secret room behind a
// cracked wall, and the candle puzzles of puzzle rooms
export module LB.Engine:Features;

import std;
import LB.Dungeon;
import :World;

export namespace LB::Engine {

    enum class GateKind : std::uint8_t {
        Locked,   // iron bars: a key opens them
        Secret,   // a cracked wall: a blow breaks it
        Lever     // iron bars: a lever in another room opens them (design-dungeon-and-world.md 6.3)
    };

    /// <summary>
    /// A gate across the corridor cell into a dead-end treasure room: solid until opened
    /// </summary>
    struct Gate {
        GateKind kind{GateKind::Locked};
        std::int32_t cellX{0};
        std::int32_t cellY{0};
        WorldPoint center{};
        float yaw{0.0f};                  // across the corridor
        std::int32_t room{-1};            // the room behind it
        bool open{false};
        float openSeconds{0.0f};          // since it opened (the bars sink into the floor)
        bool warned{false};               // "locked" said once per approach
    };

    /// <summary>
    /// A lever that opens a gate elsewhere on the floor (design-dungeon-and-world.md 6.3)
    /// </summary>
    struct Lever {
        WorldPoint position{};
        std::size_t gate{0};              // the gate it opens (FloorFeatures::Gates)
        std::int32_t room{-1};            // the room it stands in
        bool pulled{false};
    };

    /// <summary>
    /// One of a floor's two waystones (design-dungeon-and-world.md 6.3, teleporters): by the
    /// entrance, and in the room before the way down
    /// </summary>
    struct Waystone {
        WorldPoint position{};
        std::int32_t room{-1};
    };

    /// <summary>
    /// A candle of a puzzle (lit or not), standing near a wall of the room
    /// </summary>
    struct PuzzleCandle {
        WorldPoint position{};
        bool lit{false};
    };

    /// <summary>
    /// A ring of candles: touching one flips it and its two neighbors; all lit, the room's chest
    /// opens (lights out on a ring, design-dungeon-and-world.md 4.3 puzzle gates)
    /// </summary>
    struct CandlePuzzle {
        std::int32_t room{-1};
        std::vector<PuzzleCandle> candles;
        bool solved{false};

        void Touch(std::size_t index) {
            if (solved || candles.empty()) return;
            const auto count = candles.size();
            for (const auto i : { (index + count - 1) % count, index, (index + 1) % count }) candles[i].lit = !candles[i].lit;
            solved = std::ranges::all_of(candles, &PuzzleCandle::lit);
        }
    };

    /// <summary>
    /// A floor's gates and puzzles, built from its layout and seed (the same floor, the same
    /// features). Closed gates block their cells in the world.
    /// </summary>
    class FloorFeatures {
    public:
        static constexpr std::size_t PuzzleCandles = 4;
        static constexpr std::int32_t LockedFromDepth = 2;
        static constexpr std::int32_t SecretFromDepth = 3;
        static constexpr std::int32_t LeverFromDepth = 4;      // from here a locked gate may answer a lever instead of a key
        static constexpr std::int32_t WaystoneFromDepth = 2;
        static constexpr float CandleInset = 0.9f;   // tiles in from the room's wall

        /// <summary>
        /// Dead-end treasure rooms: the first behind a locked gate (from LockedFromDepth; its key
        /// falls in the critical-path combat room the branch leaves from, or the nearest one on the
        /// way), the second behind a secret wall (from SecretFromDepth). Puzzle rooms get their
        /// candles, scrambled by touches from all lit (so they can always be solved).
        /// </summary>
        void Build(WorldMap& world, std::int32_t depth, std::uint32_t seed, bool restFloor) {
            gates_.clear();
            puzzles_.clear();
            levers_.clear();
            waystones_.clear();
            waystonesAwake_ = false;
            keyRoom_ = -1;
            keyDropped_ = false;
            world.ClearBlocked();
            if (restFloor) return;
            std::mt19937 rng(seed);
            const auto& layout = world.Layout();

            std::vector<std::int32_t> hidden;
            for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
                const auto& room = layout.rooms[i];
                if (room.type == Dungeon::RoomType::Treasure && !room.onCriticalPath && room.connections.size() == 1) hidden.push_back(static_cast<std::int32_t>(i));
            }
            std::ranges::shuffle(hidden, rng);
            if (!hidden.empty() && depth >= LockedFromDepth) {
                // From LeverFromDepth, half the floors open it with a lever where the key would fall
                const bool lever = depth >= LeverFromDepth && rng() % 2 == 0;
                if (AddGate(world, hidden[0], lever ? GateKind::Lever : GateKind::Locked)) {
                    const auto from = KeyRoomFor(layout, hidden[0]);
                    if (lever) levers_.push_back({ InsideTheWall(world, from), gates_.size() - 1, from, false });
                    else keyRoom_ = from;
                }
            }
            if (hidden.size() > 1 && depth >= SecretFromDepth) AddGate(world, hidden[1], GateKind::Secret);

            for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
                if (layout.rooms[i].type != Dungeon::RoomType::Puzzle) continue;
                const auto& area = layout.rooms[i].area;
                const auto corner = world.CellCorner(area.x, area.y);
                const float w = static_cast<float>(area.width) * WorldMap::CellTiles;
                const float h = static_cast<float>(area.height) * WorldMap::CellTiles;
                CandlePuzzle puzzle;
                puzzle.room = static_cast<std::int32_t>(i);
                // One by each wall, a little to the side of its middle (doorways open there)
                for (const auto& [fx, fy] : { std::pair{ 0.3f, 0.0f }, std::pair{ 1.0f, 0.3f }, std::pair{ 0.7f, 1.0f }, std::pair{ 0.0f, 0.7f } }) {
                    const float x = corner.x + std::clamp(fx * w, CandleInset, w - CandleInset);
                    const float y = corner.y + std::clamp(fy * h, CandleInset, h - CandleInset);
                    puzzle.candles.push_back({ { x, y }, true });
                }
                // Scrambled by one to three touches (never back to all lit)
                const auto touches = std::uniform_int_distribution<int>(1, 3)(rng);
                for (int t = 0; t < touches; ++t) puzzle.Touch(std::uniform_int_distribution<std::size_t>(0, PuzzleCandles - 1)(rng));
                puzzle.solved = false;
                if (std::ranges::all_of(puzzle.candles, &PuzzleCandle::lit)) puzzle.Touch(0);
                puzzle.solved = false;
                puzzles_.push_back(std::move(puzzle));
            }

            // Waystones: by the entrance, and in the critical-path room before the exit's
            if (depth >= WaystoneFromDepth) {
                const auto far = RoomBeforeTheExit(layout);
                if (far >= 0 && far != layout.entranceRoom) {
                    waystones_.push_back({ BesideTheCenter(world, layout.entranceRoom), layout.entranceRoom });
                    waystones_.push_back({ BesideTheCenter(world, far), far });
                }
            }
        }

        /// <summary>
        /// Open a gate: its cell is free from now on
        /// </summary>
        void Open(std::size_t index, WorldMap& world) {
            if (index >= gates_.size() || gates_[index].open) return;
            gates_[index].open = true;
            gates_[index].openSeconds = 0.0f;
            world.SetBlocked(gates_[index].cellX, gates_[index].cellY, false);
            for (auto& lever : levers_) {
                if (lever.gate == index) lever.pulled = true;
            }
        }

        /// <summary>
        /// Pull a lever: its gate opens. False when there is no such lever or it is pulled already.
        /// </summary>
        bool Pull(std::size_t index, WorldMap& world) {
            if (index >= levers_.size() || levers_[index].pulled) return false;
            Open(levers_[index].gate, world);
            return true;
        }

        [[nodiscard]] const std::vector<Lever>& Levers() const noexcept { return levers_; }

        /// <summary>
        /// The waystones: none, or the entrance's and the far one; travel between them once the
        /// far one is awake (the hero reached it)
        /// </summary>
        [[nodiscard]] const std::vector<Waystone>& Waystones() const noexcept { return waystones_; }
        [[nodiscard]] bool WaystonesAwake() const noexcept { return waystonesAwake_; }
        void SetWaystonesAwake(bool awake) noexcept { waystonesAwake_ = awake && !waystones_.empty(); }

        void Update(float elapsedSeconds) {
            for (auto& gate : gates_) {
                if (gate.open) gate.openSeconds += elapsedSeconds;
            }
        }

        [[nodiscard]] const std::vector<Gate>& Gates() const noexcept { return gates_; }
        [[nodiscard]] std::vector<Gate>& Gates() noexcept { return gates_; }
        [[nodiscard]] const std::vector<CandlePuzzle>& Puzzles() const noexcept { return puzzles_; }
        [[nodiscard]] std::vector<CandlePuzzle>& Puzzles() noexcept { return puzzles_; }

        /// <summary>
        /// The room whose clearing drops the key (-1: no locked gate), and whether it has
        /// </summary>
        [[nodiscard]] std::int32_t KeyRoom() const noexcept { return keyRoom_; }
        [[nodiscard]] bool KeyDropped() const noexcept { return keyDropped_; }
        void SetKeyDropped(bool dropped) noexcept { keyDropped_ = dropped; }

        /// <summary>
        /// Is this room's chest behind a gate or a puzzle (a greater chest)?
        /// </summary>
        [[nodiscard]] bool IsGuarded(std::int32_t room) const noexcept {
            return std::ranges::any_of(gates_, [&](const Gate& gate) { return gate.room == room; })
                || std::ranges::any_of(puzzles_, [&](const CandlePuzzle& puzzle) { return puzzle.room == room; });
        }

        /// <summary>
        /// May a room's chest be opened? (a puzzle room's only once solved)
        /// </summary>
        [[nodiscard]] bool ChestReady(std::int32_t room) const noexcept {
            const auto puzzle = std::ranges::find(puzzles_, room, &CandlePuzzle::room);
            return puzzle == puzzles_.end() || puzzle->solved;
        }

    private:
        // The gate goes on the corridor cell touching the room (a dead end has one)
        bool AddGate(WorldMap& world, std::int32_t room, GateKind kind) {
            const auto& layout = world.Layout();
            const auto& area = layout.rooms[static_cast<std::size_t>(room)].area;
            for (std::int32_t y = area.y - 1; y <= area.y + area.height; ++y) {
                for (std::int32_t x = area.x - 1; x <= area.x + area.width; ++x) {
                    if (area.Contains(x, y) || !layout.IsFloor(x, y) || layout.RoomAt(x, y) >= 0) continue;
                    const bool beside = (x == area.x - 1 || x == area.x + area.width) && y >= area.y && y < area.y + area.height;
                    const bool above = (y == area.y - 1 || y == area.y + area.height) && x >= area.x && x < area.x + area.width;
                    if (!beside && !above) continue;
                    // Across the corridor: a corridor running east-west takes a gate turned a quarter
                    Gate gate;
                    gate.kind = kind;
                    gate.cellX = x;
                    gate.cellY = y;
                    gate.center = world.CellCenter(x, y);
                    gate.yaw = beside ? std::numbers::pi_v<float> * 0.5f : 0.0f;
                    gate.room = room;
                    world.SetBlocked(x, y, true);
                    gates_.push_back(gate);
                    return true;
                }
            }
            return false;
        }

        // The critical-path room a branch leaves from (a combat room: a fight drops the key), or
        // else the critical combat room nearest the entrance side of it
        static std::int32_t KeyRoomFor(const Dungeon::FloorLayout& layout, std::int32_t treasure) {
            std::int32_t at = treasure;
            std::vector<bool> seen(layout.rooms.size(), false);
            while (at >= 0 && !layout.rooms[static_cast<std::size_t>(at)].onCriticalPath) {
                seen[static_cast<std::size_t>(at)] = true;
                std::int32_t next = -1;
                for (const auto other : layout.rooms[static_cast<std::size_t>(at)].connections) {
                    if (!seen[static_cast<std::size_t>(other)]) next = other;
                }
                at = next;
            }
            for (std::int32_t i = at; i >= 0; --i) {
                const auto& room = layout.rooms[static_cast<std::size_t>(i)];
                if (room.onCriticalPath && room.type == Dungeon::RoomType::Combat) return i;
            }
            for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
                if (layout.rooms[i].onCriticalPath && layout.rooms[i].type == Dungeon::RoomType::Combat) return static_cast<std::int32_t>(i);
            }
            return layout.entranceRoom;
        }

        // About a tile in from a room's north wall, a quarter of the way along
        static WorldPoint InsideTheWall(const WorldMap& world, std::int32_t room) {
            const auto& area = world.Layout().rooms[static_cast<std::size_t>(room)].area;
            const auto corner = world.CellCorner(area.x, area.y);
            const float w = static_cast<float>(area.width) * WorldMap::CellTiles;
            return { corner.x + std::clamp(w * 0.25f, 1.2f, w - 1.2f), corner.y + 1.1f };
        }

        // Beside a room's center (where chests, relics, and the hero arriving stand)
        static WorldPoint BesideTheCenter(const WorldMap& world, std::int32_t room) {
            const auto& area = world.Layout().rooms[static_cast<std::size_t>(room)].area;
            const auto center = world.RoomCenter(room);
            const float reach = std::min(1.8f, static_cast<float>(std::min(area.width, area.height)) * WorldMap::CellTiles * 0.5f - 1.0f);
            return { center.x + reach, center.y + reach };
        }

        // The critical-path room the exit's room is reached from (-1: none)
        static std::int32_t RoomBeforeTheExit(const Dungeon::FloorLayout& layout) {
            if (layout.exitRoom < 0) return -1;
            for (const auto other : layout.rooms[static_cast<std::size_t>(layout.exitRoom)].connections) {
                if (layout.rooms[static_cast<std::size_t>(other)].onCriticalPath) return other;
            }
            return -1;
        }

        std::vector<Gate> gates_;
        std::vector<CandlePuzzle> puzzles_;
        std::vector<Lever> levers_;
        std::vector<Waystone> waystones_;
        bool waystonesAwake_{false};
        std::int32_t keyRoom_{-1};
        bool keyDropped_{false};
    };

} // namespace LB::Engine
