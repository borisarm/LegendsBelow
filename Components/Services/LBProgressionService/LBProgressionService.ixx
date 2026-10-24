// LB.ProgressionService.ixx - Character progression by the game's data: the experience curve (progression.json), each class's table and growth (classes.json), the abilities its spells open (spells.json)
export module LB.ProgressionService;

import LB.Core;
import LB.Character;
import std;

export import LB.ProgressionService.ExperienceCurve;
export import LB.ProgressionService.LevelProgressionResult;
export import LB.ProgressionService.AbilityUnlock;

export namespace LB::Services::Progression {
    using LB::AttributeType;
    using LB::Attributes;
    using LB::String;
    using LB::Character::AttributeGrowth;
    using LB::Character::CharacterClass;
    using LB::Character::ClassProgression;

    /// <summary>
    /// How one class advances, as its data says: its level table (hit points, mana, attack and
    /// save bonuses), its primary attributes and attribute growth, and the abilities it opens by level
    /// </summary>
    struct ClassProgressionRules {
        ClassProgression table;
        std::vector<AttributeType> primaryAttributes;
        AttributeGrowth growth;
        std::vector<AbilityUnlock> abilities;
    };

    /// <summary>
    /// Progression service (design-loot-and-progression.md 6, design-character-system.md 5): what
    /// levels take and bring, with no rules of its own: the engine configures it from the content
    /// (SetExperienceCurve, SetClassRules). A class it has no rules for gains nothing.
    /// </summary>
    class ProgressionService {
    public:
        void SetExperienceCurve(const ExperienceCurve& curve) noexcept { curve_ = curve; }
        [[nodiscard]] const ExperienceCurve& GetExperienceCurve() const noexcept { return curve_; }

        void SetClassRules(CharacterClass characterClass, ClassProgressionRules rules) {
            std::ranges::stable_sort(rules.abilities, {}, &AbilityUnlock::requiredLevel);
            rules_[characterClass] = std::move(rules);
        }
        [[nodiscard]] bool HasClassRules(CharacterClass characterClass) const { return rules_.contains(characterClass); }
        [[nodiscard]] const ClassProgressionRules* GetClassRules(CharacterClass characterClass) const {
            const auto it = rules_.find(characterClass);
            return it != rules_.end() ? &it->second : nullptr;
        }

        [[nodiscard]] std::int32_t GetExperienceForLevel(std::int32_t level) const noexcept { return curve_.GetExperienceForLevel(level); }
        [[nodiscard]] std::int32_t GetLevelForExperience(std::int32_t experience) const noexcept { return curve_.GetLevelForExperience(experience); }
        [[nodiscard]] double GetLevelProgress(std::int32_t level, std::int32_t experience) const noexcept { return curve_.GetLevelProgress(level, experience); }

        /// <summary>
        /// What going from one level to another brings, within the cap: the table's hit points
        /// (with the CON modifier for each level) and mana, the automatic attribute points to the
        /// primaries in turn and the free ones, and the abilities opened on the way
        /// </summary>
        [[nodiscard]] LevelProgressionResult CalculateProgression(CharacterClass characterClass, std::int32_t oldLevel, std::int32_t newLevel,
                                                                  const Attributes& attributes) const {
            LevelProgressionResult result;
            result.oldLevel = std::clamp(oldLevel, 1, curve_.maxLevel);
            result.newLevel = std::clamp(newLevel, 1, curve_.maxLevel);
            result.levelsGained = std::max(0, result.newLevel - result.oldLevel);
            const auto* rules = GetClassRules(characterClass);
            if (result.levelsGained == 0 || !rules) return result;

            const auto before = rules->table.TotalsAt(result.oldLevel);
            const auto after = rules->table.TotalsAt(result.newLevel);
            result.hitPointGain = after.maxHitPoints - before.maxHitPoints + attributes.GetConstitutionModifier() * result.levelsGained;
            result.manaPointGain = after.maxManaPoints - before.maxManaPoints;
            result.attributePointsGained = rules->growth.freePointsPerLevel * result.levelsGained;
            if (!rules->primaryAttributes.empty()) {
                const auto automatic = rules->growth.autoPointsPerLevel * result.levelsGained;
                for (std::int32_t point = 0; point < automatic; ++point) {
                    const auto attribute = rules->primaryAttributes[static_cast<std::size_t>(point) % rules->primaryAttributes.size()];
                    result.attributeGains.Set(attribute, result.attributeGains.Get(attribute) + 1);
                }
            }
            result.newAbilities = GetAbilitiesForLevelRange(characterClass, result.oldLevel + 1, result.newLevel);
            return result;
        }

        /// <summary>
        /// The abilities a class opens from one level to another (inclusive), by level
        /// </summary>
        [[nodiscard]] std::vector<String> GetAbilitiesForLevelRange(CharacterClass characterClass, std::int32_t startLevel, std::int32_t endLevel) const {
            std::vector<String> abilities;
            if (const auto* rules = GetClassRules(characterClass)) {
                for (const auto& unlock : rules->abilities) {
                    if (unlock.requiredLevel >= startLevel && unlock.requiredLevel <= endLevel) abilities.push_back(unlock.abilityName);
                }
            }
            return abilities;
        }

        [[nodiscard]] std::vector<String> GetAbilitiesAtLevel(CharacterClass characterClass, std::int32_t level) const {
            return GetAbilitiesForLevelRange(characterClass, level, level);
        }

        /// <summary>
        /// Has a character of this class and level opened the ability?
        /// </summary>
        [[nodiscard]] bool MeetsAbilityRequirements(const String& abilityName, std::int32_t level, CharacterClass characterClass) const {
            const auto opened = GetAbilitiesForLevelRange(characterClass, 1, level);
            return std::ranges::find(opened, abilityName) != opened.end();
        }

    private:
        ExperienceCurve curve_{};
        std::map<CharacterClass, ClassProgressionRules> rules_;
    };

} // namespace LB::Services::Progression
