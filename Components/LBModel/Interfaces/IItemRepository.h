#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "../Entities/Item.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Repository interface for item data access
    /// Defines contract for loading item definitions and metadata
    /// </summary>
    class IItemRepository {
    public:
        virtual ~IItemRepository() = default;

        /// <summary>
        /// Get a specific item by ID
        /// </summary>
        virtual std::optional<Item> GetItem(ItemId id) const = 0;

        /// <summary>
        /// Get all items of a specific category
        /// </summary>
        virtual std::vector<ItemId> GetItemsByCategory(ItemCategory category) const = 0;

        /// <summary>
        /// Get items of a specific rarity
        /// </summary>
        virtual std::vector<ItemId> GetItemsByRarity(ItemRarity rarity) const = 0;

        /// <summary>
        /// Get consumable items (potions, food, etc.)
        /// </summary>
        virtual std::vector<ItemId> GetConsumableItems() const = 0;

        /// <summary>
        /// Get ammunition items for specific weapon types
        /// </summary>
        virtual std::vector<ItemId> GetAmmunition() const = 0;

        /// <summary>
        /// Load all available item IDs
        /// </summary>
        virtual std::vector<ItemId> GetAllItemIds() const = 0;

        /// <summary>
        /// Check if an item exists
        /// </summary>
        virtual bool ItemExists(ItemId id) const = 0;

        /// <summary>
        /// Get item category information
        /// </summary>
        virtual std::optional<String> GetCategoryDescription(ItemCategory category) const = 0;
    };

} // namespace Domain
} // namespace LB