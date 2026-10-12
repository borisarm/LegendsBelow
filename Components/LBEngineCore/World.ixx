// World.ixx - The walkable world of a floor: collision, sight lines, and paths on its cell grid
export module LB.Engine:World;

import std;
import LB.Dungeon;

export namespace LB::Engine {

    /// <summary>
    /// Continuous position in world tiles (tile (0, 0) covers [0, 1) x [0, 1))
    /// </summary>
    struct WorldPoint {
        float x{0.0f};
        float y{0.0f};
    };

    /// <summary>
    /// Straight-line distance between two ground points (tiles)
    /// </summary>
    [[nodiscard]] inline float Distance(WorldPoint a, WorldPoint b) noexcept {
        return std::hypot(b.x - a.x, b.y - a.y);
    }

    /// <summary>
    /// The ground direction a yaw faces (radians around the vertical axis; 0 faces +Z)
    /// </summary>
    [[nodiscard]] inline WorldPoint Forward(float yaw) noexcept {
        return { std::sin(yaw), std::cos(yaw) };
    }

    /// <summary>
    /// A floor layout placed in the world. Each layout cell is CellTiles x CellTiles tiles (one
    /// KayKit wall segment long); floor cells are open, every other cell is solid. Moves slide
    /// along walls, and a flow field leads enemies around them.
    /// </summary>
    class WorldMap {
    public:
        static constexpr float CellTiles = 2.0f;

        WorldMap() = default;

        /// <summary>
        /// A layout whose cell (0, 0) starts at a world point (in tiles)
        /// </summary>
        WorldMap(Dungeon::FloorLayout layout, WorldPoint origin) : layout_(std::move(layout)), origin_(origin) {}

        /// <summary>
        /// The test arena: one room of 2 x halfWidth by 2 x halfHeight tiles centered on the origin
        /// (both whole numbers of cells)
        /// </summary>
        [[nodiscard]] static WorldMap Arena(float halfWidth, float halfHeight) {
            const auto cellsX = static_cast<std::int32_t>(std::lround(2.0f * halfWidth / CellTiles));
            const auto cellsY = static_cast<std::int32_t>(std::lround(2.0f * halfHeight / CellTiles));
            return WorldMap(Dungeon::FloorLayout::SingleRoom(cellsX, cellsY), { -halfWidth - CellTiles, -halfHeight - CellTiles });
        }

        [[nodiscard]] const Dungeon::FloorLayout& Layout() const noexcept { return layout_; }
        [[nodiscard]] WorldPoint Origin() const noexcept { return origin_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return layout_.cells.empty(); }

        /// <summary>
        /// The cell under a world point (may be outside the layout)
        /// </summary>
        [[nodiscard]] std::pair<std::int32_t, std::int32_t> CellOf(WorldPoint p) const noexcept {
            return { static_cast<std::int32_t>(std::floor((p.x - origin_.x) / CellTiles)),
                     static_cast<std::int32_t>(std::floor((p.y - origin_.y) / CellTiles)) };
        }

        [[nodiscard]] WorldPoint CellCorner(std::int32_t x, std::int32_t y) const noexcept {
            return { origin_.x + static_cast<float>(x) * CellTiles, origin_.y + static_cast<float>(y) * CellTiles };
        }

        [[nodiscard]] WorldPoint CellCenter(std::int32_t x, std::int32_t y) const noexcept {
            const auto corner = CellCorner(x, y);
            return { corner.x + CellTiles * 0.5f, corner.y + CellTiles * 0.5f };
        }

        [[nodiscard]] bool IsOpen(WorldPoint p) const noexcept {
            const auto [x, y] = CellOf(p);
            return layout_.IsFloor(x, y);
        }

        /// <summary>
        /// The room a point is in, or -1 (corridors, walls)
        /// </summary>
        [[nodiscard]] std::int32_t RoomAt(WorldPoint p) const noexcept {
            const auto [x, y] = CellOf(p);
            return layout_.IsFloor(x, y) ? layout_.RoomAt(x, y) : -1;
        }

        /// <summary>
        /// The world point at the middle of a room
        /// </summary>
        [[nodiscard]] WorldPoint RoomCenter(std::int32_t room) const noexcept {
            const auto& area = layout_.rooms[static_cast<std::size_t>(room)].area;
            const auto corner = CellCorner(area.x, area.y);
            return { corner.x + static_cast<float>(area.width) * CellTiles * 0.5f, corner.y + static_cast<float>(area.height) * CellTiles * 0.5f };
        }

        /// <summary>
        /// A round obstacle (a prop): bodies keep out of it; sight passes over it
        /// </summary>
        void AddObstacle(WorldPoint center, float radius) { obstacles_.push_back({ center, radius }); }
        void ClearObstacles() noexcept { obstacles_.clear(); }
        [[nodiscard]] std::size_t ObstacleCount() const noexcept { return obstacles_.size(); }

