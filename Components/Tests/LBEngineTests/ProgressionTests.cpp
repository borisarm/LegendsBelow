// ProgressionTests.cpp - LB.Engine tests: loot, wave scaling, the character screen, and progression
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// Loot, Wave Scaling, and Character Screen Model Tests
// ============================================================================

TEST(ItemRefTests, PackAndUnpack_KeepKindAndId) {
    const ItemRef potion{ ItemKind::Consumable, 3 };
    const ItemRef sword{ ItemKind::Weapon, 1 };

    EXPECT_EQ(ItemRef::Unpack(potion.Pack()), potion);
    EXPECT_EQ(ItemRef::Unpack(sword.Pack()), sword);
    EXPECT_NE(potion.Pack(), ItemRef({ ItemKind::Armor, 3 }).Pack());
    EXPECT_FALSE(ItemRef::Unpack(42).IsValid());
}

TEST(LootRollerTests, CertainTable_AlwaysDropsGoldAndAnItem) {
    EncounterContent content;
    const auto& database = *content.engine.GetContent();
    LB::Content::LootTableDefinition table{};
    table.goldChance = 1.0f;
    table.gold = LB::Dice{ 2, 6, 1 };
    table.itemChance = 1.0f;
    table.items = { { "consumable_potion_healing_minor", 1 } };
    LootRoller roller(CombatSeed);

    for (int i = 0; i < 50; ++i) {
        const auto drops = roller.Roll(table, database);
        ASSERT_EQ(drops.size(), 2u);
        EXPECT_TRUE(drops[0].IsGold());
        EXPECT_GE(drops[0].quantity, 3);
        EXPECT_LE(drops[0].quantity, 13);
        EXPECT_EQ(drops[1].item, ItemRef::FromKey(database, "consumable_potion_healing_minor"));
    }
}

TEST(LootRollerTests, Weights_PickItemsInProportion) {
    EncounterContent content;
    const auto& database = *content.engine.GetContent();
    LB::Content::LootTableDefinition table{};
    table.itemChance = 1.0f;
    table.items = { { "consumable_potion_healing_minor", 3 }, { "armor_head_leather_cap_t1", 1 } };
    LootRoller roller(CombatSeed);

    int potions = 0;
    for (int i = 0; i < 2000; ++i) {
        if (roller.Roll(table, database)[0].item.kind == ItemKind::Consumable) ++potions;
    }
    EXPECT_NEAR(potions / 2000.0, 0.75, 0.04);
}

TEST(EncounterTests, LaterWaves_TougherAndWorthMore) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);

    const auto first = encounter.Spawn(*content.minion, { 3.0f, 0.0f }, 0.0f, false, 1);
    const auto fifth = encounter.Spawn(*content.minion, { -3.0f, 0.0f }, 0.0f, false, 5);

    const auto& a = *encounter.FindEnemy(first);
    const auto& b = *encounter.FindEnemy(fifth);
    EXPECT_EQ(a.maxHitPoints, content.minion->hitPoints);
    EXPECT_EQ(a.bonusDamage, 0);
    EXPECT_EQ(a.experience, content.minion->experience);
    EXPECT_EQ(b.maxHitPoints, static_cast<std::int32_t>(std::lround(content.minion->hitPoints * 1.6f)));
    EXPECT_EQ(b.bonusDamage, 2);
    EXPECT_EQ(b.experience, static_cast<std::int32_t>(std::lround(content.minion->experience * 1.4f)));
    EXPECT_EQ(b.hitPoints, b.maxHitPoints);
}

TEST(CharacterScreenTests, Cursor_SkipsRowsThatCannotBeChosen) {
    CharacterScreen screen;
    screen.Open();
    std::vector<ScreenRow> rows(5);
    rows[1].action = ScreenAction::Equip;
    rows[3].action = ScreenAction::Use;
    screen.SetRows(rows);

    EXPECT_EQ(screen.GetCursor(), 1u);
    EXPECT_TRUE(screen.Step(+1));
    EXPECT_EQ(screen.GetCursor(), 3u);
    EXPECT_FALSE(screen.Step(+1));         // nothing selectable below
    EXPECT_EQ(screen.GetCursor(), 3u);
    EXPECT_EQ(screen.Selected()->action, ScreenAction::Use);

    rows.resize(3);                        // the used item is gone: back to the last choice
    screen.SetRows(rows);
    EXPECT_EQ(screen.GetCursor(), 1u);
}

TEST(CharacterScreenTests, NoChoices_NothingSelected) {
    CharacterScreen screen;
    screen.Open();
    screen.SetRows(std::vector<ScreenRow>(3));

    EXPECT_EQ(screen.Selected(), nullptr);
    screen.Close();
    EXPECT_FALSE(screen.IsOpen());
}

// ============================================================================
// GameEngine Progression and Loot Tests
// ============================================================================

