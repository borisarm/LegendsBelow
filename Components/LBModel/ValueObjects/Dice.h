#pragma once
#include <random>
#include <algorithm>
#include "../Common/Types.h"
#include "../Enums/DamageType.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Represents a dice roll with number of dice, die type, and flat bonus
    /// Example: 2d6+3 = Roll(2, 6, 3)
    /// </summary>
    class Dice {
    public:
        /// <summary>
        /// Create a dice specification
        /// </summary>
        /// <param name="count">Number of dice to roll (1-20)</param>
        /// <param name="sides">Type of die (4, 6, 8, 10, 12, 20, etc.)</param>
        /// <param name="bonus">Flat bonus to add to the result</param>
        constexpr Dice(int32_t count = 1, int32_t sides = 6, int32_t bonus = 0) noexcept
            : count_(std::clamp(count, 1, 20))
            , sides_(std::clamp(sides, 2, 100))
            , bonus_(bonus)
        {
        }

        // Accessors
        [[nodiscard]] constexpr int32_t Count() const noexcept { return count_; }
        [[nodiscard]] constexpr int32_t Sides() const noexcept { return sides_; }
        [[nodiscard]] constexpr int32_t Bonus() const noexcept { return bonus_; }

        // Calculate min/max possible values
        [[nodiscard]] constexpr int32_t MinValue() const noexcept { return count_ + bonus_; }
        [[nodiscard]] constexpr int32_t MaxValue() const noexcept { return (count_ * sides_) + bonus_; }
        [[nodiscard]] constexpr double AverageValue() const noexcept { 
            return (count_ * (sides_ + 1) * 0.5) + bonus_; 
        }

        /// <summary>
        /// Roll the dice using provided random engine
        /// </summary>
        template<typename RandomEngine>
        [[nodiscard]] int32_t Roll(RandomEngine& engine) const {
            std::uniform_int_distribution<int32_t> dist(1, sides_);
            int32_t result = bonus_;
            for (int32_t i = 0; i < count_; ++i) {
                result += dist(engine);
            }
            return result;
        }

        // Comparison operators
        [[nodiscard]] constexpr bool operator==(const Dice& other) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const Dice& other) const noexcept = default;

        // String representation for debugging
        [[nodiscard]] std::string ToString() const {
            if (bonus_ == 0) {
                return std::to_string(count_) + "d" + std::to_string(sides_);
            }
            else if (bonus_ > 0) {
                return std::to_string(count_) + "d" + std::to_string(sides_) + "+" + std::to_string(bonus_);
            }
            else {
                return std::to_string(count_) + "d" + std::to_string(sides_) + std::to_string(bonus_);
            }
        }

        // Common dice presets
        static constexpr Dice D4() noexcept { return Dice(1, 4); }
        static constexpr Dice D6() noexcept { return Dice(1, 6); }
        static constexpr Dice D8() noexcept { return Dice(1, 8); }
        static constexpr Dice D10() noexcept { return Dice(1, 10); }
        static constexpr Dice D12() noexcept { return Dice(1, 12); }
        static constexpr Dice D20() noexcept { return Dice(1, 20); }

    private:
        int32_t count_;
        int32_t sides_;
        int32_t bonus_;
    };

    /// <summary>
    /// Damage roll with associated damage type
    /// </summary>
    class DamageRoll {
    public:
        constexpr DamageRoll(Dice dice, DamageType type) noexcept
            : dice_(dice), damageType_(type) {}

        [[nodiscard]] constexpr const Dice& GetDice() const noexcept { return dice_; }
        [[nodiscard]] constexpr DamageType GetDamageType() const noexcept { return damageType_; }

        template<typename RandomEngine>
        [[nodiscard]] int32_t Roll(RandomEngine& engine) const {
            return dice_.Roll(engine);
        }

        [[nodiscard]] constexpr bool operator==(const DamageRoll& other) const noexcept = default;

    private:
        Dice dice_;
        DamageType damageType_;
    };

} // namespace Domain
} // namespace LB