        /// <summary>
        /// Does a circle stand only on open cells, clear of the obstacles?
        /// </summary>
        [[nodiscard]] bool CircleFits(WorldPoint p, float radius) const noexcept {
            for (const auto& obstacle : obstacles_) {
                if (Distance(p, obstacle.center) < radius + obstacle.radius) return false;
            }
            const auto [x0, y0] = CellOf({ p.x - radius, p.y - radius });
            const auto [x1, y1] = CellOf({ p.x + radius, p.y + radius });
            for (std::int32_t y = y0; y <= y1; ++y) {
                for (std::int32_t x = x0; x <= x1; ++x) {
                    if (layout_.IsFloor(x, y)) continue;
                    // Closest point of the solid cell to the circle's center
                    const auto corner = CellCorner(x, y);
                    const float cx = std::clamp(p.x, corner.x, corner.x + CellTiles);
                    const float cy = std::clamp(p.y, corner.y, corner.y + CellTiles);
                    if ((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy) < radius * radius) return false;
                }
            }
            return true;
        }

        /// <summary>
        /// Move a circle by a step, sliding along walls: each axis moves on its own, as far as it
        /// can before touching a solid cell
        /// </summary>
        [[nodiscard]] WorldPoint Move(WorldPoint from, WorldPoint step, float radius) const noexcept {
            WorldPoint p = from;
            p.x = MoveAxis(p, step.x, radius, true);
            p.y = MoveAxis(p, step.y, radius, false);
            // Out of the obstacles it ran into (sliding around them); back where it was when that
            // would put it in a wall
            for (const auto& obstacle : obstacles_) {
                const float dx = p.x - obstacle.center.x;
                const float dy = p.y - obstacle.center.y;
                const float distance = std::hypot(dx, dy);
                const float minimum = radius + obstacle.radius;
                if (distance >= minimum) continue;
                const float nx = distance > 1e-4f ? dx / distance : 1.0f;
                const float ny = distance > 1e-4f ? dy / distance : 0.0f;
                const WorldPoint out{ obstacle.center.x + nx * minimum, obstacle.center.y + ny * minimum };
                p = CircleFitsCells(out, radius) ? out : from;
            }
            return p;
        }

        /// <summary>
        /// Is the straight line between two points open all the way? (sampled every quarter tile)
        /// </summary>
        [[nodiscard]] bool LineOfSight(WorldPoint a, WorldPoint b) const noexcept {
            const float length = std::hypot(b.x - a.x, b.y - a.y);
            const int samples = std::max(1, static_cast<int>(std::ceil(length / 0.25f)));
            for (int i = 0; i <= samples; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(samples);
                if (!IsOpen({ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t })) return false;
            }
            return true;
        }

        /// <summary>
        /// Distances (in cell steps, diagonals included where both sides are open) from every open
        /// cell to the target's cell; -1 where the target cannot be reached
        /// </summary>
        void BuildFlowField(WorldPoint target) const {
            flow_.assign(layout_.cells.size(), -1);
            flowTarget_ = CellOf(target);
            const auto [tx, ty] = flowTarget_;
            if (!layout_.IsFloor(tx, ty)) return;

            std::vector<std::pair<std::int32_t, std::int32_t>> frontier{ { tx, ty } };
            flow_[Index(tx, ty)] = 0;
            for (std::size_t head = 0; head < frontier.size(); ++head) {
                const auto [x, y] = frontier[head];
                const auto distance = flow_[Index(x, y)];
                for (const auto& [dx, dy] : Neighbors) {
                    if (!Passable(x, y, dx, dy) || flow_[Index(x + dx, y + dy)] >= 0) continue;
                    flow_[Index(x + dx, y + dy)] = distance + 1;
                    frontier.emplace_back(x + dx, y + dy);
                }
            }
        }

        /// <summary>
        /// Where to head from a point to follow the flow field toward its target: the center of the
        /// neighboring cell closest to it (the target itself once in its cell); null without a path
        /// </summary>
        [[nodiscard]] std::optional<WorldPoint> NextWaypoint(WorldPoint from) const noexcept {
            const auto [x, y] = CellOf(from);
            if (!layout_.IsFloor(x, y) || flow_.size() != layout_.cells.size() || flow_[Index(x, y)] < 0) return std::nullopt;
            if (std::pair{ x, y } == flowTarget_) return std::nullopt;

            std::int32_t best = flow_[Index(x, y)];
            std::optional<WorldPoint> waypoint;
            for (const auto& [dx, dy] : Neighbors) {
                if (!Passable(x, y, dx, dy)) continue;
                const auto distance = flow_[Index(x + dx, y + dy)];
                if (distance >= 0 && distance < best) {
                    best = distance;
                    waypoint = CellCenter(x + dx, y + dy);
                }
            }
            return waypoint;
        }

