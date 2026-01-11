// LB.InventoryService.ixx - Inventory and equipment management service using C++20 modules
export module LB.InventoryService;

// Import only required domains
import LB.Core;
import LB.Character;
import LB.Combat;
import LB.Inventory;
import std;

/// <summary>
/// Inventory service for LegendsBelow
/// Provides business logic for item management, equipment slots, and inventory capacity
/// Integrates with character service for equipment bonuses
/// </summary>
export namespace LB::Services::Inventory {
    // Re-export domain types for service convenience
    using LB::Character::CharacterId;
    using LB::Character::ItemId;
    using LB::Combat::EquipmentSlot;
    using LB::Combat::WeaponType;
    using LB::Combat::ArmorCategory;
    using LB::Inventory::InventoryItem;
    using LB::Inventory::Equipment;
    using LB::Inventory::EquippedItems;
    using LB::Inventory::InventoryOperationResult;

    /// <summary>
    /// Core inventory service
    /// Manages items, equipment, and inventory capacity
    /// </summary>
    class InventoryService {
    public:
        /// <summary>
        /// Create inventory for a character
        /// </summary>
        bool CreateInventory(CharacterId characterId, int32_t capacity = 20) {
            if (inventories_.contains(characterId)) {
                return false; // Inventory already exists
            }

            inventories_[characterId] = {};
            capacities_[characterId] = capacity;
            equippedItems_[characterId] = EquippedItems{};

            return true;
        }

        /// <summary>
        /// Add item to inventory
        /// Handles stacking automatically
        /// </summary>
        InventoryOperationResult AddItem(CharacterId characterId, const InventoryItem& item) {
            InventoryOperationResult result{};

            if (!inventories_.contains(characterId)) {
                result.errorMessage = "Character inventory not found";
                return result;
            }

            auto& inventory = inventories_[characterId];
            int32_t remainingQuantity = item.quantity;

            // Try to stack with existing items first
            if (item.CanStack()) {
                for (auto& existingItem : inventory) {
                    if (existingItem.itemId == item.itemId && !existingItem.IsStackFull()) {
                        int32_t spaceAvailable = existingItem.GetRemainingStackSpace();
                        int32_t amountToAdd = std::min(spaceAvailable, remainingQuantity);

                        existingItem.quantity += amountToAdd;
                        remainingQuantity -= amountToAdd;
                        result.itemsAffected += amountToAdd;

                        if (remainingQuantity <= 0) {
                            result.success = true;
                            return result;
                        }
                    }
                }
            }

            // Add new stacks if needed
            while (remainingQuantity > 0) {
                if (!HasInventorySpace(characterId)) {
                    result.errorMessage = "Inventory full";
                    result.success = result.itemsAffected > 0; // Partial success
                    return result;
                }

                InventoryItem newStack = item;
                newStack.quantity = std::min(remainingQuantity, item.maxStack);
                inventory.push_back(newStack);

                remainingQuantity -= newStack.quantity;
                result.itemsAffected += newStack.quantity;
            }

            result.success = true;
            return result;
        }

        /// <summary>
        /// Remove item from inventory
        /// </summary>
        InventoryOperationResult RemoveItem(CharacterId characterId, ItemId itemId, int32_t quantity = 1) {
            InventoryOperationResult result{};

            if (!inventories_.contains(characterId)) {
                result.errorMessage = "Character inventory not found";
                return result;
            }

            auto& inventory = inventories_[characterId];
            int32_t remainingToRemove = quantity;

            for (auto it = inventory.begin(); it != inventory.end(); ) {
                if (it->itemId == itemId) {
                    int32_t amountToRemove = std::min(it->quantity, remainingToRemove);
                    it->quantity -= amountToRemove;
                    remainingToRemove -= amountToRemove;
                    result.itemsAffected += amountToRemove;

                    if (it->quantity <= 0) {
                        it = inventory.erase(it);
                    } else {
                        ++it;
                    }

                    if (remainingToRemove <= 0) {
                        result.success = true;
                        return result;
                    }
                } else {
                    ++it;
                }
            }

            if (result.itemsAffected > 0) {
                result.success = true; // Partial success
                result.errorMessage = "Not enough items to remove";
            } else {
                result.errorMessage = "Item not found in inventory";
            }

            return result;
        }

        /// <summary>
        /// Equip item to character
        /// </summary>
        bool EquipItem(CharacterId characterId, const Equipment& equipment) {
            if (!inventories_.contains(characterId)) {
                return false;
            }

            if (equipment.slot == EquipmentSlot::None) {
                return false; // Invalid slot
            }

            auto& equipped = equippedItems_[characterId];

            // Unequip existing item in slot if present
            if (equipped.HasItemInSlot(equipment.slot)) {
                UnequipItem(characterId, equipment.slot);
            }

            equipped.equipment[equipment.slot] = equipment;
            return true;
        }

