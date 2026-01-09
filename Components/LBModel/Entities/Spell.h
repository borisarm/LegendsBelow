#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "Entity.h"
#include "../Enums/SpellSchool.h"
#include "../Enums/SpellCategory.h"
#include "../Enums/CharacterClass.h"
#include "../Enums/DamageType.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Magic spell entity with casting properties and effects
    /// Based on spells.json data structure
    /// </summary>
    class Spell : public Entity {
    public:
        /// <summary>
        /// Create spell with complete properties
        /// </summary>
        Spell(Id id,
              String name,
              SpellSchool school,
              int32_t level,
              std::vector<SpellCategory> categories,
              std::optional<CharacterClass> requiredClass,
              int32_t requiredLevel,
              int32_t manaCost,
              float castTimeSeconds,
              float cooldownSeconds,
              float range = 0.0f,
              float radius = 0.0f)
            : Entity(id)
            , name_(std::move(name))
            , school_(school)
            , level_(level)
            , categories_(std::move(categories))
            , requiredClass_(requiredClass)
            , requiredLevel_(requiredLevel)
            , manaCost_(manaCost)
            , castTimeSeconds_(castTimeSeconds)
            , cooldownSeconds_(cooldownSeconds)
            , range_(range)
            , radius_(radius)
        {
        }

        // Basic properties
        [[nodiscard]] const String& GetName() const noexcept { return name_; }
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

        // Damage properties (for damage spells)
        [[nodiscard]] int32_t GetBaseDamage() const noexcept { return baseDamage_; }
        [[nodiscard]] float GetPowerCoefficient() const noexcept { return powerCoefficient_; }
        [[nodiscard]] std::optional<DamageType> GetDamageType() const noexcept { return damageType_; }

        // Healing properties (for heal spells)
        [[nodiscard]] int32_t GetBaseHeal() const noexcept { return baseHeal_; }

        // Duration (for buffs/debuffs)
        [[nodiscard]] float GetDurationSeconds() const noexcept { return durationSeconds_; }

        // Special properties
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

    private:
        // Basic properties
        String name_;
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

        // Effect properties (optional, depends on spell type)
        int32_t baseDamage_ = 0;
        float powerCoefficient_ = 0.0f;
        std::optional<DamageType> damageType_;
        int32_t baseHeal_ = 0;
        float durationSeconds_ = 0.0f;
        float lifestealFraction_ = 0.0f;
        String notes_;
    };

    // Type aliases for spell IDs
    using SpellId = Id;

} // namespace Domain
} // namespace LB