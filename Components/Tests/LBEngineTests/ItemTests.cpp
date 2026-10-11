// ItemTests.cpp - LB.Engine tests: rarities and affixes on dropped gear (rolled items), what they
// add when equipped, saving them, and ammunition for bows and crossbows
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    // Drop minion loot at the player until a rolled weapon lies there, then walk over it; its reference
    ItemRef PickUpARolledWeapon(GameEngine& engine) {
        for (int i = 0; i < 400; ++i) {
            engine.DropLoot("loot_skeleton_minion", engine.GetPlayerPosition());
            RunSteps(engine, 1);
            for (const auto& stack : engine.GetInventory()) {
                const auto item = ItemRef::Unpack(stack.itemId);
                if (item.kind == ItemKind::Rolled && engine.GetItemRolls().BaseOf(item).kind == ItemKind::Weapon) return item;
            }
        }
        return {};
    }
}

TEST(ItemRollTests, Rarity_OddsShiftWithDepth) {
    EncounterContent content;
    const auto& database = *content.engine.GetContent();
    LootRoller roller(CombatSeed);
    const auto commonShare = [&](std::int32_t depth) {
        int common = 0;
        for (int i = 0; i < 4000; ++i) common += roller.RollRarity(database, depth)->affixes == 0 ? 1 : 0;
        return static_cast<float>(common) / 4000.0f;
    };

    EXPECT_NEAR(commonShare(1), 0.70f, 0.03f);    // design-loot-and-progression.md 4.2: early depth
    EXPECT_NEAR(commonShare(10), 0.40f, 0.04f);   // ... and late
}

TEST(ItemRollTests, Affixes_APrefixThenASuffix_ForTheItemKind) {
    EncounterContent content;
    const auto& database = *content.engine.GetContent();
    LootRoller roller(CombatSeed);

    const auto affixes = roller.RollAffixes(database, true, 2);

    ASSERT_EQ(affixes.size(), 2u);
    EXPECT_TRUE(affixes[0]->prefix);
    EXPECT_FALSE(affixes[1]->prefix);
    for (const auto* affix : affixes) EXPECT_TRUE(affix->onWeapons);
}

TEST(ItemRollTests, Name_PrefixBaseSuffix) {
    EncounterContent content;
    const auto& database = *content.engine.GetContent();
    ItemRolls rolls;

    const auto item = rolls.Add(RolledItem{ ItemRef::FromKey(database, "weapon_sword_rusted_shortsword_t1"), database.FindRarity("rarity_rare"),
        { database.FindAffix("affix_prefix_keen"), database.FindAffix("affix_suffix_of_the_bear") }, {} }, database);

    EXPECT_EQ(rolls.Find(item)->name, "Keen Rusted Shortsword of the Bear");
    EXPECT_EQ(rolls.BaseOf(item), ItemRef::FromKey(database, "weapon_sword_rusted_shortsword_t1"));
    EXPECT_EQ(rolls.BonusesOf(item).attackBonus, 1);
    EXPECT_EQ(rolls.BonusesOf(item).damage, 1);
}

TEST(ItemRollTests, RolledWeapon_AddsItsAffixesWhenEquipped) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto item = PickUpARolledWeapon(engine);
    ASSERT_TRUE(item.IsValid());
    const auto bonuses = engine.GetItemRolls().BonusesOf(item);
    const auto* weapon = engine.GetContent()->FindWeapon(engine.GetItemRolls().BaseOf(item).id);

    ASSERT_TRUE(engine.EquipItemRef(item));   // the minion's weapons all suit a warrior

    EXPECT_EQ(engine.GetPlayerWeaponItem(), item);
    EXPECT_EQ(engine.GetPlayerWeapon(), weapon);
    EXPECT_EQ(engine.GetPlayerAttackProfile().attackBonus, engine.GetCharacters().GetAttackBonus(*engine.GetPlayerId(), weapon->category).value_or(0) + bonuses.attackBonus);
    EXPECT_EQ(engine.CountItem(GameEngine::StartingWeapon), 1);   // the sword went to the bag
}

TEST(ItemRollTests, SaveAndLoad_KeepRolledItems) {
    TempSaveDir dir;
    std::string name;
    {
        GameEngine engine;
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
            .spawnEnemies = false, .randomSeed = CombatSeed }));
        const auto item = PickUpARolledWeapon(engine);
        ASSERT_TRUE(item.IsValid());
        name = engine.ItemName(item);
        ASSERT_TRUE(engine.SaveGame(1));
    }
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(),
        .spawnEnemies = false, .randomSeed = CombatSeed }));

    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_TRUE(std::ranges::any_of(engine.GetInventory(), [&](const auto& stack) { return stack.name == name; }));
}

TEST(AmmunitionTests, BowShots_SpendArrowsUntilNoneAreLeft) {
    GameEngine engine;
    InitializeForCombat(engine);
    ASSERT_TRUE(engine.EquipItem(GameEngine::StartingBow));
    const auto start = engine.AmmoCount("Arrow");
    ASSERT_GT(start, 0);

    engine.OnKeyDown(Keys::Space);
    for (int i = 0; i < 60 * 60 && engine.AmmoCount("Arrow") > 0; ++i) engine.Tick(OneSixtieth);
    engine.OnKeyUp(Keys::Space);
    ASSERT_EQ(engine.AmmoCount("Arrow"), 0);
    RunSteps(engine, 120);   // the last arrows land

    TapKey(engine, Keys::Space);
    RunSteps(engine, 60);

    EXPECT_TRUE(engine.GetEncounter().GetProjectiles().empty() || std::ranges::none_of(engine.GetEncounter().GetProjectiles(), [](const auto& p) { return p.InFlight(); }));
    EXPECT_TRUE(std::ranges::any_of(engine.GetFloatingTexts(), [](const auto& t) { return t.text == "NO ARROWS"; }));
}

TEST(AmmunitionTests, Hud_CountsTheArrows) {
    GameEngine engine;
    InitializeForCombat(engine);
    ASSERT_TRUE(engine.EquipItem(GameEngine::StartingBow));

    engine.Tick(OneSixtieth);

    const auto expected = std::format("ARROWS {}", engine.AmmoCount("Arrow"));
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [&](const auto& t) { return t.text == expected; }));
}