        /// <summary>
        /// A random point in a room, a margin (tiles) away from its walls
        /// </summary>
        template<typename Random>
        [[nodiscard]] WorldPoint RandomPointInRoom(std::int32_t room, float margin, Random& rng) const {
            const auto& area = layout_.rooms[static_cast<std::size_t>(room)].area;
            const auto corner = CellCorner(area.x, area.y);
            const float width = static_cast<float>(area.width) * CellTiles;
            const float height = static_cast<float>(area.height) * CellTiles;
            const float mx = std::min(margin, width * 0.5f);
            const float my = std::min(margin, height * 0.5f);
            std::uniform_real_distribution<float> x(corner.x + mx, corner.x + width - mx);
            std::uniform_real_distribution<float> y(corner.y + my, corner.y + height - my);
            return { x(rng), y(rng) };
        }

    private:
        // The coordinate a circle reaches moving `delta` along one axis: the target, or the first
        // contact with a solid cell in the way (its face, or its corner where the circle only
        // grazes it)
        struct Obstacle {
            WorldPoint center{};
            float radius{0.5f};
        };

        // Does a circle stand only on open cells (obstacles aside)?
        [[nodiscard]] bool CircleFitsCells(WorldPoint p, float radius) const noexcept {
            const auto [x0, y0] = CellOf({ p.x - radius, p.y - radius });
            const auto [x1, y1] = CellOf({ p.x + radius, p.y + radius });
            for (std::int32_t y = y0; y <= y1; ++y) {
                for (std::int32_t x = x0; x <= x1; ++x) {
                    if (layout_.IsFloor(x, y)) continue;
                    const auto corner = CellCorner(x, y);
                    const float cx = std::clamp(p.x, corner.x, corner.x + CellTiles);
                    const float cy = std::clamp(p.y, corner.y, corner.y + CellTiles);
                    if ((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy) < radius * radius) return false;
                }
            }
            return true;
        }

        [[nodiscard]] float MoveAxis(WorldPoint p, float delta, float radius, bool alongX) const noexcept {
            const float start = alongX ? p.x : p.y;
            const float other = alongX ? p.y : p.x;
            if (delta == 0.0f) return start;
            const float target = start + delta;

            // Cells the circle sweeps over
            const float lo = std::min(start, target) - radius;
            const float hi = std::max(start, target) + radius;
            const WorldPoint a = alongX ? WorldPoint{ lo, other - radius } : WorldPoint{ other - radius, lo };
            const WorldPoint b = alongX ? WorldPoint{ hi, other + radius } : WorldPoint{ other + radius, hi };
            const auto [x0, y0] = CellOf(a);
            const auto [x1, y1] = CellOf(b);

            float reached = target;
            for (std::int32_t y = y0; y <= y1; ++y) {
                for (std::int32_t x = x0; x <= x1; ++x) {
                    if (layout_.IsFloor(x, y)) continue;
                    const auto corner = CellCorner(x, y);
                    const float cellLo = alongX ? corner.x : corner.y;
                    const float cellHi = cellLo + CellTiles;
                    const float sideLo = alongX ? corner.y : corner.x;
                    const float sideHi = sideLo + CellTiles;

                    // How far the circle's center is from the cell across the motion
                    const float across = other < sideLo ? sideLo - other : other > sideHi ? other - sideHi : 0.0f;
                    if (across >= radius) continue;
                    const float reach = std::sqrt(radius * radius - across * across);
                    if (delta > 0.0f && cellLo >= start) reached = std::min(reached, cellLo - reach);
                    if (delta < 0.0f && cellHi <= start) reached = std::max(reached, cellHi + reach);
                }
            }
            // Never backwards (already touching)
            return delta > 0.0f ? std::max(start, reached) : std::min(start, reached);
        }

        static constexpr std::array<std::pair<std::int32_t, std::int32_t>, 8> Neighbors{ {
            { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } } };

        [[nodiscard]] std::size_t Index(std::int32_t x, std::int32_t y) const noexcept {
            return static_cast<std::size_t>(y * layout_.width + x);
        }

        // A diagonal step only where both cells beside it are open (no cutting wall corners)
        [[nodiscard]] bool Passable(std::int32_t x, std::int32_t y, std::int32_t dx, std::int32_t dy) const noexcept {
            if (!layout_.IsFloor(x + dx, y + dy)) return false;
            return dx == 0 || dy == 0 || (layout_.IsFloor(x + dx, y) && layout_.IsFloor(x, y + dy));
        }

        Dungeon::FloorLayout layout_;
        std::vector<Obstacle> obstacles_;
        WorldPoint origin_{};
        // Last flow field (a cache: rebuilt when enemies need a path)
        mutable std::vector<std::int32_t> flow_;
        mutable std::pair<std::int32_t, std::int32_t> flowTarget_{ -1, -1 };
    };

} // namespace LB::Engine
