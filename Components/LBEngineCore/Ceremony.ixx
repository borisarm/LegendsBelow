// Ceremony.ixx - The big moments of a hero's growth: a banner across the screen when they reach a
// level or take a path (Fanfare), and the cards to choose a path from (PathChoice)
export module LB.Engine:Ceremony;

import std;
import LB.RenderingSystem;
import :Menus;

export namespace LB::Engine {

    /// <summary>
    /// What a banner says: a title ("LEVEL 5"), who it is about, what was gained (one line each,
    /// appearing one after another), and its color (display RGB)
    /// </summary>
    struct FanfareCard {
        std::string title;
        std::string subtitle;
        std::vector<std::string> lines;
        std::array<float, 3> color{ 0.95f, 0.75f, 0.30f };
    };

    /// <summary>
    /// A banner across the upper screen: it swells in, holds while its lines appear, and fades.
    /// Banners shown while one is up wait their turn. It is only a look: the game goes on.
    /// </summary>
    class Fanfare {
    public:
        static constexpr float InSeconds = 0.4f;
        static constexpr float HoldSeconds = 3.4f;
        static constexpr float OutSeconds = 0.6f;
        static constexpr float LineSeconds = 0.25f;   // between one line's appearing and the next's
        static constexpr std::int32_t Layer = 25;     // over the HUD, under the menus

        void Show(FanfareCard card) {
            if (current_) waiting_.push_back(std::move(card));
            else Start(std::move(card));
        }

        void Update(float elapsedSeconds) {
            if (!current_) return;
            ageSeconds_ += elapsedSeconds;
            if (ageSeconds_ < InSeconds + HoldSeconds + OutSeconds) return;
            current_.reset();
            if (!waiting_.empty()) {
                auto next = std::move(waiting_.front());
                waiting_.erase(waiting_.begin());
                Start(std::move(next));
            }
        }

        [[nodiscard]] const FanfareCard* Current() const noexcept { return current_ ? &*current_ : nullptr; }
        [[nodiscard]] float Age() const noexcept { return ageSeconds_; }

        void Clear() noexcept {
            current_.reset();
            waiting_.clear();
        }

        void Draw(Systems::Rendering::RenderingSystem& renderer, const MenuArt& art) const {
            if (!current_) return;
            using Systems::Rendering::Color;
            const auto& card = *current_;
            std::int32_t width = 0, height = 0;
            renderer.GetUiSize(width, height);
            const std::int32_t center = width / 2;

            // In: swelling from large; out: fading
            const float in = std::clamp(ageSeconds_ / InSeconds, 0.0f, 1.0f);
            const float eased = 1.0f - (1.0f - in) * (1.0f - in);
            const float out = std::clamp((ageSeconds_ - InSeconds - HoldSeconds) / OutSeconds, 0.0f, 1.0f);
            const float alpha = eased * (1.0f - out);
            const auto display = [&](float r, float g, float b, float a) { return MenuView::Display(r, g, b, a * alpha); };
            const auto [r, g, b] = card.color;
            const float shimmer = 0.85f + 0.15f * std::sin(ageSeconds_ * 6.0f);

            // A dark band edged in the card's color
            const std::int32_t bandTop = height / 5;
            const std::int32_t bandHeight = 118 + static_cast<std::int32_t>(card.lines.size()) * 26;
            renderer.DrawRectangle({ 0, bandTop, width, bandHeight }, display(0.02f, 0.02f, 0.03f, 0.72f), Layer);
            renderer.DrawRectangle({ 0, bandTop, width, 3 }, display(r, g, b, 0.9f), Layer + 1);
            renderer.DrawRectangle({ 0, bandTop + bandHeight - 3, width, 3 }, display(r, g, b, 0.9f), Layer + 1);
            // A glow behind the title, widening as it lands: layered bands, faintest widest
            for (int layer = 0; layer < 4; ++layer) {
                const float spread = 1.0f + 0.45f * static_cast<float>(layer);
                const std::int32_t glow = static_cast<std::int32_t>(static_cast<float>(width) * (0.16f + 0.14f * eased) * spread);
                const std::int32_t tall = 50 + layer * 8;
                renderer.DrawRectangle({ center - glow / 2, bandTop + 48 - tall / 2, glow, tall }, display(r, g, b, 0.07f * shimmer), Layer + 1);
            }

            const auto centered = [&](const std::string& text, std::int32_t y, float scale, Color color) {
                const auto size = renderer.MeasureText(text, scale);
                renderer.DrawText(Systems::Rendering::TextRenderData{ text, LB::Position{ center - static_cast<std::int32_t>(size.width / 2.0f), y }, {}, color, scale, Layer + 2, true });
                return size.width;
            };
            const float titleScale = 6.0f * (1.0f + 0.45f * (1.0f - eased));
            centered(card.title, bandTop + 18 - static_cast<std::int32_t>(20.0f * (1.0f - eased)), titleScale, display(r * shimmer + (1.0f - shimmer), g, b, 1.0f));
            if (!card.subtitle.empty()) {
                const auto subtitleWidth = centered(card.subtitle, bandTop + 82, 2.0f, display(0.88f, 0.86f, 0.80f, 1.0f));
                if (alpha > 0.5f) MenuView::HeadingOrnaments(renderer, art, center, bandTop + 82, subtitleWidth, 14, Layer + 2);
            }
            // The gains, one after another
            for (std::size_t i = 0; i < card.lines.size(); ++i) {
                const float shown = std::clamp((ageSeconds_ - InSeconds - LineSeconds * static_cast<float>(i)) / 0.2f, 0.0f, 1.0f);
                if (shown <= 0.0f) continue;
                centered(card.lines[i], bandTop + 112 + static_cast<std::int32_t>(i) * 26, 2.0f, display(1.0f, 0.95f, 0.82f, shown));
            }
        }

