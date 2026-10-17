// FinalBossTests.cpp - LB.Engine tests: the final floor (the Red Library) and its boss, the First
// Archivist (three phases, echoes of the earlier tiers, a second telegraphed attack, the torches
// guttering in its last phase), and the victory (the way up, the victory page, the codex's last page)
#include "pch.h"
#include "EngineTestHelpers.h"

namespace {
    struct ArchivistFight {
        EncounterContent content;
        Encounter encounter{ CombatSeed };
        const LB::Content::EnemyDefinition* archivist{ content.engine.GetContent()->FindEnemy(GameEngine::FinalBoss) };
        std::uint32_t boss{0};

        ArchivistFight() {
            encounter.Configure(content.minion, content.warrior, content.world);
            encounter.SetContent(content.engine.GetContent().get());
            boss = encounter.Spawn(*archivist, { 5.0f, 0.0f }, FacingWest, false);
        }

        std::vector<const Enemy*> Summons() const {
            std::vector<const Enemy*> raised;
            for (const auto& enemy : encounter.GetEnemies()) {
                if (enemy.summoner == boss) raised.push_back(&enemy);
            }
            return raised;
        }

        // Down to just below a share of its health, and a step for it to notice
        void WoundTo(float fraction) {
            const auto* enemy = encounter.FindEnemy(boss);
            const auto left = static_cast<std::int32_t>(static_cast<float>(enemy->maxHitPoints) * fraction) - 1;
            encounter.DamageEnemy(boss, enemy->hitPoints - left, LB::Combat::DamageType::None);
            encounter.Update(Step, PlayerAt(0.0f, 0.0f));
        }

        const Enemy& Boss() const { return *encounter.FindEnemy(boss); }
    };

    void InitializeFinalFloor(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .randomSeed = CombatSeed, .dungeon = true }))
            << engine.GetInitializationError();
        engine.EnterFloor(GameEngine::FinalDepth);
        engine.Tick(OneSixtieth);
    }

    // Into the great hall: its boss appears to meet the hero (and is ready once it has)
    std::uint32_t EnterTheHall(GameEngine& engine) {
        engine.EnterFloorPosition(engine.GetWorld().RoomCenter(engine.GetWorld().Layout().exitRoom));
        RunSteps(engine, static_cast<int>(60 * EnemyBehavior::SpawnSeconds) + 2);
        for (const auto& enemy : engine.GetEncounter().GetEnemies()) {
            if (enemy.definition->key == GameEngine::FinalBoss) return enemy.id;
        }
        ADD_FAILURE() << "no Archivist in the hall";
        return 0;
    }
}

TEST(FinalBossTests, FinalFloor_IsTheGreatHallBelowTheLastRestingPlace) {
    EXPECT_TRUE(GameEngine::IsRestDepth(GameEngine::FinalDepth - 1));
    EXPECT_EQ(GameEngine::BossForDepth(GameEngine::FinalDepth), GameEngine::FinalBoss);

    GameEngine engine;
    InitializeFinalFloor(engine);

    EXPECT_TRUE(engine.IsFinalFloor());
    EXPECT_FALSE(engine.IsRestFloor());
    const auto& layout = engine.GetWorld().Layout();
    ASSERT_EQ(layout.rooms.size(), 2u);
    EXPECT_EQ(layout.rooms[static_cast<std::size_t>(layout.exitRoom)].type, LB::Dungeon::RoomType::Exit);
    EXPECT_EQ(ThemeForDepth(GameEngine::FinalDepth).name, "Archive");
    EXPECT_EQ(engine.GetObjective().kind, ObjectiveKind::None);   // nothing asked of the hero but the end
    EXPECT_FALSE(engine.GetLoreBook().has_value());
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "DEPTH 14 - THE RED LIBRARY"; }));
}

TEST(FinalBossTests, FinalFloor_TheArchivistWaitsAloneInTheHall) {
    GameEngine engine;
    InitializeFinalFloor(engine);
    EXPECT_EQ(engine.GetEncounter().LivingEnemyCount(), 0u);

    ASSERT_NE(EnterTheHall(engine), 0u);

    EXPECT_EQ(engine.GetEncounter().LivingEnemyCount(), 1u);
}

TEST(FinalBossTests, Archivist_RaisesNoneInItsFirstPhase) {
    ArchivistFight fight;
    ASSERT_NE(fight.archivist, nullptr);

    RunEncounter(fight.encounter, PlayerAt(0.0f, 0.0f), fight.archivist->summonCooldownSeconds * 1.5f);

    EXPECT_EQ(fight.Boss().Phase(), 1);
    EXPECT_TRUE(fight.Summons().empty());
}