        /// <summary>
        /// Unequip item from slot
        /// </summary>
        bool UnequipItem(CharacterId characterId, EquipmentSlot slot) {
            if (!equippedItems_.contains(characterId)) {
                return false;
            }

            auto& equipped = equippedItems_[characterId];
            return equipped.equipment.erase(slot) > 0;
        }

        /// <summary>
        /// Get equipped items for character
        /// </summary>
        std::optional<EquippedItems> GetEquippedItems(CharacterId characterId) const {
            auto it = equippedItems_.find(characterId);
            if (it == equippedItems_.end()) {
                return std::nullopt;
            }
            return it->second;
        }

        /// <summary>
        /// Get all items in inventory
        /// </summary>
        std::vector<InventoryItem> GetInventory(CharacterId characterId) const {
            auto it = inventories_.find(characterId);
            if (it == inventories_.end()) {
                return {};
            }
            return it->second;
        }

        /// <summary>
        /// Get item count in inventory
        /// </summary>
        int32_t GetItemCount(CharacterId characterId, ItemId itemId) const {
            auto it = inventories_.find(characterId);
            if (it == inventories_.end()) {
                return 0;
            }

            int32_t count = 0;
            for (const auto& item : it->second) {
                if (item.itemId == itemId) {
                    count += item.quantity;
                }
            }
            return count;
        }

        /// <summary>
        /// Check if inventory has space
        /// </summary>
        bool HasInventorySpace(CharacterId characterId) const {
            auto inventoryIt = inventories_.find(characterId);
            auto capacityIt = capacities_.find(characterId);

            if (inventoryIt == inventories_.end() || capacityIt == capacities_.end()) {
                return false;
            }

            return static_cast<int32_t>(inventoryIt->second.size()) < capacityIt->second;
        }

        /// <summary>
        /// Get current inventory usage
        /// </summary>
        int32_t GetInventorySize(CharacterId characterId) const {
            auto it = inventories_.find(characterId);
            if (it == inventories_.end()) {
                return 0;
            }
            return static_cast<int32_t>(it->second.size());
        }

        /// <summary>
        /// Get inventory capacity
        /// </summary>
        int32_t GetInventoryCapacity(CharacterId characterId) const {
            auto it = capacities_.find(characterId);
            if (it == capacities_.end()) {
                return 0;
            }
            return it->second;
        }

        /// <summary>
        /// Set inventory capacity
        /// </summary>
        bool SetInventoryCapacity(CharacterId characterId, int32_t capacity) {
            if (!inventories_.contains(characterId)) {
                return false;
            }

            if (capacity < 0) {
                return false;
            }

            capacities_[characterId] = capacity;
            return true;
        }

        /// <summary>
        /// Clear inventory
        /// </summary>
        bool ClearInventory(CharacterId characterId) {
            if (!inventories_.contains(characterId)) {
                return false;
            }

            inventories_[characterId].clear();
            return true;
        }

        /// <summary>
        /// Remove character inventory completely
        /// </summary>
        bool RemoveInventory(CharacterId characterId) {
            bool removed = false;
            removed |= inventories_.erase(characterId) > 0;
            removed |= capacities_.erase(characterId) > 0;
            removed |= equippedItems_.erase(characterId) > 0;
            return removed;
        }

        /// <summary>
        /// Check if character has inventory
        /// </summary>
        bool HasInventory(CharacterId characterId) const {
            return inventories_.contains(characterId);
        }

        /// <summary>
        /// Find items by name
        /// </summary>
        std::vector<InventoryItem> FindItems(CharacterId characterId, const LB::String& nameQuery) const {
            std::vector<InventoryItem> results;

            auto it = inventories_.find(characterId);
            if (it == inventories_.end()) {
                return results;
            }

            for (const auto& item : it->second) {
                if (item.name.find(nameQuery) != LB::String::npos) {
                    results.push_back(item);
                }
            }

            return results;
        }

        /// <summary>
        /// Get total weight (if implementing weight system)
        /// Placeholder for future weight mechanics
        /// </summary>
        float GetTotalWeight(CharacterId characterId) const {
            // Future: implement weight system
            return 0.0f;
        }

    private:
        std::unordered_map<CharacterId, std::vector<InventoryItem>> inventories_;
        std::unordered_map<CharacterId, int32_t> capacities_;
        std::unordered_map<CharacterId, EquippedItems> equippedItems_;
    };

} // namespace LB::Services::Inventory