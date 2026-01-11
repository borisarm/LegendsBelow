#include "pch.h"

import LB.ProgressionService;

using namespace LB::Services::Progression;


// ============================================================================
// Experience Curve Tests
// ============================================================================

TEST(ProgressionServiceTests, ExperienceCurve_Level1_ZeroXP) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xp = curve.GetExperienceForLevel(1);

    EXPECT_EQ(xp, 0);
}

TEST(ProgressionServiceTests, ExperienceCurve_Level2_BaseXP) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xp = curve.GetExperienceForLevel(2);

    EXPECT_EQ(xp, 100); // Base XP for level 2
}

TEST(ProgressionServiceTests, ExperienceCurve_HigherLevels_Exponential) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xp2 = curve.GetExperienceForLevel(2);
    int32_t xp3 = curve.GetExperienceForLevel(3);
    int32_t xp4 = curve.GetExperienceForLevel(4);

    // Should be exponential, each level requires more XP
    EXPECT_GT(xp3 - xp2, xp2 - 0);
    EXPECT_GT(xp4 - xp3, xp3 - xp2);
}

TEST(ProgressionServiceTests, GetLevelForExperience_ZeroXP_Level1) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t level = curve.GetLevelForExperience(0);

    EXPECT_EQ(level, 1);
}

TEST(ProgressionServiceTests, GetLevelForExperience_ExactThreshold_CorrectLevel) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xpForLevel3 = curve.GetExperienceForLevel(3);
    int32_t level = curve.GetLevelForExperience(xpForLevel3);

    EXPECT_EQ(level, 3);
}

TEST(ProgressionServiceTests, GetLevelForExperience_BetweenLevels_LowerLevel) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xpForLevel2 = curve.GetExperienceForLevel(2);
    int32_t xpForLevel3 = curve.GetExperienceForLevel(3);
    int32_t midXP = (xpForLevel2 + xpForLevel3) / 2;

    int32_t level = curve.GetLevelForExperience(midXP);

    EXPECT_EQ(level, 2);
}

TEST(ProgressionServiceTests, GetExperienceToNextLevel_BelowMax_ReturnsXP) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xpNeeded = curve.GetExperienceToNextLevel(5);

    EXPECT_GT(xpNeeded, 0);
    EXPECT_EQ(xpNeeded, curve.GetExperienceForLevel(6));
}

TEST(ProgressionServiceTests, GetExperienceToNextLevel_AtMax_ReturnsZero) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xpNeeded = curve.GetExperienceToNextLevel(100);

    EXPECT_EQ(xpNeeded, 0);
}

TEST(ProgressionServiceTests, GetLevelProgress_StartOfLevel_ZeroPercent) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xpForLevel5 = curve.GetExperienceForLevel(5);
    double progress = curve.GetLevelProgress(5, xpForLevel5);

    EXPECT_DOUBLE_EQ(progress, 0.0);
}

TEST(ProgressionServiceTests, GetLevelProgress_MidLevel_HalfProgress) {
    auto curve = ProgressionService::DefaultExperienceCurve();

    int32_t xpForLevel5 = curve.GetExperienceForLevel(5);
    int32_t xpForLevel6 = curve.GetExperienceForLevel(6);
    int32_t midXP = (xpForLevel5 + xpForLevel6) / 2;

    double progress = curve.GetLevelProgress(5, midXP);

    EXPECT_NEAR(progress, 0.5, 0.01);
}

// ============================================================================
// Stat Growth Tests
// ============================================================================

TEST(ProgressionServiceTests, GetStatGrowth_Warrior_StrengthConstitution) {
    auto growth = ProgressionService::GetStatGrowth(CharacterClass::Warrior);

    EXPECT_GT(growth.strengthPerLevel, 0);
    EXPECT_GT(growth.constitutionPerLevel, 0);
    EXPECT_EQ(growth.intelligencePerLevel, 0);
}

TEST(ProgressionServiceTests, GetStatGrowth_Mage_IntelligenceWisdom) {
    auto growth = ProgressionService::GetStatGrowth(CharacterClass::Mage);

    EXPECT_GT(growth.intelligencePerLevel, 0);
    EXPECT_GT(growth.wisdomPerLevel, 0);
    EXPECT_EQ(growth.strengthPerLevel, 0);
}

TEST(ProgressionServiceTests, GetStatGrowth_Rogue_DexterityCharisma) {
    auto growth = ProgressionService::GetStatGrowth(CharacterClass::Rogue);

    EXPECT_GT(growth.dexterityPerLevel, 0);
    EXPECT_GT(growth.charismaPerLevel, 0);
    EXPECT_EQ(growth.strengthPerLevel, 0);
}

