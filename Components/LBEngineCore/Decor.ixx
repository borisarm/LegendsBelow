// Decor.ixx - What makes floors differ: a look per tier of floors (crypts, caves, ruins), props in
// the rooms' corners, and spike traps in the corridors
export module LB.Engine:Decor;

import std;
import LB.Dungeon;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// The pieces a tier of floors is built from (design-dungeon-and-world.md 7.1: every tier has
    /// its own look)
    /// </summary>
    struct FloorTheme {
        std::string_view name;
        std::string_view floor;          // one tile
        std::string_view floorVariant;   // scattered among them
        std::string_view wall;           // a two-tile segment
        std::string_view corner;
    };

    inline constexpr std::array<FloorTheme, 3> FloorThemes{ {
        { "Crypts", "tileBrickB_small.glb", "tileBrickA_small.glb", "wall.glb", "pillar.glb" },
        { "Caves", "floor_dirt_small_A.gltf", "floor_dirt_small_weeds.gltf", "wall_cracked.gltf", "pillar.glb" },
        { "Ruins", "floor_tile_small_broken_A.gltf", "floor_tile_small_weeds_A.gltf", "wall_broken.gltf", "pillar.glb" },
    } };

    /// <summary>
    /// The tier of a depth (three floors each, a boss at the end), cycling through the themes
    /// </summary>
    [[nodiscard]] inline const FloorTheme& ThemeForDepth(std::int32_t depth) noexcept {
        return FloorThemes[static_cast<std::size_t>(std::max(0, depth - 1) / 3) % FloorThemes.size()];
    }

    /// <summary>
    /// A piece of furniture in a room: a model, where it stands, which way it faces, and how wide
    /// it is (a round obstacle in the world)
    /// </summary>
    struct Prop {
        std::string_view model;
        WorldPoint position{};
        float yaw{0.0f};
        float radius{0.5f};
    };

    /// <summary>
    /// Spikes in a corridor cell: down, then a warning (they stir), then up for a moment; up, they
    /// strike whoever stands on the cell, once a cycle
    /// </summary>
    struct SpikeTrap {
        std::int32_t cellX{0};
        std::int32_t cellY{0};
        WorldPoint center{};
        float phase{0.0f};              // seconds into its cycle at time 0
        bool struck{false};             // this cycle's strike is spent
        bool wasUp{false};
    };

    /// <summary>
    /// What the traps did this step: spikes that rose (for their sound), and whether they struck
    /// the player
    /// </summary>
    struct TrapStep {
        std::vector<WorldPoint> rose;
        bool playerStruck{false};
    };

    /// <summary>
    /// A floor's props and traps, built from its layout and seed (the same floor, the same decor).
    /// Props become obstacles of the world.
    /// </summary>
    class FloorDecor {
    public:
        static constexpr float TrapPeriodSeconds = 3.0f;
        static constexpr float TrapUpSeconds = 0.9f;
        static constexpr float TrapWarningSeconds = 0.4f;   // the spikes stir before they rise
        static constexpr float PropRadius = 0.55f;

        // Every model the decor may use (loaded with the world)
        static constexpr std::array<std::string_view, 15> Models = {
            "barrel_large.gltf", "barrel_large_decorated.gltf", "barrel_small_stack.gltf", "crates_stacked.gltf",
            "crate_large_decorated.gltf", "trunk_large_A.gltf", "coin_stack_large.gltf", "table_medium_decorated_A.gltf",
            "rocks.gltf", "rubble_half.gltf", "floor_tile_big_spikes.gltf", "floor_tile_big_grate.gltf",
            "floor_dirt_small_A.gltf", "floor_dirt_small_weeds.gltf", "floor_tile_small_broken_A.gltf" };
        static constexpr std::array<std::string_view, 3> MoreModels = { "floor_tile_small_weeds_A.gltf", "wall_cracked.gltf", "wall_broken.gltf" };
        static constexpr std::string_view SpikesModel = "floor_tile_big_spikes.gltf";
        static constexpr std::string_view GrateModel = "floor_tile_big_grate.gltf";

        /// <summary>
        /// Props in the rooms' corners (not by a doorway), by room type and theme; spike traps in
        /// corridor cells from depth 2 (none on a resting floor). The world gains the props as obstacles.
        /// </summary>
        void Build(WorldMap& world, std::int32_t depth, std::uint32_t seed, bool restFloor) {
            props_.clear();
            traps_.clear();
            world.ClearObstacles();
            std::mt19937 rng(seed);
            const auto& layout = world.Layout();
            const bool caves = ThemeForDepth(depth).name == "Caves";

            for (const auto& room : layout.rooms) {
                const auto choices = PropsFor(room.type, caves);
                if (choices.empty()) continue;
                const auto& area = room.area;
                const WorldPoint center = world.CellCenter(area.CenterX(), area.CenterY());
                // The four inner corners: (cell, outward direction)
                const std::array<std::tuple<std::int32_t, std::int32_t, std::int32_t, std::int32_t>, 4> corners{ {
                    { area.x, area.y, -1, -1 }, { area.x + area.width - 1, area.y, 1, -1 },
                    { area.x, area.y + area.height - 1, -1, 1 }, { area.x + area.width - 1, area.y + area.height - 1, 1, 1 } } };
                const auto count = std::uniform_int_distribution<int>(2, 3)(rng);
                int placed = 0;
                for (const auto& [cx, cy, dx, dy] : corners) {
                    if (placed >= count) break;
                    // A corridor leaving by this corner would be blocked
                    if (layout.IsFloor(cx + dx, cy) || layout.IsFloor(cx, cy + dy)) continue;
                    const auto corner = world.CellCorner(cx, cy);
                    const float x = dx < 0 ? corner.x + 0.85f : corner.x + WorldMap::CellTiles - 0.85f;
                    const float y = dy < 0 ? corner.y + 0.85f : corner.y + WorldMap::CellTiles - 0.85f;
                    const auto model = choices[std::uniform_int_distribution<std::size_t>(0, choices.size() - 1)(rng)];
                    props_.push_back({ model, { x, y }, std::atan2(center.x - x, center.y - y), PropRadius });
                    world.AddObstacle({ x, y }, PropRadius);
                    ++placed;
                }
            }

            if (restFloor || depth < 2) return;
            std::vector<std::pair<std::int32_t, std::int32_t>> corridors;
            for (std::int32_t cy = 0; cy < layout.height; ++cy) {
                for (std::int32_t cx = 0; cx < layout.width; ++cx) {
                    if (layout.IsFloor(cx, cy) && world.RoomAt(world.CellCenter(cx, cy)) < 0) corridors.emplace_back(cx, cy);
                }
            }
            std::ranges::shuffle(corridors, rng);
            const auto count = std::min<std::size_t>(corridors.size(), static_cast<std::size_t>(std::min(1 + depth / 3, 4)));
            for (std::size_t i = 0; i < count; ++i) {
                const auto [cx, cy] = corridors[i];
                traps_.push_back({ cx, cy, world.CellCenter(cx, cy), std::uniform_real_distribution<float>(0.0f, TrapPeriodSeconds)(rng) });
            }
        }

        /// <summary>
        /// Run the traps' cycles to a time (the game clock); a trap that is up strikes a living
        /// player standing on its cell, once a cycle
        /// </summary>
        TrapStep UpdateTraps(double timeSeconds, WorldPoint player, float playerRadius, bool alive) {
            TrapStep step;
            for (auto& trap : traps_) {
                const bool up = Raise(trap, timeSeconds) >= 1.0f;
                if (up && !trap.wasUp) step.rose.push_back(trap.center);
                if (!up) trap.struck = false;
                trap.wasUp = up;
                const float half = WorldMap::CellTiles * 0.5f + playerRadius * 0.5f;
                const bool on = std::abs(player.x - trap.center.x) < half && std::abs(player.y - trap.center.y) < half;
                if (up && on && alive && !trap.struck) {
                    trap.struck = true;
                    step.playerStruck = true;
                }
            }
            return step;
        }

        /// <summary>
        /// How far a trap's spikes are out at a time: 0 down, a little while they stir, 1 up
        /// </summary>
        [[nodiscard]] static float Raise(const SpikeTrap& trap, double timeSeconds) noexcept {
            const float t = static_cast<float>(std::fmod(timeSeconds + trap.phase, static_cast<double>(TrapPeriodSeconds)));
            const float upAt = TrapPeriodSeconds - TrapUpSeconds;
            if (t >= upAt) return 1.0f;
            if (t >= upAt - TrapWarningSeconds) return 0.15f;
            return 0.0f;
        }

        [[nodiscard]] bool IsTrapCell(std::int32_t cx, std::int32_t cy) const noexcept {
            return std::ranges::any_of(traps_, [&](const SpikeTrap& trap) { return trap.cellX == cx && trap.cellY == cy; });
        }

        [[nodiscard]] const std::vector<Prop>& Props() const noexcept { return props_; }
        [[nodiscard]] const std::vector<SpikeTrap>& Traps() const noexcept { return traps_; }

        void Clear() noexcept {
            props_.clear();
            traps_.clear();
        }

    private:
        static std::vector<std::string_view> PropsFor(Dungeon::RoomType type, bool caves) {
            using Dungeon::RoomType;
            switch (type) {
            case RoomType::Combat:
                return caves ? std::vector<std::string_view>{ "rocks.gltf", "rubble_half.gltf", "barrel_large.gltf" }
                             : std::vector<std::string_view>{ "barrel_large.gltf", "barrel_small_stack.gltf", "crates_stacked.gltf" };
            case RoomType::Treasure: return { "trunk_large_A.gltf", "coin_stack_large.gltf", "barrel_large_decorated.gltf" };
            case RoomType::Elite: return { "crate_large_decorated.gltf", "barrel_large_decorated.gltf", "crates_stacked.gltf" };
            case RoomType::Entrance: return { "table_medium_decorated_A.gltf", "barrel_small_stack.gltf" };
            case RoomType::Exit:
            case RoomType::Boss: return { "barrel_large.gltf", "crates_stacked.gltf" };
            default: return {};
            }
        }

        std::vector<Prop> props_;
        std::vector<SpikeTrap> traps_;
    };

} // namespace LB::Engine
