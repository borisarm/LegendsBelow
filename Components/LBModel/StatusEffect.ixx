// StatusEffect.ixx - Status effect entity partition
export module LB.Entities:StatusEffect;

import LB.Common;
import LB.Enums;
import LB.ValueObjects;
import :Entity;

export namespace LB::Domain {

    /// <summary>
    /// Status effect for temporary character modifications
    /// Represents buffs, debuffs, damage-over-time, and control effects
    /// </summary>
    export class StatusEffect : public Entity {
    private:
        CharacterId targetId_;
        SpellCategory category_;     // Buff, Debuff, Control
        int32_t duration_;           // Remaining duration in turns
        int32_t maxDuration_;        // Original duration for reference
        bool isPermanent_;
        bool isStacking_;            // Can multiple instances stack?
        
        // Attribute modifiers
        std::optional<Attributes> attributeModifiers_;
        
        // Damage/Healing over time
        std::optional<int32_t> damagePerTurn_;
        std::optional<DamageType> damageType_;
        std::optional<int32_t> healPerTurn_;
        
        // Combat modifiers
        std::optional<int32_t> armorClassModifier_;
        std::optional<int32_t> attackBonusModifier_;
        std::optional<int32_t> damageModifier_;
        std::optional<float> movementSpeedModifier_;  // Multiplier (1.0 = normal, 0.5 = half speed)
        
        // Control effects
        bool isStunned_;
        bool isParalyzed_;
        bool isSlowed_;
        bool isSilenced_;    // Cannot cast spells
        bool isBlinded_;     // Attack penalties
        
        // Resistance modifiers
        std::unordered_map<DamageType, float> resistanceModifiers_;
        
    public:
        /// <summary>
        /// Create status effect with basic properties
        /// </summary>
        StatusEffect(Id id,
                     String name,
                     CharacterId targetId,
                     SpellCategory category,
                     int32_t duration)
            : Entity(id, std::move(name))
            , targetId_(targetId)
            , category_(category)
            , duration_(duration)
            , maxDuration_(duration)
            , isPermanent_(duration == 0)
            , isStacking_(false)
            , isStunned_(false)
            , isParalyzed_(false)
            , isSlowed_(false)
            , isSilenced_(false)
            , isBlinded_(false)
        {}

        // Core Properties
        [[nodiscard]] CharacterId GetTargetId() const noexcept { return targetId_; }
        [[nodiscard]] SpellCategory GetCategory() const noexcept { return category_; }
        [[nodiscard]] int32_t GetDuration() const noexcept { return duration_; }
        [[nodiscard]] int32_t GetMaxDuration() const noexcept { return maxDuration_; }
        [[nodiscard]] bool IsPermanent() const noexcept { return isPermanent_; }
        [[nodiscard]] bool IsStacking() const noexcept { return isStacking_; }

        // Status Checks
        [[nodiscard]] bool IsExpired() const noexcept {
            return !isPermanent_ && duration_ <= 0;
        }

        [[nodiscard]] bool IsActive() const noexcept {
            return isPermanent_ || duration_ > 0;
        }

        [[nodiscard]] bool IsBuff() const noexcept {
            return category_ == SpellCategory::Buff;
        }

        [[nodiscard]] bool IsDebuff() const noexcept {
            return category_ == SpellCategory::Debuff;
        }

        [[nodiscard]] bool IsControl() const noexcept {
            return category_ == SpellCategory::Control;
        }

        // Control Effect Checks
        [[nodiscard]] bool IsStunned() const noexcept { return isStunned_; }
        [[nodiscard]] bool IsParalyzed() const noexcept { return isParalyzed_; }
        [[nodiscard]] bool IsSlowed() const noexcept { return isSlowed_; }
        [[nodiscard]] bool IsSilenced() const noexcept { return isSilenced_; }
        [[nodiscard]] bool IsBlinded() const noexcept { return isBlinded_; }

        [[nodiscard]] bool PreventsActions() const noexcept {
            return isStunned_ || isParalyzed_;
        }

        [[nodiscard]] bool PreventsSpellcasting() const noexcept {
            return isStunned_ || isParalyzed_ || isSilenced_;
        }

        // Duration Management
        void Tick() {
            if (!isPermanent_ && duration_ > 0) {
                duration_--;
            }
        }

