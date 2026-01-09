#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Standard C++20 includes for domain model
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>
#include <functional>
#include <algorithm>
#include <random>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <concepts>
#include <atomic>

// Domain-specific type aliases
namespace LB {
    // Common ID types
    using Id = uint32_t;
    constexpr Id InvalidId = std::numeric_limits<Id>::max();

    // Common string type 
    using String = std::string;

    // Random number generation
    using RandomEngine = std::mt19937;
}