TEST(GameEngineProgressTests, StartingKit_SwordAndAPotion) {
    GameEngine engine;
    InitializeForCombat(engine);

    EXPECT_EQ(engine.GetPlayerWeapon()->key, GameEngine::StartingWeapon);
    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), GameEngine::StartingPotions);
    EXPECT_EQ(engine.GetGold(), 0);
}

TEST(GameEngineProgressTests, Kill_AwardsTheEnemysExperience) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });
    const auto worth = engine.GetEncounter().FindEnemy(id)->experience;

    KillWithSword(engine, id);

    ASSERT_NE(engine.GetEncounter().FindEnemy(id), nullptr);
    ASSERT_TRUE(engine.GetEncounter().FindEnemy(id)->IsDead());
    EXPECT_EQ(PlayerExperience(engine), worth);
}

TEST(GameEngineProgressTests, EnoughKills_LevelUpWithPointsAndText) {
    GameEngine engine;
    InitializeForCombat(engine);

    LevelUp(engine);

    ASSERT_EQ(PlayerLevel(engine), 2);
    EXPECT_GT(engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetUnspentAttributePoints(), 0);
}

TEST(GameEngineProgressTests, DropLoot_WalkOverToPickItUp) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto right = Ground(engine, 1.0f, 0.0f);
    for (int i = 0; i < 10; ++i) engine.DropLoot("loot_skeleton_warrior", { right.x * 3.0f, right.y * 3.0f });
    ASSERT_FALSE(engine.GetGroundItems().empty());
    std::int32_t gold = 0;
    for (const auto& ground : engine.GetGroundItems()) if (ground.IsGold()) gold += ground.quantity;
    const auto items = std::ranges::count_if(engine.GetGroundItems(), [](const GroundItem& g) { return !g.IsGold(); });
    const auto bagBefore = engine.GetInventory().size();

    HoldKeysFor(engine, { Keys::FromChar('D') }, 75);   // across the pile, screen-right

    EXPECT_EQ(engine.GetGold(), gold);
    EXPECT_TRUE(engine.GetGroundItems().empty());
    if (items > 0) EXPECT_GT(engine.GetInventory().size(), bagBefore);
}

TEST(GameEngineProgressTests, BagFull_ItemStaysOnTheFloor) {
    GameEngine engine;
    InitializeForCombat(engine);
    for (int i = 0; i < GameEngine::BagCapacity; ++i) engine.GiveItem("armor_head_leather_cap_t1");
    for (int i = 0; i < 100 && std::ranges::none_of(engine.GetGroundItems(), [](const GroundItem& g) { return !g.IsGold(); }); ++i) {
        engine.DropLoot("loot_skeleton_minion", { 0.0f, 0.0f });
    }

    RunSteps(engine, 2);

    EXPECT_TRUE(std::ranges::any_of(engine.GetGroundItems(), [](const GroundItem& g) { return !g.IsGold() && g.fullWarningShown; }));
}

TEST(GameEngineProgressTests, EquipWeapon_ChangesTheAttack) {
    GameEngine engine;
    InitializeForCombat(engine);
    ASSERT_EQ(engine.GiveItem("weapon_axe_splintered_hatchet_t1"), 1);

    ASSERT_TRUE(engine.EquipItem("weapon_axe_splintered_hatchet_t1"));

    EXPECT_EQ(engine.GetPlayerWeapon()->key, "weapon_axe_splintered_hatchet_t1");
    EXPECT_EQ(engine.CountItem(GameEngine::StartingWeapon), 1);      // the sword went to the bag
    EXPECT_EQ(engine.CountItem("weapon_axe_splintered_hatchet_t1"), 0);
    const auto* axe = engine.GetContent()->FindWeapon("weapon_axe_splintered_hatchet_t1");
    const auto strength = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetAttributes().GetStrengthModifier();
    EXPECT_EQ(engine.GetPlayerAttackProfile().damageBonus, axe->damageBonus + strength);
}

TEST(GameEngineProgressTests, Equip_RequirementsAndClassChecked) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.GiveItem("weapon_sword_iron_longsword_t1");     // level 4
    engine.GiveItem("armor_head_novice_hood_t1");          // not for Warriors

    EXPECT_FALSE(engine.EquipItem("weapon_sword_iron_longsword_t1"));
    EXPECT_FALSE(engine.EquipItem("armor_head_novice_hood_t1"));
    EXPECT_FALSE(engine.EquipItem("armor_head_leather_cap_t1"));     // not in the bag
    EXPECT_EQ(engine.GetPlayerWeapon()->key, GameEngine::StartingWeapon);
}

TEST(GameEngineProgressTests, EquipAndUnequipArmour_ChangesDefense) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.GiveItem("armor_head_leather_cap_t1");
    engine.GiveItem("armor_head_chain_coif_t1");

    ASSERT_TRUE(engine.EquipItem("armor_head_leather_cap_t1"));
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, 1);
    ASSERT_TRUE(engine.EquipItem("armor_head_chain_coif_t1"));        // the cap goes back to the bag
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, 2);
    EXPECT_EQ(engine.CountItem("armor_head_leather_cap_t1"), 1);

    ASSERT_TRUE(engine.UnequipArmorSlot(LB::Combat::EquipmentSlot::Head));
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, 0);
    EXPECT_EQ(engine.CountItem("armor_head_chain_coif_t1"), 1);
}

