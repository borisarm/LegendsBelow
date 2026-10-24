// LB.Character.Progression.ixx - Class progression tables (design-character-system.md, section 3)
export module LB.Character.Progression;

import LB.Core;
import std;


export namespace LB::Character {

    /// <summary>
    /// One row of a class progression table. Level 1 holds the starting values;
    /// later levels hold the gains for that level.
    /// </summary>
    struct ClassLevelGains {
        std::int32_t level{1};
        std::int32_t hpGain{0};
        std::int32_t mpGain{0};
        std::int32_t meleeAttackBonus{0};
        std::int32_t rangedAttackBonus{0};
        std::int32_t physicalSaveBonus{0};
        std::int32_t mentalSaveBonus{0};
    };

    /// <summary>
    /// Cumulative class values at a level (sum of the progression rows up to it)
    /// </summary>
    struct ClassLevelTotals {
        std::int32_t maxHitPoints{0};
        std::int32_t maxManaPoints{0};
        std::int32_t meleeAttackBonus{0};
        std::int32_t rangedAttackBonus{0};
        std::int32_t physicalSaveBonus{0};
        std::int32_t mentalSaveBonus{0};
    };

    /// <summary>
    /// How a class's attributes grow (design-character-system.md 5.2: 2 points per level, split
    /// between automatic class points and points the player assigns), and the per-class caps
    /// </summary>
    struct AttributeGrowth {
        std::int32_t autoPointsPerLevel{1};    // go to the class's primary attributes, alternating
        std::int32_t freePointsPerLevel{1};    // assigned by the player
        std::int32_t primaryAttributeCap{25};  // design 4.1 absolute cap
        std::int32_t otherAttributeCap{18};    // top of the creation range
    };

    /// <summary>
    /// How strongly a class feels an armour category's penalties: 0.5 signature, 1 trained,
    /// 1.5 untrained, 2 restricted. Scales the armour pieces' movement and cast penalties.
    /// </summary>
    struct ArmorAdjustment {
        float movementScale{1.0f};
        float castingScale{1.0f};
    };

    /// <summary>
    /// The experience a level takes: base x (level - 1)^exponent (progression.json; level 1 takes
    /// none), up to the level cap
    /// </summary>
    struct ExperienceCurve {
        float baseExperience{100.0f};
        float exponent{2.0f};
        std::int32_t maxLevel{20};

        [[nodiscard]] std::int32_t GetExperienceForLevel(std::int32_t level) const noexcept {
            if (level <= 1) return 0;
            level = std::min(level, maxLevel);
            return static_cast<std::int32_t>(std::lround(baseExperience * std::pow(static_cast<float>(level - 1), exponent)));
        }

        [[nodiscard]] std::int32_t GetLevelForExperience(std::int32_t experience) const noexcept {
            std::int32_t level = 1;
            while (level < maxLevel && GetExperienceForLevel(level + 1) <= experience) ++level;
            return level;
        }

        /// <summary>
        /// The total a level after this one takes (0 at the cap)
        /// </summary>
        [[nodiscard]] std::int32_t GetExperienceToNextLevel(std::int32_t currentLevel) const noexcept {
            return currentLevel >= maxLevel ? 0 : GetExperienceForLevel(currentLevel + 1);
        }

        /// <summary>
        /// How far into its level an amount of experience is (0..1; 1 at the cap)
        /// </summary>
        [[nodiscard]] double GetLevelProgress(std::int32_t currentLevel, std::int32_t currentExperience) const noexcept {
            if (currentLevel >= maxLevel) return 1.0;
            const auto start = GetExperienceForLevel(currentLevel);
            const auto end = GetExperienceForLevel(currentLevel + 1);
            return end > start ? std::clamp(static_cast<double>(currentExperience - start) / (end - start), 0.0, 1.0) : 1.0;
        }
    };

    /// <summary>
    /// A class's level 1..MaxLevel progression table
    /// </summary>
    struct ClassProgression {
        static constexpr std::int32_t MaxLevel = 20;

        std::vector<ClassLevelGains> levels;  // in order, levels 1..MaxLevel

        /// <summary>
        /// True when the table lists exactly levels 1..MaxLevel in order
        /// </summary>
        [[nodiscard]] bool IsComplete() const {
            if (levels.size() != static_cast<std::size_t>(MaxLevel)) return false;
            for (std::size_t i = 0; i < levels.size(); ++i) {
                if (levels[i].level != static_cast<std::int32_t>(i + 1)) return false;
            }
            return true;
        }

        /// <summary>
        /// Totals at a level, clamped to [1, MaxLevel] (no table gains beyond the cap)
        /// </summary>
        [[nodiscard]] ClassLevelTotals TotalsAt(std::int32_t level) const {
            ClassLevelTotals totals{};
            const std::int32_t last = std::clamp(level, 1, MaxLevel);
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