TEST(ProgressionServiceTests, GetStatGrowth_Cleric_ConstitutionWisdom) {
    auto growth = ProgressionService::GetStatGrowth(CharacterClass::Cleric);

    EXPECT_GT(growth.constitutionPerLevel, 0);
    EXPECT_GT(growth.wisdomPerLevel, 0);
    EXPECT_EQ(growth.dexterityPerLevel, 0);
}

TEST(ProgressionServiceTests, StatGrowthCalculateGains_MultipleLevels_Scales) {
    auto growth = ProgressionService::GetStatGrowth(CharacterClass::Warrior);

    auto gains = growth.CalculateGains(5);

    EXPECT_EQ(gains.strength, growth.strengthPerLevel * 5);
    EXPECT_EQ(gains.constitution, growth.constitutionPerLevel * 5);
}

// ============================================================================
// Resource Growth Tests
// ============================================================================

TEST(ProgressionServiceTests, GetResourceGrowth_Warrior_HighHP_NoMP) {
    auto growth = ProgressionService::GetResourceGrowth(CharacterClass::Warrior);

    EXPECT_GT(growth.hitPointsPerLevel, 3);
    EXPECT_EQ(growth.manaPointsPerLevel, 0);
}

TEST(ProgressionServiceTests, GetResourceGrowth_Mage_LowHP_HighMP) {
    auto growth = ProgressionService::GetResourceGrowth(CharacterClass::Mage);

    EXPECT_LT(growth.hitPointsPerLevel, 4);
    EXPECT_GT(growth.manaPointsPerLevel, 5);
}

TEST(ProgressionServiceTests, CalculateHitPointGain_WithConstitution_AddsBonus) {
    auto growth = ProgressionService::GetResourceGrowth(CharacterClass::Warrior);

    int32_t hpGain = growth.CalculateHitPointGain(1, 3); // +3 CON modifier

    EXPECT_EQ(hpGain, growth.hitPointsPerLevel + 3);
}

TEST(ProgressionServiceTests, CalculateManaPointGain_WithIntelligence_AddsBonus) {
    auto growth = ProgressionService::GetResourceGrowth(CharacterClass::Mage);

    int32_t mpGain = growth.CalculateManaPointGain(1, 4); // +4 INT modifier

    EXPECT_EQ(mpGain, growth.manaPointsPerLevel + (4 * growth.intelligenceBonusMultiplier));
}

// ============================================================================
// Skill Points Tests
// ============================================================================

TEST(ProgressionServiceTests, CalculateSkillPointsPerLevel_Rogue_Highest) {
    int32_t rogueSkills = ProgressionService::CalculateSkillPointsPerLevel(CharacterClass::Rogue, 0);
    int32_t warriorSkills = ProgressionService::CalculateSkillPointsPerLevel(CharacterClass::Warrior, 0);
    int32_t mageSkills = ProgressionService::CalculateSkillPointsPerLevel(CharacterClass::Mage, 0);

    EXPECT_GT(rogueSkills, warriorSkills);
    EXPECT_GT(rogueSkills, mageSkills);
}

TEST(ProgressionServiceTests, CalculateSkillPointsPerLevel_WithIntelligence_AddsBonus) {
    int32_t baseSkills = ProgressionService::CalculateSkillPointsPerLevel(CharacterClass::Warrior, 0);
    int32_t bonusSkills = ProgressionService::CalculateSkillPointsPerLevel(CharacterClass::Warrior, 3);

    EXPECT_EQ(bonusSkills, baseSkills + 3);
}

TEST(ProgressionServiceTests, CalculateSkillPointsPerLevel_NegativeInt_NoBonus) {
    int32_t skills = ProgressionService::CalculateSkillPointsPerLevel(CharacterClass::Warrior, -2);

    EXPECT_GT(skills, 0); // Negative INT doesn't reduce below base
}

// ============================================================================
// Level Progression Tests
// ============================================================================

TEST(ProgressionServiceTests, CalculateProgression_SingleLevel_CorrectGains) {
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto result = ProgressionService::CalculateProgression(CharacterClass::Warrior, 1, 2, baseAttribs);

    EXPECT_TRUE(result.HasLeveledUp());
    EXPECT_EQ(result.oldLevel, 1);
    EXPECT_EQ(result.newLevel, 2);
    EXPECT_EQ(result.levelsGained, 1);
    EXPECT_GT(result.hitPointGain, 0);
    EXPECT_GT(result.skillPointGain, 0);
}

TEST(ProgressionServiceTests, CalculateProgression_MultipleLevels_AccumulatesGains) {
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto result = ProgressionService::CalculateProgression(CharacterClass::Warrior, 1, 5, baseAttribs);

    EXPECT_EQ(result.levelsGained, 4);
    EXPECT_GT(result.attributeGains.strength, 1);
    EXPECT_GT(result.hitPointGain, 10);
}

