// LB.ProgressionService.ixx - Character progression service using C++20 modules
export module LB.ProgressionService;

// Import required domains
import LB.Core;
import LB.Character;
import std;

// Import service DTOs
import LB.ProgressionService.ExperienceCurve;
import LB.ProgressionService.StatGrowthConfig;
import LB.ProgressionService.ResourceGrowthConfig;
import LB.ProgressionService.LevelProgressionResult;
import LB.ProgressionService.AbilityUnlock;

using namespace std;

/// <summary>
/// Progression service for LegendsBelow
/// Provides business logic for character advancement, XP curves, and stat scaling
/// Follows D&D-inspired progression with modern game balance
/// </summary>
export namespace LB::Services::Progression {
    // Re-export domain types for service convenience
    using LB::Character::CharacterClass;
    using LB::Attributes;
    using LB::String;

    // Re-export service DTOs
    export using LB::Services::Progression::ExperienceCurve;
    export using LB::Services::Progression::StatGrowthConfig;
    export using LB::Services::Progression::ResourceGrowthConfig;
    export using LB::Services::Progression::LevelProgressionResult;
    export using LB::Services::Progression::AbilityUnlock;

    /// <summary>
    /// Core progression service
    /// Handles all character advancement calculations
    /// </summary>
    export class ProgressionService {
    public:
        /// <summary>
        /// Default experience curve (D&D-inspired)
        /// </summary>
        static constexpr ExperienceCurve DefaultExperienceCurve() noexcept {
            return ExperienceCurve{100.0f, 1.5f, 100};
        }

        /// <summary>
        /// Get stat growth configuration for a class
        /// </summary>
        [[nodiscard]] static StatGrowthConfig GetStatGrowth(CharacterClass characterClass) noexcept {
            switch (characterClass) {
                case CharacterClass::Warrior:
                    return StatGrowthConfig{1, 0, 1, 0, 0, 0}; // STR, CON
                case CharacterClass::Rogue:
                    return StatGrowthConfig{0, 1, 0, 0, 0, 1}; // DEX, CHA
                case CharacterClass::Mage:
                    return StatGrowthConfig{0, 0, 0, 1, 1, 0}; // INT, WIS
                case CharacterClass::Cleric:
                    return StatGrowthConfig{0, 0, 1, 0, 1, 0}; // CON, WIS
                default:
                    return StatGrowthConfig{};
            }
        }

        /// <summary>
        /// Get resource growth configuration for a class
        /// </summary>
        [[nodiscard]] static ResourceGrowthConfig GetResourceGrowth(CharacterClass characterClass) noexcept {
            switch (characterClass) {
                case CharacterClass::Warrior:
                    return ResourceGrowthConfig{5, 0, 1, 0}; // High HP, no MP
                case CharacterClass::Rogue:
                    return ResourceGrowthConfig{4, 1, 1, 0}; // Moderate HP, low MP
                case CharacterClass::Mage:
                    return ResourceGrowthConfig{2, 8, 1, 2}; // Low HP, high MP
                case CharacterClass::Cleric:
                    return ResourceGrowthConfig{3, 5, 1, 1}; // Moderate HP, moderate MP
                default:
                    return ResourceGrowthConfig{};
            }
        }

        /// <summary>
        /// Calculate skill points gained per level
        /// </summary>
        [[nodiscard]] static int32_t CalculateSkillPointsPerLevel(
            CharacterClass characterClass,
            int32_t intelligenceModifier) noexcept {

            int32_t baseSkillPoints = 2;
            int32_t classBonus = 0;

            switch (characterClass) {
                case CharacterClass::Warrior:
                    classBonus = 1; // Combat skills
                    break;
                case CharacterClass::Rogue:
                    classBonus = 4; // Skill specialist
                    break;
                case CharacterClass::Mage:
                    classBonus = 2; // Academic knowledge
                    break;
                case CharacterClass::Cleric:
                    classBonus = 2; // Divine wisdom
                    break;
                default:
                    break;
            }

            return baseSkillPoints + classBonus + std::max(0, intelligenceModifier);
        }

