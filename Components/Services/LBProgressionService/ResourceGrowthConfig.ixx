// ResourceGrowthConfig.ixx - HP and MP growth configuration per class
export module LB.ProgressionService.ResourceGrowthConfig;

import std;

using namespace std;
export namespace LB::Services::Progression {

    /// <summary>
    /// Hit points and mana growth per class
    /// </summary>
    export struct ResourceGrowthConfig {
        int32_t hitPointsPerLevel{3};
        int32_t manaPointsPerLevel{0};
        int32_t constitutionBonusMultiplier{1};
        int32_t intelligenceBonusMultiplier{0};

        /// <summary>
        /// Calculate HP gain with constitution bonus
        /// </summary>
        [[nodiscard]] int32_t CalculateHitPointGain(int32_t levels, int32_t constitutionModifier) const noexcept {
            return (hitPointsPerLevel + constitutionModifier * constitutionBonusMultiplier) * levels;
        }

        /// <summary>
        /// Calculate MP gain with intelligence bonus
        /// </summary>
        [[nodiscard]] int32_t CalculateManaPointGain(int32_t levels, int32_t intelligenceModifier) const noexcept {
            return (manaPointsPerLevel + intelligenceModifier * intelligenceBonusMultiplier) * levels;
        }
    };

} // namespace LB::Services::Progression
