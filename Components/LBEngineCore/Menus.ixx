// Menus.ixx - The game's menus: the title screen, the pause menu, options, and credits (the pages,
// the cursor, and going back), and drawing a page
export module LB.Engine:Menus;

import std;
import LB.Core;
import LB.RenderingSystem;

export namespace LB::Engine {

    /// <summary>
    /// A menu page
    /// </summary>
    enum class MenuPage {
        None,       // no menu: playing
        Title,      // the main menu, at startup and after quitting a game
        Pause,      // during a game (the game is paused while any menu is open)
        Options,
        Credits
    };

    /// <summary>
    /// What choosing an item does
    /// </summary>
    enum class MenuCommand {
        NewGame,
        Continue,       // load the quick save
        Resume,
        SaveGame,       // to the quick save slot
        LoadGame,       // the quick save
        Options,
        Credits,
        QuitToTitle,
        Quit,           // leave the game (the host closes the window)
        Back,
        MasterVolume,   // left/right change it
        MusicVolume,
        EffectsVolume
    };

    /// <summary>
    /// One item of a page; disabled items are shown but skipped by the cursor
    /// </summary>
    struct MenuItem {
        std::string text;
        MenuCommand command{MenuCommand::Back};
        bool enabled{true};
        std::string value;    // shown on the right ("70%")

        [[nodiscard]] bool IsAdjustable() const noexcept {
            return command == MenuCommand::MasterVolume || command == MenuCommand::MusicVolume || command == MenuCommand::EffectsVolume;
        }
    };

    /// <summary>
    /// Which page is open, the pages it was opened from (Back returns to them), and the cursor.
    /// The engine sets the page's items every frame; the cursor keeps its place on enabled items.
    /// </summary>
    class GameMenu {
    public:
        [[nodiscard]] bool IsOpen() const noexcept { return page_ != MenuPage::None; }
        [[nodiscard]] MenuPage Page() const noexcept { return page_; }

        /// <summary>
        /// Open a page on its own (the title, the pause menu), forgetting where it came from
        /// </summary>
        void Open(MenuPage page) {
            stack_.clear();
            Show(page);
        }

        /// <summary>
        /// Open a page from the current one; Back returns
        /// </summary>
        void Push(MenuPage page) {
            if (page_ != MenuPage::None) stack_.push_back({ page_, cursor_ });
            Show(page);
        }

        /// <summary>
        /// Back to the page this one was opened from; closes the menu from a page opened on its own.
        /// Returns false when the menu closed.
        /// </summary>
        bool Back() {
            if (stack_.empty()) {
                Close();
                return false;
            }
            page_ = stack_.back().page;
            cursor_ = stack_.back().cursor;
            stack_.pop_back();
            items_.clear();
            message_.clear();
            return true;
        }

        void Close() noexcept {
            page_ = MenuPage::None;
            stack_.clear();
            items_.clear();
            message_.clear();
        }

        /// <summary>
        /// Replace the page's items, keeping the cursor on an enabled item
        /// </summary>
        void SetItems(std::vector<MenuItem> items) {
            items_ = std::move(items);
            if (items_.empty()) return;
            cursor_ = std::min(cursor_, items_.size() - 1);
            if (!items_[cursor_].enabled && !Step(+1)) Step(-1);
        }

