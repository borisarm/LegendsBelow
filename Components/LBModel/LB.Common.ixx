// LB.Common.ixx - Common types and utilities module interface
export module LB.Common;

import std;


export namespace LB {
    // Common ID types
    using Id = std::int32_t;
    constexpr Id InvalidId{ std::numeric_limits<Id>::max() };
    
    // Common string type 
    using String = std::string;
}