        void ExtendDuration(int32_t additionalTurns) {
            if (!isPermanent_ && additionalTurns > 0) {
                duration_ += additionalTurns;
                maxDuration_ = std::max(maxDuration_, duration_);
            }
        }

        void SetPermanent(bool permanent) {
            isPermanent_ = permanent;
            if (permanent) {
                duration_ = 0;
            }
        }

        void SetStacking(bool stacking) {
            isStacking_ = stacking;
        }

        // Attribute Modifiers
        void SetAttributeModifiers(const Attributes& modifiers) {
            attributeModifiers_ = modifiers;
        }

        [[nodiscard]] std::optional<Attributes> GetAttributeModifiers() const noexcept {
            return attributeModifiers_;
        }

        [[nodiscard]] bool HasAttributeModifiers() const noexcept {
            return attributeModifiers_.has_value();
        }

        // Damage Over Time
        void SetDamageOverTime(int32_t damagePerTurn, DamageType type) {
            damagePerTurn_ = damagePerTurn;
            damageType_ = type;
        }

        [[nodiscard]] std::optional<int32_t> GetDamagePerTurn() const noexcept {
            return damagePerTurn_;
        }

        [[nodiscard]] std::optional<DamageType> GetDamageType() const noexcept {
            return damageType_;
        }

        [[nodiscard]] bool DealsDamageOverTime() const noexcept {
            return damagePerTurn_.has_value() && damagePerTurn_.value() > 0;
        }

        // Healing Over Time
        void SetHealOverTime(int32_t healPerTurn) {
            healPerTurn_ = healPerTurn;
        }

        [[nodiscard]] std::optional<int32_t> GetHealPerTurn() const noexcept {
            return healPerTurn_;
        }

        [[nodiscard]] bool HealsOverTime() const noexcept {
            return healPerTurn_.has_value() && healPerTurn_.value() > 0;
        }

        // Combat Modifiers
        void SetArmorClassModifier(int32_t modifier) {
            armorClassModifier_ = modifier;
        }

        void SetAttackBonusModifier(int32_t modifier) {
            attackBonusModifier_ = modifier;
        }

        void SetDamageModifier(int32_t modifier) {
            damageModifier_ = modifier;
        }

        void SetMovementSpeedModifier(float multiplier) {
            if (multiplier >= 0.0f && multiplier <= 2.0f) {
                movementSpeedModifier_ = multiplier;
            }
        }

        [[nodiscard]] std::optional<int32_t> GetArmorClassModifier() const noexcept {
            return armorClassModifier_;
        }

        [[nodiscard]] std::optional<int32_t> GetAttackBonusModifier() const noexcept {
            return attackBonusModifier_;
        }

        [[nodiscard]] std::optional<int32_t> GetDamageModifier() const noexcept {
            return damageModifier_;
        }

        [[nodiscard]] std::optional<float> GetMovementSpeedModifier() const noexcept {
            return movementSpeedModifier_;
        }

        // Control Effects
        void SetStunned(bool stunned) {
            isStunned_ = stunned;
        }

        void SetParalyzed(bool paralyzed) {
            isParalyzed_ = paralyzed;
        }

        void SetSlowed(bool slowed) {
            isSlowed_ = slowed;
        }

        void SetSilenced(bool silenced) {
            isSilenced_ = silenced;
        }

        void SetBlinded(bool blinded) {
            isBlinded_ = blinded;
        }

        // Resistance Modifiers
        void SetResistanceModifier(DamageType type, float modifier) {
            if (modifier >= -1.0f && modifier <= 1.0f) {
                resistanceModifiers_[type] = modifier;
            }
        }

        [[nodiscard]] float GetResistanceModifier(DamageType type) const noexcept {
            auto it = resistanceModifiers_.find(type);
            return it != resistanceModifiers_.end() ? it->second : 0.0f;
        }

        [[nodiscard]] bool HasResistanceModifier(DamageType type) const noexcept {
            return resistanceModifiers_.contains(type);
        }

        // Progress Indicator
        [[nodiscard]] float GetRemainingPercentage() const noexcept {
            if (isPermanent_ || maxDuration_ == 0) {
                return 1.0f;
            }
            return static_cast<float>(duration_) / maxDuration_;
        }

        [[nodiscard]] int32_t GetElapsedTurns() const noexcept {
            if (isPermanent_) {
                return 0;
            }
            return maxDuration_ - duration_;
        }
    };

} // namespace LB::Domain
