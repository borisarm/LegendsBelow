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
    EXPECT_EQ(content_.GetSpells().size(), 55u);   // 45, the rogue's four abilities, the warrior's three, the healing circle, two enemies' spells
    EXPECT_EQ(content_.GetWeapons().size(), 29u);   // 12, and tier 2 and 3 of each kind (17)
    EXPECT_EQ(content_.GetArmour().size(), 41u);   // 15, and tier 2 and 3 of each kind (26)
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
    EXPECT_EQ(spell->manaCost, 6);
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

    EXPECT_EQ(repo->GetAllSpellIds().size(), 55u);

    const auto* bolt = content_.FindSpell("spell_evocation_arcane_bolt_l1");
    auto spell = repo->GetSpell(bolt->id);
    ASSERT_TRUE(spell.has_value());
    EXPECT_EQ(spell->name, "Arcane Bolt");
    EXPECT_EQ(spell->manaCost, 6);
    EXPECT_EQ(spell->baseDamage, 8);

    // Warriors get their own martial abilities and the spells without a class restriction
    // (Generic, and currently Shadow, which the data leaves unassigned)
    const auto warriorSpells = repo->GetSpellsByClass(CharacterClass::Warrior);
    EXPECT_FALSE(warriorSpells.empty());
    for (auto id : warriorSpells) {
        const auto& required = content_.FindSpell(id)->requiredClass;
        EXPECT_TRUE(!required.has_value() || *required == CharacterClass::Warrior);
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

    EXPECT_EQ(result.errors.size(), 12u) << ErrorsOf(result);  // spells, weapons, armour, classes, enemies, consumables, loot, statuses, rarities, affixes, allies, objectives
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
    EXPECT_FLOAT_EQ(warrior->GetArmorAdjustment(ArmorCategory::Heavy).movementScale, 0.5f);  // signature
    EXPECT_TRUE(warrior->IsPrimaryAttribute(LB::AttributeType::Strength));
    EXPECT_TRUE(warrior->IsPrimaryAttribute(LB::AttributeType::Constitution));
    EXPECT_FALSE(warrior->IsPrimaryAttribute(LB::AttributeType::Intelligence));
    EXPECT_EQ(warrior->spellSchools, std::vector<SpellSchool>{ SpellSchool::Utility });   // its martial abilities
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
    EXPECT_EQ(content_.FindClass(CharacterClass::Rogue)->TotalsAt(20).maxHitPoints, 11 + 9 * 7 + 10 * 8);
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

    constexpr const char* AllArmorAdjustments =
        R"({"Cloth":{"movement":1,"casting":1},"Light":{"movement":1,"casting":1},"Medium":{"movement":1,"casting":1},)"
        R"("Heavy":{"movement":1,"casting":1},"Shield":{"movement":1,"casting":1}})";

    constexpr const char* DefaultGrowth =
        R"({"autoPointsPerLevel":1,"freePointsPerLevel":1,"primaryAttributeCap":25,"otherAttributeCap":18})";

    std::string ClassJson(const std::string& characterClass, const std::string& weapons = AllWeaponAdjustments,
                          const std::string& armour = AllArmorAdjustments, const std::string& schools = "[]", int levels = 20,
                          const std::string& growth = DefaultGrowth) {
        std::string lower = characterClass;
        std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return std::format(R"({{"id":"class_{}","class":"{}","name":"{}","resource":"Mana","baseMovementSpeed":4.0,)"
                           R"("startingAttributes":{{"STR":10,"DEX":10,"CON":10,"INT":10,"WIS":10,"CHA":10}},)"
                           R"("primaryAttributes":["STR","CON"],"attributeGrowth":{},)"
                           R"("armorAdjustments":{},"weaponAdjustments":{},"spellSchools":{},"progression":{}}})",
                           lower, characterClass, characterClass, growth, armour, weapons, schools, Progression(levels));
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

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", AllWeaponAdjustments, AllArmorAdjustments, "[]", 19)));

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

TEST(ClassValidationTests, MissingArmorCategory_Rejected) {
    ContentDatabase content;

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", AllWeaponAdjustments, R"({"Cloth":{"movement":1,"casting":1}})")));

    EXPECT_TRUE(AnyErrorContains(result, "'armorAdjustments' has no value for 'Heavy'")) << ErrorsOf(result);
}

TEST(ClassValidationTests, NegativeArmorScale_Rejected) {
    ContentDatabase content;
    std::string armour = AllArmorAdjustments;
    armour.replace(armour.find(R"("Heavy":{"movement":1)"), 21, R"("Heavy":{"movement":-1)");

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", AllWeaponAdjustments, armour)));

    EXPECT_TRUE(AnyErrorContains(result, "armorAdjustments.Heavy: scales must not be negative")) << ErrorsOf(result);
}

TEST(ClassValidationTests, ArmourAllowedForAnyClass_Accepted) {
    ContentDatabase content;
    // ValidArmor is Cloth allowed for Mage; penalties are scaled, nothing is banned

    auto result = content.LoadFromJson("[]", "[]", Array(ValidArmor), AllClasses());

    EXPECT_TRUE(result.Succeeded()) << ErrorsOf(result);
}

TEST(ClassValidationTests, PrimaryCapBelowOtherCap_Rejected) {
    ContentDatabase content;
    const std::string growth = R"({"autoPointsPerLevel":1,"freePointsPerLevel":1,"primaryAttributeCap":15,"otherAttributeCap":18})";

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(ClassJson("Warrior", AllWeaponAdjustments, AllArmorAdjustments, "[]", 20, growth)));

    EXPECT_TRUE(AnyErrorContains(result, "'primaryAttributeCap' must be at least 'otherAttributeCap'")) << ErrorsOf(result);
}

TEST(ClassValidationTests, StartingAttributeAboveCap_Rejected) {
    ContentDatabase content;
    std::string warrior = ClassJson("Warrior");
    warrior.replace(warrior.find(R"("INT":10)"), 8, R"("INT":20)");  // INT is not a Warrior primary: cap 18

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(warrior));

    EXPECT_TRUE(AnyErrorContains(result, "starting INT 20 is above the class cap 18")) << ErrorsOf(result);
}

