export module LB.Entities:Spell;

import LB.Common;
import LB.Enums;
import LB.ValueObjects;
import :Entity;

export namespace LB::Domain {
    // Type aliases
    using SpellId = Id;
    using WeaponId = Id;
    using ArmorId = Id;
    using CharacterId = Id;
    using ItemId = Id;


    /// <summary>
    /// Spell entity representing magical abilities
    /// Contains all properties needed for spell mechanics and display
    /// </summary>
    export class Spell : public Entity {
    private:
        // Basic spell properties
        SpellSchool school_;
        int32_t level_;
        std::vector<SpellCategory> categories_;
        std::optional<CharacterClass> requiredClass_;
        int32_t requiredLevel_;

        // Casting properties
        int32_t manaCost_;
        float castTimeSeconds_;
        float cooldownSeconds_;
        float range_;
        float radius_;

        // Optional spell properties (for specific spell types)
        int32_t baseDamage_ = 0;
        float powerCoefficient_ = 0.0f;
        std::optional<DamageType> damageType_;
        int32_t baseHeal_ = 0;
        float durationSeconds_ = 0.0f;
        float lifestealFraction_ = 0.0f;
        String notes_;

    public:
        /// <summary>
        /// Create spell with all required properties
        /// </summary>
        Spell(Id id,
            String name,
            SpellSchool school,
            int32_t level,
            std::vector<SpellCategory> categories,
            CharacterClass requiredClass,
            int32_t requiredLevel,
            int32_t manaCost,
            float castTimeSeconds,
            float cooldownSeconds,
            float range,
            float radius)
            : Entity(id, std::move(name))
            , school_(school)
            , level_(level)
            , categories_(std::move(categories))
            , requiredClass_(requiredClass)
            , requiredLevel_(requiredLevel)
            , manaCost_(manaCost)
            , castTimeSeconds_(castTimeSeconds)
            , cooldownSeconds_(cooldownSeconds)
            , range_(range)
            , radius_(radius) {
        }

        // Basic properties
        [[nodiscard]] SpellSchool GetSchool() const noexcept { return school_; }
        [[nodiscard]] int32_t GetLevel() const noexcept { return level_; }
        [[nodiscard]] const std::vector<SpellCategory>& GetCategories() const noexcept { return categories_; }
        [[nodiscard]] std::optional<CharacterClass> GetRequiredClass() const noexcept { return requiredClass_; }
        [[nodiscard]] int32_t GetRequiredLevel() const noexcept { return requiredLevel_; }

        // Casting properties
        [[nodiscard]] int32_t GetManaCost() const noexcept { return manaCost_; }
        [[nodiscard]] float GetCastTimeSeconds() const noexcept { return castTimeSeconds_; }
        [[nodiscard]] float GetCooldownSeconds() const noexcept { return cooldownSeconds_; }
        [[nodiscard]] float GetRange() const noexcept { return range_; }
        [[nodiscard]] float GetRadius() const noexcept { return radius_; }

        // Optional properties
        [[nodiscard]] int32_t GetBaseDamage() const noexcept { return baseDamage_; }
        [[nodiscard]] float GetPowerCoefficient() const noexcept { return powerCoefficient_; }
        [[nodiscard]] std::optional<DamageType> GetDamageType() const noexcept { return damageType_; }
        [[nodiscard]] int32_t GetBaseHeal() const noexcept { return baseHeal_; }
        [[nodiscard]] float GetDurationSeconds() const noexcept { return durationSeconds_; }
        [[nodiscard]] float GetLifestealFraction() const noexcept { return lifestealFraction_; }
        [[nodiscard]] const String& GetNotes() const noexcept { return notes_; }

        // Category checks
        [[nodiscard]] bool IsDamageSpell() const noexcept {
            return std::find(categories_.begin(), categories_.end(), SpellCategory::Damage) != categories_.end();
        }

        [[nodiscard]] bool IsHealSpell() const noexcept {
            return std::find(categories_.begin(), categories_.end(), SpellCategory::Heal) != categories_.end();
        }

        [[nodiscard]] bool IsBuffSpell() const noexcept {
            return std::find(categories_.begin(), categories_.end(), SpellCategory::Buff) != categories_.end();
        }

        [[nodiscard]] bool IsDebuffSpell() const noexcept {
            return std::find(categories_.begin(), categories_.end(), SpellCategory::Debuff) != categories_.end();
        }

        [[nodiscard]] bool IsControlSpell() const noexcept {
            return std::find(categories_.begin(), categories_.end(), SpellCategory::Control) != categories_.end();
        }

        [[nodiscard]] bool IsUtilitySpell() const noexcept {
            return std::find(categories_.begin(), categories_.end(), SpellCategory::Utility) != categories_.end();
        }

        // Class restrictions
        [[nodiscard]] bool CanBeCastBy(CharacterClass characterClass) const noexcept {
            if (!requiredClass_.has_value()) {
                return true; // Generic spell, any class can cast
            }
            return requiredClass_.value() == characterClass;
        }

        // Setters for optional properties (used during construction from JSON)
        void SetDamageProperties(int32_t baseDamage, float powerCoefficient, DamageType damageType) {
            baseDamage_ = baseDamage;
            powerCoefficient_ = powerCoefficient;
            damageType_ = damageType;
        }

        void SetHealProperties(int32_t baseHeal) {
            baseHeal_ = baseHeal;
        }

        void SetDuration(float durationSeconds) {
            durationSeconds_ = durationSeconds;
        }

        void SetLifesteal(float lifestealFraction) {
            lifestealFraction_ = lifestealFraction;
        }

        void SetNotes(String notes) {
            notes_ = std::move(notes);
        }
    };
}