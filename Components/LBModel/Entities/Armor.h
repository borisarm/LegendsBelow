#pragma once
#include <vector>
#include <unordered_map>
#include "../Common/Types.h"
#include "Entity.h"
#include "../Enums/EquipmentSlot.h"
#include "../Enums/ArmorCategory.h"
#include "../Enums/CharacterClass.h"
#include "../Enums/DamageType.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Resistance values for different damage types
    /// Used for both elemental and spiritual resistances
    /// </summary>
    using ResistanceMap = std::unordered_map<DamageType, int32_t>;

    /// <summary>
    /// Armor entity with protection values and restrictions
    /// Based on armor.json data structure
    /// </summary>
    class Armor : public Entity {
    public:
        /// <summary>
        /// Create armor with complete properties
        /// </summary>
        Armor(Id id,
              String name,
              EquipmentSlot slot,
              ArmorCategory category,
              int32_t requiredLevel,
              std::vector<CharacterClass> allowedClasses,
              int32_t physicalAR,
              ResistanceMap elementalAR,
              ResistanceMap spiritualAR,
              int32_t weight,
              float movementPenalty,
              float castPenalty,
              int32_t affixSlots)
            : Entity(id)
            , name_(std::move(name))
            , slot_(slot)
            , category_(category)
            , requiredLevel_(requiredLevel)
            , allowedClasses_(std::move(allowedClasses))
            , physicalAR_(physicalAR)
            , elementalAR_(std::move(elementalAR))
            , spiritualAR_(std::move(spiritualAR))
            , weight_(weight)
            , movementPenalty_(movementPenalty)
            , castPenalty_(castPenalty)
            , affixSlots_(affixSlots)
        {
        }

        // Basic properties
        [[nodiscard]] const String& GetName() const noexcept { return name_; }
        [[nodiscard]] EquipmentSlot GetSlot() const noexcept { return slot_; }
        [[nodiscard]] ArmorCategory GetCategory() const noexcept { return category_; }
        [[nodiscard]] int32_t GetRequiredLevel() const noexcept { return requiredLevel_; }
        [[nodiscard]] const std::vector<CharacterClass>& GetAllowedClasses() const noexcept { return allowedClasses_; }

        // Protection properties
        [[nodiscard]] int32_t GetPhysicalAR() const noexcept { return physicalAR_; }
        [[nodiscard]] const ResistanceMap& GetElementalAR() const noexcept { return elementalAR_; }
        [[nodiscard]] const ResistanceMap& GetSpiritualAR() const noexcept { return spiritualAR_; }

        // Get specific resistance value
        [[nodiscard]] int32_t GetElementalResistance(DamageType damageType) const noexcept {
            auto it = elementalAR_.find(damageType);
            return it != elementalAR_.end() ? it->second : 0;
        }

        [[nodiscard]] int32_t GetSpiritualResistance(DamageType damageType) const noexcept {
            auto it = spiritualAR_.find(damageType);
            return it != spiritualAR_.end() ? it->second : 0;
        }

        // Get total resistance for any damage type
        [[nodiscard]] int32_t GetTotalResistance(DamageType damageType) const noexcept {
            // Physical damage uses physical AR
            if (damageType == DamageType::Slashing || 
                damageType == DamageType::Piercing || 
                damageType == DamageType::Bludgeoning) {
                return physicalAR_;
            }
            
            // Check elemental resistances
            int32_t resistance = GetElementalResistance(damageType);
            if (resistance > 0) {
                return resistance;
            }
            
            // Check spiritual resistances
            return GetSpiritualResistance(damageType);
        }

        // Physical properties
        [[nodiscard]] int32_t GetWeight() const noexcept { return weight_; }
        [[nodiscard]] float GetMovementPenalty() const noexcept { return movementPenalty_; }
        [[nodiscard]] float GetCastPenalty() const noexcept { return castPenalty_; }
        [[nodiscard]] int32_t GetAffixSlots() const noexcept { return affixSlots_; }

        // Shield-specific properties (only valid for shields)
        [[nodiscard]] int32_t GetArmorBonus() const noexcept { return armorBonus_; }
        [[nodiscard]] int32_t GetBlockChance() const noexcept { return blockChance_; }
        [[nodiscard]] int32_t GetBlockValue() const noexcept { return blockValue_; }

        // Property checks
        [[nodiscard]] bool IsShield() const noexcept { 
            return category_ == ArmorCategory::Shield; 
        }

        [[nodiscard]] bool HasMovementPenalty() const noexcept { 
            return movementPenalty_ > 0.0f; 
        }

        [[nodiscard]] bool HasCastPenalty() const noexcept { 
            return castPenalty_ > 0.0f; 
        }

        // Class restrictions
        [[nodiscard]] bool CanBeWornBy(CharacterClass characterClass) const noexcept {
            return std::find(allowedClasses_.begin(), allowedClasses_.end(), characterClass) != allowedClasses_.end();
        }

        // Setters for shield properties (used during construction from JSON)
        void SetShieldProperties(int32_t armorBonus, int32_t blockChance, int32_t blockValue) {
            armorBonus_ = armorBonus;
            blockChance_ = blockChance;
            blockValue_ = blockValue;
        }

    private:
        // Basic properties
        String name_;
        EquipmentSlot slot_;
        ArmorCategory category_;
        int32_t requiredLevel_;
        std::vector<CharacterClass> allowedClasses_;

        // Protection properties
        int32_t physicalAR_;
        ResistanceMap elementalAR_;
        ResistanceMap spiritualAR_;

        // Physical properties
        int32_t weight_;
        float movementPenalty_;
        float castPenalty_;
        int32_t affixSlots_;

        // Shield-specific properties (only valid for shields)
        int32_t armorBonus_ = 0;
        int32_t blockChance_ = 0;
        int32_t blockValue_ = 0;
    };

    // Type aliases for armor IDs
    using ArmorId = Id;

} // namespace Domain
} // namespace LB