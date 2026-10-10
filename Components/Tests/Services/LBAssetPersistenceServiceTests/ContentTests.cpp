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

    EXPECT_EQ(result.errors.size(), 4u) << ErrorsOf(result);  // spells, weapons, armour, classes
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

// ============================================================================
// Shipped Class Data Tests (classes.json)
// ============================================================================

TEST_F(ShippedContentTests, Classes_AllFourDefined) {
    ASSERT_EQ(content_.GetClasses().size(), 4u);
    for (auto c : { CharacterClass::Warrior, CharacterClass::Rogue, CharacterClass::Mage, CharacterClass::Cleric }) {
        EXPECT_NE(content_.FindClass(c), nullptr);
    }
    EXPECT_EQ(content_.FindClass("class_mage"), content_.FindClass(CharacterClass::Mage));
}

TEST_F(ShippedContentTests, Warrior_MatchesDesign) {
    const auto* warrior = content_.FindClass(CharacterClass::Warrior);
    ASSERT_NE(warrior, nullptr);

    // game-design.md: STR 16, DEX 12, CON 14, INT 8, WIS 10, CHA 10
    EXPECT_EQ(warrior->startingAttributes.strength, 16);
    EXPECT_EQ(warrior->startingAttributes.dexterity, 12);
    EXPECT_EQ(warrior->startingAttributes.constitution, 14);
    EXPECT_EQ(warrior->startingAttributes.intelligence, 8);
    EXPECT_EQ(warrior->resource, "Stamina");
    EXPECT_TRUE(warrior->CanWear(ArmorCategory::Heavy));
    EXPECT_TRUE(warrior->spellSchools.empty());
}

TEST_F(ShippedContentTests, WeaponAdjustments_SwordColumn) {
    // Class adjustment model: a sword is +3 for Warriors, +1 Rogues, -1 Mages, -5 Clerics
    EXPECT_EQ(content_.FindClass(CharacterClass::Warrior)->GetWeaponAdjustment(WeaponType::Sword), 3);
    EXPECT_EQ(content_.FindClass(CharacterClass::Rogue)->GetWeaponAdjustment(WeaponType::Sword), 1);
    EXPECT_EQ(content_.FindClass(CharacterClass::Mage)->GetWeaponAdjustment(WeaponType::Sword), -1);
    EXPECT_EQ(content_.FindClass(CharacterClass::Cleric)->GetWeaponAdjustment(WeaponType::Sword), -5);
}

TEST_F(ShippedContentTests, WeaponAdjustments_EveryClassCoversEveryCategory) {
    for (const auto& definition : content_.GetClasses()) {
        EXPECT_EQ(definition.weaponAdjustments.size(), 9u) << definition.key;  // Polearm is Spear
        EXPECT_TRUE(definition.weaponAdjustments.contains(WeaponType::Spear)) << definition.key;
    }
}

TEST_F(ShippedContentTests, ProgressionTotals_MatchDesignTables) {
    // design-character-system.md section 3: level 1 base + per-level gains
    EXPECT_EQ(content_.FindClass(CharacterClass::Warrior)->TotalsAt(1).maxHitPoints, 12);
    EXPECT_EQ(content_.FindClass(CharacterClass::Warrior)->TotalsAt(20).maxHitPoints, 12 + 9 * 8 + 10 * 10);
    EXPECT_EQ(content_.FindClass(CharacterClass::Rogue)->TotalsAt(20).maxHitPoints, 8 + 9 * 6 + 10 * 7);
    EXPECT_EQ(content_.FindClass(CharacterClass::Mage)->TotalsAt(20).maxManaPoints, 10 + 9 * 8 + 10 * 9);
    EXPECT_EQ(content_.FindClass(CharacterClass::Cleric)->TotalsAt(10).maxHitPoints, 10 + 9 * 7);

    const auto mage = content_.FindClass(CharacterClass::Mage)->TotalsAt(20);
    EXPECT_EQ(mage.meleeAttackBonus, 0);
    EXPECT_EQ(mage.rangedAttackBonus, 11);  // +2 at level 1, +1 on every odd level 3-19
    EXPECT_EQ(mage.mentalSaveBonus, 11);
}

