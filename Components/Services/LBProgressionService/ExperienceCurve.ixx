// ExperienceCurve.ixx - Experience calculation configuration
export module LB.ProgressionService.ExperienceCurve;

import std;
using namespace std;

export namespace LB::Services::Progression {

    /// <summary>
    /// Experience curve configuration
    /// Defines how experience scales with level
    /// </summary>
    export struct ExperienceCurve {
        float baseExperience{100.0f};
        float exponent{1.5f};
        int32_t maxLevel{100};

        /// <summary>
        /// Calculate XP required for a specific level
        /// Formula: baseXP * (level - 1)^exponent
        /// </summary>
        [[nodiscard]] int32_t GetExperienceForLevel(int32_t level) const noexcept {
            if (level <= 1) return 0;
            if (level > maxLevel) level = maxLevel;
            return static_cast<int32_t>(baseExperience * std::pow(level - 1, exponent));
        }

        /// <summary>
        /// Calculate level from total experience
        /// </summary>
        [[nodiscard]] int32_t GetLevelForExperience(int32_t experience) const noexcept {
            if (experience <= 0) return 1;
            int32_t level = 1;
            while (level < maxLevel && GetExperienceForLevel(level + 1) <= experience) {
                level++;
            }
            return level;
        }

        /// <summary>
        /// Calculate XP needed for next level from current level
        /// </summary>
        [[nodiscard]] int32_t GetExperienceToNextLevel(int32_t currentLevel) const noexcept {
            if (currentLevel >= maxLevel) return 0;
            return GetExperienceForLevel(currentLevel + 1);
        }

        /// <summary>
        /// Calculate XP progress percentage in current level
        /// </summary>
        [[nodiscard]] double GetLevelProgress(int32_t currentLevel, int32_t currentExperience) const noexcept {
            if (currentLevel >= maxLevel) return 1.0;
            int32_t levelStartXP = GetExperienceForLevel(currentLevel);
            int32_t levelEndXP = GetExperienceForLevel(currentLevel + 1);
            int32_t xpInLevel = currentExperience - levelStartXP;
            int32_t xpNeeded = levelEndXP - levelStartXP;
            if (xpNeeded <= 0) return 1.0;
            return static_cast<double>(xpInLevel) / xpNeeded;
        }
    };

} // namespace LB::Services::Progression
