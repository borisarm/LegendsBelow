// EngineTestHelpers.h - Shared setup and helpers of the engine tests (every test file includes
// it after pch.h): imports, a fixed step, engine setups for each kind of test, input taps,
// walking, fights, and finding draws in the render queues
#pragma once

import LB.Engine;
import LB.Core;
import LB.Combat;
import LB.Content;
import LB.CombatService;
import LB.Dungeon;
import LB.AudioSystem;
import std;
using namespace LB::Engine;
using namespace std::chrono_literals;
using LB::Systems::Rendering::IsometricCamera;
using LB::Systems::Rendering::ZoomLevel;

namespace EngineTests {

    // One fixed step at the default 60 Hz rate, as a real-time duration.
    constexpr auto OneSixtieth = std::chrono::duration_cast<StepTimer::Clock::duration>(
        std::chrono::duration<double>(1.0 / 60.0));

    namespace Keys = LB::Systems::Input::Keys;
    using LB::Systems::Input::MouseButton;

    inline void TapKey(GameEngine& engine, std::int32_t key) {
        engine.OnKeyDown(key);
        engine.OnKeyUp(key);
    }

    using LB::Systems::Input::ButtonMask;
    using LB::Systems::Input::GamepadButton;
    using LB::Systems::Input::GamepadState;

    inline GamepadState PadWith(std::uint32_t buttons) {
        GamepadState state{};
        state.connected = true;
        state.buttons = buttons;
        return state;
    }

    inline void TapGamepadButton(GameEngine& engine, GamepadButton button) {
        engine.OnGamepadState(0, PadWith(ButtonMask(button)));
        engine.OnGamepadState(0, PadWith(0));
    }

    using LB::Systems::Audio::AudioChannel;

    // Temporary save directory for one test, removed on destruction
    struct TempSaveDir {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "LegendsBelowTests" /
            ("engine_" + std::to_string(std::random_device{}()));

        ~TempSaveDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        std::string Utf8() const {
            const auto text = path.u8string();
            return std::string(text.begin(), text.end());
        }
    };

    inline void RunSteps(GameEngine& engine, int steps) {
        for (int i = 0; i < steps; ++i) {
            engine.Tick(OneSixtieth);
        }
    }

    // Assets/Data, found by walking up from the working directory (tests run from x64/<Config>)
    inline std::string ShippedContentDirectory() {
        for (auto dir = std::filesystem::current_path(); ; dir = dir.parent_path()) {
            const auto candidate = dir / "Assets" / "Data";
            if (std::filesystem::exists(candidate / "classes.json")) {
                const auto text = candidate.u8string();
                return std::string(text.begin(), text.end());
            }
            if (dir == dir.root_path()) return {};
        }
    }