TEST(ClassValidationTests, UnknownPrimaryAttribute_Rejected) {
    ContentDatabase content;
    std::string warrior = ClassJson("Warrior");
    warrior.replace(warrior.find(R"(["STR","CON"])"), 13, R"(["STR","LUCK"])");

    auto result = content.LoadFromJson("[]", "[]", "[]", AllClasses(warrior));

    EXPECT_TRUE(AnyErrorContains(result, "'primaryAttributes' has unknown value 'LUCK'")) << ErrorsOf(result);
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

// ============================================================================
// Enemy Tests (enemies.json)
// ============================================================================

namespace {
    constexpr const char* ValidEnemy = R"({"id":"enemy_a","name":"E","hitPoints":5,"armorClass":11,"attackBonus":2,"damageDice":"1d4","damageType":"Slashing","attackTimeSeconds":1.0,"attackRange":1.0,"movementSpeed":2.0,"model":"E.glb"})";

    // ValidEnemy with one field replaced or added ("field":value)
    std::string EnemyWith(std::string_view field, std::string_view value) {
        std::string enemy = ValidEnemy;
        const auto key = std::format("\"{}\":", field);
        const auto at = enemy.find(key);
        if (at == std::string::npos) {
            enemy.insert(enemy.size() - 1, std::format(",{}{}", key, value));
            return enemy;
        }
        const auto start = at + key.size();
        auto end = start;
        int depth = 0;
        for (; end < enemy.size(); ++end) {
            const char c = enemy[end];
            if (c == '{' || c == '[') ++depth;
            if (c == '}' || c == ']') { if (depth == 0) break; --depth; }
            if (c == ',' && depth == 0) break;
        }
        enemy.replace(start, end - start, value);
        return enemy;
    }

    ContentLoadResult LoadEnemies(ContentDatabase& content, const std::string& enemies) {
        return content.LoadFromJson("[]", "[]", "[]", "[]", enemies);
    }
}

TEST_F(ShippedContentTests, Enemies_SkeletonMinionAndWarrior) {
    ASSERT_EQ(content_.GetEnemies().size(), 15u);   // the crypts' skeletons and golem, the caves' and ruins' creatures, three bosses

    const auto* minion = content_.FindEnemy("enemy_skeleton_minion");
    ASSERT_NE(minion, nullptr);
    EXPECT_EQ(minion->model, "Skeleton_Minion.glb");
    EXPECT_EQ(minion->mainHandModel, "Skeleton_Blade.gltf");
    EXPECT_EQ(minion->damageDice.sides, 4);
    EXPECT_FLOAT_EQ(minion->GetResistance(DamageType::Piercing), 0.25f);
    EXPECT_LT(minion->GetResistance(DamageType::Bludgeoning), 0.0f);   // bones break

    const auto* warrior = content_.FindEnemy("enemy_skeleton_warrior");
    ASSERT_NE(warrior, nullptr);
    EXPECT_TRUE(warrior->tank);                            // leads its group
    EXPECT_FALSE(minion->tank);

    const auto* warlord = content_.FindEnemy("enemy_golem_warlord");
    ASSERT_NE(warlord, nullptr);
    EXPECT_TRUE(warlord->boss);
    ASSERT_TRUE(warlord->HasSlam());                       // a telegraphed slam around itself
    EXPECT_FALSE(warlord->slamAtTarget);
    EXPECT_EQ(warlord->slamDamage.count, 3);
    EXPECT_TRUE(content_.FindEnemy("enemy_necromancer")->slamAtTarget);   // an eruption under the player
    EXPECT_GT(warrior->hitPoints, minion->hitPoints);
    EXPECT_GT(warrior->armorClass, minion->armorClass);
    EXPECT_EQ(warrior->offHandModel, "Skeleton_Shield_Large_A.gltf");

    // Attack clips from the KayKit CombatMelee set, with their strike moments
    EXPECT_EQ(minion->attackAnimation, "Melee_1H_Attack_Slice_Diagonal");
    EXPECT_EQ(warrior->attackAnimation, "Melee_1H_Attack_Chop");
    EXPECT_GT(warrior->attackImpactFraction, minion->attackImpactFraction);
}

TEST(EnemyValidationTests, ValidEnemy_LoadsWithDefaults) {
    ContentDatabase content;

    auto result = LoadEnemies(content, Array(ValidEnemy));

    ASSERT_TRUE(result.Succeeded()) << ErrorsOf(result);
    const auto* enemy = content.FindEnemy("enemy_a");
    ASSERT_NE(enemy, nullptr);
    EXPECT_EQ(content.FindEnemy(enemy->id), enemy);
    EXPECT_EQ(enemy->hitPoints, 5);
    EXPECT_EQ(enemy->damageType, DamageType::Slashing);
    EXPECT_EQ(enemy->level, 1);
    EXPECT_EQ(enemy->critRangeMin, 20);
    EXPECT_FLOAT_EQ(enemy->critMultiplier, 1.5f);
    EXPECT_FLOAT_EQ(enemy->modelScale, 0.5f);
    EXPECT_FLOAT_EQ(enemy->GetResistance(DamageType::Fire), 0.0f);
    EXPECT_TRUE(enemy->mainHandModel.empty());
    EXPECT_TRUE(enemy->attackAnimation.empty());
    EXPECT_FLOAT_EQ(enemy->attackImpactFraction, 0.45f);
}

TEST(EnemyValidationTests, Resistances_ReadAsFractions) {
    ContentDatabase content;

    auto result = LoadEnemies(content, Array(EnemyWith("resistances", R"({"Piercing":0.5,"Radiant":-0.5})")));

    ASSERT_TRUE(result.Succeeded()) << ErrorsOf(result);
    const auto* enemy = content.FindEnemy("enemy_a");
    EXPECT_FLOAT_EQ(enemy->GetResistance(DamageType::Piercing), 0.5f);
    EXPECT_FLOAT_EQ(enemy->GetResistance(DamageType::Holy), -0.5f);
}

TEST(EnemyValidationTests, BadValues_AllReported) {
    ContentDatabase content;

    auto bad = LoadEnemies(content, Array(EnemyWith("attackTimeSeconds", "0")));
    EXPECT_TRUE(AnyErrorContains(bad, "'attackTimeSeconds' must be positive")) << ErrorsOf(bad);

    bad = LoadEnemies(content, Array(EnemyWith("movementSpeed", "-1")));
    EXPECT_TRUE(AnyErrorContains(bad, "'movementSpeed' must be positive")) << ErrorsOf(bad);

    bad = LoadEnemies(content, Array(EnemyWith("resistances", R"({"Fire":1.5,"Mud":0.1})")));
    EXPECT_TRUE(AnyErrorContains(bad, "'resistances' value for 'Fire' must be in [-1, 1]")) << ErrorsOf(bad);
    EXPECT_TRUE(AnyErrorContains(bad, "'resistances' has unknown value 'Mud'")) << ErrorsOf(bad);

    bad = LoadEnemies(content, Array(EnemyWith("attackImpactFraction", "1.0")));
    EXPECT_TRUE(AnyErrorContains(bad, "'attackImpactFraction' must be between 0 and 1")) << ErrorsOf(bad);

    bad = LoadEnemies(content, Array(EnemyWith("damageDice", R"("lots")")));
    EXPECT_TRUE(AnyErrorContains(bad, "'damageDice' has invalid dice 'lots'")) << ErrorsOf(bad);

    bad = LoadEnemies(content, Array(EnemyWith("hitPoints", "0")));
    EXPECT_TRUE(AnyErrorContains(bad, "enemies.json: entry 1 (enemy_a): 'hitPoints' must be at least 1")) << ErrorsOf(bad);
    EXPECT_TRUE(content.IsEmpty());
}

TEST(EnemyValidationTests, MissingModel_Rejected) {
    ContentDatabase content;
    std::string enemy = ValidEnemy;
    enemy.erase(enemy.find(R"(,"model":"E.glb")"), std::string_view(R"(,"model":"E.glb")").size());

    auto result = LoadEnemies(content, Array(enemy));

    EXPECT_TRUE(AnyErrorContains(result, "missing required field 'model'")) << ErrorsOf(result);
}

TEST(EnemyValidationTests, DuplicateIds_Rejected) {
    ContentDatabase content;

    auto result = LoadEnemies(content, "[" + std::string(ValidEnemy) + "," + ValidEnemy + "]");

    EXPECT_TRUE(AnyErrorContains(result, "duplicate id 'enemy_a'")) << ErrorsOf(result);
}

// ============================================================================
// Consumable and Loot Tests (consumables.json, loot.json)
// ============================================================================

namespace {
    constexpr const char* ValidPotion = R"({"id":"potion_a","name":"P","healing":"2d4+2","model":"p.glb"})";
    constexpr const char* ValidTable = R"({"id":"loot_a","goldChance":0.5,"gold":"1d6","itemChance":0.5,"items":[{"item":"potion_a","weight":3},{"item":"weapon_a"}]})";

    ContentLoadResult LoadLoot(ContentDatabase& content, const std::string& consumables, const std::string& loot, const std::string& enemies = "[]") {
        return content.LoadFromJson("[]", Array(ValidWeapon), "[]", "[]", enemies, consumables, loot);
    }
}

TEST_F(ShippedContentTests, Loot_TablesForBothSkeletonsAndAPotion) {
    const auto* potion = content_.FindConsumable("consumable_potion_healing_minor");
    ASSERT_NE(potion, nullptr);
    EXPECT_GT(potion->healing.count, 0);
    for (const auto& enemy : content_.GetEnemies()) {
        const auto* table = content_.FindLootTable(enemy.lootTable);
        ASSERT_NE(table, nullptr) << enemy.key;
        EXPECT_GT(table->TotalWeight(), 0);
    }
}

TEST(LootValidationTests, ValidTable_LoadsEntriesAndWeights) {
    ContentDatabase content;

    auto result = LoadLoot(content, Array(ValidPotion), Array(ValidTable));

    ASSERT_TRUE(result.Succeeded()) << ErrorsOf(result);
    const auto* table = content.FindLootTable("loot_a");
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->items.size(), 2u);
    EXPECT_EQ(table->items[1].weight, 1);    // default
    EXPECT_EQ(table->TotalWeight(), 4);
    EXPECT_EQ(content.FindConsumable("potion_a")->healing.bonus, 2);
    EXPECT_EQ(content.FindConsumable("potion_a")->maxStack, 10);
}

