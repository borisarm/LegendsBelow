#include "pch.h"

import LB.Core;
import LB.Character;
import LB.Combat;
import LB.Content;
import LB.ContentRepositories;
import LB.Repositories;
import std;

using namespace LB::Content;
using LB::Character::CharacterClass;
using LB::Combat::ArmorCategory;
using LB::Combat::DamageType;
using LB::Combat::EquipmentSlot;
using LB::Combat::SpellCategory;
using LB::Combat::SpellSchool;
using LB::Combat::WeaponType;

namespace {
    // Find Assets/Data by walking up from the working directory (tests run from x64/<Config>)
    std::optional<std::string> FindContentDirectory() {
        for (auto dir = std::filesystem::current_path(); ; dir = dir.parent_path()) {
            const auto candidate = dir / "Assets" / "Data";
            if (std::filesystem::exists(candidate / "spells.json")) {
                const auto text = candidate.u8string();
                return std::string(text.begin(), text.end());
            }
            if (dir == dir.root_path()) return std::nullopt;
        }
    }

    std::string ErrorsOf(const ContentLoadResult& result) {
        std::string text;
        for (const auto& error : result.errors) text += error + "\n";
        return text;
    }

    bool AnyErrorContains(const ContentLoadResult& result, std::string_view needle) {
        return std::ranges::any_of(result.errors, [needle](const std::string& e) { return e.find(needle) != std::string::npos; });
    }

    // Minimal valid entries; tests override one field at a time
    constexpr const char* ValidSpell = R"({"id":"spell_a","name":"A","school":"Evocation","level":1,"categories":["Damage"],"manaCost":5,"castTimeSeconds":1.0})";
    constexpr const char* ValidWeapon = R"({"id":"weapon_a","name":"W","category":"Sword","allowedClasses":["Warrior"],"damageDice":"1d6","damageType":["Slashing"],"attackTimeSeconds":1.0})";
    constexpr const char* ValidArmor = R"({"id":"armor_a","name":"H","slot":"Head","category":"Cloth","allowedClasses":["Mage"]})";

    std::string Array(const std::string& entry) { return "[" + entry + "]"; }
}

// ============================================================================
// Shipped Content Tests (Assets/Data)
// ============================================================================

class ShippedContentTests : public ::testing::Test {
protected:
    void SetUp() override {
        const auto dir = FindContentDirectory();
        ASSERT_TRUE(dir.has_value()) << "Assets/Data not found above " << std::filesystem::current_path().string();
        result_ = content_.LoadFromDirectory(*dir);
        ASSERT_TRUE(result_.Succeeded()) << ErrorsOf(result_);
    }

    ContentDatabase content_;
    ContentLoadResult result_;
};

TEST_F(ShippedContentTests, LoadsAllEntries) {
    EXPECT_EQ(content_.GetSpells().size(), 45u);
    EXPECT_EQ(content_.GetWeapons().size(), 12u);
    EXPECT_EQ(content_.GetArmour().size(), 15u);
}

TEST_F(ShippedContentTests, ArcaneBolt_FieldsMapped) {
    const auto* spell = content_.FindSpell("spell_evocation_arcane_bolt_l1");
    ASSERT_NE(spell, nullptr);
    EXPECT_EQ(spell->name, "Arcane Bolt");
    EXPECT_EQ(spell->school, SpellSchool::Evocation);
    EXPECT_EQ(spell->level, 1);
    EXPECT_TRUE(spell->HasCategory(SpellCategory::Damage));
    ASSERT_TRUE(spell->requiredClass.has_value());
    EXPECT_EQ(*spell->requiredClass, CharacterClass::Mage);
    EXPECT_EQ(spell->manaCost, 10);
    EXPECT_FLOAT_EQ(spell->castTimeSeconds, 0.5f);
    EXPECT_FLOAT_EQ(spell->range, 12.0f);
    EXPECT_EQ(spell->baseDamage, 8);
    EXPECT_EQ(spell->damageType, DamageType::Arcane);
}

TEST_F(ShippedContentTests, GenericSpells_UsableByAnyClass) {
    const auto* haste = content_.FindSpell("spell_generic_haste_l2");
    ASSERT_NE(haste, nullptr);
    EXPECT_EQ(haste->school, SpellSchool::Generic);
    EXPECT_FALSE(haste->requiredClass.has_value());
    EXPECT_TRUE(haste->CanBeCastBy(CharacterClass::Warrior));
}