        /// <summary>
        /// Move to the next (+1) or previous (-1) enabled item, wrapping around; false when there
        /// is no other
        /// </summary>
        bool Step(int direction) {
            const auto count = static_cast<std::ptrdiff_t>(items_.size());
            for (std::ptrdiff_t n = 1; n < count; ++n) {
                const auto i = ((static_cast<std::ptrdiff_t>(cursor_) + direction * n) % count + count) % count;
                if (items_[static_cast<std::size_t>(i)].enabled) {
                    cursor_ = static_cast<std::size_t>(i);
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] const MenuItem* Selected() const noexcept {
            return cursor_ < items_.size() && items_[cursor_].enabled ? &items_[cursor_] : nullptr;
        }

        [[nodiscard]] const std::vector<MenuItem>& Items() const noexcept { return items_; }
        [[nodiscard]] std::size_t Cursor() const noexcept { return cursor_; }

        /// <summary>
        /// A short note under the items ("Game saved"), until the page changes
        /// </summary>
        void SetMessage(std::string message) { message_ = std::move(message); }
        [[nodiscard]] const std::string& Message() const noexcept { return message_; }

        /// <summary>
        /// The credits page's scroll position (first line shown)
        /// </summary>
        void Scroll(int lines, std::size_t lineCount, std::size_t visible) noexcept {
            const auto maxFirst = static_cast<std::ptrdiff_t>(lineCount > visible ? lineCount - visible : 0);
            scroll_ = static_cast<std::size_t>(std::clamp(static_cast<std::ptrdiff_t>(scroll_) + lines, std::ptrdiff_t{0}, maxFirst));
        }
        [[nodiscard]] std::size_t ScrollPosition() const noexcept { return scroll_; }

    private:
        void Show(MenuPage page) {
            page_ = page;
            cursor_ = 0;
            scroll_ = 0;
            items_.clear();
            message_.clear();
        }

        struct Opened {
            MenuPage page;
            std::size_t cursor;
        };

        MenuPage page_{MenuPage::None};
        std::vector<Opened> stack_;
        std::vector<MenuItem> items_;
        std::size_t cursor_{0};
        std::size_t scroll_{0};
        std::string message_;
    };

    /// <summary>
    /// What a menu page shows this frame
    /// </summary>
    struct MenuModel {
        std::string heading;                     // the page's title ("PAUSED"); the title page shows the game's name
        bool titlePage{false};
        std::vector<MenuItem> items;
        std::size_t cursor{0};
        std::string message;
        std::vector<std::string> lines;          // the credits
        std::size_t firstLine{0};
    };

    /// <summary>
    /// Draws a menu page over the (dimmed) game: the heading, the items with the selected one
    /// marked, values on the right, a message, and for the credits a scrolling list of lines
    /// </summary>
    class MenuView {
    public:
        static constexpr std::size_t CreditLinesShown = 12;   // the rest scroll (fits an 800 x 600 window)

        static void Draw(Systems::Rendering::RenderingSystem& renderer, const MenuModel& menu) {
            using Systems::Rendering::Color;
            const auto display = [](float r, float g, float b, float a = 1.0f) {
                return Color{ std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), a };
            };
            const auto text = [&](const std::string& s, std::int32_t x, std::int32_t y, float scale, Color color) {
                renderer.DrawText(Systems::Rendering::TextRenderData{ s, LB::Position{ x, y }, {}, color, scale, 32, true });
            };
            const auto centered = [&](const std::string& s, std::int32_t centerX, std::int32_t y, float scale, Color color) {
                const auto size = renderer.MeasureText(s, scale);
                text(s, centerX - static_cast<std::int32_t>(size.width / 2.0f), y, scale, color);
            };

            std::int32_t width = 0;
            std::int32_t height = 0;
            renderer.GetUiSize(width, height);
            const std::int32_t center = width / 2;

            // Panel behind the page
            const std::int32_t panelWidth = std::min(width - 40, 560);
            const std::int32_t panelTop = menu.titlePage ? height / 3 : height / 6;
            const std::int32_t panelHeight = height - panelTop - height / 10;
            renderer.DrawRectangle({ center - panelWidth / 2, panelTop, panelWidth, panelHeight }, display(0.03f, 0.03f, 0.05f, 0.82f), 30);

            const auto gold = display(0.95f, 0.75f, 0.30f);
            if (menu.titlePage) {
                centered("LEGENDS BELOW", center, height / 3 - 110, 7.0f, gold);
                centered("a dungeon crawler", center, height / 3 - 34, 2.0f, display(0.80f, 0.78f, 0.72f));
            }
            else {
                centered(menu.heading, center, panelTop + 18, 4.0f, gold);
            }

            std::int32_t y = panelTop + (menu.titlePage ? 30 : 90);
            constexpr std::int32_t rowHeight = 40;
            if (!menu.lines.empty()) {
                const auto last = std::min(menu.lines.size(), menu.firstLine + CreditLinesShown);
                for (std::size_t i = menu.firstLine; i < last; ++i) {
                    centered(menu.lines[i], center, y, 1.6f, display(0.88f, 0.86f, 0.80f));
                    y += 24;
                }
                y += 12;
            }

            const std::int32_t left = center - panelWidth / 2 + 48;
            const std::int32_t right = center + panelWidth / 2 - 48;
            for (std::size_t i = 0; i < menu.items.size(); ++i) {
                const auto& item = menu.items[i];
                const bool selected = i == menu.cursor && item.enabled;
                if (selected) {
                    renderer.DrawRectangle({ left - 20, y - 4, right - left + 40, rowHeight - 4 }, display(0.30f, 0.22f, 0.10f, 0.9f), 31);
                }
                const auto color = !item.enabled ? display(0.45f, 0.44f, 0.42f) : selected ? display(1.0f, 0.92f, 0.70f) : display(0.85f, 0.83f, 0.78f);
                text(item.text, left, y, 2.6f, color);
                if (!item.value.empty()) {
                    const auto value = item.IsAdjustable() ? std::format("< {} >", item.value) : item.value;
                    const auto size = renderer.MeasureText(value, 2.6f);
                    text(value, right - static_cast<std::int32_t>(size.width), y, 2.6f, color);
                }
                y += rowHeight;
            }

            if (!menu.message.empty()) {
                centered(menu.message, center, y + 10, 2.0f, display(0.55f, 0.95f, 0.55f));
            }
        }
    };

} // namespace LB::Engine