TEST(LootValidationTests, UnknownItemsAndTables_Rejected) {
    ContentDatabase content;
    const std::string table = R"({"id":"loot_a","itemChance":1.0,"items":[{"item":"nothing_here"}]})";
    std::string enemy = ValidEnemy;
    enemy.insert(enemy.size() - 1, R"(,"lootTable":"loot_missing")");

    auto result = LoadLoot(content, Array(ValidPotion), Array(table), Array(enemy));

    EXPECT_TRUE(AnyErrorContains(result, "loot.json: loot_a: item 'nothing_here' is not a weapon, armour piece, or consumable")) << ErrorsOf(result);
    EXPECT_TRUE(AnyErrorContains(result, "enemies.json: enemy_a: loot table 'loot_missing' is not in loot.json")) << ErrorsOf(result);
    EXPECT_TRUE(content.IsEmpty());
}

TEST(LootValidationTests, BadChancesAndWeights_Rejected) {
    ContentDatabase content;
    const std::string table = R"({"id":"loot_a","goldChance":1.5,"gold":"1d4","itemChance":0.5,"items":[{"item":"potion_a","weight":0}]})";

    auto result = LoadLoot(content, Array(ValidPotion), Array(table));

    EXPECT_TRUE(AnyErrorContains(result, "'goldChance' must be in [0, 1]")) << ErrorsOf(result);
    EXPECT_TRUE(AnyErrorContains(result, "'weight' must be at least 1")) << ErrorsOf(result);
}