    private:
        void Start(FanfareCard card) {
            current_ = std::move(card);
            ageSeconds_ = 0.0f;
        }

        std::optional<FanfareCard> current_;
        std::vector<FanfareCard> waiting_;
        float ageSeconds_{0.0f};
    };

    /// <summary>
    /// One path to choose: its name, what it is about, what it gives, and its color (display RGB)
    /// </summary>
    struct PathCard {
        std::string key;
        std::string name;
        std::string flavor;
        std::vector<std::string> bonuses;
        std::array<float, 3> color{ 0.95f, 0.75f, 0.30f };
    };

    /// <summary>
    /// The path choice: a card for each path side by side, the one under the cursor raised and
    /// framed in its color; left and right compare, confirm takes it
    /// </summary>
    class PathChoice {
    public:
        static constexpr std::int32_t Layer = 40;   // over the character screen

        void Open() noexcept {
            open_ = true;
            cursor_ = 0;
        }
        void Close() noexcept { open_ = false; }
        [[nodiscard]] bool IsOpen() const noexcept { return open_; }
        [[nodiscard]] std::size_t Cursor() const noexcept { return cursor_; }

        void Step(int direction, std::size_t count) noexcept {
            if (count == 0) return;
            cursor_ = static_cast<std::size_t>((static_cast<int>(cursor_) + direction + static_cast<int>(count)) % static_cast<int>(count));
        }

