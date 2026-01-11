// LB.CoreValueObjects.ixx - Core value objects shared across domains
export module LB.CoreValueObjects;

import LB.Common;
import std;

export namespace LB {
    /// <summary>
    /// 2D Position for grid-based movement
    /// </summary>
    struct Position {
        int32_t x{0};
        int32_t y{0};

        Position() = default;
        Position(int32_t x_, int32_t y_) : x(x_), y(y_) {}

        [[nodiscard]] bool operator==(const Position& other) const noexcept {
            return x == other.x && y == other.y;
        }

        [[nodiscard]] int32_t ManhattanDistance(const Position& other) const noexcept {
            return std::abs(x - other.x) + std::abs(y - other.y);
        }
    };

    /// <summary>
    /// Dice roll configuration (e.g., 2d6 = count:2, sides:6)
    /// </summary>
    struct Dice {
        int32_t count{1};
        int32_t sides{6};
        int32_t bonus{0};

        [[nodiscard]] int32_t GetMaxRoll() const noexcept {
            return (count * sides) + bonus;
        }

        [[nodiscard]] int32_t GetMinRoll() const noexcept {
            return count + bonus;
        }

        [[nodiscard]] int32_t GetAverageRoll() const noexcept {
            return ((count * sides) / 2) + bonus;
        }
    };

    /// <summary>
    /// Six core attributes (D&D-style)
    /// Shared by Character, Combat, and Progression systems
    /// </summary>
    struct Attributes {
        int32_t strength{10};
        int32_t dexterity{10};
        int32_t constitution{10};
        int32_t intelligence{10};
        int32_t wisdom{10};
        int32_t charisma{10};

        /// <summary>
        /// Calculate D&D-style modifier: (attribute - 10) / 2
        /// </summary>
        [[nodiscard]] constexpr int32_t GetModifier(int32_t attribute) const noexcept {
            return (attribute - 10) / 2;
        }

        [[nodiscard]] constexpr int32_t GetStrengthModifier() const noexcept { return GetModifier(strength); }
        [[nodiscard]] constexpr int32_t GetDexterityModifier() const noexcept { return GetModifier(dexterity); }
        [[nodiscard]] constexpr int32_t GetConstitutionModifier() const noexcept { return GetModifier(constitution); }
        [[nodiscard]] constexpr int32_t GetIntelligenceModifier() const noexcept { return GetModifier(intelligence); }
        [[nodiscard]] constexpr int32_t GetWisdomModifier() const noexcept { return GetModifier(wisdom); }
        [[nodiscard]] constexpr int32_t GetCharismaModifier() const noexcept { return GetModifier(charisma); }

        /// <summary>
        /// Validate all attributes are within valid range [1-30]
        /// </summary>
        [[nodiscard]] bool IsValid() const noexcept {
            return strength >= 1 && strength <= 30 &&
                   dexterity >= 1 && dexterity <= 30 &&
                   constitution >= 1 && constitution <= 30 &&
                   intelligence >= 1 && intelligence <= 30 &&
                   wisdom >= 1 && wisdom <= 30 &&
                   charisma >= 1 && charisma <= 30;
        }
    };

} // namespace LB