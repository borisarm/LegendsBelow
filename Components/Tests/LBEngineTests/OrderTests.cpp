// OrderTests.cpp - LB.Engine tests: the tactical pause and orders (right click to move or attack,
// the d-pad and spell keys while paused), carried out when the pause ends
#include "pch.h"
#include "EngineTestHelpers.h"

TEST(OrderTests, Tab_TogglesTheTacticalPause) {
    GameEngine engine;
    InitializeForCombat(engine);

    TapKey(engine, Keys::Tab);
    engine.Tick(OneSixtieth);
    EXPECT_TRUE(engine.IsTacticallyPaused());
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text.starts_with("PAUSED"); }));

    TapKey(engine, Keys::Tab);
    engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.IsTacticallyPaused());
}

TEST(OrderTests, RightClickOnTheFloor_WalksThere) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.Tick(OneSixtieth);

    RightClickAt(engine, { 3.0f, 2.0f });
    ASSERT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::Move);
    for (int i = 0; i < 60 * 5 && engine.GetPlayerOrder().kind != GameEngine::OrderKind::None; ++i) engine.Tick(OneSixtieth);

    EXPECT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::None);
    EXPECT_NEAR(engine.GetPlayerPosition().x, 3.0f, 0.3f);
    EXPECT_NEAR(engine.GetPlayerPosition().y, 2.0f, 0.3f);
}

TEST(OrderTests, RightClickOnAnEnemy_GoesAfterItUntilItFalls) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 5.0f, 0.0f });
    engine.Tick(OneSixtieth);

    RightClickAt(engine, engine.GetEncounter().FindEnemy(id)->position);
    ASSERT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::Attack);
    EXPECT_EQ(engine.GetPlayerOrder().target, std::optional<std::uint32_t>{ id });

    bool killed = false;
    for (int i = 0; i < 60 * 20 && !killed; ++i) {
        engine.Tick(OneSixtieth);
        const auto* enemy = engine.GetEncounter().FindEnemy(id);
        killed = !enemy || enemy->IsDead();
    }
    EXPECT_TRUE(killed);
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::None);   // done with its target
}

TEST(OrderTests, OrdersGivenWhilePaused_WaitForTheGameToResume) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.Tick(OneSixtieth);
    TapKey(engine, Keys::Tab);
    engine.Tick(OneSixtieth);
    const auto start = engine.GetPlayerPosition();

    RightClickAt(engine, { -3.0f, 0.0f });
    RunSteps(engine, 30);
    EXPECT_FLOAT_EQ(engine.GetPlayerPosition().x, start.x);   // no hidden simulation while paused
    EXPECT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::Move);

    TapKey(engine, Keys::Tab);
    RunSteps(engine, 30);
    EXPECT_LT(engine.GetPlayerPosition().x, start.x - 0.5f);
}

TEST(OrderTests, MovingYourself_CancelsTheOrder) {
    GameEngine engine;
    InitializeForCombat(engine);
    engine.Tick(OneSixtieth);
    RightClickAt(engine, { 4.0f, 0.0f });
    ASSERT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::Move);

    HoldKeysFor(engine, { Keys::FromChar('W') }, 3);

    EXPECT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::None);
}

TEST(OrderTests, WhilePaused_TheDpadPicksAndAttackOrders) {
    GameEngine engine;
    InitializeForCombat(engine);
    const auto near = *engine.SpawnEnemy(GameEngine::LightEnemy, { 3.0f, 0.0f });
    const auto far = *engine.SpawnEnemy(GameEngine::LightEnemy, { -6.0f, 0.0f });
    TapKey(engine, Keys::Tab);
    engine.Tick(OneSixtieth);

    TapKey(engine, Keys::Right);
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.GetPickedEnemy(), std::optional<std::uint32_t>{ near });   // nearest first
    TapKey(engine, Keys::Right);
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.GetPickedEnemy(), std::optional<std::uint32_t>{ far });

    TapKey(engine, Keys::Space);
    engine.Tick(OneSixtieth);
    EXPECT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::Attack);
    EXPECT_EQ(engine.GetPlayerOrder().target, std::optional<std::uint32_t>{ far });
}

TEST(OrderTests, WhilePaused_ASpellKeyOrdersACastAtThePickedEnemy) {
    GameEngine engine;
    InitializeMage(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 6.0f, 0.0f });
    TapKey(engine, Keys::Tab);
    engine.Tick(OneSixtieth);
    TapKey(engine, Keys::Right);
    engine.Tick(OneSixtieth);

    TapKey(engine, Keys::FromChar('1'));
    engine.Tick(OneSixtieth);
    ASSERT_EQ(engine.GetPlayerOrder().kind, GameEngine::OrderKind::Cast);
    EXPECT_FALSE(engine.IsPlayerCasting());

    TapKey(engine, Keys::Tab);
    engine.Tick(OneSixtieth);
    EXPECT_TRUE(engine.IsPlayerCasting());
    const auto& bolt = *engine.GetSpellBar()[0];
    RunSteps(engine, static_cast<int>(bolt.castTimeSeconds * 60.0f) + 1);
    ASSERT_FALSE(engine.GetEncounter().GetProjectiles().empty());
    const auto& missile = engine.GetEncounter().GetProjectiles()[0];
    const auto* enemy = engine.GetEncounter().FindEnemy(id);
    EXPECT_GT(missile.direction.x * (enemy->position.x - engine.GetPlayerPosition().x), 0.0f);   // toward it
}
