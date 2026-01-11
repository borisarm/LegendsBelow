// StatGrowthConfig.ixx - Stat scaling configuration per class
export module LB.ProgressionService.StatGrowthConfig;

import LB.Core;
import std;

using namespace std;
export namespace LB::Services::Progression {

    /// <summary>
    /// Stat growth configuration per class
    /// Defines how attributes scale with levels
    /// </summary>
    export struct StatGrowthConfig {
        int32_t strengthPerLevel{0};
        int32_t dexterityPerLevel{0};
        int32_t constitutionPerLevel{0};
        int32_t intelligencePerLevel{0};
        int32_t wisdomPerLevel{0};
        int32_t charismaPerLevel{0};

        /// <summary>
        /// Calculate total stat gains for multiple levels
        /// </summary>
        [[nodiscard]] LB::Attributes CalculateGains(int32_t levels) const noexcept {
            return LB::Attributes{
                strengthPerLevel * levels,
                dexterityPerLevel * levels,
                constitutionPerLevel * levels,
                intelligencePerLevel * levels,
                wisdomPerLevel * levels,
                charismaPerLevel * levels
            };
        }
    };

} // namespace LB::Services::Progression
