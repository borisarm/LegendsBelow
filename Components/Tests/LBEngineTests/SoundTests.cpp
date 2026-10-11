// SoundTests.cpp - LB.Engine tests: the sound bank and the sounds gameplay plays
#include "pch.h"
#include "EngineTestHelpers.h"

// ============================================================================
// Sound Bank Tests
// ============================================================================

TEST(SoundBankTests, EverySound_HasClips_AllShipped) {
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(GameSound::Count); ++i) {
        EXPECT_FALSE(SoundBank::RecipeFor(static_cast<GameSound>(i)).clips.empty()) << static_cast<int>(i);
    }
    const auto directory = std::filesystem::path(ShippedSoundDirectory());
    for (const auto clip : SoundBank::AllClips()) {
        EXPECT_TRUE(std::filesystem::exists(directory / (std::string(clip) + ".ogg"))) << clip;
    }
}

TEST(SoundBankTests, PickClip_NeverTheSameTwiceInARow) {
    SoundBank bank;
    auto last = bank.PickClip(GameSound::Footstep);
    for (int i = 0; i < 50; ++i) {
        const auto next = bank.PickClip(GameSound::Footstep);
        EXPECT_NE(next, last);
        last = next;
    }
    EXPECT_EQ(bank.PickClip(GameSound::BagFull), bank.PickClip(GameSound::BagFull));   // one clip: always it
}

TEST(SoundBankTests, Placement_FadesWithDistance_PansBySide) {
    const WorldPoint player{ 0.0f, 0.0f };
    const WorldPoint right{ 1.0f, 0.0f };

    EXPECT_FLOAT_EQ(SoundBank::Placement({ 1.0f, 0.0f }, player, right).first, 1.0f);
    EXPECT_GT(SoundBank::Placement({ 10.0f, 0.0f }, player, right).first, 0.0f);
    EXPECT_LT(SoundBank::Placement({ 10.0f, 0.0f }, player, right).first, 1.0f);
    EXPECT_FLOAT_EQ(SoundBank::Placement({ 30.0f, 0.0f }, player, right).first, 0.0f);

    EXPECT_GT(SoundBank::Placement({ 4.0f, 0.0f }, player, right).second, 0.0f);    // to the right
    EXPECT_LT(SoundBank::Placement({ -4.0f, 0.0f }, player, right).second, 0.0f);   // to the left
    EXPECT_FLOAT_EQ(SoundBank::Placement({ 0.0f, 5.0f }, player, right).second, 0.0f);   // straight ahead
}

// ============================================================================
// GameEngine Sound Tests
// ============================================================================

TEST(GameEngineSoundTests, NoSoundDirectory_Silent) {
    GameEngine engine;
    InitializeForCombat(engine);
    TapKey(engine, Keys::Space);
    RunSteps(engine, 30);

    EXPECT_TRUE(engine.GetAudio().TakePlayedClips().empty());
}

TEST(GameEngineSoundTests, InvalidSoundDirectory_StopsStartupListingClips) {
    GameEngine engine;

    EXPECT_FALSE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .soundDirectory = "Z:/no/such/sounds" }));

    EXPECT_NE(engine.GetInitializationError().find(std::format("({} problems)", SoundBank::AllClips().size())), std::string::npos)
        << engine.GetInitializationError();
}

TEST(GameEngineSoundTests, Swing_AndAKill_Heard) {
    GameEngine engine;
    InitializeWithSounds(engine);
    const auto id = *engine.SpawnEnemy(GameEngine::LightEnemy, { 1.0f, 0.0f });

    KillWithSword(engine, id);

    const auto played = engine.GetAudio().TakePlayedClips();
    EXPECT_TRUE(Played(played, GameSound::Swing));
    EXPECT_TRUE(Played(played, GameSound::HitEnemy) || Played(played, GameSound::CriticalHit));
    EXPECT_EQ(CountPlayed(played, GameSound::EnemyDies), 1u);
}

TEST(GameEngineSoundTests, Walking_Footsteps_StandingStill_None) {
    GameEngine engine;
    InitializeWithSounds(engine);
    RunSteps(engine, 60);
    EXPECT_EQ(CountPlayed(engine.GetAudio().TakePlayedClips(), GameSound::Footstep), 0u);

    HoldKeysFor(engine, { Keys::FromChar('D') }, 90);   // about 1.5 seconds of walking

    EXPECT_GE(CountPlayed(engine.GetAudio().TakePlayedClips(), GameSound::Footstep), 2u);
}

TEST(GameEngineSoundTests, RisingEnemies_AndTheirSwings_Heard) {
    GameEngine engine;
    InitializeWithSounds(engine, true);   // the first wave rises

    RunSteps(engine, 60 * 6);   // they rise, close in, and attack

    const auto played = engine.GetAudio().TakePlayedClips();
    EXPECT_GE(CountPlayed(played, GameSound::EnemyRises), 2u);
    EXPECT_TRUE(Played(played, GameSound::EnemySwing));
}

TEST(GameEngineSoundTests, CharacterScreen_OpenAndClose_HeardWhilePaused) {
    GameEngine engine;
    InitializeWithSounds(engine);

    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);
    TapKey(engine, Keys::Down);
    engine.Tick(OneSixtieth);
    TapKey(engine, Keys::FromChar('I'));
    engine.Tick(OneSixtieth);

    const auto played = engine.GetAudio().TakePlayedClips();
    EXPECT_TRUE(Played(played, GameSound::ScreenOpen));
    EXPECT_TRUE(Played(played, GameSound::MenuMove));
    EXPECT_TRUE(Played(played, GameSound::ScreenClose));
}

TEST(GameEngineSoundTests, Chest_AndTheWayDown_Heard) {
    std::optional<std::uint32_t> seed;
    for (std::uint32_t candidate = 1; candidate < 40 && !seed; ++candidate) {
        GameEngine probe;
        InitializeDungeon(probe, candidate);
        if (FindRoom(probe, LB::Dungeon::RoomType::Treasure) >= 0) seed = candidate;
    }
    ASSERT_TRUE(seed.has_value());
    GameEngine engine;
    ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
        .contentDirectory = ShippedContentDirectory(), .soundDirectory = ShippedSoundDirectory(), .randomSeed = *seed, .dungeon = true }));
    const auto room = FindRoom(engine, LB::Dungeon::RoomType::Treasure);

    WalkTo(engine, engine.GetWorld().RoomCenter(room), 60 * 90, true);
    engine.Descend();

    const auto played = engine.GetAudio().TakePlayedClips();
    EXPECT_TRUE(Played(played, GameSound::ChestOpen));
    EXPECT_TRUE(Played(played, GameSound::Gold));   // the chest always holds gold
    EXPECT_TRUE(Played(played, GameSound::Descend));
}
