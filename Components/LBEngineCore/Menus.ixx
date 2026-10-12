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
        Credits,
        ChooseClass, // New Game: the hero's class
        Controls,    // the keyboard and controller assignments
        Fallen,      // the hero went down: the run so far, and what next
        SaveSlots,   // Save Game: the slots to save into
        LoadSlots,   // Load Game: the saves to load (autosave, quick save, the slots)
        Legacy,      // the records of every run, and the boons they unlock
        Codex        // the pages of lore found (lore.json), kept across runs
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
        Controls,
        RiseAgain,      // a fallen hero gets up at the floor's entrance
        OpenSaveSlots,
        OpenLoadSlots,
        SaveToSlot,     // the item's slot
        LoadFromSlot,
        Legacy,
        Codex,
        Credits,
        QuitToTitle,
        Quit,           // leave the game (the host closes the window)
        Back,
        MasterVolume,   // left/right change it
        MusicVolume,
        EffectsVolume,
        ScreenMargin,       // the interface's safe area (left and right)
        PlayWarrior,    // a new game as this class
        PlayMage,
        PlayRogue,
        PlayCleric
    };

    /// <summary>
    /// One item of a page; disabled items are shown but skipped by the cursor
    /// </summary>
    struct MenuItem {
        std::string text;
        MenuCommand command{MenuCommand::Back};
        bool enabled{true};
        std::string value;    // shown on the right ("70%")
        std::int32_t slot{-1};   // a save slot (SaveToSlot, LoadFromSlot)

        [[nodiscard]] bool IsAdjustable() const noexcept {
            return command == MenuCommand::MasterVolume || command == MenuCommand::MusicVolume || command == MenuCommand::EffectsVolume
                || command == MenuCommand::ScreenMargin;
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
    /// The images that frame the menus (Kenney Fantasy UI Borders, white, tinted when drawn): the
    /// panels' frame, the selected item's frame, and the ornament beside headings. All invalid
    /// without a UI directory: the menus are then plain panels.
    /// </summary>
    struct MenuArt {
        static constexpr std::string_view FrameFile = "panel-border-014.png";
        static constexpr std::string_view SelectionFile = "panel-border-008.png";
        static constexpr std::string_view DividerFile = "divider-fade-003.png";
        static constexpr float Slice = 16.0f;       // image pixels of each frame corner

        Systems::Rendering::ImageId frame{Systems::Rendering::InvalidImageId};
        Systems::Rendering::ImageId selection{Systems::Rendering::InvalidImageId};
        Systems::Rendering::ImageId divider{Systems::Rendering::InvalidImageId};

        [[nodiscard]] bool Loaded() const noexcept {
            return frame != Systems::Rendering::InvalidImageId && selection != Systems::Rendering::InvalidImageId && divider != Systems::Rendering::InvalidImageId;
        }
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
        // The controls: what, on the keyboard and mouse, on a controller (a row with only `action` is a
        // section's title); `binding` names the input action a row changes (empty: it cannot be)
        struct ControlRow {
            std::string action;
            std::string keyboard;
            std::string controller;
            std::string binding;
        };
        std::vector<ControlRow> controls;
        std::optional<std::size_t> controlsCursor;   // the chosen row (remapping)
        bool listening{false};                       // it waits for a key or button
        MenuArt art;
    };

    /// <summary>
    /// Draws a menu page over the (dimmed) game: the heading, the items with the selected one
    /// marked, values on the right, a message, and for the credits a scrolling list of lines
    /// </summary>
    class MenuView {
    public:
        static constexpr std::size_t CreditLinesShown = 12;   // the rest scroll (fits an 800 x 600 window)
        static constexpr std::size_t ControlRowsShown = 13;

        // Display (sRGB) colors to the renderer's linear ones
        static Systems::Rendering::Color Display(float r, float g, float b, float a = 1.0f) {
            return { std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), a };
        }

        /// <summary>
        /// A panel: a dark fill, framed in gold when the art is loaded
        /// </summary>
        static void Panel(Systems::Rendering::RenderingSystem& renderer, const MenuArt& art, Systems::Rendering::Rectangle panel, std::int32_t layer) {
            renderer.DrawRectangle(panel, Display(0.03f, 0.03f, 0.05f, 0.85f), layer);
            if (!art.Loaded()) return;
            Systems::Rendering::ImageRenderData frame{};
            frame.image = art.frame;
            frame.rect = { panel.x - 6, panel.y - 6, panel.width + 12, panel.height + 12 };
            frame.color = Display(0.85f, 0.66f, 0.30f);
            frame.layerDepth = layer;
            frame.nineSlice = MenuArt::Slice;
            frame.corner = 24.0f;
            renderer.DrawImage(frame);
        }

        /// <summary>
        /// The selected row: a framed highlight (or a plain bar without the art)
        /// </summary>
        static void Selection(Systems::Rendering::RenderingSystem& renderer, const MenuArt& art, Systems::Rendering::Rectangle row, std::int32_t layer) {
            renderer.DrawRectangle(row, art.Loaded() ? Display(0.30f, 0.22f, 0.10f, 0.55f) : Display(0.30f, 0.22f, 0.10f, 0.9f), layer);
            if (!art.Loaded()) return;
            Systems::Rendering::ImageRenderData frame{};
            frame.image = art.selection;
            frame.rect = row;
            frame.color = Display(1.0f, 0.88f, 0.60f);
            frame.layerDepth = layer;
            frame.nineSlice = MenuArt::Slice;
            frame.corner = std::min(12.0f, static_cast<float>(row.height) * 0.4f);
            renderer.DrawImage(frame);
        }

        /// <summary>
        /// Ornaments either side of a centered heading (`textWidth` wide at `y`, `height` tall):
        /// fading lines that end in a cross, the right one mirrored
        /// </summary>
        static void HeadingOrnaments(Systems::Rendering::RenderingSystem& renderer, const MenuArt& art, std::int32_t centerX, std::int32_t y,
                                     float textWidth, std::int32_t height, std::int32_t layer) {
            if (!art.Loaded()) return;
            constexpr std::int32_t width = 132;   // UI units: the 96 x 18 image drawn larger
            constexpr std::int32_t tall = 24;
            const std::int32_t gap = 16;
            const std::int32_t middle = y + height / 2 - tall / 2;
            const std::int32_t half = static_cast<std::int32_t>(textWidth / 2.0f);
            Systems::Rendering::ImageRenderData ornament{};
            ornament.image = art.divider;
            ornament.color = Display(0.85f, 0.66f, 0.30f);
            ornament.layerDepth = layer;
            ornament.rect = { centerX - half - gap - width, middle, width, tall };
            renderer.DrawImage(ornament);
            ornament.rect = { centerX + half + gap, middle, width, tall };
            ornament.mirrored = true;
            renderer.DrawImage(ornament);
        }

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

            // Panel behind the page (wider for the controls' three columns)
            const std::int32_t panelWidth = std::min(width - 40, menu.controls.empty() ? 560 : 740);
            const std::int32_t panelTop = menu.titlePage ? height / 3 : height / 6;
            // Tall enough for what the page holds: the heading, the credit lines, the items, a message
            const std::int32_t shownLines = static_cast<std::int32_t>(std::min(menu.lines.size(), CreditLinesShown))
                                          + (menu.controls.empty() ? 0 : static_cast<std::int32_t>(std::min(menu.controls.size(), ControlRowsShown)) + 1);
            const std::int32_t content = (menu.titlePage ? 30 : 90) + (shownLines > 0 ? shownLines * 24 + 12 : 0)
                                       + static_cast<std::int32_t>(menu.items.size()) * 40 + (menu.message.empty() ? 0 : 40) + 24;
            const std::int32_t panelHeight = std::min(content, height - panelTop - 16);
            Panel(renderer, menu.art, { center - panelWidth / 2, panelTop, panelWidth, panelHeight }, 30);

            const auto gold = display(0.95f, 0.75f, 0.30f);
            if (menu.titlePage) {
                centered("LEGENDS BELOW", center, height / 3 - 110, 7.0f, gold);
                const std::string subtitle = "a dungeon crawler";
                centered(subtitle, center, height / 3 - 34, 2.0f, display(0.80f, 0.78f, 0.72f));
                HeadingOrnaments(renderer, menu.art, center, height / 3 - 34, renderer.MeasureText(subtitle, 2.0f).width, 14, 32);
            }
            else {
                centered(menu.heading, center, panelTop + 18, 4.0f, gold);
                HeadingOrnaments(renderer, menu.art, center, panelTop + 18, renderer.MeasureText(menu.heading, 4.0f).width, 28, 32);
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
            if (!menu.controls.empty()) {
                // Three columns under their titles; section rows in gold; the rest scroll
                const std::int32_t keyboardX = left + (right - left) * 34 / 100;
                const std::int32_t controllerX = left + (right - left) * 66 / 100;
                const auto dim = display(0.70f, 0.66f, 0.58f);
                text("KEYBOARD AND MOUSE", keyboardX, y, 1.4f, dim);
                text("CONTROLLER", controllerX, y, 1.4f, dim);
                y += 24;
                const auto last = std::min(menu.controls.size(), menu.firstLine + ControlRowsShown);
                for (std::size_t i = menu.firstLine; i < last; ++i) {
                    const auto& row = menu.controls[i];
                    const bool chosen = menu.controlsCursor == i;
                    if (chosen) Selection(renderer, menu.art, { left - 20, y - 5, right - left + 40, 24 }, 31);
                    if (row.keyboard.empty() && row.controller.empty() && row.binding.empty()) {
                        text(row.action, left, y, 1.6f, gold);
                    }
                    else {
                        const auto light = chosen ? display(1.0f, 0.92f, 0.70f) : display(0.88f, 0.86f, 0.80f);
                        text(row.action, left + 12, y, 1.5f, light);
                        if (chosen && menu.listening) {
                            text("press a key or a button  (Esc, Start: keep it)", keyboardX, y, 1.5f, gold);
                        }
                        else {
                            text(row.keyboard, keyboardX, y, 1.5f, light);
                            text(row.controller, controllerX, y, 1.5f, light);
                        }
                    }
                    y += 24;
                }
                const auto more = menu.controls.size() > ControlRowsShown
                    ? std::format("{}-{} of {}   Enter, A: change the chosen one   Esc, B: back", menu.firstLine + 1, last, menu.controls.size())
                    : std::string("Enter, A: change the chosen one   Esc, B: back");
                centered(more, center, y, 1.2f, dim);
                y += 24;
            }
            for (std::size_t i = 0; i < menu.items.size(); ++i) {
                const auto& item = menu.items[i];
                const bool selected = i == menu.cursor && item.enabled && !menu.controlsCursor;   // the controls' cursor is on their rows
                if (selected) {
                    Selection(renderer, menu.art, { left - 20, y - 8, right - left + 40, rowHeight - 2 }, 31);
                }
                const auto color = !item.enabled ? display(0.45f, 0.44f, 0.42f) : selected ? display(1.0f, 0.92f, 0.70f) : display(0.85f, 0.83f, 0.78f);
                text(item.text, left, y, 2.6f, color);
                if (!item.value.empty()) {
                    const auto value = item.IsAdjustable() ? std::format("< {} >", item.value) : item.value;
                    const float valueScale = item.slot >= 0 ? 1.7f : 2.6f;   // a save's summary is long
                    const auto size = renderer.MeasureText(value, valueScale);
                    text(value, right - static_cast<std::int32_t>(size.width), y + (item.slot >= 0 ? 8 : 0), valueScale, color);
                }
                y += rowHeight;
            }

            if (!menu.message.empty()) {
                centered(menu.message, center, y + 10, 2.0f, display(0.55f, 0.95f, 0.55f));
            }
        }
    };

} // namespace LB::Engine