TEST_F(ShippedContentTests, RadiantAndNecrotic_MapToHolyAndShadow) {
    const bool anyHoly = std::ranges::any_of(content_.GetSpells(), [](const SpellDefinition& s) { return s.damageType == DamageType::Holy; });
    const bool anyShadow = std::ranges::any_of(content_.GetSpells(), [](const SpellDefinition& s) { return s.damageType == DamageType::Shadow; });
    EXPECT_TRUE(anyHoly);
    EXPECT_TRUE(anyShadow);
}

TEST_F(ShippedContentTests, Weapons_DiceCritAndCategoriesParsed) {
    const auto* sword = content_.FindWeapon("weapon_sword_rusted_shortsword_t1");
    ASSERT_NE(sword, nullptr);
    EXPECT_EQ(sword->category, WeaponType::Sword);
    EXPECT_EQ(sword->damageDice.count, 1);
    EXPECT_EQ(sword->damageDice.sides, 6);
    EXPECT_EQ(sword->critRangeMin, 20);
    EXPECT_TRUE(sword->IsUsableBy(CharacterClass::Rogue));
    EXPECT_FALSE(sword->IsUsableBy(CharacterClass::Mage));

    const bool anyWideCrit = std::ranges::any_of(content_.GetWeapons(), [](const WeaponDefinition& w) { return w.critRangeMin == 19; });
    const bool anyTwoDice = std::ranges::any_of(content_.GetWeapons(), [](const WeaponDefinition& w) { return w.damageDice.count == 2; });
    const bool anyPolearm = std::ranges::any_of(content_.GetWeapons(), [](const WeaponDefinition& w) { return w.category == WeaponType::Spear; });
    EXPECT_TRUE(anyWideCrit);
    EXPECT_TRUE(anyTwoDice);
    EXPECT_TRUE(anyPolearm);
}

TEST_F(ShippedContentTests, Armour_SlotsResistancesAndShields) {
    const auto* hood = content_.FindArmor("armor_head_novice_hood_t1");
    ASSERT_NE(hood, nullptr);
    EXPECT_EQ(hood->slot, EquipmentSlot::Head);
    EXPECT_EQ(hood->category, ArmorCategory::Cloth);
    EXPECT_TRUE(hood->elementalAR.contains(DamageType::Acid));
    EXPECT_TRUE(hood->spiritualAR.contains(DamageType::Psychic));

    const bool anyHands = std::ranges::any_of(content_.GetArmour(), [](const ArmorDefinition& a) { return a.slot == EquipmentSlot::Hands; });
    const auto shield = std::ranges::find(content_.GetArmour(), ArmorCategory::Shield, &ArmorDefinition::category);
    EXPECT_TRUE(anyHands);
    ASSERT_NE(shield, content_.GetArmour().end());
    EXPECT_GT(shield->blockChance, 0.0f);
}

TEST_F(ShippedContentTests, Ids_AreOneBasedAndUnique) {
    const auto& spells = content_.GetSpells();
    for (std::size_t i = 0; i < spells.size(); ++i) {
        EXPECT_EQ(spells[i].id, static_cast<ContentId>(i + 1));
        EXPECT_EQ(content_.FindSpell(spells[i].id), &spells[i]);
        EXPECT_EQ(content_.FindSpell(spells[i].key), &spells[i]);
    }
}

TEST_F(ShippedContentTests, GetWeaponsFor_FiltersByClassAndLevel) {
    const auto mageWeapons = content_.GetWeaponsFor(CharacterClass::Mage, 1);

    ASSERT_FALSE(mageWeapons.empty());
    for (const auto* weapon : mageWeapons) {
        EXPECT_TRUE(weapon->IsUsableBy(CharacterClass::Mage));
        EXPECT_LE(weapon->requiredLevel, 1);
    }
}

