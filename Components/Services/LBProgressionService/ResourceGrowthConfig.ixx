// ResourceGrowthConfig.ixx - HP and MP growth configuration per class
export module LB.ProgressionService.ResourceGrowthConfig;

import std;

export namespace LB::Services::Progression {

    /// <summary>
    /// Hit points and mana growth per class
    /// </summary>
    export struct ResourceGrowthConfig {
        std::int32_t hitPointsPerLevel{3};
        std::int32_t manaPointsPerLevel{0};
        std::int32_t constitutionBonusMultiplier{1};
        std::int32_t intelligenceBonusMultiplier{0};

        /// <summary>
        /// Calculate HP gain with constitution bonus
        /// </summary>
        [[nodiscard]] std::int32_t CalculateHitPointGain(std::int32_t levels, std::int32_t constitutionModifier) const noexcept {
            return (hitPointsPerLevel + constitutionModifier * constitutionBonusMultiplier) * levels;
        }

        /// <summary>
        /// Calculate MP gain with intelligence bonus
        /// </summary>
        [[nodiscard]] std::int32_t CalculateManaPointGain(std::int32_t levels, std::int32_t intelligenceModifier) const noexcept {
            return (manaPointsPerLevel + intelligenceModifier * intelligenceBonusMultiplier) * levels;
        }
    };

} // namespace LB::Services::Progression