TEST(GameEngineProgressTests, PotionKey_HealsAndUsesOne) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f });
    const auto max = PlayerMaxHitPoints(engine);
    StepUntil(engine, [&] { return PlayerHitPoints(engine) < max; }, 20.0f);
    ASSERT_LT(PlayerHitPoints(engine), max);
    ASSERT_TRUE(engine.IsPlayerAlive());

    TapKey(engine, Keys::FromChar('R'));
    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), GameEngine::StartingPotions - 1);
    EXPECT_TRUE(std::ranges::any_of(engine.GetFloatingTexts(), [](const FloatingText& t) { return t.text.starts_with("+"); }));
}

TEST(GameEngineProgressTests, Potion_NotWastedAtFullHealth) {
    GameEngine engine;
    InitializeForCombat(engine);

    EXPECT_FALSE(engine.UseItem(GameEngine::HealingPotion));
    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), GameEngine::StartingPotions);
}

TEST(GameEngineProgressTests, CharacterScreen_PausesAndSpendsPoints) {
    GameEngine engine;
    InitializeForCombat(engine);
    LevelUp(engine);
    ASSERT_EQ(PlayerLevel(engine), 2);
    const auto& character = engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get();
    const auto points = character.GetUnspentAttributePoints();
    const auto strength = character.GetAttributes().strength;

    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);
    ASSERT_TRUE(engine.GetCharacterScreen().IsOpen());
    const auto time = engine.GetGameTimeSeconds();
    ASSERT_NE(engine.GetCharacterScreen().Selected(), nullptr);
    EXPECT_EQ(engine.GetCharacterScreen().Selected()->action, ScreenAction::SpendPoint);   // STR, the first choice

    TapKey(engine, Keys::Enter);
    engine.Tick(OneSixtieth);
    EXPECT_EQ(character.GetUnspentAttributePoints(), points - 1);
    EXPECT_EQ(character.GetAttributes().strength, strength + 1);
    RunSteps(engine, 30);
    EXPECT_DOUBLE_EQ(engine.GetGameTimeSeconds(), time);   // the world waits

    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.GetCharacterScreen().IsOpen());
    EXPECT_FALSE(engine.IsPaused());                        // Esc closed the screen, it did not pause
}

TEST(GameEngineProgressTests, CharacterScreen_EquipFromTheBag) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.GiveItem("armor_head_leather_cap_t1");

    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);
    for (int i = 0; i < 6 && (!engine.GetCharacterScreen().Selected() || engine.GetCharacterScreen().Selected()->action != ScreenAction::Equip); ++i) {
        TapKey(engine, Keys::Down);
        engine.Tick(OneSixtieth);
    }
    ASSERT_NE(engine.GetCharacterScreen().Selected(), nullptr);
    ASSERT_EQ(engine.GetCharacterScreen().Selected()->action, ScreenAction::Equip);
    TapKey(engine, Keys::Enter);
    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, 1);
    EXPECT_EQ(engine.CountItem("armor_head_leather_cap_t1"), 0);
}

TEST(GameEngineProgressTests, SaveAndLoad_KeepProgressGoldAndGear) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .saveDirectory = dir.Utf8(), .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false, .randomSeed = CombatSeed }));
    KillWithSword(engine, *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f }));
    engine.GiveItem("armor_head_leather_cap_t1");
    engine.GiveItem("weapon_axe_splintered_hatchet_t1");
    ASSERT_TRUE(engine.EquipItem("armor_head_leather_cap_t1"));
    ASSERT_TRUE(engine.EquipItem("weapon_axe_splintered_hatchet_t1"));
    for (int i = 0; i < 5; ++i) engine.DropLoot("loot_skeleton_warrior", engine.GetPlayerPosition());
    RunSteps(engine, 2);
    const auto experience = PlayerExperience(engine);
    const auto gold = engine.GetGold();
    const auto bag = engine.GetInventory().size();
    ASSERT_TRUE(engine.SaveGame(1));

    // Change it all, then load
    engine.GiveItem(GameEngine::HealingPotion, 3);
    ASSERT_TRUE(engine.EquipItem(GameEngine::StartingWeapon));
    ASSERT_TRUE(engine.UnequipArmorSlot(LB::Combat::EquipmentSlot::Head));
    ASSERT_TRUE(engine.LoadGame(1));

    EXPECT_EQ(PlayerExperience(engine), experience);
    EXPECT_EQ(engine.GetGold(), gold);
    EXPECT_EQ(engine.GetPlayerWeapon()->key, "weapon_axe_splintered_hatchet_t1");
    EXPECT_EQ(engine.GetPlayerDefense().physicalArmor, 1);
    EXPECT_EQ(engine.GetInventory().size(), bag);
}
