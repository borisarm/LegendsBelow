// LB.Character.Progression.ixx - Class progression tables (design-character-system.md, section 3)
export module LB.Character.Progression;

import LB.Core;
import std;

using namespace std;

export namespace LB::Character {

    /// <summary>
    /// One row of a class progression table. Level 1 holds the starting values;
    /// later levels hold the gains for that level.
    /// </summary>
    struct ClassLevelGains {
        int32_t level{1};
        int32_t hpGain{0};
        int32_t mpGain{0};
        int32_t meleeAttackBonus{0};
        int32_t rangedAttackBonus{0};
        int32_t physicalSaveBonus{0};
        int32_t mentalSaveBonus{0};
    };

    /// <summary>
    /// Cumulative class values at a level (sum of the progression rows up to it)
    /// </summary>
    struct ClassLevelTotals {
        int32_t maxHitPoints{0};
        int32_t maxManaPoints{0};
        int32_t meleeAttackBonus{0};
        int32_t rangedAttackBonus{0};
        int32_t physicalSaveBonus{0};
        int32_t mentalSaveBonus{0};
    };

    /// <summary>
    /// A class's level 1..MaxLevel progression table
    /// </summary>
    struct ClassProgression {
        static constexpr int32_t MaxLevel = 20;

        std::vector<ClassLevelGains> levels;  // in order, levels 1..MaxLevel

        /// <summary>
        /// True when the table lists exactly levels 1..MaxLevel in order
        /// </summary>
        [[nodiscard]] bool IsComplete() const {
            if (levels.size() != static_cast<std::size_t>(MaxLevel)) return false;
            for (std::size_t i = 0; i < levels.size(); ++i) {
                if (levels[i].level != static_cast<int32_t>(i + 1)) return false;
            }
            return true;
        }

        /// <summary>
        /// Totals at a level, clamped to [1, MaxLevel] (no table gains beyond the cap)
        /// </summary>
        [[nodiscard]] ClassLevelTotals TotalsAt(int32_t level) const {
            ClassLevelTotals totals{};
            const int32_t last = std::clamp(level, 1, MaxLevel);
            for (const auto& row : levels) {
                if (row.level > last) break;
                totals.maxHitPoints += row.hpGain;
                totals.maxManaPoints += row.mpGain;
                totals.meleeAttackBonus += row.meleeAttackBonus;
                totals.rangedAttackBonus += row.rangedAttackBonus;
                totals.physicalSaveBonus += row.physicalSaveBonus;
                totals.mentalSaveBonus += row.mentalSaveBonus;
            }
            return totals;
        }
    };

} // namespace LB::Character
