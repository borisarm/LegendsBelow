// HealthTests.cpp - LB.Engine tests: equipment kept in one place (the inventory's slots), orders
// walking around walls, and the run's events
#include "pch.h"
#include "EngineTestHelpers.h"

using LB::Combat::EquipmentSlot;

TEST(EquipmentTests, TheInventorysSlots_AreTheOneRecord) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto weapon = engine.GetPlayerWeaponItem();
    ASSERT_TRUE(weapon.IsValid());
    EXPECT_EQ(engine.EquippedItem(EquipmentSlot::MainHand).Pack(), weapon.Pack());

    // Armour put on is in its slot, and CharacterService works the character out from it
    ASSERT_TRUE(engine.EquipPlayerArmor("armor_chest_chain_shirt_t1"));
    ASSERT_TRUE(engine.EquippedItem(EquipmentSlot::Chest).IsValid());
    ASSERT_EQ(engine.WornItems().size(), 1u);
    ASSERT_EQ(engine.GetCharacters().GetEquippedArmor(*engine.GetPlayerId()).size(), 1u);

    // Taken off, it is gone from both and back in the bag
    ASSERT_TRUE(engine.UnequipArmorSlot(EquipmentSlot::Chest));
    EXPECT_FALSE(engine.EquippedItem(EquipmentSlot::Chest).IsValid());
    EXPECT_TRUE(engine.GetCharacters().GetEquippedArmor(*engine.GetPlayerId()).empty());
    EXPECT_EQ(engine.CountItem("armor_chest_chain_shirt_t1"), 1);
}

TEST(PathTests, FindPath_GoesThroughTheCorridor) {
    const auto world = LShapedWorld();
    const auto from = world.CellCenter(1, 1);
    const WorldPoint to{ world.CellCenter(6, 6).x + 0.3f, world.CellCenter(6, 6).y };

    const auto path = world.FindPath(from, to);
    ASSERT_GE(path.size(), 5u);
    EXPECT_FLOAT_EQ(path.back().x, to.x);   // ends at the point itself
    EXPECT_TRUE(world.LineOfSight(from, path.front()));
    for (std::size_t i = 1; i < path.size(); ++i) EXPECT_TRUE(world.LineOfSight(path[i - 1], path[i])) << i;
    EXPECT_TRUE(std::ranges::any_of(path, [&](WorldPoint p) { return world.CellOf(p) == std::pair{ 2, 5 }; }));   // the corridor

    EXPECT_TRUE(world.FindPath(from, from).empty());
}

TEST(PathTests, AnOrderToAnotherRoom_WalksAroundTheWalls) {
    GameEngine engine;
    InitializeDungeon(engine);
    engine.GrantExperience(1600);   // level 5: the rooms on the way rise, and a new hero walking past them can fall
    engine.Tick(OneSixtieth);
    const auto& layout = engine.GetWorld().Layout();
    const auto goal = engine.GetWorld().RoomCenter(layout.exitRoom);
    ASSERT_FALSE(engine.GetWorld().LineOfSight(engine.GetPlayerPosition(), goal));

    engine.OrderMoveTo(goal);
    engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.GetOrderRoute().empty());   // a way around the walls
    for (int i = 0; i < 60 * 60 && engine.GetPlayerOrder().kind != GameEngine::OrderKind::None; ++i) engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::None);
    EXPECT_LT(Encounter::Distance(engine.GetPlayerPosition(), goal), 0.5f);
}

TEST(EventTests, AKill_IsPublished_AndProgressionAnswersIt) {
    GameEngine engine;
    InitializeForCombat(engine);
    std::vector<EnemyKilledEvent> kills;
    engine.GetEvents().Subscribe<EnemyKilledEvent>([&](const EnemyKilledEvent& kill) { kills.push_back(kill); });
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    KillWithSword(engine, id);
    engine.Tick(OneSixtieth);

    ASSERT_EQ(kills.size(), 1u);
    EXPECT_EQ(kills[0].enemyId, id);
    EXPECT_EQ(kills[0].enemyKey, GameEngine::LightEnemy);
    EXPECT_GT(kills[0].experience, 0);
    EXPECT_EQ(engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetExperience(), kills[0].experience);
}

TEST(EventTests, ALevel_IsPublishedWithTheLevelsGained) {
    GameEngine engine;
    InitializeForCombat(engine);
    std::vector<LevelGainedEvent> levels;
    engine.GetEvents().Subscribe<LevelGainedEvent>([&](const LevelGainedEvent& gained) { levels.push_back(gained); });

    engine.GrantExperience(1000);   // several levels at once
    engine.Tick(OneSixtieth);

    ASSERT_EQ(levels.size(), 1u);
    EXPECT_EQ(levels[0].previous, 1);
    EXPECT_EQ(levels[0].level, PlayerLevel(engine));
    EXPECT_GT(levels[0].level, 2);
}
