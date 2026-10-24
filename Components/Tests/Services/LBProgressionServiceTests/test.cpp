// test.cpp - LBProgressionService tests: progression from data (an experience curve, a class's table, growth, and abilities), with no rules of its own
#include "pch.h"

import LB.Core;
import LB.Character;
import LB.ProgressionService;
import std;

using namespace LB::Services::Progression;
using LB::AttributeType;
using LB::Attributes;
using LB::Character::CharacterClass;
using LB::Character::ClassLevelGains;

namespace {
    // A class as classes.json would give it: 10 hit points and 2 mana at level 1, then +6 and +1 a
    // level (+8 hit points from level 11); STR and CON primaries; one point each way a level
    ClassProgressionRules TestRules() {
        ClassProgressionRules rules;
        for (std::int32_t level = 1; level <= LB::Character::ClassProgression::MaxLevel; ++level) {
            ClassLevelGains row{};
            row.level = level;
            row.hpGain = level == 1 ? 10 : level >= 11 ? 8 : 6;
            row.mpGain = level == 1 ? 2 : 1;
            rules.table.levels.push_back(row);
        }
        rules.primaryAttributes = { AttributeType::Strength, AttributeType::Constitution };
        rules.growth.autoPointsPerLevel = 1;
        rules.growth.freePointsPerLevel = 1;
        rules.abilities = { { "Whirlwind", 5, CharacterClass::Warrior }, { "Cleave", 3, CharacterClass::Warrior }, { "Shield Bash", 3, CharacterClass::Warrior } };
        return rules;
    }

    ProgressionService Configured() {
        ProgressionService service;
        service.SetExperienceCurve(ExperienceCurve{ 100.0f, 2.0f, 20 });
        service.SetClassRules(CharacterClass::Warrior, TestRules());
        return service;
    }
}

// ============================================================================
// The experience curve (progression.json)
// ============================================================================

TEST(ProgressionCurveTests, Level1_TakesNothing) {
    EXPECT_EQ(Configured().GetExperienceForLevel(1), 0);
    EXPECT_EQ(Configured().GetExperienceForLevel(0), 0);
}

TEST(ProgressionCurveTests, FollowsBaseTimesLevelPower) {
    const auto service = Configured();
    EXPECT_EQ(service.GetExperienceForLevel(2), 100);
    EXPECT_EQ(service.GetExperienceForLevel(3), 400);
    EXPECT_EQ(service.GetExperienceForLevel(5), 1600);
    EXPECT_EQ(service.GetExperienceForLevel(20), 36100);
}

TEST(ProgressionCurveTests, StopsAtTheCap) {
    const auto service = Configured();
    EXPECT_EQ(service.GetExperienceForLevel(25), service.GetExperienceForLevel(20));
    EXPECT_EQ(service.GetLevelForExperience(1'000'000), 20);
    EXPECT_EQ(service.GetExperienceCurve().GetExperienceToNextLevel(20), 0);
}

TEST(ProgressionCurveTests, LevelForExperience_IsTheInverse) {
    const auto service = Configured();
    EXPECT_EQ(service.GetLevelForExperience(0), 1);
    EXPECT_EQ(service.GetLevelForExperience(99), 1);
    EXPECT_EQ(service.GetLevelForExperience(100), 2);
    EXPECT_EQ(service.GetLevelForExperience(1599), 4);
    EXPECT_EQ(service.GetLevelForExperience(1600), 5);
}

TEST(ProgressionCurveTests, Progress_IsTheShareIntoTheLevel) {
    const auto service = Configured();
    EXPECT_DOUBLE_EQ(service.GetLevelProgress(2, 100), 0.0);
    EXPECT_DOUBLE_EQ(service.GetLevelProgress(2, 250), 0.5);   // 100..400
    EXPECT_DOUBLE_EQ(service.GetLevelProgress(20, 0), 1.0);
}

TEST(ProgressionCurveTests, ADifferentCurve_FromTheData) {
    ProgressionService service;
    service.SetExperienceCurve(ExperienceCurve{ 50.0f, 1.5f, 20 });
    EXPECT_EQ(service.GetExperienceForLevel(2), 50);
    EXPECT_EQ(service.GetExperienceForLevel(5), 400);   // 50 x 4^1.5
}

