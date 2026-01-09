#pragma once
#include <cmath>
#include <functional>
#include "../Common/Types.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// World position in the isometric game grid
    /// Immutable value object representing tile coordinates
    /// </summary>
    class Position {
    public:
        constexpr Position(int32_t x = 0, int32_t y = 0) noexcept
            : x_(x), y_(y) {}

        // Accessors
        [[nodiscard]] constexpr int32_t X() const noexcept { return x_; }
        [[nodiscard]] constexpr int32_t Y() const noexcept { return y_; }

        // Comparison operators
        [[nodiscard]] constexpr bool operator==(const Position& other) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const Position& other) const noexcept = default;

        // Position arithmetic
        [[nodiscard]] constexpr Position operator+(const Position& other) const noexcept {
            return Position(x_ + other.x_, y_ + other.y_);
        }

        [[nodiscard]] constexpr Position operator-(const Position& other) const noexcept {
            return Position(x_ - other.x_, y_ - other.y_);
        }

        // Distance calculations
        [[nodiscard]] int32_t ManhattanDistance(const Position& other) const noexcept {
            return abs(x_ - other.x_) + abs(y_ - other.y_);
        }

        [[nodiscard]] double EuclideanDistance(const Position& other) const noexcept {
            int32_t dx = x_ - other.x_;
            int32_t dy = y_ - other.y_;
            return std::sqrt(static_cast<double>(dx * dx + dy * dy));
        }

        // Check if position is adjacent (8-directional)
        [[nodiscard]] bool IsAdjacentTo(const Position& other) const noexcept {
            int32_t dx = abs(x_ - other.x_);
            int32_t dy = abs(y_ - other.y_);
            return (dx <= 1 && dy <= 1) && (dx + dy > 0);
        }

        // Direction helpers
        [[nodiscard]] constexpr Position North() const noexcept { return Position(x_, y_ - 1); }
        [[nodiscard]] constexpr Position South() const noexcept { return Position(x_, y_ + 1); }
        [[nodiscard]] constexpr Position East() const noexcept { return Position(x_ + 1, y_); }
        [[nodiscard]] constexpr Position West() const noexcept { return Position(x_ - 1, y_); }

        // String representation
        [[nodiscard]] std::string ToString() const {
            return "(" + std::to_string(x_) + ", " + std::to_string(y_) + ")";
        }

        // Common positions
        static constexpr Position Origin() noexcept { return Position(0, 0); }
        static constexpr Position Invalid() noexcept { 
            return Position(std::numeric_limits<int32_t>::min(), 
                          std::numeric_limits<int32_t>::min()); 
        }

    private:
        int32_t x_;
        int32_t y_;
    };

} // namespace Domain
} // namespace LB

// Hash function for Position to use in unordered containers
namespace std {
    template<>
    struct hash<LB::Domain::Position> {
        std::size_t operator()(const LB::Domain::Position& pos) const noexcept {
            // Combine x and y coordinates into a single hash
            std::size_t h1 = std::hash<int32_t>{}(pos.X());
            std::size_t h2 = std::hash<int32_t>{}(pos.Y());
            return h1 ^ (h2 << 1);
        }
    };
}