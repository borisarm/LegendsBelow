#pragma once
#include <cstdint>
#include <string>
#include <limits>

//
// Common type definitions for LegendsBelow Domain Model
// This header defines basic types used throughout the domain
//

namespace LB {
    // Common ID types
    using Id = uint32_t;
    constexpr Id InvalidId = std::numeric_limits<Id>::max();
    
    // Common string type 
    using String = std::string;
}