TEST(LootValidationTests, ItemChanceWithoutItems_Rejected) {
    ContentDatabase content;

    auto result = LoadLoot(content, "[]", Array(R"({"id":"loot_a","itemChance":0.5,"items":[]})"));

    EXPECT_TRUE(AnyErrorContains(result, "'items' must list at least one item")) << ErrorsOf(result);
}

// ============================================================================
// Ranged Combat Tests
// ============================================================================

namespace {
    std::string RangedEnemy(std::string_view speed, std::string_view model, std::string_view keepAway) {
        std::string enemy = EnemyWith("attackRange", "6.0");
        if (!speed.empty()) enemy.insert(enemy.size() - 1, std::format(",\"projectileSpeed\":{}", speed));
        if (!model.empty()) enemy.insert(enemy.size() - 1, std::format(",\"projectileModel\":\"{}\"", model));
        if (!keepAway.empty()) enemy.insert(enemy.size() - 1, std::format(",\"keepAwayRange\":{}", keepAway));
        return enemy;
    }
}

TEST(RangedContentTests, RangedEnemy_LoadsItsShot) {
    ContentDatabase content;
    auto result = LoadEnemies(content, Array(RangedEnemy("9.0", "Bolt.gltf", "3.0")));

    ASSERT_TRUE(result.Succeeded());
    const auto& enemy = content.GetEnemies().front();
    EXPECT_TRUE(enemy.IsRanged());
    EXPECT_FLOAT_EQ(enemy.projectileSpeed, 9.0f);
    EXPECT_EQ(enemy.projectileModel, "Bolt.gltf");
    EXPECT_FLOAT_EQ(enemy.keepAwayRange, 3.0f);
}

