// LB.Inventory.Entities.ixx - Inventory domain entities
export module LB.Inventory.Entities;

import LB.Core;
import LB.Character;
import LB.Combat;
import std;

export namespace LB::Inventory {
    using LB::Character::ItemId;
    using LB::Combat::EquipmentSlot;
    using LB::Combat::WeaponType;
    using LB::Combat::ArmorCategory;

    /// <summary>
    /// Item instance in inventory
    /// </summary>
    struct InventoryItem {
        ItemId itemId;
        LB::String name;
        LB::String description;
        std::int32_t quantity{1};
        std::int32_t maxStack{1};
        bool isEquippable{false};
        bool isConsumable{false};
        EquipmentSlot equipmentSlot{EquipmentSlot::None};

        [[nodiscard]] bool CanStack() const noexcept {
            return maxStack > 1;
        }

        [[nodiscard]] bool IsStackFull() const noexcept {
            return quantity >= maxStack;
        }

        [[nodiscard]] std::int32_t GetRemainingStackSpace() const noexcept {
            return std::max(0, maxStack - quantity);
        }
    };

    /// <summary>
    /// Equipment piece with stats
    /// </summary>
    struct Equipment {
        ItemId itemId;
        LB::String name;
        EquipmentSlot slot;
        WeaponType weaponType{WeaponType::None};
        ArmorCategory armorCategory{ArmorCategory::None};

        // Stat bonuses
        std::int32_t strengthBonus{0};
        std::int32_t dexterityBonus{0};
        std::int32_t constitutionBonus{0};
        std::int32_t intelligenceBonus{0};
        std::int32_t wisdomBonus{0};
        std::int32_t charismaBonus{0};

        // Combat stats
        std::int32_t armorClassBonus{0};
        std::int32_t attackBonus{0};
        std::int32_t damageBonus{0};

        [[nodiscard]] LB::Attributes GetAttributeBonuses() const noexcept {
            return LB::Attributes{
                strengthBonus,
                dexterityBonus,
                constitutionBonus,
                intelligenceBonus,
                wisdomBonus,
                charismaBonus
            };
        }

        [[nodiscard]] bool IsWeapon() const noexcept {
            return slot == EquipmentSlot::MainHand || slot == EquipmentSlot::OffHand;
        }

        [[nodiscard]] bool IsArmor() const noexcept {
            return slot == EquipmentSlot::Head ||
                   slot == EquipmentSlot::Chest ||
                   slot == EquipmentSlot::Legs ||
                   slot == EquipmentSlot::Hands ||
                   slot == EquipmentSlot::Feet;
        }
    };

    /// <summary>
    /// Equipped items per character
    /// </summary>
    struct EquippedItems {
        std::unordered_map<EquipmentSlot, Equipment> equipment;

        [[nodiscard]] bool HasItemInSlot(EquipmentSlot slot) const {
            return equipment.contains(slot);
        }

        [[nodiscard]] std::optional<Equipment> GetEquipment(EquipmentSlot slot) const {
            auto it = equipment.find(slot);
            if (it == equipment.end()) return std::nullopt;
            return it->second;
        }

        [[nodiscard]] LB::Attributes GetTotalAttributeBonuses() const noexcept {
            LB::Attributes total{0, 0, 0, 0, 0, 0};

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

        [[nodiscard]] std::int32_t GetTotalArmorClass() const noexcept {
            std::int32_t total = 0;
            for (const auto& [slot, equip] : equipment) {
                total += equip.armorClassBonus;
            }
            return total;
        }

        [[nodiscard]] std::int32_t GetTotalAttackBonus() const noexcept {
            std::int32_t total = 0;
            for (const auto& [slot, equip] : equipment) {
                total += equip.attackBonus;
            }
            return total;
        }

        [[nodiscard]] std::int32_t GetTotalDamageBonus() const noexcept {
            std::int32_t total = 0;
            for (const auto& [slot, equip] : equipment) {
                total += equip.damageBonus;
            }
            return total;
        }
    };

} // namespace LB::Inventory