TEST(ProgressionServiceTests, CalculateProgression_NoLevelUp_NoGains) {
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto result = ProgressionService::CalculateProgression(CharacterClass::Warrior, 5, 5, baseAttribs);

    EXPECT_FALSE(result.HasLeveledUp());
    EXPECT_EQ(result.levelsGained, 0);
    EXPECT_EQ(result.hitPointGain, 0);
}

TEST(ProgressionServiceTests, CalculateProgression_Mage_GainsIntelligence) {
    Attributes baseAttribs{10, 10, 10, 10, 10, 10};

    auto result = ProgressionService::CalculateProgression(CharacterClass::Mage, 1, 3, baseAttribs);

    EXPECT_GT(result.attributeGains.intelligence, 0);
    EXPECT_GT(result.manaPointGain, 0);
}

TEST(ProgressionServiceTests, CalculateProgression_HighConstBuff_BetterHPGain) {
    Attributes lowCon{10, 10, 10, 10, 10, 10}; // +0 CON
    Attributes highCon{10, 10, 18, 10, 10, 10}; // +4 CON

    auto resultLow = ProgressionService::CalculateProgression(CharacterClass::Warrior, 1, 2, lowCon);
    auto resultHigh = ProgressionService::CalculateProgression(CharacterClass::Warrior, 1, 2, highCon);

    EXPECT_GT(resultHigh.hitPointGain, resultLow.hitPointGain);
}

// ============================================================================
// Ability Unlock Tests
// ============================================================================

TEST(ProgressionServiceTests, GetAbilitiesAtLevel_Level1_NoAbilities) {
    auto abilities = ProgressionService::GetAbilitiesAtLevel(CharacterClass::Warrior, 1);

    EXPECT_TRUE(abilities.empty());
}

TEST(ProgressionServiceTests, GetAbilitiesAtLevel_Level2_UnlocksFirst) {
    auto abilities = ProgressionService::GetAbilitiesAtLevel(CharacterClass::Warrior, 2);

    EXPECT_FALSE(abilities.empty());
    EXPECT_EQ(abilities.size(), size_t(1));
}

TEST(ProgressionServiceTests, GetAbilitiesAtLevel_Warrior_CombatAbilities) {
    auto abilities = ProgressionService::GetAbilitiesAtLevel(CharacterClass::Warrior, 2);

    ASSERT_FALSE(abilities.empty());
    EXPECT_EQ(abilities[0], "Power Attack");
}

TEST(ProgressionServiceTests, GetAbilitiesAtLevel_Mage_SpellAbilities) {
    auto abilities = ProgressionService::GetAbilitiesAtLevel(CharacterClass::Mage, 2);

    ASSERT_FALSE(abilities.empty());
    EXPECT_EQ(abilities[0], "Magic Missile");
}

TEST(ProgressionServiceTests, GetAbilitiesForLevelRange_MultipleLevel_AllAbilities) {
    auto abilities = ProgressionService::GetAbilitiesForLevelRange(CharacterClass::Warrior, 2, 5);

    EXPECT_GE(abilities.size(), size_t(2)); // At least 2 abilities in that range
}

TEST(ProgressionServiceTests, GetAbilitiesForLevelRange_Level1To1_Empty) {
    auto abilities = ProgressionService::GetAbilitiesForLevelRange(CharacterClass::Warrior, 1, 1);

    EXPECT_TRUE(abilities.empty());
}

// ============================================================================
// Stat Priority Tests
// ============================================================================

TEST(ProgressionServiceTests, GetStatPriority_Warrior_StrengthFirst) {
    auto priority = ProgressionService::GetStatPriority(CharacterClass::Warrior);

    ASSERT_FALSE(priority.empty());
    EXPECT_EQ(priority[0], "Strength");
}

TEST(ProgressionServiceTests, GetStatPriority_Mage_IntelligenceFirst) {
    auto priority = ProgressionService::GetStatPriority(CharacterClass::Mage);

    ASSERT_FALSE(priority.empty());
    EXPECT_EQ(priority[0], "Intelligence");
}

TEST(ProgressionServiceTests, GetStatPriority_Rogue_DexterityFirst) {
    auto priority = ProgressionService::GetStatPriority(CharacterClass::Rogue);

    ASSERT_FALSE(priority.empty());
    EXPECT_EQ(priority[0], "Dexterity");
}

TEST(ProgressionServiceTests, GetStatPriority_Cleric_WisdomFirst) {
    auto priority = ProgressionService::GetStatPriority(CharacterClass::Cleric);

    ASSERT_FALSE(priority.empty());
    EXPECT_EQ(priority[0], "Wisdom");
}

