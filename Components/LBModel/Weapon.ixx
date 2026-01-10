// Weapon.ixx - Weapon entity partition
export module LB.Entities:Weapon;

import LB.Common;
import LB.Enums;
import LB.ValueObjects;
import :Entity;

export namespace LB::Domain {

    /// <summary>
    /// Weapon entity for combat damage calculation
    /// Represents melee and ranged weapons with damage properties
    /// </summary>
    export class Weapon : public Entity {
    private:
        WeaponType type_;
        DamageType damageType_;
        Dice damageDice_;
        int32_t attackBonus_;
        int32_t criticalRange_;      // 20 for normal, 19-20 for improved, etc.
        int32_t criticalMultiplier_;  // Usually 2x, some weapons 3x or 4x
        int32_t range_;              // 0 for melee, feet for ranged
        bool isTwoHanded_;
        int32_t requiredStrength_;
        float weight_;

    public:
        /// <summary>
        /// Create weapon with basic properties
        /// </summary>
        Weapon(Id id,
               String name,
               WeaponType type,
               DamageType damageType,
               const Dice& damageDice,
               int32_t attackBonus = 0,
               int32_t range = 0,
               bool isTwoHanded = false)
            : Entity(id, std::move(name))
            , type_(type)
            , damageType_(damageType)
            , damageDice_(damageDice)
            , attackBonus_(attackBonus)
            , criticalRange_(20)         // Default: critical on natural 20
            , criticalMultiplier_(2)     // Default: 2x damage on crit
            , range_(range)
            , isTwoHanded_(isTwoHanded)
            , requiredStrength_(0)
            , weight_(0.0f)
        {}

        /// <summary>
        /// Create weapon with all properties
        /// </summary>
        Weapon(Id id,
               String name,
               WeaponType type,
               DamageType damageType,
               const Dice& damageDice,
               int32_t attackBonus,
               int32_t criticalRange,
               int32_t criticalMultiplier,
               int32_t range,
               bool isTwoHanded,
               int32_t requiredStrength,
               float weight)
            : Entity(id, std::move(name))
            , type_(type)
            , damageType_(damageType)
            , damageDice_(damageDice)
            , attackBonus_(attackBonus)
            , criticalRange_(criticalRange)
            , criticalMultiplier_(criticalMultiplier)
            , range_(range)
            , isTwoHanded_(isTwoHanded)
            , requiredStrength_(requiredStrength)
            , weight_(weight)
        {}

        // Basic Properties
        [[nodiscard]] WeaponType GetWeaponType() const noexcept { return type_; }
        [[nodiscard]] DamageType GetDamageType() const noexcept { return damageType_; }
        [[nodiscard]] const Dice& GetDamageDice() const noexcept { return damageDice_; }
        [[nodiscard]] int32_t GetAttackBonus() const noexcept { return attackBonus_; }
        [[nodiscard]] int32_t GetCriticalRange() const noexcept { return criticalRange_; }
        [[nodiscard]] int32_t GetCriticalMultiplier() const noexcept { return criticalMultiplier_; }
        [[nodiscard]] int32_t GetRange() const noexcept { return range_; }
        [[nodiscard]] bool IsTwoHanded() const noexcept { return isTwoHanded_; }
        [[nodiscard]] int32_t GetRequiredStrength() const noexcept { return requiredStrength_; }
        [[nodiscard]] float GetWeight() const noexcept { return weight_; }

        // Weapon Type Checks
        [[nodiscard]] bool IsRangedWeapon() const noexcept {
            return range_ > 0;
        }

        [[nodiscard]] bool IsMeleeWeapon() const noexcept {
            return range_ == 0;
        }

        [[nodiscard]] bool IsFinesse() const noexcept {
            // Light weapons that can use DEX for attack rolls
            return type_ == WeaponType::Dagger || type_ == WeaponType::Wand;
        }

        // Combat Calculations
        [[nodiscard]] int32_t GetMinimumDamage() const noexcept {
            return damageDice_.MinValue();
        }

        [[nodiscard]] int32_t GetMaximumDamage() const noexcept {
            return damageDice_.MaxValue();
        }

        [[nodiscard]] double GetAverageDamage() const noexcept {
            return damageDice_.AverageValue();
        }

        [[nodiscard]] bool IsCriticalThreat(int32_t rollValue) const noexcept {
            return rollValue >= criticalRange_;
        }

        // Setters for weapon properties
        void SetAttackBonus(int32_t bonus) {
            attackBonus_ = bonus;
        }

        void SetCriticalRange(int32_t range) {
            if (range > 0 && range <= 20) {
                criticalRange_ = range;
            }
        }

        void SetCriticalMultiplier(int32_t multiplier) {
            if (multiplier >= 2 && multiplier <= 4) {
                criticalMultiplier_ = multiplier;
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

        // Weapon Proficiency Check
        [[nodiscard]] bool CanBeUsedBy(int32_t strengthScore) const noexcept {
            return strengthScore >= requiredStrength_;
        }

        // Damage Type Categorization
        [[nodiscard]] bool IsPhysicalDamage() const noexcept {
            return damageType_ == DamageType::Slashing ||
                   damageType_ == DamageType::Piercing ||
                   damageType_ == DamageType::Bludgeoning;
        }

        [[nodiscard]] bool IsMagicalDamage() const noexcept {
            return damageType_ == DamageType::Fire ||
                   damageType_ == DamageType::Cold ||
                   damageType_ == DamageType::Lightning ||
                   damageType_ == DamageType::Arcane ||
                   damageType_ == DamageType::Holy ||
                   damageType_ == DamageType::Shadow;
        }
    };

} // namespace LB::Domain
