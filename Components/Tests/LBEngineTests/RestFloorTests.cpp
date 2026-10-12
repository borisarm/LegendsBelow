// RestFloorTests.cpp - LB.Engine tests: resting places after the bosses (no enemies, a shrine, a
// merchant), buying and selling
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    void InitializeRestFloor(GameEngine& engine, LB::Character::CharacterClass characterClass = LB::Character::CharacterClass::Warrior) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = CombatSeed,
            .dungeon = true, .playerClass = characterClass })) << engine.GetInitializationError();
        engine.EnterFloor(4);
        engine.Tick(OneSixtieth);
    }

    // Walk up to a point (the merchant, the shrine) and stop
    void GoTo(GameEngine& engine, WorldPoint at) {
        for (int i = 0; i < 60 * 8 && Encounter::Distance(engine.GetPlayerPosition(), at) > 0.9f; ++i) {
            SteerToward(engine, at);
            engine.Tick(OneSixtieth);
        }
        GamepadState still{};
        still.connected = true;
        engine.OnGamepadState(0, still);   // let go of the stick
        engine.Tick(OneSixtieth);
    }

    // Gold for shopping: a boss's purse dropped at the hero's feet
    void Earn(GameEngine& engine, std::int32_t gold) {
        for (int i = 0; i < 40 && engine.GetGold() < gold; ++i) {
            engine.DropLoot("loot_necromancer", engine.GetPlayerPosition());
            RunSteps(engine, 2);
        }
    }
}

TEST(RestFloorTests, AfterEachBoss_ARestingPlace) {
    EXPECT_FALSE(GameEngine::IsRestDepth(1));
    EXPECT_FALSE(GameEngine::IsRestDepth(3));   // the boss floor
    EXPECT_TRUE(GameEngine::IsRestDepth(4));
    EXPECT_FALSE(GameEngine::IsRestDepth(5));
    EXPECT_TRUE(GameEngine::IsRestDepth(7));
}

TEST(RestFloorTests, RestFloor_QuietWithAMerchantAndAShrine) {
    GameEngine engine;
    InitializeRestFloor(engine);

    EXPECT_TRUE(engine.IsRestFloor());
    ASSERT_TRUE(engine.GetMerchantPosition().has_value());
    ASSERT_TRUE(engine.GetShrinePosition().has_value());
    for (const auto& room : engine.GetRoomStates()) EXPECT_TRUE(room.cleared);
    const auto& stock = engine.GetMerchantStock();
    ASSERT_GE(stock.size(), 3u);
    EXPECT_EQ(engine.ItemName(stock[0].item), "Minor Healing Potion");
    EXPECT_TRUE(stock[0].unlimited);
    EXPECT_EQ(stock[0].price, engine.PriceOf(stock[0].item));
}

TEST(RestFloorTests, Shrine_RestoresOnce) {
    GameEngine engine;
    InitializeRestFloor(engine, LB::Character::CharacterClass::Mage);
    TapKey(engine, Keys::FromChar('1'));   // spend some mana on a bolt
    RunSteps(engine, 60);
    ASSERT_LT(PlayerMana(engine), PlayerMaxMana(engine));

    GoTo(engine, *engine.GetShrinePosition());

    EXPECT_TRUE(engine.IsShrineUsed());
    EXPECT_EQ(PlayerMana(engine), PlayerMaxMana(engine));
}

TEST(RestFloorTests, Merchant_SellsForGoldAndBuysAtAFraction) {
    GameEngine engine;
    InitializeRestFloor(engine);
    Earn(engine, 40);
    ASSERT_GE(engine.GetGold(), 40);
    EXPECT_FALSE(engine.BuyFromMerchant(0));   // too far away
    GoTo(engine, *engine.GetMerchantPosition());

    const auto gold = engine.GetGold();
    const auto potions = engine.CountItem(GameEngine::HealingPotion);
    ASSERT_TRUE(engine.BuyFromMerchant(0));
    EXPECT_EQ(engine.GetGold(), gold - engine.GetMerchantStock()[0].price);
    EXPECT_EQ(engine.CountItem(GameEngine::HealingPotion), potions + 1);

    const auto bow = ItemRef::FromKey(*engine.GetContent(), GameEngine::StartingBow);
    const auto before = engine.GetGold();
    ASSERT_TRUE(engine.SellToMerchant(bow));
    EXPECT_EQ(engine.GetGold(), before + engine.SellPriceOf(bow));
    EXPECT_EQ(engine.CountItem(GameEngine::StartingBow), 0);
    EXPECT_LT(engine.SellPriceOf(bow), engine.PriceOf(bow));
}

TEST(RestFloorTests, Interact_OpensTheTradeScreen) {
    GameEngine engine;
    InitializeRestFloor(engine);
    GoTo(engine, *engine.GetMerchantPosition());
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text.starts_with("F or d-pad up"); }));

    TapKey(engine, Keys::FromChar('F'));
    engine.Tick(OneSixtieth);

    ASSERT_TRUE(engine.GetTradeScreen().IsOpen());
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "MERCHANT"; }));
    TapKey(engine, Keys::Escape);
    engine.Tick(OneSixtieth);
    EXPECT_FALSE(engine.GetTradeScreen().IsOpen());
    EXPECT_FALSE(engine.GetMenu().IsOpen());   // Esc closed the trade, not into the pause menu
}