TEST_F(ShippedContentTests, TotalsAt_ClampsLevel) {
    const auto* warrior = content_.FindClass(CharacterClass::Warrior);

    EXPECT_EQ(warrior->TotalsAt(0).maxHitPoints, warrior->TotalsAt(1).maxHitPoints);
    EXPECT_EQ(warrior->TotalsAt(99).maxHitPoints, warrior->TotalsAt(20).maxHitPoints);
}

TEST_F(ShippedContentTests, Classes_SpellSchoolsAndSpeeds) {
    EXPECT_TRUE(content_.FindClass(CharacterClass::Mage)->HasSpellSchool(SpellSchool::Evocation));
    EXPECT_TRUE(content_.FindClass(CharacterClass::Cleric)->HasSpellSchool(SpellSchool::Holy));
    EXPECT_FALSE(content_.FindClass(CharacterClass::Cleric)->HasSpellSchool(SpellSchool::Frost));
    EXPECT_GT(content_.FindClass(CharacterClass::Rogue)->baseMovementSpeed,
              content_.FindClass(CharacterClass::Warrior)->baseMovementSpeed);
}

// ============================================================================
// Class Validation Tests
// ============================================================================

namespace {
    std::string Progression(int levels) {
        std::string rows;
        for (int level = 1; level <= levels; ++level) {
            if (level > 1) rows += ",";
            rows += std::format(R"({{"level":{},"hpGain":5,"mpGain":5,"meleeAttackBonus":0,"rangedAttackBonus":0,"physicalSaveBonus":0,"mentalSaveBonus":0}})", level);
        }
        return "[" + rows + "]";
    }

    constexpr const char* AllWeaponAdjustments =
        R"({"Sword":0,"Axe":0,"Mace":0,"Dagger":0,"Polearm":0,"Bow":0,"Crossbow":0,"Staff":0,"Wand":0})";

    std::string ClassJson(const std::string& characterClass, const std::string& weapons = AllWeaponAdjustments,
                          const std::string& armour = R"(["Cloth"])", const std::string& schools = "[]", int levels = 20) {
        std::string lower = characterClass;
        std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return std::format(R"({{"id":"class_{}","class":"{}","name":"{}","resource":"Mana","baseMovementSpeed":4.0,)"
                           R"("startingAttributes":{{"STR":10,"DEX":10,"CON":10,"INT":10,"WIS":10,"CHA":10}},)"
                           R"("armorProficiencies":{},"weaponAdjustments":{},"spellSchools":{},"progression":{}}})",
                           lower, characterClass, characterClass, armour, weapons, schools, Progression(levels));
    }

    std::string AllClasses(const std::string& warriorOverride = {}) {
        return "[" + (warriorOverride.empty() ? ClassJson("Warrior") : warriorOverride) + "," + ClassJson("Rogue") + "," +
               ClassJson("Mage") + "," + ClassJson("Cleric") + "]";
    }
}

TEST(ClassValidationTests, ValidClasses_Load) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses());

    ASSERT_TRUE(result.Succeeded()) << ErrorsOf(result);
    EXPECT_EQ(content.GetClasses().size(), 4u);
}

TEST(ClassValidationTests, MissingClass_Rejected) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[]", "[]", "[]", "[" + ClassJson("Warrior") + "," + ClassJson("Rogue") + "," + ClassJson("Mage") + "]");

    EXPECT_TRUE(AnyErrorContains(result, "class 'Cleric' is not defined")) << ErrorsOf(result);
}

TEST(ClassValidationTests, DuplicateClass_Rejected) {
    ContentDatabase content;
    std::string second = ClassJson("Warrior");
    second.replace(second.find("class_warrior"), 13, "class_warrior2");

    auto result = content.LoadFromJson("[]", "[]", "[]", "[" + second + "," + AllClasses().substr(1));

    EXPECT_TRUE(AnyErrorContains(result, "class 'Warrior' is defined 2 times")) << ErrorsOf(result);
}

