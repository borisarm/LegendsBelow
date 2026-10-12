// Minimap.ixx - What the player has seen of the floor, and the north-up map drawn from it
export module LB.Engine:Minimap;

import std;
import LB.RenderingSystem;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// What the minimap marks besides the explored cells; markers on cells not explored yet are
    /// left out (enemies are only passed when in sight)
    /// </summary>
    struct MinimapMarkers {
        WorldPoint player{};
        std::optional<WorldPoint> exit;
        std::vector<WorldPoint> chests;
        std::vector<WorldPoint> enemies;
        std::optional<WorldPoint> objective;   // the floor's relic or captive (or the champion's room)
    };

    /// <summary>
    /// The explored floor cells (revealed within RevealCells in sight, and whole rooms on entering)
    /// shown in the top right corner centered on the player, or large in the middle with the whole
    /// floor
    /// </summary>
    class Minimap {
    public:
        static constexpr std::int32_t RevealCells = 4;

        /// <summary>
        /// Forget everything: a new floor of this size, nothing seen yet
        /// </summary>
        void Reset(const WorldMap& world) {
            explored_.assign(world.Layout().cells.size(), 0);
            width_ = world.Layout().width;
        }

        void Clear() noexcept {
            explored_.clear();
            width_ = 0;
        }

        /// <summary>
        /// Floor cells near the player and in sight become explored; so does the whole room they are in
        /// </summary>
        void Reveal(const WorldMap& world, WorldPoint player) {
            const auto& layout = world.Layout();
            if (explored_.size() != layout.cells.size()) return;
            const auto mark = [&](std::int32_t x, std::int32_t y) {
                if (layout.IsFloor(x, y)) explored_[static_cast<std::size_t>(y * layout.width + x)] = 1;
            };

            const auto [px, py] = world.CellOf(player);
            for (std::int32_t y = py - RevealCells; y <= py + RevealCells; ++y) {
                for (std::int32_t x = px - RevealCells; x <= px + RevealCells; ++x) {
                    if (!layout.IsFloor(x, y) || (x - px) * (x - px) + (y - py) * (y - py) > RevealCells * RevealCells) continue;
                    if (world.LineOfSight(player, world.CellCenter(x, y))) mark(x, y);
                }
            }

            if (const auto room = world.RoomAt(player); room >= 0) {
                const auto& area = layout.rooms[static_cast<std::size_t>(room)].area;
                for (std::int32_t y = area.y; y < area.y + area.height; ++y) {
                    for (std::int32_t x = area.x; x < area.x + area.width; ++x) mark(x, y);
                }
            }
        }

        [[nodiscard]] bool IsExplored(std::int32_t x, std::int32_t y) const noexcept {
            if (x < 0 || y < 0 || x >= width_ || width_ == 0) return false;
            const auto index = static_cast<std::size_t>(y * width_ + x);
            return index < explored_.size() && explored_[index] != 0;
        }

        [[nodiscard]] bool IsExpanded() const noexcept { return expanded_; }
        void ToggleExpanded() noexcept { expanded_ = !expanded_; }

        void Draw(Systems::Rendering::RenderingSystem& renderer, const WorldMap& world, const MinimapMarkers& markers) const;

    private:
        std::vector<std::uint8_t> explored_;
        std::int32_t width_{0};
        bool expanded_{false};
    };

    inline void Minimap::Draw(Systems::Rendering::RenderingSystem& renderer, const WorldMap& world, const MinimapMarkers& markers) const {
        const auto& layout = world.Layout();
        if (explored_.size() != layout.cells.size() || layout.width == 0) return;

        std::int32_t screenWidth = 0;
        std::int32_t screenHeight = 0;
        renderer.GetUiSize(screenWidth, screenHeight);
        const auto cellOf = [&](WorldPoint at) {
            return std::pair{ (at.x - world.Origin().x) / WorldMap::CellTiles, (at.y - world.Origin().y) / WorldMap::CellTiles };
        };

        // Panel (the visible window) and where cell (0, 0) lands
        std::int32_t cellPixels = 0, panelLeft = 0, panelTop = 0, panelWidth = 0, panelHeight = 0, left = 0, top = 0;
        if (expanded_) {
            const std::int32_t side = std::min(screenWidth, screenHeight) * 3 / 4;
            cellPixels = std::max(2, side / std::max(layout.width, layout.height));
            panelWidth = cellPixels * layout.width;
            panelHeight = cellPixels * layout.height;
            panelLeft = left = (screenWidth - panelWidth) / 2;
            panelTop = top = (screenHeight - panelHeight) / 2;
        }
        else {
            cellPixels = 7;
            panelWidth = panelHeight = std::min(180, std::min(screenWidth, screenHeight) / 3);
            panelLeft = screenWidth - panelWidth - 14;
            panelTop = 14;
            const auto [u, v] = cellOf(markers.player);
            left = panelLeft + panelWidth / 2 - static_cast<std::int32_t>(u * static_cast<float>(cellPixels));
            top = panelTop + panelHeight / 2 - static_cast<std::int32_t>(v * static_cast<float>(cellPixels));
        }

        const auto color = [](float r, float g, float b, float a) {
            return Systems::Rendering::Color{ std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), a };
        };
        // Rectangles are cut to the panel
        const auto rect = [&](std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, Systems::Rendering::Color c, std::int32_t layer) {
            const std::int32_t x0 = std::max(x, panelLeft);
            const std::int32_t y0 = std::max(y, panelTop);
            const std::int32_t x1 = std::min(x + w, panelLeft + panelWidth);
            const std::int32_t y1 = std::min(y + h, panelTop + panelHeight);
            if (x1 > x0 && y1 > y0) renderer.DrawRectangle({ x0, y0, x1 - x0, y1 - y0 }, c, layer);
        };
        // A marker centered on a world point
        const auto marker = [&](WorldPoint at, std::int32_t size, Systems::Rendering::Color c, std::int32_t layer) {
            const auto [u, v] = cellOf(at);
            rect(left + static_cast<std::int32_t>(u * static_cast<float>(cellPixels)) - size / 2,
                 top + static_cast<std::int32_t>(v * static_cast<float>(cellPixels)) - size / 2, size, size, c, layer);
        };
        const auto seen = [&](WorldPoint at) {
            const auto [x, y] = world.CellOf(at);
            return IsExplored(x, y);
        };

        renderer.DrawRectangle({ panelLeft - 4, panelTop - 4, panelWidth + 8, panelHeight + 8 }, color(0.02f, 0.02f, 0.03f, expanded_ ? 0.85f : 0.6f), 14);
        const auto here = world.RoomAt(markers.player);
        for (std::int32_t y = 0; y < layout.height; ++y) {
            for (std::int32_t x = 0; x < layout.width; ++x) {
                if (!IsExplored(x, y)) continue;
                const bool current = here >= 0 && layout.rooms[static_cast<std::size_t>(here)].area.Contains(x, y);
                rect(left + x * cellPixels, top + y * cellPixels, cellPixels, cellPixels,
                     current ? color(0.62f, 0.56f, 0.46f, 0.95f) : color(0.40f, 0.38f, 0.35f, 0.9f), 15);
            }
        }

        const std::int32_t dot = std::max(3, cellPixels);
        if (markers.exit && seen(*markers.exit)) marker(*markers.exit, dot + 2, color(0.35f, 0.65f, 1.0f, 1.0f), 16);
        for (const auto chest : markers.chests) {
            if (seen(chest)) marker(chest, dot, color(0.95f, 0.80f, 0.30f, 1.0f), 16);
        }
        if (markers.objective && seen(*markers.objective)) marker(*markers.objective, dot + 1, color(0.55f, 0.95f, 0.55f, 1.0f), 16);
        for (const auto enemy : markers.enemies) {
            marker(enemy, std::max(3, dot - 1), color(0.90f, 0.20f, 0.15f, 1.0f), 17);
        }
        marker(markers.player, dot + 2, color(1.0f, 1.0f, 1.0f, 1.0f), 18);
    }

} // namespace LB::Engine