        /// <summary>
        /// Calculate full progression for leveling up
        /// </summary>
        [[nodiscard]] static LevelProgressionResult CalculateProgression(
            CharacterClass characterClass,
            int32_t oldLevel,
            int32_t newLevel,
            const Attributes& currentAttributes) noexcept {

            LevelProgressionResult result;
            result.oldLevel = oldLevel;
            result.newLevel = std::clamp(newLevel, 1, 100);
            result.levelsGained = result.newLevel - oldLevel;

            if (result.levelsGained <= 0) {
                return result;
            }

            // Calculate stat gains
            auto statGrowth = GetStatGrowth(characterClass);
            result.attributeGains = statGrowth.CalculateGains(result.levelsGained);

            // Calculate resource gains
            auto resourceGrowth = GetResourceGrowth(characterClass);
            int32_t conMod = currentAttributes.GetConstitutionModifier();
            int32_t intMod = currentAttributes.GetIntelligenceModifier();

            result.hitPointGain = resourceGrowth.CalculateHitPointGain(result.levelsGained, conMod);
            result.manaPointGain = resourceGrowth.CalculateManaPointGain(result.levelsGained, intMod);

            // Calculate skill points
            result.skillPointGain = CalculateSkillPointsPerLevel(characterClass, intMod) * result.levelsGained;

            // Get new abilities for level range
            result.newAbilities = GetAbilitiesForLevelRange(characterClass, oldLevel + 1, result.newLevel);

            return result;
        }

        /// <summary>
        /// Get abilities unlocked in a level range
        /// </summary>
        [[nodiscard]] static std::vector<String> GetAbilitiesForLevelRange(
            CharacterClass characterClass,
            int32_t startLevel,
            int32_t endLevel) noexcept {

            std::vector<LB::String> abilities;

            for (int32_t level = startLevel; level <= endLevel; ++level) {
                auto levelAbilities = GetAbilitiesAtLevel(characterClass, level);
                abilities.insert(abilities.end(), levelAbilities.begin(), levelAbilities.end());
            }

            return abilities;
        }

        /// <summary>
        /// Get abilities unlocked at a specific level
        /// </summary>
        [[nodiscard]] static std::vector<String> GetAbilitiesAtLevel(
            CharacterClass characterClass,
            int32_t level) noexcept {

            std::vector<String> abilities;

            switch (characterClass) {
                case CharacterClass::Warrior:
                    if (level == 2) abilities.push_back("Power Attack");
                    if (level == 3) abilities.push_back("Cleave");
                    if (level == 5) abilities.push_back("Extra Attack");
                    if (level == 7) abilities.push_back("Whirlwind Attack");
                        if (level == 10) abilities.push_back("Weapon Mastery");
                        break;

                    case CharacterClass::Rogue:
                        if (level == 2) abilities.push_back("Sneak Attack");
                    if (level == 3) abilities.push_back("Backstab");
                    if (level == 5) abilities.push_back("Evasion");
                    if (level == 7) abilities.push_back("Shadow Step");
                        if (level == 10) abilities.push_back("Assassinate");
                        break;

                    case CharacterClass::Mage:
                        if (level == 2) abilities.push_back("Magic Missile");
                    if (level == 3) abilities.push_back("Shield");
                    if (level == 5) abilities.push_back("Fireball");
                    if (level == 7) abilities.push_back("Lightning Bolt");
                        if (level == 10) abilities.push_back("Meteor");
                        break;

                    case CharacterClass::Cleric:
                        if (level == 2) abilities.push_back("Turn Undead");
                    if (level == 3) abilities.push_back("Cure Wounds");
                    if (level == 5) abilities.push_back("Divine Favor");
                    if (level == 7) abilities.push_back("Holy Smite");
                    if (level == 10) abilities.push_back("Mass Heal");
                    break;

                default:
                    break;
            }

            return abilities;
        }