TEST(FinalBossTests, Archivist_RaisesEchoesOfTheEarlierTiersInItsSecondPhase) {
    ArchivistFight fight;
    fight.WoundTo(fight.archivist->phaseTwoHealthFraction);
    ASSERT_EQ(fight.Boss().Phase(), 2);
    ASSERT_EQ(fight.encounter.GetPhaseChanges().size(), 1u);

    RunEncounter(fight.encounter, PlayerAt(0.0f, 0.0f), fight.archivist->summonCooldownSeconds * 0.5f + 0.5f);   // two raisings

    std::set<std::string> kinds;
    for (const auto* summoned : fight.Summons()) kinds.insert(summoned->definition->key);
    EXPECT_EQ(kinds, (std::set<std::string>(fight.archivist->summons.begin(), fight.archivist->summons.end())));   // each in turn
    EXPECT_LE(fight.Summons().size(), static_cast<std::size_t>(fight.archivist->maxSummons));
}

TEST(FinalBossTests, Archivist_ThirdPhaseOpensItsLastAttack) {
    ArchivistFight fight;
    ASSERT_TRUE(fight.archivist->phaseThreeSlam.has_value());
    const auto last = *fight.archivist->phaseThreeSlam;
    fight.WoundTo(fight.archivist->phaseTwoHealthFraction);
    fight.WoundTo(fight.archivist->phaseThreeHealthFraction);
    ASSERT_EQ(fight.Boss().Phase(), 3);
    EXPECT_TRUE(fight.Boss().phaseTwo);   // still enraged

    // A hero close by: the ring around it, wider than its first
    const auto player = PlayerAt(3.0f, 0.0f);
    bool windingUp = false;
    for (int i = 0; i < 60 * 4 && !windingUp; ++i) {
        fight.encounter.Update(Step, player);
        windingUp = fight.Boss().slamSeconds > 0.0f && fight.Boss().lastSlam;
    }
    ASSERT_TRUE(windingUp);
    EXPECT_FLOAT_EQ(fight.Boss().ActiveSlam().radius, last.radius);
    EXPECT_GT(last.radius, fight.archivist->slamRadius);

    bool landed = false;
    for (int i = 0; i < 60 * 3 && !landed; ++i) {
        const auto events = fight.encounter.Update(Step, player);
        landed = std::ranges::any_of(fight.encounter.GetBursts(), [&](const SpellBurst& burst) { return burst.radius == last.radius; });
        if (landed) EXPECT_GE(CountKind(events, CombatEventKind::EnemyAttack), 1u);
    }
    EXPECT_TRUE(landed);
    EXPECT_GT(fight.Boss().lastSlamCooldown, 0.0f);
}

TEST(FinalBossTests, Necromancer_HasOnlyTwoPhases) {
    EncounterContent content;
    auto encounter = MakeEncounter(content);
    encounter.SetContent(content.engine.GetContent().get());
    const auto* necromancer = content.engine.GetContent()->FindEnemy(GameEngine::BossEnemy);
    const auto id = encounter.Spawn(*necromancer, { 5.0f, 0.0f }, FacingWest, false);

    encounter.DamageEnemy(id, encounter.FindEnemy(id)->maxHitPoints - 1, LB::Combat::DamageType::None);
    encounter.Update(Step, PlayerAt(0.0f, 0.0f));

    EXPECT_EQ(encounter.FindEnemy(id)->Phase(), 2);
}

TEST(FinalBossTests, Torches_GutterInItsLastPhaseAndBurnAgainWhenItFalls) {
    GameEngine engine;
    InitializeFinalFloor(engine);
    const auto boss = EnterTheHall(engine);
    ASSERT_FALSE(engine.GetTorches().empty());
    EXPECT_FLOAT_EQ(engine.GetTorchBrightness(), 1.0f);

    ASSERT_TRUE(engine.WoundEnemy(boss, 0.2f));
    RunSteps(engine, static_cast<int>(60 * (TorchSet::GutterSeconds + 0.5f)));
    ASSERT_EQ(engine.GetEncounter().FindEnemy(boss)->Phase(), 3);
    EXPECT_FLOAT_EQ(engine.GetTorchBrightness(), TorchSet::GutteredBrightness);

    ASSERT_TRUE(engine.DefeatEnemy(boss));
    RunSteps(engine, static_cast<int>(60 * (TorchSet::GutterSeconds + 0.5f)));
    EXPECT_FLOAT_EQ(engine.GetTorchBrightness(), 1.0f);
}

