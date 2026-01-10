// LB.ValueObjects.ixx - Domain value objects module interface
export module LB.ValueObjects;

export import LB.Common;
import std;

using namespace std;

export namespace LB::Domain {

    /// <summary>
    /// Character attributes following D&D convention
    /// Core stats that define character capabilities
    /// </summary>
    struct Attributes {
        int32_t strength = 10;      // Physical power, melee damage
        int32_t dexterity = 10;     // Agility, ranged damage, dodging
        int32_t constitution = 10;  // Health, stamina, poison resistance
        int32_t intelligence = 10;  // Arcane magic, spell power
        int32_t wisdom = 10;        // Divine magic, perception
        int32_t charisma = 10;      // Social skills, some magic

        /// <summary>
        /// Calculate attribute modifier using D&D formula: (score - 10) / 2
        /// </summary>
        [[nodiscard]] static constexpr int32_t GetModifier(int32_t attributeScore) noexcept {
            return (attributeScore - 10) / 2;
        }

        /// <summary>
        /// Get modifier for specific attribute
        /// </summary>
        [[nodiscard]] constexpr int32_t GetStrengthModifier() const noexcept { return GetModifier(strength); }
        [[nodiscard]] constexpr int32_t GetDexterityModifier() const noexcept { return GetModifier(dexterity); }
        [[nodiscard]] constexpr int32_t GetConstitutionModifier() const noexcept { return GetModifier(constitution); }
        [[nodiscard]] constexpr int32_t GetIntelligenceModifier() const noexcept { return GetModifier(intelligence); }
        [[nodiscard]] constexpr int32_t GetWisdomModifier() const noexcept { return GetModifier(wisdom); }
        [[nodiscard]] constexpr int32_t GetCharismaModifier() const noexcept { return GetModifier(charisma); }

        // Equality operators
        constexpr bool operator==(const Attributes& other) const noexcept = default;
        constexpr bool operator!=(const Attributes& other) const noexcept = default;
    };

    /// <summary>
    /// Dice rolling mechanics for damage and random events
    /// Supports D&D-style dice notation (e.g., 2d6+3)
    /// </summary>
    class Dice {
    private:
        int32_t count_;     // Number of dice
        int32_t sides_;     // Sides per die
        int32_t bonus_;     // Fixed bonus added

    public:
        /// <summary>
        /// Create dice with count, sides, and optional bonus
        /// </summary>
        constexpr Dice(int32_t count, int32_t sides, int32_t bonus = 0) noexcept
            : count_(std::max(1, count))
            , sides_(std::max(1, sides))
            , bonus_(bonus) {}

        // Accessors
        [[nodiscard]] constexpr int32_t Count() const noexcept { return count_; }
        [[nodiscard]] constexpr int32_t Sides() const noexcept { return sides_; }
        [[nodiscard]] constexpr int32_t Bonus() const noexcept { return bonus_; }

        /// <summary>
        /// Calculate minimum possible roll
        /// </summary>
        [[nodiscard]] constexpr int32_t MinValue() const noexcept {
            return count_ + bonus_;
        }

        /// <summary>
        /// Calculate maximum possible roll
        /// </summary>
        [[nodiscard]] constexpr int32_t MaxValue() const noexcept {
            return (count_ * sides_) + bonus_;
        }

        /// <summary>
        /// Calculate average roll value
        /// </summary>
        [[nodiscard]] constexpr double AverageValue() const noexcept {
            double avgPerDie = (sides_ + 1) / 2.0;
            return (count_ * avgPerDie) + bonus_;
        }

        // Equality operators
        constexpr bool operator==(const Dice& other) const noexcept = default;
        constexpr bool operator!=(const Dice& other) const noexcept = default;
    };

    /// <summary>
    /// 2D position for world coordinates
    /// Used for character and object positioning
    /// </summary>
    class Position {
    private:
        int32_t x_;
        int32_t y_;

    public:
        /// <summary>
        /// Default constructor creates position at origin
        /// </summary>
        constexpr Position(int32_t x = 0, int32_t y = 0) noexcept
            : x_(x), y_(y) {}

        // Accessors
        [[nodiscard]] constexpr int32_t X() const noexcept { return x_; }
        [[nodiscard]] constexpr int32_t Y() const noexcept { return y_; }

        /// <summary>
        /// Calculate Manhattan distance to another position
        /// </summary>
        [[nodiscard]] constexpr int32_t ManhattanDistanceTo(const Position& other) const noexcept {
            int32_t dx = (x_ > other.x_) ? (x_ - other.x_) : (other.x_ - x_);
            int32_t dy = (y_ > other.y_) ? (y_ - other.y_) : (other.y_ - y_);
            return dx + dy;
        }

        /// <summary>
        /// Calculate squared Euclidean distance (avoids sqrt for performance)
        /// </summary>
        [[nodiscard]] constexpr int32_t SquaredDistanceTo(const Position& other) const noexcept {
            int32_t dx = x_ - other.x_;
            int32_t dy = y_ - other.y_;
            return dx * dx + dy * dy;
        }

        /// <summary>
        /// Position arithmetic
        /// </summary>
        constexpr Position operator+(const Position& other) const noexcept {
            return Position(x_ + other.x_, y_ + other.y_);
        }

                constexpr Position operator-(const Position& other) const noexcept {
                    return Position(x_ - other.x_, y_ - other.y_);
                }

                // Equality operators
                constexpr bool operator==(const Position& other) const noexcept = default;
                constexpr bool operator!=(const Position& other) const noexcept = default;
            };

            /// <summary>
            /// Dice roller utility for generating random dice roll results
            /// Separates random number generation logic from the Dice value object
            /// </summary>
            class DiceRoller {
            private:
                static std::mt19937& GetRandomEngine() {
                    static thread_local std::mt19937 engine{std::random_device{}()};
                    return engine;
                }

            public:
                /// <summary>
                /// Roll dice and return the result
                /// </summary>
                [[nodiscard]] static int32_t Roll(const Dice& dice) {
                    if (dice.Count() == 0 || dice.Sides() == 0) {
                        return dice.Bonus();
                    }

                    std::uniform_int_distribution<int32_t> distribution(1, dice.Sides());
                    int32_t total = dice.Bonus();

                    for (int32_t i = 0; i < dice.Count(); ++i) {
                        total += distribution(GetRandomEngine());
                    }

                    return total;
                }

                /// <summary>
                /// Roll a simple d20 (common for attack rolls)
                /// </summary>
                [[nodiscard]] static int32_t RollD20() {
                    static const Dice d20(1, 20, 0);
                    return Roll(d20);
                }

                /// <summary>
                /// Roll with advantage (roll twice, take higher)
                /// </summary>
                [[nodiscard]] static int32_t RollWithAdvantage(const Dice& dice) {
                    int32_t roll1 = Roll(dice);
                    int32_t roll2 = Roll(dice);
                    return std::max(roll1, roll2);
                }

                /// <summary>
                /// Roll with disadvantage (roll twice, take lower)
                /// </summary>
                [[nodiscard]] static int32_t RollWithDisadvantage(const Dice& dice) {
                    int32_t roll1 = Roll(dice);
                    int32_t roll2 = Roll(dice);
                    return std::min(roll1, roll2);
                }

                /// <summary>
                /// Roll multiple times and sum (useful for critical hits)
                /// </summary>
                [[nodiscard]] static int32_t RollMultiple(const Dice& dice, int32_t times) {
                    int32_t total = 0;
                    for (int32_t i = 0; i < times; ++i) {
                        total += Roll(dice);
                    }
                    return total;
                }
            };

        } // namespace LB::Domain