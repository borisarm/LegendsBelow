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
    /// Item instance in inventory
    /// Represents a specific item with quantity and metadata
    /// </summary>
    struct InventoryItem {
        LB::Domain::ItemId itemId;
        LB::String name;
        LB::String description;
        int32_t quantity{1};
        int32_t maxStack{1};
        bool isEquippable{false};
        bool isConsumable{false};
        LB::Domain::EquipmentSlot equipmentSlot{LB::Domain::EquipmentSlot::None};

        [[nodiscard]] bool CanStack() const noexcept {
            return maxStack > 1;
        }

        [[nodiscard]] bool IsStackFull() const noexcept {
            return quantity >= maxStack;
        }

        [[nodiscard]] int32_t GetRemainingStackSpace() const noexcept {
            return std::max(0, maxStack - quantity);
        }
    };

    /// <summary>
    /// Equipment piece with stats
    /// </summary>
    struct Equipment {
        LB::Domain::ItemId itemId;
        LB::String name;
        LB::Domain::EquipmentSlot slot;
        LB::Domain::WeaponType weaponType{LB::Domain::WeaponType::None};
        LB::Domain::ArmorCategory armorCategory{LB::Domain::ArmorCategory::None};

        // Stat bonuses
        int32_t strengthBonus{0};
        int32_t dexterityBonus{0};
        int32_t constitutionBonus{0};
        int32_t intelligenceBonus{0};
        int32_t wisdomBonus{0};
        int32_t charismaBonus{0};

        // Combat stats
        int32_t armorClassBonus{0};
        int32_t attackBonus{0};
        int32_t damageBonus{0};

        [[nodiscard]] LB::Domain::Attributes GetAttributeBonuses() const noexcept {
            return LB::Domain::Attributes{
                strengthBonus,
                dexterityBonus,
                constitutionBonus,
                intelligenceBonus,
                wisdomBonus,
                charismaBonus
            };
        }

        [[nodiscard]] bool IsWeapon() const noexcept {
            return slot == LB::Domain::EquipmentSlot::MainHand || 
                   slot == LB::Domain::EquipmentSlot::OffHand;
        }

        [[nodiscard]] bool IsArmor() const noexcept {
            return slot == LB::Domain::EquipmentSlot::Head ||
                   slot == LB::Domain::EquipmentSlot::Chest ||
                   slot == LB::Domain::EquipmentSlot::Legs ||
                   slot == LB::Domain::EquipmentSlot::Feet;
        }
    };

    /// <summary>
    /// Equipped items per character
    /// </summary>
    struct EquippedItems {
        std::unordered_map<LB::Domain::EquipmentSlot, Equipment> equipment;

        [[nodiscard]] bool HasItemInSlot(LB::Domain::EquipmentSlot slot) const {
            return equipment.contains(slot);
        }

        [[nodiscard]] std::optional<Equipment> GetEquipment(LB::Domain::EquipmentSlot slot) const {
            auto it = equipment.find(slot);
            if (it == equipment.end()) {
                return std::nullopt;
            }
            return it->second;
        }

        [[nodiscard]] LB::Domain::Attributes GetTotalAttributeBonuses() const noexcept {
            LB::Domain::Attributes total{0, 0, 0, 0, 0, 0};

            for (const auto& [slot, equip] : equipment) {
                auto bonuses = equip.GetAttributeBonuses();
                total.strength += bonuses.strength;
                total.dexterity += bonuses.dexterity;
                total.constitution += bonuses.constitution;
                total.intelligence += bonuses.intelligence;
                total.wisdom += bonuses.wisdom;
                total.charisma += bonuses.charisma;
            }

            return total;
        }

        [[nodiscard]] int32_t GetTotalArmorClass() const noexcept {
            int32_t total = 0;
            for (const auto& [slot, equip] : equipment) {
                total += equip.armorClassBonus;
            }
            return total;
        }

        [[nodiscard]] int32_t GetTotalAttackBonus() const noexcept {
            int32_t total = 0;
            for (const auto& [slot, equip] : equipment) {
                total += equip.attackBonus;
            }
            return total;
        }

        [[nodiscard]] int32_t GetTotalDamageBonus() const noexcept {
            int32_t total = 0;
            for (const auto& [slot, equip] : equipment) {
                total += equip.damageBonus;
            }
            return total;
        }
    };

    /// <summary>
    /// Inventory operation result
    /// </summary>
    struct InventoryOperationResult {
        bool success{false};
        LB::String errorMessage;
        int32_t itemsAffected{0};
    };

    /// <summary>
    /// Core inventory service
    /// Manages items, equipment, and inventory capacity
    /// </summary>
    class InventoryService {
    public:
        /// <summary>
        /// Create inventory for a character
        /// </summary>
        bool CreateInventory(LB::Domain::CharacterId characterId, int32_t capacity = 20) {
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
        InventoryOperationResult AddItem(LB::Domain::CharacterId characterId, const InventoryItem& item) {
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
        InventoryOperationResult RemoveItem(LB::Domain::CharacterId characterId, LB::Domain::ItemId itemId, int32_t quantity = 1) {
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
        bool EquipItem(LB::Domain::CharacterId characterId, const Equipment& equipment) {
            if (!inventories_.contains(characterId)) {
                return false;
            }

            if (equipment.slot == LB::Domain::EquipmentSlot::None) {
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
        bool UnequipItem(LB::Domain::CharacterId characterId, LB::Domain::EquipmentSlot slot) {
            if (!equippedItems_.contains(characterId)) {
                return false;
            }

            auto& equipped = equippedItems_[characterId];
            return equipped.equipment.erase(slot) > 0;
        }

        /// <summary>
        /// Get equipped items for character
        /// </summary>
        std::optional<EquippedItems> GetEquippedItems(LB::Domain::CharacterId characterId) const {
            auto it = equippedItems_.find(characterId);
            if (it == equippedItems_.end()) {
                return std::nullopt;
            }
            return it->second;
        }

        /// <summary>
        /// Get all items in inventory
        /// </summary>
        std::vector<InventoryItem> GetInventory(LB::Domain::CharacterId characterId) const {
            auto it = inventories_.find(characterId);
            if (it == inventories_.end()) {
                return {};
            }
            return it->second;
        }

        /// <summary>
        /// Get item count in inventory
        /// </summary>
        int32_t GetItemCount(LB::Domain::CharacterId characterId, LB::Domain::ItemId itemId) const {
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
        bool HasInventorySpace(LB::Domain::CharacterId characterId) const {
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
        int32_t GetInventorySize(LB::Domain::CharacterId characterId) const {
            auto it = inventories_.find(characterId);
            if (it == inventories_.end()) {
                return 0;
            }
            return static_cast<int32_t>(it->second.size());
        }

        /// <summary>
        /// Get inventory capacity
        /// </summary>
        int32_t GetInventoryCapacity(LB::Domain::CharacterId characterId) const {
            auto it = capacities_.find(characterId);
            if (it == capacities_.end()) {
                return 0;
            }
            return it->second;
        }

        /// <summary>
        /// Set inventory capacity
        /// </summary>
        bool SetInventoryCapacity(LB::Domain::CharacterId characterId, int32_t capacity) {
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
        bool ClearInventory(LB::Domain::CharacterId characterId) {
            if (!inventories_.contains(characterId)) {
                return false;
            }

            inventories_[characterId].clear();
            return true;
        }

        /// <summary>
        /// Remove character inventory completely
        /// </summary>
        bool RemoveInventory(LB::Domain::CharacterId characterId) {
            bool removed = false;
            removed |= inventories_.erase(characterId) > 0;
            removed |= capacities_.erase(characterId) > 0;
            removed |= equippedItems_.erase(characterId) > 0;
            return removed;
        }

        /// <summary>
        /// Check if character has inventory
        /// </summary>
        bool HasInventory(LB::Domain::CharacterId characterId) const {
            return inventories_.contains(characterId);
        }

        /// <summary>
        /// Find items by name
        /// </summary>
        std::vector<InventoryItem> FindItems(LB::Domain::CharacterId characterId, const LB::String& nameQuery) const {
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
        float GetTotalWeight(LB::Domain::CharacterId characterId) const {
            // Future: implement weight system
            return 0.0f;
        }

    private:
        std::unordered_map<LB::Domain::CharacterId, std::vector<InventoryItem>> inventories_;
        std::unordered_map<LB::Domain::CharacterId, int32_t> capacities_;
        std::unordered_map<LB::Domain::CharacterId, EquippedItems> equippedItems_;
    };

} // namespace LB::Services::Inventory