TEST(FinalBossTests, Hud_NamesItsLastPhase) {
    GameEngine engine;
    InitializeFinalFloor(engine);
    const auto boss = EnterTheHall(engine);

    ASSERT_TRUE(engine.WoundEnemy(boss, 0.2f));
    RunSteps(engine, 2);

    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "The First Archivist - UNBOUND"; }));
}

TEST(FinalBossTests, Archivist_IsDrawnRed) {
    GameEngine engine;
    InitializeWithModels(engine);
    engine.SpawnEnemy(GameEngine::FinalBoss, { 6.0f, 0.0f });

    RunSteps(engine, 3);

    const auto draws = DrawsOf(engine, "Lorekeeper");
    ASSERT_EQ(draws.size(), 1u);
    EXPECT_LT(draws[0].tint.g, draws[0].tint.r * 0.6f);
}

TEST(FinalBossTests, Victory_ItsFallOpensTheWayUpNotATrapdoor) {
    GameEngine engine;
    InitializeFinalFloor(engine);
    const auto boss = EnterTheHall(engine);
    EXPECT_FALSE(engine.IsWayUpOpen());

    ASSERT_TRUE(engine.DefeatEnemy(boss));
    RunSteps(engine, 2);   // the hall counts as cleared at the next step

    EXPECT_TRUE(engine.IsWayUpOpen());
    ASSERT_NE(engine.GetFanfare().Current(), nullptr);
    EXPECT_EQ(engine.GetFanfare().Current()->title, "THE BELOW IS SILENT");   // before any legacy banner
    EXPECT_EQ(engine.GetRunStats().bosses, 1);
    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Victory);   // the hero stood in the hall's middle: the way up
    EXPECT_EQ(engine.GetDepth(), GameEngine::FinalDepth);    // no way down
}

TEST(FinalBossTests, Victory_SteppingIntoTheWayUpOpensThePage_LingeringClosesIt) {
    GameEngine engine;
    InitializeFinalFloor(engine);
    const auto boss = EnterTheHall(engine);
    const auto aside = WorldPoint{ engine.GetWayUp().x - 3.0f, engine.GetWayUp().y };
    engine.EnterFloorPosition(aside);
    ASSERT_TRUE(engine.DefeatEnemy(boss));
    RunSteps(engine, 2);
    EXPECT_FALSE(engine.GetMenu().IsOpen());

    engine.EnterFloorPosition(engine.GetWayUp());
    RunSteps(engine, 2);
    ASSERT_EQ(engine.GetMenu().Page(), MenuPage::Victory);
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "VICTORY"; }));
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text.ends_with("has silenced the Below"); }));
    EXPECT_TRUE(std::ranges::any_of(engine.GetRendering().GetQueuedText(), [](const auto& t) { return t.text == "Bosses defeated: 1"; }));

    TapKey(engine, Keys::Down);   // past Descend Again
    RunSteps(engine, 1);
    ASSERT_EQ(engine.GetMenu().Selected()->command, MenuCommand::Linger);
    TapKey(engine, Keys::Enter);
    RunSteps(engine, 2);
    EXPECT_FALSE(engine.GetMenu().IsOpen());   // standing in it does not open it again
    RunSteps(engine, 30);
    EXPECT_FALSE(engine.GetMenu().IsOpen());

    engine.EnterFloorPosition(aside);
    RunSteps(engine, 2);
    engine.EnterFloorPosition(engine.GetWayUp());
    RunSteps(engine, 2);
    EXPECT_EQ(engine.GetMenu().Page(), MenuPage::Victory);   // stepping in again does
}

TEST(FinalBossTests, Codex_TheLastPageHasItsOwnHeading) {
    GameEngine engine;
    InitializeFinalFloor(engine);
    const auto* page = engine.GetContent()->FindLore(GameEngine::FinalLore);
    ASSERT_NE(page, nullptr);
    EXPECT_GT(static_cast<std::size_t>(page->tier), FloorThemes.size());   // no floor holds it

    const auto lines = engine.CodexLines();
    EXPECT_NE(std::ranges::find(lines, std::format("- {} -", GameEngine::FinalFloorTitle)), lines.end());
}