TEST(ClassValidationTests, IncompleteProgression_Rejected) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", AllWeaponAdjustments, R"(["Cloth"])", "[]", 19)));

    EXPECT_TRUE(AnyErrorContains(result, "'progression' must list levels 1 to 20 in order")) << ErrorsOf(result);
}

TEST(ClassValidationTests, UnknownWeaponCategory_Rejected) {
    ContentDatabase content;
    std::string weapons = AllWeaponAdjustments;
    weapons.insert(1, R"("Whip":2,)");

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", weapons)));

    EXPECT_TRUE(AnyErrorContains(result, "'weaponAdjustments' has unknown weapon category 'Whip'")) << ErrorsOf(result);
}

TEST(ClassValidationTests, MissingWeaponCategory_Rejected) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", R"({"Sword":3})")));

    EXPECT_TRUE(AnyErrorContains(result, "'weaponAdjustments' has no value for 'Axe'")) << ErrorsOf(result);
    EXPECT_TRUE(AnyErrorContains(result, "'weaponAdjustments' has no value for 'Spear'")) << ErrorsOf(result);
}

TEST(ClassValidationTests, PolearmAndSpearBoth_Rejected) {
    ContentDatabase content;
    std::string weapons = AllWeaponAdjustments;
    weapons.insert(1, R"("Spear":1,)");

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", weapons)));

    EXPECT_TRUE(AnyErrorContains(result, "lists 'Spear' more than once")) << ErrorsOf(result);
}

TEST(ClassValidationTests, WeaponAllowedForAnyAdjustment_Accepted) {
    ContentDatabase content;
    // ValidWeapon is a Sword allowed for Warrior; a -5 adjustment is a penalty, not a ban
    std::string weapons = AllWeaponAdjustments;
    weapons.replace(weapons.find(R"("Sword":0)"), 9, R"("Sword":-5)");

    auto result = content.LoadFromJson("[]", Array(ValidWeapon), "[]", AllClasses(ClassJson("Warrior", weapons)));

    EXPECT_TRUE(result.Succeeded()) << ErrorsOf(result);
}

TEST(ClassValidationTests, ArmourAllowedForClassWithoutProficiency_Rejected) {
    ContentDatabase content;
    // ValidArmor is Cloth allowed for Mage; this Mage can only wear Light
    std::string classes = "[" + ClassJson("Warrior") + "," + ClassJson("Rogue") + "," + ClassJson("Mage", AllWeaponAdjustments, R"(["Light"])") + "," + ClassJson("Cleric") + "]";

    auto result = content.LoadFromJson("[]", "[]", Array(ValidArmor), classes);

    EXPECT_TRUE(AnyErrorContains(result, "armour.json: armor_a: allows Mage but class_mage has no 'Cloth' armour proficiency")) << ErrorsOf(result);
}

TEST(ClassValidationTests, ClassSpellOutsideClassSchools_Rejected) {
    ContentDatabase content;
    const std::string spell = R"({"id":"spell_a","name":"A","school":"Holy","level":1,"categories":["Heal"],"requiredClass":"Mage","manaCost":5,"castTimeSeconds":1.0})";

    auto result = content.LoadFromJson(Array(spell), "[]", "[]", AllClasses());

    EXPECT_TRUE(AnyErrorContains(result, "spells.json: spell_a: requires Mage but class_mage has no 'Holy' spell school")) << ErrorsOf(result);
}

TEST(ClassValidationTests, BadStartingAttributes_Rejected) {
    ContentDatabase content;
    std::string warrior = ClassJson("Warrior");
    warrior.replace(warrior.find(R"("STR":10)"), 8, R"("STR":40)");

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(warrior));

    EXPECT_TRUE(AnyErrorContains(result, "'startingAttributes' values must be in [1, 30]")) << ErrorsOf(result);
}