    // The player is created from class data, so movement tests load the shipped content. They
    // test the player and the arena, so no enemies come.
    inline void InitializeWithContent(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false }))
            << engine.GetInitializationError();
    }

    inline float PlayerSpeed(const GameEngine& engine) {
        return *engine.GetCharacters().GetMovementSpeed(*engine.GetPlayerId());
    }

    inline double StepSeconds(const GameEngine& engine) {
        return StepTimer::TicksToSeconds(engine.GetTimer().GetTargetElapsedTicks());
    }

    inline void HoldKeysFor(GameEngine& engine, std::initializer_list<std::int32_t> keys, int steps) {
        for (auto key : keys) engine.OnKeyDown(key);
        RunSteps(engine, steps);
        for (auto key : keys) engine.OnKeyUp(key);
    }

    inline GamepadState Stick(float x, float y) {
        GamepadState state{};
        state.connected = true;
        state.leftThumbX = x;
        state.leftThumbY = y;
        return state;
    }

    inline GamepadState RightStick(float x, float y) {
        GamepadState state{};
        state.connected = true;
        state.rightThumbX = x;
        state.rightThumbY = y;
        return state;
    }

    // Distance from the spawn point (the origin)
    inline float Travelled(const GameEngine& engine) {
        const auto p = engine.GetPlayerPosition();
        return std::sqrt(p.x * p.x + p.y * p.y);
    }

    // Ground direction of a screen direction through the engine's camera (unit for unit input)
    inline LB::Systems::Rendering::GroundPoint Ground(const GameEngine& engine, float right, float up) {
        return engine.GetCamera().ScreenToGround(right, up);
    }

    // Where a ground point is drawn, through the camera the engine last rendered with
    inline LB::Systems::Rendering::ScreenPoint OnScreen(const GameEngine& engine, float x, float y) {
        return engine.GetRendering().WorldToScreen({ x, 0.0f, y });
    }

    // Placeholder arena: floor tiles plus the ring of wall blocks around them
    inline std::size_t ArenaMeshCount() {
        return static_cast<std::size_t>((GameEngine::ArenaHalfWidth * 2 + 2) * (GameEngine::ArenaHalfHeight * 2 + 2));
    }

    constexpr std::size_t PlayerMeshCount = 3;  // body, head, visor

    inline std::optional<std::string> FindShippedContent() {
        for (auto dir = std::filesystem::current_path(); ; dir = dir.parent_path()) {
            const auto candidate = dir / "Assets" / "Data";
            if (std::filesystem::exists(candidate / "spells.json")) {
                const auto text = candidate.u8string();
                return std::string(text.begin(), text.end());
            }
            if (dir == dir.root_path()) return std::nullopt;
        }
    }

    // Assets/Models next to Assets/Data
    inline std::string ShippedModelDirectory() {
        const auto content = ShippedContentDirectory();
        if (content.empty()) return {};
        const auto models = std::filesystem::path(std::u8string(content.begin(), content.end())).parent_path() / "Models";
        const auto text = models.u8string();
        return std::string(text.begin(), text.end());
    }

    // Arena and player only (no enemies)
    inline void InitializeWithModels(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .modelDirectory = ShippedModelDirectory(), .spawnEnemies = false }))
            << engine.GetInitializationError();
    }

    using LB::Systems::Rendering::ModelRenderData;

    // Queued draws of one model by name
    inline std::vector<ModelRenderData> DrawsOf(const GameEngine& engine, std::string_view name) {
        std::vector<ModelRenderData> draws;
        const auto& rendering = engine.GetRendering();
        for (const auto& draw : rendering.GetQueuedModels()) {
            if (rendering.GetModel(draw.model)->name == name) draws.push_back(draw);
        }
        return draws;
    }

    inline const LB::Systems::Rendering::ModelData& ModelNamed(const GameEngine& engine, std::string_view name) {
        return *engine.GetRendering().GetModel(*engine.GetRendering().FindModel(name));
    }

    inline std::size_t CountFaded(const GameEngine& engine) {
        std::size_t faded = 0;
        for (float opacity : engine.GetWallOpacities()) {
            if (opacity < 1.0f) ++faded;
        }
        return faded;
    }

    // Walk toward the camera (screen-down) until the player is at the arena edge nearest to it
    inline void WalkToNearEdge(GameEngine& engine) {
        HoldKeysFor(engine, { Keys::FromChar('S') }, 60 * 6);
    }

    constexpr float Step = 1.0f / 60.0f;
    constexpr std::uint32_t CombatSeed = 7;

    // Shipped content (and its enemy definitions), kept alive by the engine
    struct EncounterContent {
        GameEngine engine;
        const LB::Content::EnemyDefinition* minion{nullptr};
        const LB::Content::EnemyDefinition* warrior{nullptr};
        const LB::Content::EnemyDefinition* crossbowman{nullptr};
        WorldMap world = WorldMap::Arena(GameEngine::ArenaHalfWidth, GameEngine::ArenaHalfHeight);

        EncounterContent() {
            engine.Initialize(nullptr, 800, 600, EngineOptions{ .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false });
            minion = engine.GetContent()->FindEnemy(GameEngine::LightEnemy);
            warrior = engine.GetContent()->FindEnemy(GameEngine::HeavyEnemy);
            crossbowman = engine.GetContent()->FindEnemy(GameEngine::RangedEnemy);
        }
    };

    // Facing -X (yaw is measured from +Z toward +X)
    constexpr float FacingWest = -std::numbers::pi_v<float> * 0.5f;

    // A projectile for ProjectileSet tests (no attack: impacts are only reported)
    inline Projectile TestShot(WorldPoint from, WorldPoint direction, float speed, float range, ProjectileSide side = ProjectileSide::Enemies) {
        Projectile shot{};
        shot.side = side;
        shot.position = from;
        shot.direction = direction;
        shot.speed = speed;
        shot.rangeLeft = range;
        shot.model = "arrow_bow.gltf";   // an arrow sticks in walls (an orb is spent on them)
        return shot;
    }

    // The test arena split by a wall at x 0..2 (cell column 6), open only along its north edge
    inline WorldMap WalledArena() {
        const auto arena = WorldMap::Arena(GameEngine::ArenaHalfWidth, GameEngine::ArenaHalfHeight);
        auto layout = arena.Layout();
        for (std::int32_t y = 2; y < layout.height - 1; ++y) layout.cells[static_cast<std::size_t>(y * layout.width + 6)] = 0;
        return WorldMap(layout, arena.Origin());
    }

    inline Encounter MakeEncounter(const EncounterContent& content) {
        Encounter encounter(CombatSeed);
        encounter.Configure(content.minion, content.warrior, content.world);
        return encounter;
    }

    inline PlayerTarget PlayerAt(float x, float y, bool alive = true) {
        PlayerTarget player{};
        player.position = { x, y };
        player.alive = alive;
        player.defense.armorClass = 10;
        return player;
    }

    inline std::vector<CombatEvent> RunEncounter(Encounter& encounter, const PlayerTarget& player, float seconds) {
        std::vector<CombatEvent> events;
        for (int i = 0; i < static_cast<int>(std::lround(seconds / Step)); ++i) {
            for (const auto& event : encounter.Update(Step, player)) events.push_back(event);
        }
        return events;
    }

    // Hits for exactly `damage` (natural 1s still miss)
    inline LB::Services::Combat::WeaponAttackProfile FixedHit(std::int32_t damage) {
        LB::Services::Combat::WeaponAttackProfile attack{};
        attack.attackBonus = 100;
        attack.damageDice = LB::Dice{ 1, 2, 0 };
        attack.damageBonus = damage - 1;
        attack.damageType = LB::Combat::DamageType::Fire;   // no physical armour, no skeleton resistance
        attack.critRangeMin = 20;
        attack.critMultiplier = 1.0f;
        return attack;
    }

    // Strike until a hit lands (skipping natural 1s); the hit's event
    inline CombatEvent StrikeUntilHit(Encounter& encounter, std::uint32_t id, std::int32_t damage) {
        for (int i = 0; i < 20; ++i) {
            const auto event = encounter.StrikeEnemy(id, FixedHit(damage), encounter.FindEnemy(id)->position, 1.0f);
            if (event && event->result.hit) return *event;
        }
        ADD_FAILURE() << "no hit in 20 strikes";
        return {};
    }

    inline void InitializeForCombat(GameEngine& engine, bool waves = false) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .spawnEnemies = waves, .randomSeed = CombatSeed }))
            << engine.GetInitializationError();
    }

    // Point the mouse at a floor point (through the last frame's camera) and right click it
    inline void RightClickAt(GameEngine& engine, WorldPoint at) {
        const auto screen = engine.GetRendering().WorldToScreen({ at.x, 0.0f, at.y });
        engine.OnMouseMove(static_cast<std::int32_t>(std::lround(screen.x)), static_cast<std::int32_t>(std::lround(screen.y)));
        engine.OnMouseButton(LB::Systems::Input::MouseButton::Right, true);
        engine.Tick(OneSixtieth);
        engine.OnMouseButton(LB::Systems::Input::MouseButton::Right, false);
    }

    // A mage in the test arena, without waves
    inline void InitializeMage(GameEngine& engine) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .spawnEnemies = false, .randomSeed = CombatSeed,
            .playerClass = LB::Character::CharacterClass::Mage }))
            << engine.GetInitializationError();
    }

    inline std::int32_t PlayerMana(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetCurrentManaPoints();
    }

    inline std::int32_t PlayerMaxMana(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetMaxManaPoints();
    }

    inline std::int32_t PlayerHitPoints(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetCurrentHitPoints();
    }

    inline std::int32_t PlayerMaxHitPoints(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetMaxHitPoints();
    }

    // Step until a condition holds (at most `seconds`); the events seen on the way
    template<typename Condition>
    std::vector<CombatEvent> StepUntil(GameEngine& engine, Condition done, float seconds) {
        std::vector<CombatEvent> events;
        for (int i = 0; i < static_cast<int>(seconds * 60.0f) && !done(); ++i) {
            engine.Tick(OneSixtieth);
            events.insert(events.end(), engine.GetCombatEvents().begin(), engine.GetCombatEvents().end());
        }
        return events;
    }

    inline std::size_t CountKind(const std::vector<CombatEvent>& events, CombatEventKind kind) {
        return static_cast<std::size_t>(std::ranges::count(events, kind, &CombatEvent::kind));
    }

    // Kill an enemy next to the player by swinging until it dies
    // (stops early if the player goes down: getting back up clears the arena)
    inline void KillWithSword(GameEngine& engine, std::uint32_t id) {
        engine.OnKeyDown(Keys::Space);
        for (int i = 0; i < 60 * 20; ++i) {
            const auto* enemy = engine.GetEncounter().FindEnemy(id);
            if (!enemy || enemy->IsDead() || !engine.IsPlayerAlive()) break;
            engine.Tick(OneSixtieth);
        }
        engine.OnKeyUp(Keys::Space);
    }

    inline std::int32_t PlayerExperience(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetExperience();
    }

    inline std::int32_t PlayerLevel(const GameEngine& engine) {
        return engine.GetCharacters().GetCharacter(*engine.GetPlayerId())->get().GetLevel();
    }

    // Reach level 2 by killing skeleton warriors next to the player (getting back up if downed)
    inline void LevelUp(GameEngine& engine) {
        for (int i = 0; i < 8 && PlayerLevel(engine) == 1; ++i) {
            KillWithSword(engine, *engine.SpawnEnemy(GameEngine::HeavyEnemy, { 1.0f, 0.0f }));
            if (!engine.IsPlayerAlive()) RunSteps(engine, static_cast<int>(GameEngine::PlayerRespawnSeconds * 60.0f) + 2);
        }
    }

    // Two rooms joined by an L-shaped corridor:
    //   cells x 1..3, y 1..3 (room A), corridor from (2,4) down to (2,6) then right to (6,6),
    //   room B x 5..7, y 5..7
    inline WorldMap LShapedWorld() {
        LB::Dungeon::FloorLayout layout;
        layout.width = 10;
        layout.height = 10;
        layout.cells.assign(100, 0);
        for (std::int32_t y = 1; y <= 3; ++y) for (std::int32_t x = 1; x <= 3; ++x) layout.SetFloor(x, y);
        for (std::int32_t y = 5; y <= 7; ++y) for (std::int32_t x = 5; x <= 7; ++x) layout.SetFloor(x, y);
        for (std::int32_t y = 4; y <= 6; ++y) layout.SetFloor(2, y);
        for (std::int32_t x = 2; x <= 5; ++x) layout.SetFloor(x, 6);
        layout.rooms.push_back({ LB::Dungeon::RoomType::Entrance, { 1, 1, 3, 3 }, true, { 1 } });
        layout.rooms.push_back({ LB::Dungeon::RoomType::Exit, { 5, 5, 3, 3 }, true, { 0 } });
        layout.exitRoom = 1;
        return WorldMap(layout, {});
    }

    inline void InitializeDungeon(GameEngine& engine, std::uint32_t seed = CombatSeed) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .randomSeed = seed, .dungeon = true }))
            << engine.GetInitializationError();
    }

    // Steer the player with the left stick toward a world point (one step); the stick is in
    // screen space, so the ground direction goes back through the camera
    inline void SteerToward(GameEngine& engine, WorldPoint target) {
        const auto at = engine.GetPlayerPosition();
        const float dx = target.x - at.x;
        const float dy = target.y - at.y;
        const float length = std::hypot(dx, dy);
        GamepadState pad{};
        pad.connected = true;
        if (length > 0.05f) {
            const auto right = Ground(engine, 1.0f, 0.0f);
            const auto up = Ground(engine, 0.0f, 1.0f);
            pad.leftThumbX = (dx * right.x + dy * right.y) / length;
            pad.leftThumbY = (dx * up.x + dy * up.y) / length;
        }
        engine.OnGamepadState(0, pad);
    }

    // Walk (and swing at anything in reach) toward a point along the floor's flow field
    inline void WalkTo(GameEngine& engine, WorldPoint target, int maxSteps, bool attack = false) {
        auto world = engine.GetWorld();   // a copy: its flow field is separate from the enemies'
        world.BuildFlowField(target);
        if (attack) engine.OnKeyDown(Keys::Space);
        for (int i = 0; i < maxSteps; ++i) {
            if (Encounter::Distance(engine.GetPlayerPosition(), target) < 0.3f) break;
            const auto next = world.NextWaypoint(engine.GetPlayerPosition());
            SteerToward(engine, next && world.RoomAt(engine.GetPlayerPosition()) != world.RoomAt(target) ? *next : target);
            engine.Tick(OneSixtieth);
        }
        if (attack) engine.OnKeyUp(Keys::Space);
        engine.OnGamepadState(0, GamepadState{ .connected = true });
    }

    // The first room of a type on the floor
    inline std::int32_t FindRoom(const GameEngine& engine, LB::Dungeon::RoomType type) {
        const auto& rooms = engine.GetWorld().Layout().rooms;
        for (std::size_t i = 0; i < rooms.size(); ++i) {
            if (rooms[i].type == type) return static_cast<std::int32_t>(i);
        }
        return -1;
    }

    // Assets/Sounds of the repository
    inline std::string ShippedSoundDirectory() {
        const auto content = ShippedContentDirectory();
        if (content.empty()) return {};
        const auto sounds = std::filesystem::path(std::u8string(content.begin(), content.end())).parent_path() / "Sounds";
        const auto text = sounds.u8string();
        return std::string(text.begin(), text.end());
    }

    // An engine with content and the shipped sounds (no models), in the arena or a dungeon
    inline void InitializeWithSounds(GameEngine& engine, bool waves = false, bool dungeon = false) {
        ASSERT_TRUE(engine.Initialize(nullptr, 800, 600, EngineOptions{
            .contentDirectory = ShippedContentDirectory(), .soundDirectory = ShippedSoundDirectory(),
            .spawnEnemies = waves, .randomSeed = 7u, .dungeon = dungeon }))
            << engine.GetInitializationError();
    }

    // Did any of a sound's clips play among these?
    inline bool Played(const std::vector<std::string>& played, GameSound sound) {
        const auto clips = SoundBank::RecipeFor(sound).clips;
        return std::ranges::any_of(played, [&](const std::string& clip) { return std::ranges::find(clips, clip) != clips.end(); });
    }

    inline std::size_t CountPlayed(const std::vector<std::string>& played, GameSound sound) {
        const auto clips = SoundBank::RecipeFor(sound).clips;
        return static_cast<std::size_t>(std::ranges::count_if(played, [&](const std::string& clip) { return std::ranges::find(clips, clip) != clips.end(); }));
    }

} // namespace EngineTests

using namespace EngineTests;
