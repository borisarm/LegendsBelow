#pragma once
#include <vector>
#include "../Common/Types.h"
#include "Entity.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Item categories for consumables and miscellaneous items
    /// </summary>
    enum class ItemCategory : uint32_t {
        None = 0,
        Consumable = 1,     // Potions, food, scrolls
        Reagent = 2,        // Crafting materials
        Quest = 3,          // Quest-specific items
        Treasure = 4,       // Valuable items for selling
        Ammunition = 5,     // Arrows, bolts, etc.
        Tool = 6           // Utility items
    };

    /// <summary>
    /// Item rarity affecting value and power
    /// </summary>
    enum class ItemRarity : uint32_t {
        Common = 0,     // White - basic items
        Uncommon = 1,   // Green - improved items
        Rare = 2,       // Blue - significant items
        Epic = 3,       // Purple - powerful items
        Legendary = 4   // Orange - unique items
    };

    /// <summary>
    /// Basic item entity for consumables and miscellaneous objects
    /// Foundation for potions, scrolls, quest items, etc.
    /// </summary>
    class Item : public Entity {
    public:
        /// <summary>
        /// Create item with basic properties
        /// </summary>
        Item(Id id,
             String name,
             ItemCategory category,
             ItemRarity rarity,
             int32_t maxStackSize,
             int32_t weight,
             int32_t value,
             String description)
            : Entity(id)
            , name_(std::move(name))
            , category_(category)
            , rarity_(rarity)
            , maxStackSize_(maxStackSize)
            , weight_(weight)
            , value_(value)
            , description_(std::move(description))
        {
        }

        // Basic properties
        [[nodiscard]] const String& GetName() const noexcept { return name_; }
        [[nodiscard]] ItemCategory GetCategory() const noexcept { return category_; }
        [[nodiscard]] ItemRarity GetRarity() const noexcept { return rarity_; }
        [[nodiscard]] const String& GetDescription() const noexcept { return description_; }

        // Physical properties
        [[nodiscard]] int32_t GetMaxStackSize() const noexcept { return maxStackSize_; }
        [[nodiscard]] int32_t GetWeight() const noexcept { return weight_; }
        [[nodiscard]] int32_t GetValue() const noexcept { return value_; }

        // Property checks
        [[nodiscard]] bool IsConsumable() const noexcept { 
            return category_ == ItemCategory::Consumable; 
        }

        [[nodiscard]] bool IsStackable() const noexcept { 
            return maxStackSize_ > 1; 
        }

        [[nodiscard]] bool IsQuestItem() const noexcept { 
            return category_ == ItemCategory::Quest; 
        }

        [[nodiscard]] bool IsAmmunition() const noexcept { 
            return category_ == ItemCategory::Ammunition; 
        }

    private:
        // Basic properties
        String name_;
        ItemCategory category_;
        ItemRarity rarity_;
        String description_;

        // Physical properties
        int32_t maxStackSize_;
        int32_t weight_;
        int32_t value_;
    };

    // Type aliases for item IDs
    using ItemId = Id;

    /// <summary>
    /// Convert ItemCategory to string
    /// </summary>
    constexpr const char* ToString(ItemCategory category) noexcept {
        switch (category) {
            case ItemCategory::None: return "None";
            case ItemCategory::Consumable: return "Consumable";
            case ItemCategory::Reagent: return "Reagent";
            case ItemCategory::Quest: return "Quest";
            case ItemCategory::Treasure: return "Treasure";
            case ItemCategory::Ammunition: return "Ammunition";
            case ItemCategory::Tool: return "Tool";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Convert ItemRarity to string
    /// </summary>
    constexpr const char* ToString(ItemRarity rarity) noexcept {
        switch (rarity) {
            case ItemRarity::Common: return "Common";
            case ItemRarity::Uncommon: return "Uncommon";
            case ItemRarity::Rare: return "Rare";
            case ItemRarity::Epic: return "Epic";
            case ItemRarity::Legendary: return "Legendary";
            default: return "Unknown";
        }
    }

} // namespace Domain
} // namespace LB