        static void Draw(Systems::Rendering::RenderingSystem& renderer, const MenuArt& art, std::span<const PathCard> cards, std::size_t cursor,
                         std::string_view heading, float timeSeconds) {
            using Systems::Rendering::Color;
            using Systems::Rendering::Rectangle;
            std::int32_t width = 0, height = 0;
            renderer.GetUiSize(width, height);
            const std::int32_t center = width / 2;
            const auto text = [&](const std::string& s, std::int32_t x, std::int32_t y, float scale, Color color, std::int32_t layer) {
                renderer.DrawText(Systems::Rendering::TextRenderData{ s, LB::Position{ x, y }, {}, color, scale, layer, true });
            };
            const auto centered = [&](const std::string& s, std::int32_t middle, std::int32_t y, float scale, Color color, std::int32_t layer) {
                const auto size = renderer.MeasureText(s, scale);
                text(s, middle - static_cast<std::int32_t>(size.width / 2.0f), y, scale, color, layer);
                return size.width;
            };

            renderer.DrawRectangle({ 0, 0, width, height }, MenuView::Display(0.0f, 0.0f, 0.0f, 0.78f), Layer);
            const auto gold = MenuView::Display(0.95f, 0.75f, 0.30f);
            const auto headingWidth = centered(std::string(heading), center, 40, 4.0f, gold, Layer + 3);
            MenuView::HeadingOrnaments(renderer, art, center, 40, headingWidth, 28, Layer + 3);

            // Cards side by side, as wide as fit
            const auto count = static_cast<std::int32_t>(std::max<std::size_t>(1, cards.size()));
            constexpr std::int32_t gap = 28;
            const std::int32_t cardWidth = std::min(300, (width - 60 - gap * (count - 1)) / count);
            constexpr std::int32_t cardHeight = 330;
            const std::int32_t left = center - (cardWidth * count + gap * (count - 1)) / 2;
            const std::int32_t top = 120;
            const float pulse = 0.75f + 0.25f * std::sin(timeSeconds * 4.0f);
            for (std::size_t i = 0; i < cards.size(); ++i) {
                const auto& card = cards[i];
                const bool selected = i == cursor;
                const auto [r, g, b] = card.color;
                const std::int32_t x = left + static_cast<std::int32_t>(i) * (cardWidth + gap);
                const std::int32_t y = top - (selected ? 12 : 0);
                const Rectangle rect{ x, y, cardWidth, cardHeight };
                MenuView::Panel(renderer, art, rect, Layer + 1);
                // A framed glow when chosen, then the path's color: a band behind its name
                if (selected) {
                    renderer.DrawRectangle({ x - 8, y - 8, cardWidth + 16, cardHeight + 16 }, MenuView::Display(r, g, b, 0.18f * pulse), Layer);
                    MenuView::Selection(renderer, art, { x - 4, y - 4, cardWidth + 8, cardHeight + 8 }, Layer + 1);
                }
                renderer.DrawRectangle({ x + 4, y + 4, cardWidth - 8, 58 }, MenuView::Display(r, g, b, selected ? 0.55f : 0.22f), Layer + 2);
                const float dim = selected ? 1.0f : 0.6f;
                centered(card.name, x + cardWidth / 2, y + 16, 3.2f, MenuView::Display(std::min(1.0f, r + 0.25f) * dim, std::min(1.0f, g + 0.25f) * dim, std::min(1.0f, b + 0.25f) * dim), Layer + 3);

                // What it is about, wrapped to the card
                std::int32_t lineY = y + 80;
                for (const auto& line : Wrap(renderer, card.flavor, 1.6f, static_cast<float>(cardWidth - 32))) {
                    centered(line, x + cardWidth / 2, lineY, 1.6f, MenuView::Display(0.85f * dim, 0.83f * dim, 0.78f * dim), Layer + 3);
                    lineY += 22;
                }
                lineY += 16;
                for (const auto& bonus : card.bonuses) {
                    centered(bonus, x + cardWidth / 2, lineY, 2.2f, MenuView::Display(std::min(1.0f, r + 0.3f) * dim, std::min(1.0f, g + 0.3f) * dim, std::min(1.0f, b + 0.3f) * dim), Layer + 3);
                    lineY += 30;
                }
                if (selected) centered("Take this path", x + cardWidth / 2, y + cardHeight - 36, 1.8f, MenuView::Display(1.0f, 0.92f, 0.70f, pulse), Layer + 3);
            }
            centered("Left/Right: compare   Enter/A: choose   Esc/B: not yet", center, top + cardHeight + 34, 1.6f, MenuView::Display(0.65f, 0.63f, 0.58f), Layer + 3);
        }

        // Words to lines no wider than `width`
        static std::vector<std::string> Wrap(Systems::Rendering::RenderingSystem& renderer, const std::string& text, float scale, float width) {
            std::vector<std::string> lines;
            std::string line;
            std::istringstream words(text);
            for (std::string word; words >> word;) {
                const auto candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && renderer.MeasureText(candidate, scale).width > width) {
                    lines.push_back(line);
                    line = word;
                }
                else {
                    line = candidate;
                }
            }
            if (!line.empty()) lines.push_back(line);
            return lines;
        }

    private:
        bool open_{false};
        std::size_t cursor_{0};
    };

} // namespace LB::Engine
