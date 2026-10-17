// NewGamePlusTests.cpp - LB.Engine tests: New Game+ (design-loot-and-progression.md 7.3): the same
// hero down a new Below after a victory, its enemies stronger and its finds richer by the cycle
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    EngineOptions DungeonOptions(std::int32_t depth = 1, std::int32_t cycle = 0, const TempSaveDir* dir = nullptr) {
        return EngineOptions{ .saveDirectory = dir ? dir->Utf8() : std::string{}, .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false,
                              .randomSeed = CombatSeed, .dungeon = true, .startDepth = depth, .startCycle = cycle };
    }

    // The hero wins on the final floor and chooses Descend Again on the victory page
    void WinAndDescendAgain(GameEngine& engine) {
        engine.EnterFloorPosition(engine.GetWorld().RoomCenter(engine.GetWorld().Layout().exitRoom));
        RunSteps(engine, static_cast<int>(60 * EnemyBehavior::SpawnSeconds) + 2);
        const auto& enemies = engine.GetEncounter().GetEnemies();
        const auto boss = std::ranges::find_if(enemies, [](const Enemy& e) { return e.definition->key == GameEngine::FinalBoss; });
        ASSERT_NE(boss, enemies.end());
        ASSERT_TRUE(engine.DefeatEnemy(boss->id));
        RunSteps(engine, 2);
        ASSERT_EQ(engine.GetMenu().Page(), MenuPage::Victory);
        ASSERT_EQ(engine.GetMenu().Selected()->command, MenuCommand::DescendAgain);
        EXPECT_EQ(engine.GetMenu().Selected()->value, "New Game+1");
        TapKey(engine, Keys::Enter);
        RunSteps(engine, 2);
    }

    // The first enemy raised on walking into the floor's first combat room
    const Enemy* FirstOfACombatRoom(GameEngine& engine) {
        const auto room = FindRoom(engine, LB::Dungeon::RoomType::Combat);
        if (room < 0) return nullptr;
        engine.EnterFloorPosition(engine.GetWorld().RoomCenter(room));
        engine.Tick(OneSixtieth);
        const auto& enemies = engine.GetEncounter().GetEnemies();
        return enemies.empty() ? nullptr : &enemies.front();
    }
}

TEST(NewGamePlusTests, ChallengeDepth_GrowsByTheFinalDepthEachCycle) {
    GameEngine first, second;
    ASSERT_TRUE(first.Initialize(nullptr, 800, 600, DungeonOptions(3)));
    ASSERT_TRUE(second.Initialize(nullptr, 800, 600, DungeonOptions(3, 1)));

    EXPECT_EQ(first.GetCycle(), 0);
    EXPECT_EQ(first.ChallengeDepth(), 3);
    EXPECT_EQ(second.GetCycle(), 1);
    EXPECT_EQ(second.ChallengeDepth(), 3 + GameEngine::FinalDepth);
    EXPECT_EQ(second.GetTheme().name, "Crypts");   // the look and the inhabitants go by the depth
}

TEST(NewGamePlusTests, Enemies_AreStrongerInACycle) {
    GameEngine first, second;
    ASSERT_TRUE(first.Initialize(nullptr, 800, 600, DungeonOptions(2)));
    ASSERT_TRUE(second.Initialize(nullptr, 800, 600, DungeonOptions(2, 1)));   // the same seed: the same floor

    const auto* weak = FirstOfACombatRoom(first);
    const auto* strong = FirstOfACombatRoom(second);
    ASSERT_NE(weak, nullptr);
    ASSERT_NE(strong, nullptr);
    EXPECT_EQ(weak->definition->key, strong->definition->key);
    EXPECT_GT(strong->maxHitPoints, weak->maxHitPoints * 2);
    EXPECT_GT(strong->bonusDamage, weak->bonusDamage);
    EXPECT_EQ(weak->bonusAttack, 0);   // veterans: they hit truer and are harder to hit
    EXPECT_EQ(strong->bonusAttack, GameEngine::NewGamePlusAttackBonus);
    EXPECT_EQ(strong->bonusArmorClass, GameEngine::NewGamePlusArmorClass);
}

TEST(NewGamePlusTests, Finds_AreDeeperInACycle) {
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, DungeonOptions(4, 1)));   // a resting place
    ASSERT_TRUE(engine.IsRestFloor());

    bool rolled = false;
    for (const auto& entry : engine.GetMerchantStock()) {
        if (engine.ItemLevel(entry.item) == 0) continue;
        rolled = true;
        EXPECT_EQ(engine.ItemLevel(entry.item), 4 + GameEngine::FinalDepth);
    }
    EXPECT_TRUE(rolled);
}

TEST(NewGamePlusTests, DescendAgain_TheSameHeroDownANewBelow) {
    GameEngine fresh;
    ASSERT_TRUE(fresh.Initialize(nullptr, 800, 600, DungeonOptions()));
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, DungeonOptions(GameEngine::FinalDepth)));
    engine.GrantExperience(5000);
    engine.Tick(OneSixtieth);
    const auto level = PlayerLevel(engine);
    const auto weapon = engine.GetPlayerWeaponItem();

    WinAndDescendAgain(engine);

    EXPECT_FALSE(engine.GetMenu().IsOpen());
    EXPECT_EQ(engine.GetCycle(), 1);
    EXPECT_EQ(engine.GetDepth(), 1);
    EXPECT_EQ(PlayerLevel(engine), level);
    EXPECT_EQ(engine.GetPlayerWeaponItem(), weapon);
    EXPECT_EQ(PlayerHitPoints(engine), PlayerMaxHitPoints(engine));
    EXPECT_NE(engine.GetWorld().Layout().cells, fresh.GetWorld().Layout().cells);   // other floors
    ASSERT_NE(engine.GetFanfare().Current(), nullptr);
    EXPECT_EQ(engine.GetFanfare().Current()->title, "THE BELOW STIRS");
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "DEPTH 1 - NEW GAME+1"; }));
}

TEST(NewGamePlusTests, TheCycle_IsSavedAndLoaded) {
    TempSaveDir dir;
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, DungeonOptions(GameEngine::FinalDepth, 0, &dir)));
    WinAndDescendAgain(engine);
    engine.EnterFloor(2);
    ASSERT_TRUE(engine.SaveGame(GameEngine::SaveSlots[0]));
    EXPECT_TRUE(engine.SlotSummary(GameEngine::SaveSlots[0]).find("depth 2, New Game+1") != std::string::npos) << engine.SlotSummary(GameEngine::SaveSlots[0]);
    const auto cells = engine.GetWorld().Layout().cells;

    GameEngine loaded;
    ASSERT_TRUE(loaded.Initialize(nullptr, 800, 600, DungeonOptions(1, 0, &dir)));
    ASSERT_TRUE(loaded.LoadGame(GameEngine::SaveSlots[0]));
    EXPECT_EQ(loaded.GetCycle(), 1);
    EXPECT_EQ(loaded.GetDepth(), 2);
    EXPECT_EQ(loaded.ChallengeDepth(), 2 + GameEngine::FinalDepth);
    EXPECT_EQ(loaded.GetWorld().Layout().cells, cells);
}