        /// <summary>
        /// Calculate recommended stat priority for a class
        /// Returns attributes in order of importance
        /// </summary>
        [[nodiscard]] static std::vector<String> GetStatPriority(
            CharacterClass characterClass) noexcept {

            switch (characterClass) {
                case CharacterClass::Warrior:
                    return {"Strength", "Constitution", "Dexterity"};
                case CharacterClass::Rogue:
                    return {"Dexterity", "Charisma", "Intelligence"};
                case CharacterClass::Mage:
                    return {"Intelligence", "Wisdom", "Constitution"};
                case CharacterClass::Cleric:
                    return {"Wisdom", "Constitution", "Charisma"};
                default:
                    return {};
            }
        }

        /// <summary>
        /// Calculate effective character level (accounts for multiclassing)
        /// For now, just returns actual level (multiclass support future)
        /// </summary>
        [[nodiscard]] static int32_t CalculateEffectiveLevel(int32_t actualLevel) noexcept {
            return actualLevel;
        }

        /// <summary>
        /// Calculate challenge rating for encounters
        /// Based on character level
        /// </summary>
        [[nodiscard]] static float CalculateChallengeRating(int32_t level) noexcept {
            if (level <= 1) return 0.25f;
            if (level <= 3) return 0.5f;
            if (level <= 5) return 1.0f;
            return static_cast<float>(level - 4) / 2.0f;
        }

        /// <summary>
        /// Calculate recommended XP award for defeating an enemy
        /// Based on challenge rating and party size
        /// </summary>
        [[nodiscard]] static int32_t CalculateXPReward(
            float challengeRating,
            int32_t partySize = 1) noexcept {

            // Base XP by CR
            int32_t baseXP = 0;
            if (challengeRating <= 0.25f) baseXP = 50;
            else if (challengeRating <= 0.5f) baseXP = 100;
            else if (challengeRating <= 1.0f) baseXP = 200;
            else if (challengeRating <= 2.0f) baseXP = 450;
            else if (challengeRating <= 3.0f) baseXP = 700;
            else if (challengeRating <= 4.0f) baseXP = 1100;
            else if (challengeRating <= 5.0f) baseXP = 1800;
            else baseXP = static_cast<int32_t>(1800.0f * std::pow(1.5f, challengeRating - 5.0f));

            // Divide by party size
            if (partySize > 1) {
                baseXP = baseXP / partySize;
            }

            return std::max(1, baseXP);
        }

        /// <summary>
        /// Check if character meets requirements for ability
        /// </summary>
        [[nodiscard]] static bool MeetsAbilityRequirements(
            const String& abilityName,
            int32_t level,
            CharacterClass characterClass,
            const Attributes& attributes) noexcept {

            // Check level requirement
            auto abilities = GetAbilitiesForLevelRange(characterClass, 1, level);
            bool hasLevel = std::find(abilities.begin(), abilities.end(), abilityName) != abilities.end();

            // Could add attribute requirements here
            // For now, just check level

            return hasLevel;
        }

        /// <summary>
        /// Suggest next level improvements
        /// </summary>
        [[nodiscard]] static std::vector<String> SuggestImprovements(
            CharacterClass characterClass,
            int32_t currentLevel) noexcept {

            std::vector<String> suggestions;

            // Check upcoming abilities
            auto nextAbilities = GetAbilitiesAtLevel(characterClass, currentLevel + 1);
            if (!nextAbilities.empty()) {
                suggestions.push_back("Unlock new abilities at level " + std::to_string(currentLevel + 1));
            }

            // Check stat priorities
            auto statPriority = GetStatPriority(characterClass);
            if (!statPriority.empty()) {
                suggestions.push_back("Focus on " + statPriority[0] + " for maximum effectiveness");
            }

            return suggestions;
        }
    };

} // namespace LB::Services::Progression