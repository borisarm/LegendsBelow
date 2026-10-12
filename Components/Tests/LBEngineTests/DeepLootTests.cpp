// DeepLootTests.cpp - LB.Engine tests: item levels growing affixes, attribute and hit point
// affixes on the character, legendary powers, and the item icons
#include "pch.h"
#include "EngineTestHelpers.h"

using LB::Combat::EquipmentSlot;

TEST(DeepLootTests, AnItemsLevel_GrowsItsAffixes) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto low = engine.MakeItem("~weapon_sword_rusted_shortsword_t1|rarity_uncommon|affix_prefix_brutal|1");
    const auto high = engine.MakeItem("~weapon_sword_rusted_shortsword_t1|rarity_uncommon|affix_prefix_brutal|5");

    engine.GiveItemRef(low);
    ASSERT_TRUE(engine.EquipItemRef(low));
    const auto damageLow = engine.GetPlayerAttackProfile().damageBonus;
    engine.GiveItemRef(high);
    ASSERT_TRUE(engine.EquipItemRef(high));
    EXPECT_EQ(engine.GetPlayerAttackProfile().damageBonus, damageLow + 2);   // +2 damage at level 1, +4 at level 5
    EXPECT_GT(engine.PriceOf(high), engine.PriceOf(low));                    // and dearer
}

TEST(DeepLootTests, Drops_TakeTheDepthAsTheirLevel) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = CombatSeed, .dungeon = true }));
    engine.EnterFloor(6);
    engine.Tick(OneSixtieth);

    // Out of the hero's reach, until something above common falls
    const WorldPoint away{ engine.GetPlayerPosition().x + 3.0f, engine.GetPlayerPosition().y };
    const auto rolled = [&] { return std::ranges::find_if(engine.GetGroundItems(), [](const GroundItem& ground) { return ground.item.kind == ItemKind::Rolled; }); };
    for (int i = 0; i < 80 && rolled() == engine.GetGroundItems().end(); ++i) engine.DropLoot("loot_necromancer", away);
    ASSERT_NE(rolled(), engine.GetGroundItems().end());
    EXPECT_EQ(engine.ItemLevel(rolled()->item), 6);
    EXPECT_EQ(engine.ItemLevel(ItemRef::FromKey(*engine.GetContent(), GameEngine::HealingPotion)), 0);
}

TEST(DeepLootTests, AttributeAndHitPointAffixes_ReachTheCharacter) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.GetPlayerId();
    const auto strength = engine.GetCharacters().GetEffectiveAttributes(id)->strength;
    const auto maxHp = PlayerMaxHitPoints(engine);
    const auto helm = engine.MakeItem("~armor_head_chain_coif_t1|rarity_rare|affix_prefix_mighty,affix_prefix_hale|1");
    engine.GiveItemRef(helm);

    ASSERT_TRUE(engine.EquipItemRef(helm));
    EXPECT_EQ(engine.GetCharacters().GetEffectiveAttributes(id)->strength, strength + 1);
    EXPECT_EQ(engine.GetCharacters().GetCharacter(id)->get().GetAttributes().strength, strength);   // not the saved attributes
    EXPECT_EQ(PlayerMaxHitPoints(engine), maxHp + 4);

    // A level keeps the gear's hit points on top (and may raise strength itself)
    engine.GrantExperience(150);
    engine.Tick(OneSixtieth);
    const auto levelled = PlayerMaxHitPoints(engine);
    ASSERT_TRUE(engine.UnequipArmorSlot(EquipmentSlot::Head));
    EXPECT_EQ(PlayerMaxHitPoints(engine), levelled - 4);
    EXPECT_EQ(engine.GetCharacters().GetEffectiveAttributes(id)->strength, engine.GetCharacters().GetCharacter(id)->get().GetAttributes().strength);
}

TEST(DeepLootTests, Thorns_HurtWhoeverStrikesTheHero) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto chest = engine.MakeItem("~armor_chest_chain_shirt_t1|rarity_legendary|affix_prefix_stalwart,affix_power_thorns|3");
    engine.GiveItemRef(chest);
    ASSERT_TRUE(engine.EquipItemRef(chest));
    const auto enemy = *engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f });
    const auto full = engine.GetEncounter().FindEnemy(enemy)->hitPoints;

    const auto hp = PlayerHitPoints(engine);
    for (int i = 0; i < 60 * 15 && PlayerHitPoints(engine) == hp; ++i) engine.Tick(OneSixtieth);   // until a blow lands
    ASSERT_LT(PlayerHitPoints(engine), hp);
    engine.Tick(OneSixtieth);

    EXPECT_LT(engine.GetEncounter().FindEnemy(enemy)->hitPoints, full);   // the hero never swung
}

TEST(DeepLootTests, Frostbite_ChillsWhatTheHeroHits) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto sword = engine.MakeItem("~weapon_sword_rusted_shortsword_t1|rarity_legendary|affix_prefix_keen,affix_power_winter|3");
    engine.GiveItemRef(sword);
    ASSERT_TRUE(engine.EquipItemRef(sword));
    const auto enemy = *engine.SpawnEnemy("enemy_skeleton_golem", { 1.0f, 0.0f });

    bool chilled = false;
    engine.OnKeyDown(Keys::Space);
    for (int i = 0; i < 60 * 30 && !chilled; ++i) {
        engine.Tick(OneSixtieth);
        const auto* target = engine.GetEncounter().FindEnemy(enemy);
        chilled = target && target->statuses.Has("status_slow_basic");
        if (!engine.IsPlayerAlive()) break;
    }
    engine.OnKeyUp(Keys::Space);
    EXPECT_TRUE(chilled);
}

TEST(DeepLootTests, ALegendary_ShowsItsPower) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto sword = engine.MakeItem("~weapon_sword_rusted_shortsword_t1|rarity_legendary|affix_prefix_keen,affix_power_storms|4");
    EXPECT_EQ(engine.ItemName(sword), "Keen Rusted Shortsword of Storms");
    engine.GiveItemRef(sword);
    ASSERT_TRUE(engine.EquipItemRef(sword));
    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);

    const auto& rows = engine.GetCharacterScreen().GetRows();
    EXPECT_TRUE(std::ranges::any_of(rows, [](const auto& row) { return row.text.find("arc lightning") != std::string::npos; }));
}

TEST(DeepLootTests, ItemRows_ShowTheirIcons) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .uiDirectory = ShippedUiDirectory(),
        .spawnEnemies = false, .randomSeed = CombatSeed }));
    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);

    // The sword in hand, the bow and potions in the bag: three icons at least, besides the frames
    const auto icons = std::ranges::count_if(engine.GetRendering().GetQueuedImages(), [](const auto& image) { return image.nineSlice == 0.0f && image.layerDepth == 33; });
    EXPECT_GE(icons, 3);
}