TEST_F(ShippedContentTests, SpellRepository_OverShippedContent) {
    auto shared = std::make_shared<const ContentDatabase>(content_);
    LB::Repositories::SpellRepository repo = MakeContentSpellRepository(shared);

    EXPECT_EQ(repo->GetAllSpellIds().size(), 45u);

    const auto* bolt = content_.FindSpell("spell_evocation_arcane_bolt_l1");
    auto spell = repo->GetSpell(bolt->id);
    ASSERT_TRUE(spell.has_value());
    EXPECT_EQ(spell->name, "Arcane Bolt");
    EXPECT_EQ(spell->manaCost, 10);
    EXPECT_EQ(spell->baseDamage, 8);

    // Warriors have no class spells: only spells without a class restriction
    // (Generic, and currently Shadow, which the data leaves unassigned)
    const auto warriorSpells = repo->GetSpellsByClass(CharacterClass::Warrior);
    EXPECT_FALSE(warriorSpells.empty());
    for (auto id : warriorSpells) {
        EXPECT_FALSE(content_.FindSpell(id)->requiredClass.has_value());
    }
    for (auto id : repo->GetSpellsByClass(CharacterClass::Mage)) {
        const auto& required = content_.FindSpell(id)->requiredClass;
        EXPECT_TRUE(!required || *required == CharacterClass::Mage);
    }
    EXPECT_FALSE(repo->GetSpellsByCategory(SpellCategory::Heal).empty());
    EXPECT_TRUE(repo->GetSchoolDescription(SpellSchool::Generic).has_value());
}

// ============================================================================
// Content Validation Tests
// ============================================================================

TEST(ContentValidationTests, MinimalValidEntries_Load) {
    ContentDatabase content;

    auto result = content.LoadFromJson(Array(ValidSpell), Array(ValidWeapon), Array(ValidArmor));

    ASSERT_TRUE(result.Succeeded()) << ErrorsOf(result);
    EXPECT_EQ(content.GetSpells().size(), 1u);
    EXPECT_FALSE(content.GetSpells()[0].requiredClass.has_value());
}

TEST(ContentValidationTests, UnknownFields_Ignored) {
    ContentDatabase content;
    const std::string spell = R"({"id":"spell_a","name":"A","school":"Evocation","level":1,"categories":["Damage"],"manaCost":5,"castTimeSeconds":1.0,"futureField":{"x":1}})";

    auto result = content.LoadFromJson(Array(spell), "[]", "[]");

    EXPECT_TRUE(result.Succeeded()) << ErrorsOf(result);
}

TEST(ContentValidationTests, UnknownEnumValue_Rejected) {
    ContentDatabase content;
    const std::string spell = R"({"id":"spell_a","name":"A","school":"Necromancy","level":1,"categories":["Damage"],"manaCost":5,"castTimeSeconds":1.0})";

    auto result = content.LoadFromJson(Array(spell), "[]", "[]");

    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(AnyErrorContains(result, "spells.json: entry 1 (spell_a): 'school' has unknown value 'Necromancy'")) << ErrorsOf(result);
}

TEST(ContentValidationTests, MissingRequiredField_Rejected) {
    ContentDatabase content;
    const std::string weapon = R"({"id":"weapon_a","name":"W","category":"Sword","allowedClasses":["Warrior"],"damageType":["Slashing"],"attackTimeSeconds":1.0})";

    auto result = content.LoadFromJson("[]", Array(weapon), "[]");

    EXPECT_TRUE(AnyErrorContains(result, "missing required field 'damageDice'")) << ErrorsOf(result);
}

TEST(ContentValidationTests, WrongType_Rejected) {
    ContentDatabase content;
    const std::string spell = R"({"id":"spell_a","name":"A","school":"Evocation","level":"one","categories":["Damage"],"manaCost":5,"castTimeSeconds":1.0})";

    auto result = content.LoadFromJson(Array(spell), "[]", "[]");

    EXPECT_TRUE(AnyErrorContains(result, "'level' must be an integer")) << ErrorsOf(result);
}

TEST(ContentValidationTests, DuplicateId_Rejected) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[" + std::string(ValidSpell) + "," + ValidSpell + "]", "[]", "[]");

    EXPECT_TRUE(AnyErrorContains(result, "duplicate id 'spell_a'")) << ErrorsOf(result);
}