// ============================================================================
// A class's progression (classes.json)
// ============================================================================

TEST(ProgressionClassTests, HitPointsAndMana_FromTheTable) {
    const auto service = Configured();
    const auto result = service.CalculateProgression(CharacterClass::Warrior, 1, 3, Attributes{ 10, 10, 10, 10, 10, 10 });
    EXPECT_EQ(result.levelsGained, 2);
    EXPECT_EQ(result.hitPointGain, 12);   // two rows of +6
    EXPECT_EQ(result.manaPointGain, 2);
    EXPECT_TRUE(result.HasLeveledUp());
}

TEST(ProgressionClassTests, Constitution_AddsItsModifierEachLevel) {
    const auto service = Configured();
    const auto result = service.CalculateProgression(CharacterClass::Warrior, 1, 3, Attributes{ 10, 10, 14, 10, 10, 10 });
    EXPECT_EQ(result.hitPointGain, 12 + 2 * 2);
}

TEST(ProgressionClassTests, TheTableChanges_AtLevelEleven) {
    const auto service = Configured();
    EXPECT_EQ(service.CalculateProgression(CharacterClass::Warrior, 10, 12, Attributes{}).hitPointGain, 16);
}

TEST(ProgressionClassTests, AutomaticPoints_GoToThePrimariesInTurn) {
    const auto service = Configured();
    const auto result = service.CalculateProgression(CharacterClass::Warrior, 1, 4, Attributes{});
    EXPECT_EQ(result.attributeGains.strength, 2);
    EXPECT_EQ(result.attributeGains.constitution, 1);
    EXPECT_EQ(result.attributeGains.dexterity, 0);
    EXPECT_EQ(result.attributePointsGained, 3);   // the free ones
}

TEST(ProgressionClassTests, NoGain_WithoutRisingOrWithoutRules) {
    const auto service = Configured();
    EXPECT_FALSE(service.CalculateProgression(CharacterClass::Warrior, 5, 5, Attributes{}).HasLeveledUp());
    EXPECT_EQ(service.CalculateProgression(CharacterClass::Warrior, 5, 3, Attributes{}).levelsGained, 0);
    const auto unknown = service.CalculateProgression(CharacterClass::Mage, 1, 5, Attributes{});
    EXPECT_EQ(unknown.hitPointGain, 0);   // no data for the class: nothing invented
    EXPECT_TRUE(unknown.newAbilities.empty());
    EXPECT_FALSE(service.HasClassRules(CharacterClass::Mage));
}

TEST(ProgressionClassTests, PastTheCap_NothingMore) {
    const auto service = Configured();
    const auto result = service.CalculateProgression(CharacterClass::Warrior, 19, 30, Attributes{});
    EXPECT_EQ(result.newLevel, 20);
    EXPECT_EQ(result.levelsGained, 1);
    EXPECT_EQ(result.hitPointGain, 8);
}

// ============================================================================
// Abilities (the class's spells by their required level)
// ============================================================================

TEST(ProgressionAbilityTests, OpenedOnTheWay_InOrder) {
    const auto service = Configured();
    const auto result = service.CalculateProgression(CharacterClass::Warrior, 2, 5, Attributes{});
    ASSERT_EQ(result.newAbilities.size(), 3u);
    EXPECT_EQ(result.newAbilities[2], "Whirlwind");   // by level
    EXPECT_TRUE(service.CalculateProgression(CharacterClass::Warrior, 5, 6, Attributes{}).newAbilities.empty());
}

TEST(ProgressionAbilityTests, AtALevel) {
    const auto service = Configured();
    EXPECT_EQ(service.GetAbilitiesAtLevel(CharacterClass::Warrior, 3).size(), 2u);
    EXPECT_TRUE(service.GetAbilitiesAtLevel(CharacterClass::Warrior, 4).empty());
}

TEST(ProgressionAbilityTests, Requirements_FollowTheLevel) {
    const auto service = Configured();
    EXPECT_FALSE(service.MeetsAbilityRequirements("Whirlwind", 4, CharacterClass::Warrior));
    EXPECT_TRUE(service.MeetsAbilityRequirements("Whirlwind", 5, CharacterClass::Warrior));
    EXPECT_FALSE(service.MeetsAbilityRequirements("Fireball", 20, CharacterClass::Warrior));
}