TEST(RangedContentTests, MeleeEnemy_NotRanged) {
    ContentDatabase content;
    ASSERT_TRUE(LoadEnemies(content, Array(ValidEnemy)).Succeeded());
    EXPECT_FALSE(content.GetEnemies().front().IsRanged());
}

TEST(RangedContentTests, InconsistentRangedEnemies_Rejected) {
    const std::string cases[] = {
        RangedEnemy("9.0", "", ""),                 // no model to draw the shot
        RangedEnemy("", "Bolt.gltf", ""),           // a model but no speed
        RangedEnemy("-1.0", "Bolt.gltf", ""),
        RangedEnemy("9.0", "Bolt.gltf", "6.0"),     // keeps away beyond its own range
    };
    for (const auto& enemy : cases) {
        ContentDatabase content;
        EXPECT_FALSE(LoadEnemies(content, Array(enemy)).Succeeded()) << enemy;
    }
}

TEST(RangedContentTests, Bow_NeedsRangeAndSpeed_SwordHasNone) {
    const std::string bow = R"({"id":"weapon_bow","name":"B","category":"Bow","allowedClasses":["Warrior"],"damageDice":"1d6","damageType":["Piercing"],"attackTimeSeconds":1.0)";
    {
        ContentDatabase content;
        ASSERT_TRUE(content.LoadFromJson("[]", "[" + bow + R"(,"range":8.0,"projectileSpeed":12.0})" + "]", "[]").Succeeded());
        EXPECT_TRUE(content.GetWeapons().front().IsRanged());
        EXPECT_FLOAT_EQ(content.GetWeapons().front().range, 8.0f);
    }
    {
        ContentDatabase content;
        EXPECT_FALSE(content.LoadFromJson("[]", "[" + bow + "}]", "[]").Succeeded());
    }
    {
        ContentDatabase content;
        std::string sword = ValidWeapon;
        sword.insert(sword.size() - 1, R"(,"range":3.0)");
        EXPECT_FALSE(content.LoadFromJson("[]", "[" + sword + "]", "[]").Succeeded());
    }
}

