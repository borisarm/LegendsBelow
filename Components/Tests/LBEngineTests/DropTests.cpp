// DropTests.cpp - LB.Engine tests: dropping bag items on the floor (from the character screen
// too), and picking them up again only after walking away
#include "pch.h"
#include "EngineTestHelpers.h"

TEST(DropTests, ADroppedStack_LiesAtTheHerosFeet_UntilTheyComeBack) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto potions = engine.CountItem(GameEngine::HealingPotion);
    ASSERT_GT(potions, 0);

    ASSERT_TRUE(engine.DropItem(ItemRef::FromKey(*engine.GetContent(), GameEngine::HealingPotion)));
    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), 0);
    ASSERT_EQ(engine.GetGroundItems().size(), 1u);
    EXPECT_EQ(engine.GetGroundItems()[0].quantity, potions);   // the whole stack
    EXPECT_LT(Encounter::Distance(engine.GetGroundItems()[0].position, engine.GetPlayerPosition()), 0.5f);

    RunSteps(engine, 60);   // standing on it: it stays down
    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), 0);

    const auto at = engine.GetPlayerPosition();
    HoldKeysFor(engine, { Keys::FromChar('D') }, 45);   // away
    WalkTo(engine, at, 60 * 3);                           // and back
    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), potions);
    EXPECT_TRUE(engine.GetGroundItems().empty());
}

TEST(DropTests, OnlyWhatIsInTheBag) {
    GameEngine engine;
    InitializeForCombat(engine);
    EXPECT_FALSE(engine.DropItem(ItemRef::FromKey(*engine.GetContent(), "armor_chest_chain_shirt_t1")));
    EXPECT_FALSE(engine.DropItem(engine.GetPlayerWeaponItem()));   // the equipped sword is not in the bag
    EXPECT_TRUE(engine.GetGroundItems().empty());
}

TEST(DropTests, FromTheCharacterScreen_TheRowUnderTheCursor) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto bow = ItemRef::FromKey(*engine.GetContent(), GameEngine::StartingBow);
    ASSERT_EQ(engine.CountItem(GameEngine::StartingBow), 1);
    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);
    const auto& rows = engine.GetCharacterScreen().GetRows();
    const auto index = static_cast<std::size_t>(std::ranges::find_if(rows, [&](const auto& row) { return row.item == bow; }) - rows.begin());
    ASSERT_LT(index, rows.size());
    for (int i = 0; i < 100 && engine.GetCharacterScreen().GetCursor() != index; ++i) {
        TapKey(engine, Keys::Down);
        engine.Tick(OneSixtieth);
    }
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text.find("X/Del: drop") != std::string::npos; }));

    TapKey(engine, Keys::FromChar('X'));
    engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.CountItem(GameEngine::StartingBow), 0);
    ASSERT_EQ(engine.GetGroundItems().size(), 1u);
    EXPECT_EQ(engine.GetGroundItems()[0].item, bow);
}
