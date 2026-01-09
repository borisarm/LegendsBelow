#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "Entity.h"
#include "../Enums/WeaponType.h"
#include "../Enums/EquipmentSlot.h"
#include "../Enums/CharacterClass.h"
#include "../Enums/DamageType.h"
#include "../ValueObjects/Dice.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Weapon special properties affecting combat behavior
    /// </summary>
    enum class WeaponProperty : uint32_t {
        None = 0,
        HighDamage = 1,     // Increased damage potential
        Cleave = 2,         // Hits multiple targets
        BonusVsArmored = 3, // Extra damage vs armored foes
        SneakBonus = 4,     // Bonus damage from stealth
        Reach = 5,          // Extended attack range
        SpellFocus = 6      // Enhances spellcasting
    };

    /// <summary>
    /// Primary attribute that scales weapon damage
    /// </summary>
    enum class AttributeScale : uint32_t {
        None = 0,
        STR = 1,    // Strength-based weapons
        DEX = 2,    // Dexterity-based weapons  
        INT = 3,    // Intelligence-based (magical)
        WIS = 4     // Wisdom-based (divine)
    };

    /// <summary>
    /// Weapon entity with combat properties and restrictions
    /// Based on weapons.json data structure
    /// </summary>
    class Weapon : public Entity {
    public:
        /// <summary>
        /// Create weapon with complete properties
        /// </summary>
        Weapon(Id id,
               String name,
               WeaponType category,
               EquipmentSlot slot,
               int32_t requiredLevel,
               std::vector<CharacterClass> allowedClasses,
               Dice damageDice,
               int32_t damageBonus,
               std::vector<DamageType> damageTypes,
               float attackTimeSeconds,
               AttributeScale attributeScale,
               float scaleFactor,
               bool isTwoHanded,
               int32_t affixSlots)
            : Entity(id)
            , name_(std::move(name))
            , category_(category)
            , slot_(slot)
            , requiredLevel_(requiredLevel)
            , allowedClasses_(std::move(allowedClasses))
            , damageDice_(damageDice)
            , damageBonus_(damageBonus)
            , damageTypes_(std::move(damageTypes))
            , attackTimeSeconds_(attackTimeSeconds)
            , attributeScale_(attributeScale)
            , scaleFactor_(scaleFactor)
            , isTwoHanded_(isTwoHanded)
            , affixSlots_(affixSlots)
            , critRange_(20) // Default to natural 20
            , critMultiplier_(1.5f) // Default crit multiplier
        {
        }

        // Basic properties
        [[nodiscard]] const String& GetName() const noexcept { return name_; }
        [[nodiscard]] WeaponType GetCategory() const noexcept { return category_; }
        [[nodiscard]] EquipmentSlot GetSlot() const noexcept { return slot_; }
        [[nodiscard]] int32_t GetRequiredLevel() const noexcept { return requiredLevel_; }
        [[nodiscard]] const std::vector<CharacterClass>& GetAllowedClasses() const noexcept { return allowedClasses_; }

        // Combat properties
        [[nodiscard]] const Dice& GetDamageDice() const noexcept { return damageDice_; }
        [[nodiscard]] int32_t GetDamageBonus() const noexcept { return damageBonus_; }
        [[nodiscard]] const std::vector<DamageType>& GetDamageTypes() const noexcept { return damageTypes_; }
        [[nodiscard]] float GetAttackTimeSeconds() const noexcept { return attackTimeSeconds_; }
        [[nodiscard]] int32_t GetCritRange() const noexcept { return critRange_; }
        [[nodiscard]] float GetCritMultiplier() const noexcept { return critMultiplier_; }

        // Scaling properties
        [[nodiscard]] AttributeScale GetAttributeScale() const noexcept { return attributeScale_; }
        [[nodiscard]] float GetScaleFactor() const noexcept { return scaleFactor_; }

        // Physical properties
        [[nodiscard]] bool IsTwoHanded() const noexcept { return isTwoHanded_; }
        [[nodiscard]] int32_t GetAffixSlots() const noexcept { return affixSlots_; }

        // Special properties
        [[nodiscard]] const std::vector<WeaponProperty>& GetSpecialProperties() const noexcept { return specialProperties_; }
        [[nodiscard]] const std::optional<String>& GetAmmoType() const noexcept { return ammoType_; }

        // Property checks
        [[nodiscard]] bool HasProperty(WeaponProperty property) const noexcept {
            return std::find(specialProperties_.begin(), specialProperties_.end(), property) != specialProperties_.end();
        }

        [[nodiscard]] bool RequiresAmmo() const noexcept {
            return ammoType_.has_value();
        }

        // Class restrictions
        [[nodiscard]] bool CanBeUsedBy(CharacterClass characterClass) const noexcept {
            return std::find(allowedClasses_.begin(), allowedClasses_.end(), characterClass) != allowedClasses_.end();
        }

        // Primary damage type (first in list)
        [[nodiscard]] DamageType GetPrimaryDamageType() const noexcept {
            return damageTypes_.empty() ? DamageType::None : damageTypes_[0];
        }

        // Setters for optional properties (used during construction from JSON)
        void SetCriticalProperties(int32_t critRange, float critMultiplier) {
            critRange_ = critRange;
            critMultiplier_ = critMultiplier;
        }

        void SetSpecialProperties(std::vector<WeaponProperty> properties) {
            specialProperties_ = std::move(properties);
        }

        void SetAmmoType(String ammoType) {
            ammoType_ = std::move(ammoType);
        }

    private:
        // Basic properties
        String name_;
        WeaponType category_;
        EquipmentSlot slot_;
        int32_t requiredLevel_;
        std::vector<CharacterClass> allowedClasses_;

        // Combat properties
        Dice damageDice_;
        int32_t damageBonus_;
        std::vector<DamageType> damageTypes_;
        float attackTimeSeconds_;
        int32_t critRange_;
        float critMultiplier_;

        // Scaling properties
        AttributeScale attributeScale_;
        float scaleFactor_;

        // Physical properties
        bool isTwoHanded_;
        int32_t affixSlots_;

        // Special properties (optional)
        std::vector<WeaponProperty> specialProperties_;
        std::optional<String> ammoType_;
    };

    // Type aliases for weapon IDs
    using WeaponId = Id;

    /// <summary>
    /// Convert WeaponProperty to string
    /// </summary>
    constexpr const char* ToString(WeaponProperty property) noexcept {
        switch (property) {
            case WeaponProperty::None: return "None";
            case WeaponProperty::HighDamage: return "HighDamage";
            case WeaponProperty::Cleave: return "Cleave";
            case WeaponProperty::BonusVsArmored: return "BonusVsArmored";
            case WeaponProperty::SneakBonus: return "SneakBonus";
            case WeaponProperty::Reach: return "Reach";
            case WeaponProperty::SpellFocus: return "SpellFocus";
            default: return "Unknown";
        }
    }

    /// <summary>
    /// Convert AttributeScale to string
    /// </summary>
    constexpr const char* ToString(AttributeScale scale) noexcept {
        switch (scale) {
            case AttributeScale::None: return "None";
            case AttributeScale::STR: return "STR";
            case AttributeScale::DEX: return "DEX";
            case AttributeScale::INT: return "INT";
            case AttributeScale::WIS: return "WIS";
            default: return "Unknown";
        }
    }

} // namespace Domain
} // namespace LB