// Walls.ixx - The wall pieces of the world: placed from the floor layout, faded while they hide
// the player from the camera, and drawn
export module LB.Engine:Walls;

import std;
import LB.RenderingSystem;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// A wall or corner piece, placed once when a floor is built
    /// </summary>
    struct WallPiece {
        Systems::Rendering::ModelId model{};
        Systems::Rendering::Vector3 position{};
        float yaw{0.0f};
        Systems::Rendering::Vector3 boundsMin{};   // world space
        Systems::Rendering::Vector3 boundsMax{};
        float opacity{1.0f};
    };

    /// <summary>
    /// The walls of the current floor (KayKit pieces): a segment along every edge between a floor
    /// cell and a solid one, and pillars filling the outer corners. Pieces between the camera and
    /// the player fade so the player stays visible (game-design.md, occlusion handling).
    /// </summary>
    class WallSet {
    public:
        static constexpr float FadedAlpha = 0.3f;
        static constexpr float FadeSeconds = 0.2f;   // full fade in or out

        /// <summary>
        /// Place the pieces for a world's layout. Models are scaled by modelScale and stood on the
        /// floor (their lowest point at height 0).
        /// </summary>
        void Build(const WorldMap& world, const Systems::Rendering::RenderingSystem& renderer,
                   Systems::Rendering::ModelId wallModel, Systems::Rendering::ModelId cornerModel, float modelScale);

        void Clear() noexcept { pieces_.clear(); }

        /// <summary>
        /// Fade the pieces hiding the player (sight lines from the eye to the feet, chest, and
        /// head) and bring the others back, easing over FadeSeconds
        /// </summary>
        void UpdateFade(Systems::Rendering::Vector3 eye, std::optional<WorldPoint> player, float elapsedSeconds);

        void Draw(Systems::Rendering::RenderingSystem& renderer, Systems::Rendering::Color tint, float modelScale) const;

        /// <summary>
        /// The wall segment (not a corner) closest to a ground point within a distance
        /// </summary>
        [[nodiscard]] std::optional<std::size_t> FindSegmentNear(WorldPoint at, float within) const;

        [[nodiscard]] const std::vector<WallPiece>& Pieces() const noexcept { return pieces_; }
        [[nodiscard]] bool Empty() const noexcept { return pieces_.empty(); }

        /// <summary>
        /// Opacity of a piece (1 when there is no such piece)
        /// </summary>
        [[nodiscard]] float Opacity(std::optional<std::size_t> piece) const noexcept {
            return piece && *piece < pieces_.size() ? pieces_[*piece].opacity : 1.0f;
        }

    private:
        static bool SegmentHitsBox(Systems::Rendering::Vector3 from, Systems::Rendering::Vector3 to,
                                   Systems::Rendering::Vector3 lo, Systems::Rendering::Vector3 hi) noexcept;

        std::vector<WallPiece> pieces_;
        Systems::Rendering::ModelId segmentModel_{};
    };

    // ------------------------------------------------------------------------------------------

    namespace WallDetail {
        // World bounding box of a model placed with scale, a turn around Y, and a position
        // (matches the renderer's Scale * RotationY * Translation)
        inline WallPiece Place(const Systems::Rendering::ModelData& data, Systems::Rendering::ModelId model,
                               Systems::Rendering::Vector3 at, float yaw, float scale) {
            WallPiece piece{ model, at, yaw };
            const float c = std::cos(yaw);
            const float s = std::sin(yaw);
            constexpr float big = std::numeric_limits<float>::max();
            piece.boundsMin = { big, data.boundsMin.y * scale + at.y, big };
            piece.boundsMax = { -big, data.boundsMax.y * scale + at.y, -big };
            for (float cx : { data.boundsMin.x, data.boundsMax.x }) {
                for (float cz : { data.boundsMin.z, data.boundsMax.z }) {
                    const float wx = (cx * c + cz * s) * scale + at.x;
                    const float wz = (-cx * s + cz * c) * scale + at.z;
                    piece.boundsMin.x = std::min(piece.boundsMin.x, wx);
                    piece.boundsMax.x = std::max(piece.boundsMax.x, wx);
                    piece.boundsMin.z = std::min(piece.boundsMin.z, wz);
                    piece.boundsMax.z = std::max(piece.boundsMax.z, wz);
                }
            }
            return piece;
        }
    }

    inline void WallSet::Build(const WorldMap& world, const Systems::Rendering::RenderingSystem& renderer,
                               Systems::Rendering::ModelId wallModel, Systems::Rendering::ModelId cornerModel, float modelScale) {
        pieces_.clear();
        segmentModel_ = wallModel;
        const auto* wall = renderer.GetModel(wallModel);
        const auto* corner = renderer.GetModel(cornerModel);
        if (!wall || !corner) return;

        const float halfDepth = (wall->boundsMax.z - wall->boundsMin.z) * modelScale * 0.5f;
        const float wallY = -wall->boundsMin.y * modelScale;
        const float cornerY = -corner->boundsMin.y * modelScale;
        constexpr float quarterTurn = std::numbers::pi_v<float> * 0.5f;
        constexpr float cell = WorldMap::CellTiles;
        const auto segment = [&](float x, float z, float yaw) {
            pieces_.push_back(WallDetail::Place(*wall, wallModel, { x, wallY, z }, yaw, modelScale));
        };

        // A segment is one cell long; its inner face sits on the edge
        const auto& layout = world.Layout();
        for (std::int32_t y = 0; y < layout.height; ++y) {
            for (std::int32_t x = 0; x < layout.width; ++x) {
                if (!layout.IsFloor(x, y)) continue;
                const auto at = world.CellCorner(x, y);
                const float midX = at.x + cell * 0.5f;
                const float midZ = at.y + cell * 0.5f;
                if (!layout.IsFloor(x, y - 1)) segment(midX, at.y - halfDepth, 0.0f);
                if (!layout.IsFloor(x, y + 1)) segment(midX, at.y + cell + halfDepth, 0.0f);
                if (!layout.IsFloor(x - 1, y)) segment(at.x - halfDepth, midZ, quarterTurn);
                if (!layout.IsFloor(x + 1, y)) segment(at.x + cell + halfDepth, midZ, quarterTurn);
            }
        }

        // Corners: where only one of the four cells around a grid point is floor, the walls of
        // its two edges leave a gap at the point; a pillar fills it, set back from the floor
        for (std::int32_t y = 0; y <= layout.height; ++y) {
            for (std::int32_t x = 0; x <= layout.width; ++x) {
                const bool cells[4] = { layout.IsFloor(x - 1, y - 1), layout.IsFloor(x, y - 1), layout.IsFloor(x - 1, y), layout.IsFloor(x, y) };
                if (std::ranges::count(cells, true) != 1) continue;
                const float sx = cells[0] || cells[2] ? 1.0f : -1.0f;   // away from the floor cell
                const float sz = cells[0] || cells[1] ? 1.0f : -1.0f;
                const auto point = world.CellCorner(x, y);
                pieces_.push_back(WallDetail::Place(*corner, cornerModel, { point.x + sx * halfDepth, cornerY, point.y + sz * halfDepth }, 0.0f, modelScale));
            }
        }
    }

    inline bool WallSet::SegmentHitsBox(Systems::Rendering::Vector3 from, Systems::Rendering::Vector3 to,
                                        Systems::Rendering::Vector3 lo, Systems::Rendering::Vector3 hi) noexcept {
        // Slab test
        const float start[3] = { from.x, from.y, from.z };
        const float delta[3] = { to.x - from.x, to.y - from.y, to.z - from.z };
        const float minimum[3] = { lo.x, lo.y, lo.z };
        const float maximum[3] = { hi.x, hi.y, hi.z };
        float enter = 0.0f;
        float exit = 1.0f;
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(delta[axis]) < 1e-6f) {
                if (start[axis] < minimum[axis] || start[axis] > maximum[axis]) return false;
                continue;
            }
            float a = (minimum[axis] - start[axis]) / delta[axis];
            float b = (maximum[axis] - start[axis]) / delta[axis];
            if (a > b) std::swap(a, b);
            enter = std::max(enter, a);
            exit = std::min(exit, b);
            if (enter > exit) return false;
        }
        return true;
    }

    inline void WallSet::UpdateFade(Systems::Rendering::Vector3 eye, std::optional<WorldPoint> player, float elapsedSeconds) {
        const float step = elapsedSeconds / FadeSeconds * (1.0f - FadedAlpha);
        for (auto& piece : pieces_) {
            bool hides = false;
            if (player) {
                for (float height : { 0.1f, 0.6f, 1.1f }) {
                    if (SegmentHitsBox(eye, { player->x, height, player->y }, piece.boundsMin, piece.boundsMax)) {
                        hides = true;
                        break;
                    }
                }
            }
            const float goal = hides ? FadedAlpha : 1.0f;
            piece.opacity = piece.opacity < goal ? std::min(goal, piece.opacity + step) : std::max(goal, piece.opacity - step);
        }
    }

    inline void WallSet::Draw(Systems::Rendering::RenderingSystem& renderer, Systems::Rendering::Color tint, float modelScale) const {
        for (const auto& piece : pieces_) {
            auto color = tint;
            color.a = piece.opacity;
            renderer.DrawModel(Systems::Rendering::ModelRenderData{ piece.model, piece.position, piece.yaw, modelScale, color });
        }
    }

    inline std::optional<std::size_t> WallSet::FindSegmentNear(WorldPoint at, float within) const {
        std::optional<std::size_t> best;
        float bestDistance = within;
        for (std::size_t i = 0; i < pieces_.size(); ++i) {
            if (pieces_[i].model != segmentModel_) continue;
            const float d = std::hypot(pieces_[i].position.x - at.x, pieces_[i].position.z - at.y);
            if (d < bestDistance) {
                bestDistance = d;
                best = i;
            }
        }
        return best;
    }

} // namespace LB::Engine
