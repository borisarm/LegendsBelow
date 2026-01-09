#pragma once
#include <vector>
#include <algorithm>
#include <limits>
#include "../Common/Types.h"
#include "Entity.h"
#include "../Enums/CharacterClass.h"
#include "../ValueObjects/Attributes.h"
#include "../ValueObjects/Position.h"
#include "../ValueObjects/StatusEffect.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Core character entity representing players and NPCs
    /// Contains attributes, class, status, and derived stats
    /// </summary>
    class Character : public Entity {
    public:
        /// <summary>
        /// Create character with specified properties
        /// </summary>
        Character(Id id, 
                 String name, 
                 CharacterClass characterClass, 
                 const Attributes& attributes,
                 int32_t level = 1)
            : Entity(id)
            , name_(std::move(name))
            , characterClass_(characterClass)
            , attributes_(attributes)
            , level_(std::clamp(level, 1, MaxLevel))
            , position_(Position::Origin())
            , currentHitPoints_(CalculateMaxHitPoints())
            , currentManaPoints_(CalculateMaxManaPoints())
        {
        }

        // Character limits
        static constexpr int32_t MaxLevel = 20;
        static constexpr int32_t MaxHitPoints = 500;
        static constexpr int32_t MaxManaPoints = 300;

        // Accessors
        [[nodiscard]] const String& GetName() const noexcept { return name_; }
        [[nodiscard]] CharacterClass GetCharacterClass() const noexcept { return characterClass_; }
        [[nodiscard]] const Attributes& GetAttributes() const noexcept { return attributes_; }
        [[nodiscard]] int32_t GetLevel() const noexcept { return level_; }
        [[nodiscard]] const Position& GetPosition() const noexcept { return position_; }
        [[nodiscard]] int32_t GetCurrentHitPoints() const noexcept { return currentHitPoints_; }
        [[nodiscard]] int32_t GetCurrentManaPoints() const noexcept { return currentManaPoints_; }

        // Derived stats based on D&D rules
        [[nodiscard]] int32_t CalculateMaxHitPoints() const noexcept {
            int32_t baseHP = GetBaseHitPointsForClass();
            int32_t conBonus = attributes_.ConstitutionModifier() * level_;
            return std::clamp(baseHP + conBonus, 1, MaxHitPoints);
        }

        [[nodiscard]] int32_t CalculateMaxManaPoints() const noexcept {
            int32_t baseMP = GetBaseManaPointsForClass();
            int32_t mentalBonus = GetMentalModifierForClass() * level_;
            return std::clamp(baseMP + mentalBonus, 0, MaxManaPoints);
        }

        [[nodiscard]] int32_t CalculateArmorClass() const noexcept {
            return 10 + attributes_.DexterityModifier(); // Base AC + DEX, equipment would add more
        }

        [[nodiscard]] int32_t CalculateAttackBonus() const noexcept {
            int32_t baseAttack = level_; // Simple progression for now
            int32_t attributeBonus = GetAttackModifierForClass();
            return baseAttack + attributeBonus;
        }

        // Status checks
        [[nodiscard]] bool IsAlive() const noexcept { return currentHitPoints_ > 0; }
        [[nodiscard]] bool IsDefeated() const noexcept { return currentHitPoints_ <= 0; }
        [[nodiscard]] bool HasMana() const noexcept { return currentManaPoints_ > 0; }

        // Position and movement
        void SetPosition(const Position& newPosition) { position_ = newPosition; }
        void Move(const Position& delta) { position_ = position_ + delta; }

        // Health and mana management
        void TakeDamage(int32_t damage) {
            currentHitPoints_ = std::max(0, currentHitPoints_ - std::max(0, damage));
        }

        void Heal(int32_t healAmount) {
            int32_t maxHP = CalculateMaxHitPoints();
            currentHitPoints_ = std::min(maxHP, currentHitPoints_ + std::max(0, healAmount));
        }

        void SpendMana(int32_t manaAmount) {
            currentManaPoints_ = std::max(0, currentManaPoints_ - std::max(0, manaAmount));
        }

        void RestoreMana(int32_t manaAmount) {
            int32_t maxMP = CalculateMaxManaPoints();
            currentManaPoints_ = std::min(maxMP, currentManaPoints_ + std::max(0, manaAmount));
        }

        // Level advancement
        void LevelUp() {
            if (level_ < MaxLevel) {
                level_++;
                // Refresh to max on level up
                currentHitPoints_ = CalculateMaxHitPoints();
                currentManaPoints_ = CalculateMaxManaPoints();
            }
        }

        // Status effects (simplified for now)
        void AddStatusEffect(const StatusEffect& effect) {
            statusEffects_.push_back(effect);
        }

        void ClearExpiredStatusEffects() {
            statusEffects_.erase(
                std::remove_if(statusEffects_.begin(), statusEffects_.end(),
                    [](const StatusEffect& effect) { return effect.IsExpired(); }),
                statusEffects_.end());
        }

        [[nodiscard]] const std::vector<StatusEffect>& GetStatusEffects() const noexcept {
            return statusEffects_;
        }

    private:
        String name_;
        CharacterClass characterClass_;
        Attributes attributes_;
        int32_t level_;
        Position position_;
        int32_t currentHitPoints_;
        int32_t currentManaPoints_;
        std::vector<StatusEffect> statusEffects_;

        // Class-specific calculations based on game design
        [[nodiscard]] int32_t GetBaseHitPointsForClass() const noexcept {
            switch (characterClass_) {
                case CharacterClass::Warrior: return 10 + level_ * 8;  // High HP
                case CharacterClass::Rogue: return 8 + level_ * 6;     // Medium HP
                case CharacterClass::Mage: return 6 + level_ * 4;      // Low HP
                case CharacterClass::Cleric: return 8 + level_ * 6;    // Medium HP
                default: return 6 + level_ * 4;
            }
        }

        [[nodiscard]] int32_t GetBaseManaPointsForClass() const noexcept {
            switch (characterClass_) {
                case CharacterClass::Warrior: return 0;                // No mana
                case CharacterClass::Rogue: return 4 + level_ * 2;     // Low mana
                case CharacterClass::Mage: return 10 + level_ * 6;     // High mana
                case CharacterClass::Cleric: return 8 + level_ * 4;    // Medium mana
                default: return 0;
            }
        }

        [[nodiscard]] int32_t GetMentalModifierForClass() const noexcept {
            switch (characterClass_) {
                case CharacterClass::Warrior: return 0;
                case CharacterClass::Rogue: return attributes_.IntelligenceModifier();
                case CharacterClass::Mage: return attributes_.IntelligenceModifier();
                case CharacterClass::Cleric: return attributes_.WisdomModifier();
                default: return 0;
            }
        }

        [[nodiscard]] int32_t GetAttackModifierForClass() const noexcept {
            switch (characterClass_) {
                case CharacterClass::Warrior: return attributes_.StrengthModifier();
                case CharacterClass::Rogue: return attributes_.DexterityModifier();
                case CharacterClass::Mage: return attributes_.IntelligenceModifier();
                case CharacterClass::Cleric: return attributes_.WisdomModifier();
                default: return 0;
            }
        }
    };

} // namespace Domain
} // namespace LB