TEST_F(ShippedContentTests, RangedWeaponsAndCrossbowmen) {
    for (const auto& weapon : content_.GetWeapons()) {
        const bool shoots = weapon.category == WeaponType::Bow || weapon.category == WeaponType::Crossbow || weapon.category == WeaponType::Wand;
        EXPECT_EQ(weapon.IsRanged(), shoots) << weapon.key;
    }
    const auto* crossbowman = content_.FindEnemy("enemy_skeleton_rogue");
    ASSERT_NE(crossbowman, nullptr);
    EXPECT_TRUE(crossbowman->IsRanged());
    EXPECT_EQ(crossbowman->projectileModel, "Skeleton_Arrow.gltf");
    EXPECT_GT(crossbowman->keepAwayRange, 0.0f);
    EXPECT_LT(crossbowman->keepAwayRange, crossbowman->attackRange);
}

// ============================================================================
// Spells, Statuses, and Casters
// ============================================================================

namespace {
    constexpr const char* SlowStatus = R"({"id":"status_slow","displayName":"Slowed","durationSeconds":3.0,"movementMultiplier":0.5})";

    std::string BoltSpell(std::string_view extra = {}) {
        return std::format(R"({{"id":"spell_bolt","name":"Bolt","school":"Evocation","level":1,"categories":["Damage"],"manaCost":5,"castTimeSeconds":0.5,"range":10.0,"baseDamage":6,"powerCoefficient":0.5,"damageType":"Arcane","projectileSpeed":12.0{}}})", extra);
    }

    ContentLoadResult LoadSpellsAndStatuses(ContentDatabase& content, const std::string& spells, const std::string& statuses) {
        return content.LoadFromJson(spells, "[]", "[]", "[]", "[]", "[]", "[]", statuses);
    }
}

TEST(StatusContentTests, Status_LoadsWithDefaults) {
    ContentDatabase content;
    ASSERT_TRUE(LoadSpellsAndStatuses(content, "[]", Array(SlowStatus)).Succeeded());

    const auto* slow = content.FindStatus("status_slow");
    ASSERT_NE(slow, nullptr);
    EXPECT_EQ(slow->displayName, "Slowed");
    EXPECT_FLOAT_EQ(slow->movementMultiplier, 0.5f);
    EXPECT_FLOAT_EQ(slow->tickIntervalSeconds, 1.0f);
    EXPECT_FALSE(slow->preventsActions);
    EXPECT_EQ(slow->damagePerTick, 0);
}

TEST(StatusContentTests, BadStatuses_Rejected) {
    for (const char* status : {
             R"({"id":"s","displayName":"S","durationSeconds":0})",
             R"({"id":"s","displayName":"S","durationSeconds":1,"movementMultiplier":1.5})",
             R"({"id":"s","displayName":"S","durationSeconds":1,"damagePerTick":2})",          // no damage type
             R"({"id":"s","displayName":"S","durationSeconds":1,"tickIntervalSeconds":0})" }) {
        ContentDatabase content;
        EXPECT_FALSE(LoadSpellsAndStatuses(content, "[]", Array(status)).Succeeded()) << status;
    }
}

TEST(StatusContentTests, SpellNamingAnUnknownStatus_Rejected) {
    ContentDatabase content;
    auto result = LoadSpellsAndStatuses(content, Array(BoltSpell(R"(,"appliedStatusIds":["status_missing"])")), Array(SlowStatus));

    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(AnyErrorContains(result, "unknown status 'status_missing'"));
}

