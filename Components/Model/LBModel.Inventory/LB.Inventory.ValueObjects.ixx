// LB.Inventory.ValueObjects.ixx - Inventory domain value objects
export module LB.Inventory.ValueObjects;

import LB.Core;
import std;

export namespace LB::Inventory {

    /// <summary>
    /// Inventory operation result
    /// </summary>
    struct InventoryOperationResult {
        bool success{false};
        LB::String errorMessage;
        std::int32_t itemsAffected{0};
    };

} // namespace LB::Inventory