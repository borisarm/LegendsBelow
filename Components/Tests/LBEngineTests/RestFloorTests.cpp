// RestFloorTests.cpp - LB.Engine tests: resting places after the bosses (no enemies, a shrine, a
// merchant), buying and selling, retraining
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

// Retraining (game-design.md, respec options): the points the player assigned come back, for gold
TEST(RestFloorTests, Retrain_GivesBackTheAssignedPoints) {
    GameEngine engine;
    InitializeRestFloor(engine);
    engine.GrantExperience(1600);   // level 5
    engine.Tick(OneSixtieth);
    const auto& hero = [&]() -> const LB::Character::Character& { return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get(); };
    const auto strength = hero().GetAttributes().strength;
    std::int32_t spent = 0;
    while (engine.SpendAttributePoint(LB::AttributeType::Strength)) ++spent;
    ASSERT_GT(spent, 0);
    ASSERT_EQ(hero().GetAttributes().strength, strength + spent);

    // Kept in a save
    ASSERT_TRUE(engine.SaveGame(1));
    ASSERT_TRUE(engine.LoadGame(1));
    engine.Tick(OneSixtieth);

    Earn(engine, engine.RetrainCost());
    ASSERT_GE(engine.GetGold(), engine.RetrainCost());
    EXPECT_FALSE(engine.Retrain());   // not by the merchant
    GoTo(engine, *engine.GetMerchantPosition());
    const auto gold = engine.GetGold();
    const auto cost = engine.RetrainCost();
    EXPECT_EQ(cost, GameEngine::RetrainGoldPerLevel * 5);

    ASSERT_TRUE(engine.Retrain());
    EXPECT_EQ(hero().GetAttributes().strength, strength);
    EXPECT_EQ(hero().GetUnspentAttributePoints(), spent);
    EXPECT_EQ(engine.GetGold(), gold - cost);
    EXPECT_GE(hero().GetCurrentHitPoints(), 1);
    EXPECT_FALSE(engine.Retrain());   // nothing assigned now: no charge
    EXPECT_EQ(engine.GetGold(), gold - cost);

    // Spent again elsewhere
    ASSERT_TRUE(engine.SpendAttributePoint(LB::AttributeType::Dexterity));
    EXPECT_EQ(engine.GetCharacters().GetAssignedPoints(*engine.GetPlayerId()).at(LB::AttributeType::Dexterity), 1);
}

// The full respec: the path forgotten, the hero chooses again
TEST(RestFloorTests, ForgetPath_LetsTheHeroChooseAgain) {
    GameEngine engine;
    InitializeRestFloor(engine);
    engine.GrantExperience(1600);   // level 5: a path
    engine.Tick(OneSixtieth);
    const auto* classData = engine.GetContent()->FindClass(LB::Character::CharacterClass::Warrior);
    ASSERT_GE(classData->specializations.size(), 2u);
    const auto& first = classData->specializations[0].key;
    const auto& second = classData->specializations[1].key;
    ASSERT_TRUE(engine.ChoosePath(first));
    EXPECT_FALSE(engine.ChoosePath(second));   // one path at a time

    Earn(engine, engine.ForgetPathCost());
    ASSERT_GE(engine.GetGold(), engine.ForgetPathCost());
    GoTo(engine, *engine.GetMerchantPosition());
    EXPECT_GT(engine.ForgetPathCost(), engine.RetrainCost());
    const auto gold = engine.GetGold();

    ASSERT_TRUE(engine.ForgetPath());
    EXPECT_EQ(engine.GetPlayerPath(), nullptr);
    EXPECT_EQ(engine.GetGold(), gold - engine.ForgetPathCost());
    EXPECT_FALSE(engine.ForgetPath());   // no path to forget
    ASSERT_TRUE(engine.ChoosePath(second));
    EXPECT_EQ(engine.GetPlayerPath()->key, second);
}
