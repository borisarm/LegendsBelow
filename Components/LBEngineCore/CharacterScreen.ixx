// CharacterScreen.ixx - The character and inventory screen as a list of rows with a cursor
export module LB.Engine:CharacterScreen;

import std;
import LB.Core;
import LB.Combat;
import LB.RenderingSystem;
import :Loot;

export namespace LB::Engine {

    /// <summary>
    /// What choosing a row does
    /// </summary>
    enum class ScreenAction {
        None,             // a heading or information line
        SpendPoint,       // raise `attribute` with an unassigned point
        Equip,            // equip `item` from the bag
        Use,              // use `item` (a consumable) from the bag
        Unequip,          // take off the piece in `slot`
        Buy,              // buy the merchant's stock entry `index`
        Sell,             // sell one `item` from the bag
        ChoosePath,       // take the class's specialization `index`
        GiveToAlly,       // choose an item from the bag for ally `index` (an id)
        TakeFromAlly      // take ally `index`'s gear back
    };

    /// <summary>
    /// One line of the screen. Rows with an action can be selected; disabled ones explain why
    /// (`detail`) instead of acting.
    /// </summary>
    struct ScreenRow {
        std::string text;
        std::string detail;               // right-hand column: stats, requirements
        ScreenAction action{ScreenAction::None};
        bool enabled{true};
        bool heading{false};
        LB::AttributeType attribute{LB::AttributeType::Strength};
        ItemRef item{};
        LB::Combat::EquipmentSlot slot{LB::Combat::EquipmentSlot::None};
        std::optional<Systems::Rendering::Color> color;   // a rolled item's rarity color
        std::size_t index{0};                             // Buy: the stock entry

        [[nodiscard]] bool IsSelectable() const noexcept { return action != ScreenAction::None; }
    };

    /// <summary>
    /// Open/closed state and the cursor of the character screen. The engine rebuilds the rows from
    /// the character every frame; the cursor keeps its place, moving only over selectable rows.
    /// </summary>
    class CharacterScreen {
    public:
        [[nodiscard]] bool IsOpen() const noexcept { return open_; }

        void Open() noexcept {
            open_ = true;
            cursor_ = 0;
        }

        void Close() noexcept { open_ = false; }

        /// <summary>
        /// Replace the rows, keeping the cursor on a selectable row (the nearest one at or after
        /// where it was, else before)
        /// </summary>
        void SetRows(std::vector<ScreenRow> rows) {
            rows_ = std::move(rows);
            cursor_ = std::min(cursor_, rows_.empty() ? std::size_t{0} : rows_.size() - 1);
            if (!rows_.empty() && !rows_[cursor_].IsSelectable()) {
                if (!Step(+1)) Step(-1);
            }
        }

        /// <summary>
        /// Move to the next (+1) or previous (-1) selectable row; false when there is none that way
        /// </summary>
        bool Step(int direction) {
            for (auto i = static_cast<std::ptrdiff_t>(cursor_) + direction;
                 i >= 0 && i < static_cast<std::ptrdiff_t>(rows_.size()); i += direction) {
                if (rows_[static_cast<std::size_t>(i)].IsSelectable()) {
                    cursor_ = static_cast<std::size_t>(i);
                    return true;
                }
            }
            return false;
        }

        /// <summary>
        /// The row under the cursor, if it can be chosen
        /// </summary>
        [[nodiscard]] const ScreenRow* Selected() const noexcept {
            if (cursor_ >= rows_.size() || !rows_[cursor_].IsSelectable()) return nullptr;
            return &rows_[cursor_];
        }

        [[nodiscard]] const std::vector<ScreenRow>& GetRows() const noexcept { return rows_; }
        [[nodiscard]] std::size_t GetCursor() const noexcept { return cursor_; }

    private:
        bool open_{false};
        std::size_t cursor_{0};
        std::vector<ScreenRow> rows_;
    };

} // namespace LB::Engine
