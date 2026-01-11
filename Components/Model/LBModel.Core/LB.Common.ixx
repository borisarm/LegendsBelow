// LB.Common.ixx - Common types and utilities module interface
export module LB.Common;

import std;


export namespace LB {
    // Common ID types
    export using Id = std::int32_t;
    export constexpr Id InvalidId{ std::numeric_limits<Id>::max() };
    
    // Common string type 
    export using String = std::string;
}