TEST(SpellContentTests, MissilesAndBursts_AreCastable) {
    ContentDatabase content;
    const std::string nova = R"({"id":"spell_nova","name":"Nova","school":"Frost","level":1,"categories":["Control"],"manaCost":5,"castTimeSeconds":0.5,"radius":4.0,"baseDamage":6,"damageType":"Cold"})";
    const std::string sight = R"({"id":"spell_sight","name":"Sight","school":"Utility","level":1,"categories":["Utility"],"manaCost":5,"castTimeSeconds":0.5})";
    ASSERT_TRUE(content.LoadFromJson("[" + BoltSpell() + "," + nova + "," + sight + "]", "[]", "[]").Succeeded());

    EXPECT_TRUE(content.FindSpell("spell_bolt")->IsMissile());
    EXPECT_TRUE(content.FindSpell("spell_bolt")->IsCastable());
    EXPECT_TRUE(content.FindSpell("spell_nova")->IsBurstAroundCaster());
    EXPECT_TRUE(content.FindSpell("spell_nova")->IsCastable());
    EXPECT_FALSE(content.FindSpell("spell_sight")->IsCastable());   // utility spells are not built yet
}

TEST(SpellContentTests, MissileWithoutRange_Rejected) {
    ContentDatabase content;
    std::string spell = BoltSpell();
    spell.replace(spell.find(R"("range":10.0,)"), 13, "");
    EXPECT_FALSE(content.LoadFromJson("[" + spell + "]", "[]", "[]").Succeeded());
}

TEST(EnemyValidationTests, Caster_NeedsAKnownMissileSpell) {
    {
        ContentDatabase content;
        auto result = content.LoadFromJson(Array(BoltSpell()), "[]", "[]", "[]", Array(EnemyWith("spell", R"("spell_bolt")")));
        ASSERT_TRUE(result.Succeeded()) << ErrorsOf(result);
        EXPECT_TRUE(content.GetEnemies().front().IsCaster());
        EXPECT_TRUE(content.GetEnemies().front().IsRanged());
    }
    {
        ContentDatabase content;
        auto result = content.LoadFromJson("[]", "[]", "[]", "[]", Array(EnemyWith("spell", R"("spell_missing")")));
        EXPECT_TRUE(AnyErrorContains(result, "unknown spell 'spell_missing'"));
    }
}

TEST_F(ShippedContentTests, Mage_HasASpellBarManaRegenAndAKit) {
    const auto* mage = content_.FindClass(CharacterClass::Mage);
    ASSERT_NE(mage, nullptr);
    ASSERT_EQ(mage->spellBar.size(), 4u);
    for (const auto& key : mage->spellBar) {
        const auto* spell = content_.FindSpell(key);
        ASSERT_NE(spell, nullptr) << key;
        EXPECT_TRUE(spell->IsCastable()) << key;
    }
    EXPECT_GT(mage->manaRegenPerSecond, 0.0f);
    EXPECT_EQ(content_.FindWeapon(mage->startingWeapon)->category, WeaponType::Wand);
    EXPECT_TRUE(content_.FindWeapon(mage->startingWeapon)->IsRanged());

    const auto* warrior = content_.FindClass(CharacterClass::Warrior);
    EXPECT_EQ(warrior->spellBar.size(), 3u);
    EXPECT_EQ(warrior->specializations.size(), 2u);
    EXPECT_EQ(warrior->startingWeapon, "weapon_sword_rusted_shortsword_t1");
    EXPECT_EQ(content_.GetStatuses().size(), 10u);
}

TEST_F(ShippedContentTests, GolemAndNecromancer) {
    const auto* golem = content_.FindEnemy("enemy_skeleton_golem");
    ASSERT_NE(golem, nullptr);
    EXPECT_EQ(golem->rig, "Large");
    EXPECT_TRUE(golem->staggerImmune);

    const auto* boss = content_.FindEnemy("enemy_necromancer");
    ASSERT_NE(boss, nullptr);
    EXPECT_TRUE(boss->boss);
    EXPECT_TRUE(boss->IsCaster());
    EXPECT_EQ(boss->summon, "enemy_skeleton_minion");
    EXPECT_GT(boss->phaseTwoHealthFraction, 0.0f);
}

TEST(EnemyValidationTests, BadRigAndUnknownSummon_Rejected) {
    {
        ContentDatabase content;
        EXPECT_FALSE(LoadEnemies(content, Array(EnemyWith("rig", R"("Huge")"))).Succeeded());
    }
    {
        ContentDatabase content;
        auto result = LoadEnemies(content, Array(EnemyWith("summon", R"("enemy_missing")")));
        EXPECT_TRUE(AnyErrorContains(result, "unknown summon 'enemy_missing'"));
    }
}

// ============================================================================
// Rarities, Affixes, and Ammunition
// ============================================================================