TEST(ContentValidationTests, InvalidDiceAndCritRange_Rejected) {
    ContentDatabase content;
    const std::string weapon = R"({"id":"weapon_a","name":"W","category":"Sword","allowedClasses":["Warrior"],"damageDice":"d6","damageType":["Slashing"],"attackTimeSeconds":1.0,"critRange":"18-19"})";

    auto result = content.LoadFromJson("[]", Array(weapon), "[]");

    EXPECT_TRUE(AnyErrorContains(result, "invalid dice 'd6'")) << ErrorsOf(result);
    EXPECT_TRUE(AnyErrorContains(result, "invalid crit range '18-19'")) << ErrorsOf(result);
}

TEST(ContentValidationTests, DiceWithBonus_Parsed) {
    ContentDatabase content;
    const std::string weapon = R"({"id":"weapon_a","name":"W","category":"Sword","allowedClasses":["Warrior"],"damageDice":"2d8+3","damageType":"Slashing","attackTimeSeconds":1.0})";

    auto result = content.LoadFromJson("[]", Array(weapon), "[]");

    ASSERT_TRUE(result.Succeeded()) << ErrorsOf(result);
    const auto& dice = content.GetWeapons()[0].damageDice;
    EXPECT_EQ(dice.count, 2);
    EXPECT_EQ(dice.sides, 8);
    EXPECT_EQ(dice.bonus, 3);
    EXPECT_EQ(content.GetWeapons()[0].damageTypes.size(), 1u);  // single name accepted
}

TEST(ContentValidationTests, InvalidJson_Rejected) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[{", "[]", "[]");

    EXPECT_TRUE(AnyErrorContains(result, "spells.json: not valid JSON")) << ErrorsOf(result);
}

TEST(ContentValidationTests, NotAnArray_Rejected) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[]", "[]", R"({"id":"armor_a"})");

    EXPECT_TRUE(AnyErrorContains(result, "armour.json: expected a JSON array")) << ErrorsOf(result);
}

TEST(ContentValidationTests, AllErrorsReported_AcrossFiles) {
    ContentDatabase content;
    const std::string badSpell = R"({"id":"spell_a","name":"","school":"Evocation","level":0,"categories":[],"manaCost":5,"castTimeSeconds":1.0})";
    const std::string badArmor = R"({"id":"armor_a","name":"H","slot":"Tail","category":"Cloth","allowedClasses":["Mage"]})";

    auto result = content.LoadFromJson(Array(badSpell), "[]", Array(badArmor));

    EXPECT_GE(result.errors.size(), 4u) << ErrorsOf(result);
    EXPECT_TRUE(AnyErrorContains(result, "'name' must not be empty"));
    EXPECT_TRUE(AnyErrorContains(result, "'level' must be at least 1"));
    EXPECT_TRUE(AnyErrorContains(result, "'categories' must list at least one value"));
    EXPECT_TRUE(AnyErrorContains(result, "armour.json: entry 1 (armor_a): 'slot' has unknown value 'Tail'"));
}

TEST(ContentValidationTests, FailedLoad_LeavesDatabaseEmpty) {
    ContentDatabase content;
    ASSERT_TRUE(content.LoadFromJson(Array(ValidSpell), Array(ValidWeapon), Array(ValidArmor)).Succeeded());

    auto result = content.LoadFromJson(Array(ValidSpell), "[{", Array(ValidArmor));

    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(content.IsEmpty());
}

TEST(ContentValidationTests, MissingDirectory_ReportsMissingFiles) {
    ContentDatabase content;

    auto result = content.LoadFromDirectory("Z:/does/not/exist");

    EXPECT_EQ(result.errors.size(), 3u) << ErrorsOf(result);
    EXPECT_TRUE(AnyErrorContains(result, "spells.json: file not found"));
    EXPECT_TRUE(content.IsEmpty());
}

TEST(ContentValidationTests, Lookups_OutOfRange_ReturnNull) {
    ContentDatabase content;
    ASSERT_TRUE(content.LoadFromJson(Array(ValidSpell), Array(ValidWeapon), Array(ValidArmor)).Succeeded());

    EXPECT_EQ(content.FindSpell(0), nullptr);
    EXPECT_EQ(content.FindSpell(2), nullptr);
    EXPECT_EQ(content.FindWeapon("weapon_missing"), nullptr);
    EXPECT_NE(content.FindArmor("armor_a"), nullptr);
}
