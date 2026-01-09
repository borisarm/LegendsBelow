#pragma once
#include <optional>
#include <algorithm>
#include "../Common/Types.h"
#include "../Enums/DamageType.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Status effect types for buffs, debuffs, and conditions
    /// </summary>
    enum class StatusEffectType : uint32_t {
        None = 0,
        
        // Buffs (positive effects)
        Strength = 1,       // +STR modifier
        Dexterity = 2,      // +DEX modifier  
        Constitution = 3,   // +CON modifier
        Intelligence = 4,   // +INT modifier
        Wisdom = 5,         // +WIS modifier
        Charisma = 6,       // +CHA modifier
        ArmorClass = 7,     // +AC bonus
        AttackBonus = 8,    // +attack roll bonus
        DamageBonus = 9,    // +damage bonus
        
        // Debuffs (negative effects)  
        Poison = 10,        // HP loss over time
        Slow = 11,          // Reduced movement/action speed
        Blind = 12,         // Reduced hit chance, limited vision
        
        // Crowd Control
        Stun = 13,          // Cannot act or move, vulnerable to crits
        Root = 14,          // Cannot move, can still attack/cast
        Silence = 15,       // No spells or magical abilities
        Fear = 16,          // Forced movement away from source
        
        // Damage over time
        Burning = 17,       // Fire damage over time
        Freezing = 18,      // Cold damage over time, may slow
        Bleeding = 19       // Physical damage over time
    };

    /// <summary>
    /// Immutable status effect with type, magnitude, and duration
    /// </summary>
    class StatusEffect {
    public:
        /// <summary>
        /// Create a status effect
        /// </summary>
        /// <param name="type">Type of status effect</param>
        /// <param name="magnitude">Strength of the effect (positive or negative)</param>
        /// <param name="durationSeconds">How long the effect lasts</param>
        constexpr StatusEffect(StatusEffectType type, 
                             int32_t magnitude = 1, 
                             float durationSeconds = 10.0f) noexcept
            : type_(type)
            , magnitude_(magnitude)
            , duration_(durationSeconds)
        {
        }

        // Accessors
        [[nodiscard]] constexpr StatusEffectType GetType() const noexcept { return type_; }
        [[nodiscard]] constexpr int32_t GetMagnitude() const noexcept { return magnitude_; }
        [[nodiscard]] constexpr float GetDuration() const noexcept { return duration_; }

        // Check effect properties
        [[nodiscard]] constexpr bool IsBuff() const noexcept {
            return type_ >= StatusEffectType::Strength && type_ <= StatusEffectType::DamageBonus;
        }

        [[nodiscard]] constexpr bool IsDebuff() const noexcept {
            return type_ >= StatusEffectType::Poison && type_ <= StatusEffectType::Bleeding;
        }

        [[nodiscard]] constexpr bool IsCrowdControl() const noexcept {
            return type_ >= StatusEffectType::Stun && type_ <= StatusEffectType::Fear;
        }

        [[nodiscard]] constexpr bool IsDamageOverTime() const noexcept {
            return type_ == StatusEffectType::Poison || 
                   type_ == StatusEffectType::Burning || 
                   type_ == StatusEffectType::Freezing || 
                   type_ == StatusEffectType::Bleeding;
        }

        /// <summary>
        /// Get associated damage type for damage-over-time effects
        /// </summary>
        [[nodiscard]] constexpr std::optional<DamageType> GetDamageType() const noexcept {
            switch (type_) {
                case StatusEffectType::Poison: return DamageType::Acid;
                case StatusEffectType::Burning: return DamageType::Fire;
                case StatusEffectType::Freezing: return DamageType::Cold;
                case StatusEffectType::Bleeding: return DamageType::Slashing;
                default: return std::nullopt;
            }
        }

        // Comparison
        [[nodiscard]] constexpr bool operator==(const StatusEffect& other) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const StatusEffect& other) const noexcept = default;

        /// <summary>
        /// Create a new status effect with reduced duration (for ticking)
        /// </summary>
        [[nodiscard]] constexpr StatusEffect WithReducedDuration(float deltaSeconds) const noexcept {
            return StatusEffect(type_, magnitude_, std::max(0.0f, duration_ - deltaSeconds));
        }

        /// <summary>
        /// Check if effect has expired
        /// </summary>
        [[nodiscard]] constexpr bool IsExpired() const noexcept {
            return duration_ <= 0.0f;
        }

        /// <summary>
        /// Convert to string for debugging
        /// </summary>
        [[nodiscard]] std::string ToString() const {
            std::string result = ToStringType(type_);
            if (magnitude_ != 1) {
                result += " (" + std::to_string(magnitude_) + ")";
            }
            result += " [" + std::to_string(duration_) + "s]";
            return result;
        }

    private:
        StatusEffectType type_;
        int32_t magnitude_;
        float duration_;

        static constexpr const char* ToStringType(StatusEffectType type) noexcept {
            switch (type) {
                case StatusEffectType::None: return "None";
                case StatusEffectType::Strength: return "Strength";
                case StatusEffectType::Dexterity: return "Dexterity"; 
                case StatusEffectType::Constitution: return "Constitution";
                case StatusEffectType::Intelligence: return "Intelligence";
                case StatusEffectType::Wisdom: return "Wisdom";
                case StatusEffectType::Charisma: return "Charisma";
                case StatusEffectType::ArmorClass: return "ArmorClass";
                case StatusEffectType::AttackBonus: return "AttackBonus";
                case StatusEffectType::DamageBonus: return "DamageBonus";
                case StatusEffectType::Poison: return "Poison";
                case StatusEffectType::Slow: return "Slow";
                case StatusEffectType::Blind: return "Blind";
                case StatusEffectType::Stun: return "Stun";
                case StatusEffectType::Root: return "Root";
                case StatusEffectType::Silence: return "Silence";
                case StatusEffectType::Fear: return "Fear";
                case StatusEffectType::Burning: return "Burning";
                case StatusEffectType::Freezing: return "Freezing";
                case StatusEffectType::Bleeding: return "Bleeding";
                default: return "Unknown";
            }
        }
    };

} // namespace Domain
} // namespace LB