// ============================================================================
// Challenge Rating Tests
// ============================================================================

TEST(ProgressionServiceTests, CalculateChallengeRating_Level1_Low) {
    float cr = ProgressionService::CalculateChallengeRating(1);

    EXPECT_LT(cr, 1.0f);
}

TEST(ProgressionServiceTests, CalculateChallengeRating_Level10_High) {
    float cr = ProgressionService::CalculateChallengeRating(10);

    EXPECT_GT(cr, 2.0f);
}

TEST(ProgressionServiceTests, CalculateChallengeRating_HigherLevel_HigherCR) {
    float cr5 = ProgressionService::CalculateChallengeRating(5);
    float cr10 = ProgressionService::CalculateChallengeRating(10);
    float cr15 = ProgressionService::CalculateChallengeRating(15);

    EXPECT_LT(cr5, cr10);
    EXPECT_LT(cr10, cr15);
}

// ============================================================================
// XP Reward Tests
// ============================================================================

TEST(ProgressionServiceTests, CalculateXPReward_LowCR_LowXP) {
    int32_t xp = ProgressionService::CalculateXPReward(0.25f, 1);

    EXPECT_GT(xp, 0);
    EXPECT_LT(xp, 200);
}

TEST(ProgressionServiceTests, CalculateXPReward_HighCR_HighXP) {
    int32_t xp = ProgressionService::CalculateXPReward(5.0f, 1);

    EXPECT_GT(xp, 1000);
}

TEST(ProgressionServiceTests, CalculateXPReward_PartySize_DividesXP) {
    int32_t xpSolo = ProgressionService::CalculateXPReward(1.0f, 1);
    int32_t xpParty = ProgressionService::CalculateXPReward(1.0f, 4);

    EXPECT_LT(xpParty, xpSolo);
    EXPECT_NEAR(xpParty, xpSolo / 4, 1);
}

TEST(ProgressionServiceTests, CalculateXPReward_HigherCR_MoreXP) {
    int32_t xp1 = ProgressionService::CalculateXPReward(1.0f, 1);
    int32_t xp2 = ProgressionService::CalculateXPReward(2.0f, 1);
    int32_t xp3 = ProgressionService::CalculateXPReward(3.0f, 1);

    EXPECT_LT(xp1, xp2);
    EXPECT_LT(xp2, xp3);
}

// ============================================================================
// Ability Requirements Tests
// ============================================================================

TEST(ProgressionServiceTests, MeetsAbilityRequirements_CorrectLevel_ReturnsTrue) {
    Attributes attribs{10, 10, 10, 10, 10, 10};

    bool meets = ProgressionService::MeetsAbilityRequirements("Power Attack", 2, CharacterClass::Warrior, attribs);

    EXPECT_TRUE(meets);
}

TEST(ProgressionServiceTests, MeetsAbilityRequirements_TooLow_ReturnsFalse) {
    Attributes attribs{10, 10, 10, 10, 10, 10};

    bool meets = ProgressionService::MeetsAbilityRequirements("Power Attack", 1, CharacterClass::Warrior, attribs);

    EXPECT_FALSE(meets);
}

TEST(ProgressionServiceTests, MeetsAbilityRequirements_WrongClass_ReturnsFalse) {
    Attributes attribs{10, 10, 10, 10, 10, 10};

    bool meets = ProgressionService::MeetsAbilityRequirements("Power Attack", 5, CharacterClass::Mage, attribs);

    EXPECT_FALSE(meets);
}

// ============================================================================
// Progression Suggestions Tests
// ============================================================================

TEST(ProgressionServiceTests, SuggestImprovements_ReturnsRecommendations) {
    auto suggestions = ProgressionService::SuggestImprovements(CharacterClass::Warrior, 1);

    EXPECT_FALSE(suggestions.empty());
}

TEST(ProgressionServiceTests, SuggestImprovements_NearMilestone_MentionsAbilities) {
    auto suggestions = ProgressionService::SuggestImprovements(CharacterClass::Mage, 1);

    ASSERT_FALSE(suggestions.empty());
    // Should mention upcoming abilities at level 2
    bool mentionsLevel = false;
    for (const auto& suggestion : suggestions) {
        if (suggestion.find("level 2") != std::string::npos) {
            mentionsLevel = true;
            break;
        }
    }
    EXPECT_TRUE(mentionsLevel);
}

// ============================================================================
// Effective Level Tests
// ============================================================================

TEST(ProgressionServiceTests, CalculateEffectiveLevel_CurrentImplementation_ReturnsActual) {
    int32_t effectiveLevel = ProgressionService::CalculateEffectiveLevel(10);

    EXPECT_EQ(effectiveLevel, 10);
}