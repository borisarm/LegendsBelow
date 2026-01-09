#pragma once
#include <cstdint>
#include <algorithm>

namespace LB {
namespace Domain {

    /// <summary>
    /// D&D-style character attributes (immutable value object)
    /// STR, DEX, CON, INT, WIS, CHA with validation and modifiers
    /// </summary>
    class Attributes {
    public:
        static constexpr int32_t MinValue = 3;
        static constexpr int32_t MaxValue = 18;
        static constexpr int32_t DefaultValue = 10;

        /// <summary>
        /// Construct attributes with individual values (validated)
        /// </summary>
        Attributes(int32_t strength = DefaultValue, 
                  int32_t dexterity = DefaultValue, 
                  int32_t constitution = DefaultValue, 
                  int32_t intelligence = DefaultValue, 
                  int32_t wisdom = DefaultValue, 
                  int32_t charisma = DefaultValue)
            : strength_(Clamp(strength))
            , dexterity_(Clamp(dexterity))
            , constitution_(Clamp(constitution))
            , intelligence_(Clamp(intelligence))
            , wisdom_(Clamp(wisdom))
            , charisma_(Clamp(charisma)) 
        {
        }

        // Accessors
        [[nodiscard]] int32_t Strength() const noexcept { return strength_; }
        [[nodiscard]] int32_t Dexterity() const noexcept { return dexterity_; }
        [[nodiscard]] int32_t Constitution() const noexcept { return constitution_; }
        [[nodiscard]] int32_t Intelligence() const noexcept { return intelligence_; }
        [[nodiscard]] int32_t Wisdom() const noexcept { return wisdom_; }
        [[nodiscard]] int32_t Charisma() const noexcept { return charisma_; }

        // D&D-style modifiers (attribute - 10) / 2
        [[nodiscard]] int32_t StrengthModifier() const noexcept { return (strength_ - 10) / 2; }
        [[nodiscard]] int32_t DexterityModifier() const noexcept { return (dexterity_ - 10) / 2; }
        [[nodiscard]] int32_t ConstitutionModifier() const noexcept { return (constitution_ - 10) / 2; }
        [[nodiscard]] int32_t IntelligenceModifier() const noexcept { return (intelligence_ - 10) / 2; }
        [[nodiscard]] int32_t WisdomModifier() const noexcept { return (wisdom_ - 10) / 2; }
        [[nodiscard]] int32_t CharismaModifier() const noexcept { return (charisma_ - 10) / 2; }

        // Comparison operators for value semantics
        [[nodiscard]] bool operator==(const Attributes& other) const noexcept = default;
        [[nodiscard]] bool operator!=(const Attributes& other) const noexcept = default;

        /// <summary>
        /// Apply delta to existing attributes (for backgrounds, items, etc.)
        /// Returns new Attributes instance (immutable)
        /// </summary>
        [[nodiscard]] Attributes ApplyDelta(int32_t strDelta = 0, 
                                           int32_t dexDelta = 0, 
                                           int32_t conDelta = 0, 
                                           int32_t intDelta = 0, 
                                           int32_t wisDelta = 0, 
                                           int32_t chaDelta = 0) const {
            return Attributes(
                strength_ + strDelta,
                dexterity_ + dexDelta,
                constitution_ + conDelta,
                intelligence_ + intDelta,
                wisdom_ + wisDelta,
                charisma_ + chaDelta
            );
        }

    private:
        static constexpr int32_t Clamp(int32_t value) noexcept {
            return std::clamp(value, MinValue, MaxValue);
        }

        int32_t strength_;
        int32_t dexterity_;
        int32_t constitution_;
        int32_t intelligence_;
        int32_t wisdom_;
        int32_t charisma_;
    };

} // namespace Domain
} // namespace LB