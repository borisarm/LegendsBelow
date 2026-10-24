// LevelProgressionResult.ixx - What a rise in level brings, by the class's data
export module LB.ProgressionService.LevelProgressionResult;

import LB.Core;
import std;

export namespace LB::Services::Progression {

    /// <summary>
    /// What going from one level to another brings (the class's table in classes.json, the
    /// abilities its spells open in spells.json)
    /// </summary>
    struct LevelProgressionResult {
        std::int32_t oldLevel{1};
        std::int32_t newLevel{1};
        std::int32_t levelsGained{0};
        LB::Attributes attributeGains{0, 0, 0, 0, 0, 0};   // the automatic points, to the class's primaries in turn
        std::int32_t attributePointsGained{0};               // the free points the player assigns
        std::int32_t hitPointGain{0};
        std::int32_t manaPointGain{0};
        std::vector<LB::String> newAbilities{};

        [[nodiscard]] bool HasLeveledUp() const noexcept { return newLevel > oldLevel; }
    };

} // namespace LB::Services::Progression
