// LevelProgressionResult.ixx - Result DTO for level-up calculations
export module LB.ProgressionService.LevelProgressionResult;

import LB.Core;
import std;


export namespace LB::Services::Progression {

    /// <summary>
    /// Level progression result
    /// Contains all changes from leveling up
    /// </summary>
    export struct LevelProgressionResult {
        std::int32_t oldLevel{1};
        std::int32_t newLevel{1};
        std::int32_t levelsGained{0};
        LB::Attributes attributeGains{};
        std::int32_t hitPointGain{0};
        std::int32_t manaPointGain{0};
        std::int32_t skillPointGain{0};
        std::vector<LB::String> newAbilities{};

        [[nodiscard]] bool HasLeveledUp() const noexcept {
            return newLevel > oldLevel;
        }
    };

} // namespace LB::Services::Progression