TEST_F(ShippedContentTests, Rarities_GrowRarerItemsWithDepth) {
    ASSERT_EQ(content_.GetRarities().size(), 5u);   // and Legendary
    const auto* common = content_.FindRarity("rarity_common");
    const auto* rare = content_.FindRarity("rarity_rare");
    ASSERT_NE(common, nullptr);
    ASSERT_NE(rare, nullptr);
    EXPECT_EQ(common->affixes, 0);
    EXPECT_EQ(rare->affixes, 2);
    EXPECT_GT(common->WeightAt(1), common->WeightAt(10));   // design-loot-and-progression.md 4.3
    EXPECT_LT(rare->WeightAt(1), rare->WeightAt(10));
    EXPECT_NEAR(common->WeightAt(10), 43.0f, 0.01f);        // the design's late depth: ~40%

    const auto* legendary = content_.FindRarity("rarity_legendary");
    ASSERT_NE(legendary, nullptr);
    EXPECT_TRUE(legendary->legendary);
    EXPECT_EQ(legendary->WeightAt(1), 0.0f);                 // never on the first floor
    EXPECT_GT(legendary->WeightAt(8), 0.0f);
}

TEST_F(ShippedContentTests, Affixes_AttributesHitPointsAndPowers) {
    const auto* mighty = content_.FindAffix("affix_prefix_mighty");
    ASSERT_NE(mighty, nullptr);
    EXPECT_EQ(mighty->strength, 1);
    EXPECT_EQ(content_.FindAffix("affix_prefix_hale")->hitPoints, 4);
    const auto* storms = content_.FindAffix("affix_power_storms");
    ASSERT_NE(storms, nullptr);
    EXPECT_TRUE(storms->IsPower());
    EXPECT_EQ(storms->power, "ChainLightning");
    EXPECT_TRUE(storms->onWeapons);
    EXPECT_FALSE(storms->description.empty());
    EXPECT_TRUE(content_.FindAffix("affix_power_thorns")->onArmour);
}

TEST_F(ShippedContentTests, Affixes_PrefixesAndSuffixes) {
    const auto* keen = content_.FindAffix("affix_prefix_keen");
    ASSERT_NE(keen, nullptr);
    EXPECT_TRUE(keen->prefix);
    EXPECT_TRUE(keen->onWeapons);
    EXPECT_FALSE(keen->onArmour);
    const auto* bear = content_.FindAffix("affix_suffix_of_the_bear");
    ASSERT_NE(bear, nullptr);
    EXPECT_FALSE(bear->prefix);
}

TEST_F(ShippedContentTests, Ammunition_ForBowsAndCrossbows) {
    const auto* arrows = content_.FindConsumable("consumable_ammo_arrow");
    ASSERT_NE(arrows, nullptr);
    EXPECT_TRUE(arrows->IsAmmunition());
    EXPECT_EQ(arrows->ammoType, content_.FindWeapon("weapon_bow_worn_shortbow_t1")->ammoType);
    EXPECT_EQ(content_.FindConsumable("consumable_ammo_bolt")->ammoType, content_.FindWeapon("weapon_crossbow_light_crossbow_t1")->ammoType);
    const auto* chest = content_.FindLootTable("loot_chest");
    const auto it = std::ranges::find(chest->items, std::string("consumable_ammo_arrow"), &LootEntry::item);
    ASSERT_NE(it, chest->items.end());
    EXPECT_GT(it->quantity.count, 0);   // a handful
}

TEST(AffixValidationTests, BadAffixesAndRarities_Rejected) {
    for (const char* affix : { R"({"id":"a","name":"A","type":"Infix","appliesTo":["Weapon"]})",
                               R"({"id":"a","name":"A","type":"Prefix","appliesTo":[]})",
                               R"({"id":"a","name":"A","type":"Prefix","appliesTo":["Ring"]})" }) {
        ContentDatabase content;
        EXPECT_FALSE(content.LoadFromJson("[]", "[]", "[]", "[]", "[]", "[]", "[]", "[]", "[]", Array(affix)).Succeeded()) << affix;
    }
    ContentDatabase content;
    EXPECT_FALSE(content.LoadFromJson("[]", "[]", "[]", "[]", "[]", "[]", "[]", "[]",
        R"([{"id":"r","name":"R","affixes":5,"color":[1,1,1],"weight":1}])").Succeeded());
}
