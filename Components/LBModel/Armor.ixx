// Armor.ixx - Armor entity partition
export module LB.Entities:Armor;

import LB.Common;
import LB.Enums;
import LB.ValueObjects;
import :Entity;

export namespace LB::Domain {

    /// <summary>
    /// Armor entity for defense calculation and damage resistance
    /// Represents wearable protective equipment
    /// </summary>
    export class Armor : public Entity {
    private:
        ArmorCategory category_;
        int32_t armorClassBonus_;    // AC bonus from armor
        int32_t maxDexterityBonus_;  // Heavy armor limits dex bonus to AC
        int32_t damageReduction_;    // Flat damage reduction per hit
        int32_t movementPenalty_;    // Speed reduction in feet
        int32_t requiredStrength_;   // Minimum strength to wear without penalty
        float weight_;
        
        // Damage resistances (percentage reduction per type)
        std::unordered_map<DamageType, float> resistances_;
        
    public:
        /// <summary>
        /// Create armor with basic properties
        /// </summary>
        Armor(Id id,
              String name,
              ArmorCategory category,
              int32_t armorClassBonus,
              int32_t maxDexterityBonus = 10)
            : Entity(id, std::move(name))
            , category_(category)
            , armorClassBonus_(armorClassBonus)
            , maxDexterityBonus_(maxDexterityBonus)
            , damageReduction_(0)
            , movementPenalty_(0)
            , requiredStrength_(0)
            , weight_(0.0f)
        {}

        /// <summary>
        /// Create armor with all properties
        /// </summary>
        Armor(Id id,
              String name,
              ArmorCategory category,
              int32_t armorClassBonus,
              int32_t maxDexterityBonus,
              int32_t damageReduction,
              int32_t movementPenalty,
              int32_t requiredStrength,
              float weight)
            : Entity(id, std::move(name))
            , category_(category)
            , armorClassBonus_(armorClassBonus)
            , maxDexterityBonus_(maxDexterityBonus)
            , damageReduction_(damageReduction)
            , movementPenalty_(movementPenalty)
            , requiredStrength_(requiredStrength)
            , weight_(weight)
        {}

        // Basic Properties
        [[nodiscard]] ArmorCategory GetCategory() const noexcept { return category_; }
        [[nodiscard]] int32_t GetArmorClassBonus() const noexcept { return armorClassBonus_; }
        [[nodiscard]] int32_t GetMaxDexterityBonus() const noexcept { return maxDexterityBonus_; }
        [[nodiscard]] int32_t GetDamageReduction() const noexcept { return damageReduction_; }
        [[nodiscard]] int32_t GetMovementPenalty() const noexcept { return movementPenalty_; }
        [[nodiscard]] int32_t GetRequiredStrength() const noexcept { return requiredStrength_; }
        [[nodiscard]] float GetWeight() const noexcept { return weight_; }

        // Armor Type Checks
        [[nodiscard]] bool IsHeavyArmor() const noexcept {
            return category_ == ArmorCategory::Heavy;
        }

        [[nodiscard]] bool IsMediumArmor() const noexcept {
            return category_ == ArmorCategory::Medium;
        }

        [[nodiscard]] bool IsLightArmor() const noexcept {
            return category_ == ArmorCategory::Light;
        }

        [[nodiscard]] bool IsShield() const noexcept {
            return category_ == ArmorCategory::Shield;
        }

        // AC Calculation
        [[nodiscard]] int32_t GetEffectiveAC(int32_t dexterityModifier) const noexcept {
            // Apply max dex bonus limit
            int32_t effectiveDexMod = std::min(dexterityModifier, maxDexterityBonus_);
            return armorClassBonus_ + effectiveDexMod;
        }

        // Damage Resistance
        void SetResistance(DamageType type, float percentage) {
            if (percentage >= 0.0f && percentage <= 1.0f) {
                resistances_[type] = percentage;
            }
        }

        [[nodiscard]] float GetDamageResistance(DamageType type) const noexcept {
            auto it = resistances_.find(type);
            return it != resistances_.end() ? it->second : 0.0f;
        }

        [[nodiscard]] bool HasResistance(DamageType type) const noexcept {
            return resistances_.contains(type) && resistances_.at(type) > 0.0f;
        }

        [[nodiscard]] int32_t ApplyDamageReduction(int32_t incomingDamage, DamageType type) const {
            // Apply percentage resistance first
            float resistance = GetDamageResistance(type);
            int32_t damageAfterResistance = static_cast<int32_t>(incomingDamage * (1.0f - resistance));
            
            // Apply flat damage reduction
            int32_t finalDamage = std::max(0, damageAfterResistance - damageReduction_);
            
            return finalDamage;
        }

        // Proficiency Check
        [[nodiscard]] bool CanBeWornBy(int32_t strengthScore) const noexcept {
            return strengthScore >= requiredStrength_;
        }

        [[nodiscard]] bool HasMovementPenalty() const noexcept {
            return movementPenalty_ > 0;
        }

        // Setters
        void SetArmorClassBonus(int32_t bonus) {
            if (bonus >= 0) {
                armorClassBonus_ = bonus;
            }
        }

        void SetDamageReduction(int32_t reduction) {
            if (reduction >= 0) {
                damageReduction_ = reduction;
            }
        }

        void SetMovementPenalty(int32_t penalty) {
            if (penalty >= 0) {
                movementPenalty_ = penalty;
            }
        }

        void SetRequiredStrength(int32_t strength) {
            if (strength >= 0) {
                requiredStrength_ = strength;
            }
        }

        void SetWeight(float weight) {
            if (weight >= 0.0f) {
                weight_ = weight;
            }
        }

        // Special Armor Properties
        [[nodiscard]] bool PreventsSpellcasting() const noexcept {
            // Heavy armor typically interferes with arcane spellcasting
            return category_ == ArmorCategory::Heavy;
        }

        [[nodiscard]] bool ImposesStealthPenalty() const noexcept {
            // Medium and heavy armor make stealth difficult
            return category_ == ArmorCategory::Medium || category_ == ArmorCategory::Heavy;
        }
    };

} // namespace LB::Domain
