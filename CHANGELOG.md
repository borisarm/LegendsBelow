# Changelog

All notable changes to LegendsBelow. Dates are commit dates.

## [Unreleased]

### Build
- MSVC static analysis: `-p:LBStaticAnalysis=true` (`Scripts/Test.ps1 -StaticAnalysis`) compiles with `/analyze` and fails on findings; it is a fourth CI job. Fixed what it found: root-parameter and sampler arrays filled in element by element are value-initialized, and `CharacterService`'s class-rules lookups check the rules separately before taking a reference. GoogleTest's macros trip C6326 and C6262, which are off in the test projects only.
- The engine tests are split by area into eleven files (from one 3,650-line `test.cpp`), with their shared setup and helpers in `EngineTestHelpers.h`; the same 227 tests.
- vcpkg runs in manifest mode only (`Directory.Build.props` turns off its classic mode): the projects outside the manifest no longer get the machine's global `<vcpkg root>\installed` headers on their include path, so a dependency missing from `vcpkg.json` fails everywhere instead of building only where it happens to be installed.
- Line endings pinned in `.gitattributes`: text is stored with LF and checked out with CRLF on every machine, whatever `core.autocrlf` says; binary assets (`.glb`, `.bin`, `.png`, `.ico`, `.ttf`) are never converted and the UTF-16 `resource.rc` is left alone.
- vcpkg package versions are pinned: `vcpkg-configuration.json` names the microsoft/vcpkg registry at a fixed baseline (the commit the local builds used), so CI and every machine resolve the same versions.
- Module code no longer relies on names `import std` does not guarantee: fixed-width integers and `size_t` are qualified as `std::` (545 uses) and the 20 `using namespace std;` directives in module interfaces are gone.
- Every project compiles at `/W4` with warnings as errors (`Directory.Build.targets`; headers included with angle brackets, the vcpkg and GoogleTest code, are treated as external and compiled without warnings). Fixed what `/W4` found: unused parameters of placeholder service APIs, a shadowed variable in `RenderingSystem.cpp`, an unused test variable, and signed/unsigned comparisons in tests.
- AddressSanitizer builds: `-p:LBAddressSanitizer=true` instruments every project (turning off the runtime checks and the STL container annotations it cannot be combined with here). The whole test suite runs clean under it.
- `Scripts/Test.ps1` builds and runs every test executable (`-Configuration`, `-AddressSanitizer`, `-NoBuild`), failing on a build error, a failed test, an ASan report, or a test executable that did not run.
- Continuous integration (`.github/workflows/build.yml`): Debug, Release, and Debug with AddressSanitizer build and test on every push to master and pull request, on GitHub's Windows Server 2025 image (Visual Studio 2026), with vcpkg's binary cache kept between runs.

### Fixed
- Decoded images (`GltfLoader.cpp`) and glyph distance fields (`FontLoader.cpp`) are held in `std::unique_ptr` with their library's free function, so they are released even if copying them out throws.
- `wWinMain` pairs its setup with scope guards: COM is uninitialized (it never was), and the game runtime is shut down on every exit path, not only the two it was before.
- The player's character is looked up through one checked `GameEngine::PlayerCharacter()` in the bag, equipment, character screen, and save code, instead of dereferencing the lookup unchecked.
- `GameEngine::Shutdown` followed by `Initialize` (or a second `Initialize`, or one that failed part way) no longer carries state from the previous run into the next. A dungeon engine re-initialized as the arena kept the old floor's torches, whose wall indices pointed past the new walls (out-of-bounds reads on the next frame), along with the old particles, explored map cells, and map toggle. All per-run state now lives in `GameEngine::Session` (and loaded model handles in `GameEngine::Assets`); `Shutdown` replaces both wholesale, also after a failed `Initialize`, and `Initialize` starts with `Shutdown`.

### Changed
- `Encounter` split by responsibility into `LB.Engine` partitions: `:Enemies` (the enemy data types: `Enemy`, `EnemyState`, `EnemyClips`, `CombatEvent`, `PlayerTarget`), `:EnemyBehavior` (`EnemyBehavior`: one enemy's state machine and animations, chasing around walls, turning, attacking, keeping bodies apart; it reports a landed strike and the encounter resolves it), and `:Waves` (`Waves`: wave composition and scaling, spawn points). `Encounter` keeps the roster, the wave timer, spawning, and resolving attacks both ways; its public names are unchanged (aliases of the parts'). `Distance` and `Forward` are free functions of `:World`. Behavior and the dice sequence are unchanged.
- `CombatService`: the methods that roll dice (`RollDice`, `RollAttack`, `ResolveWeaponAttack`, `SpellAttackHits`, `MakesSavingThrow`) are no longer `const`, and the random generator is no longer `mutable`; the pure calculations stay `const`.
- `GameEngine` keeps its run state (`Session`, about 20 KB) on the heap: `Shutdown` swaps in a new one instead of building a large temporary on the stack.
- `LB.Core`'s parts are its partitions: `LB.Common`, `LB.CoreValueObjects`, and `LB.Entity` became `LB.Core:Common`, `:ValueObjects`, and `:Entity` (files `Common.ixx`, `ValueObjects.ixx`, `Entity.ixx`), importing each other as siblings. `import LB.Core` is the only way in; nothing could import `LB.Common` by mistake anymore.
- `LB::Entity` is a plain value base: no virtual destructor (nothing deletes entities through it; every entity carried a vtable pointer for nothing) and no hand-written `operator!=` (C++20 derives it).
- The particle pool (`ParticleField`) is a deque: at the 4,000 cap the oldest particle is dropped in constant time instead of shifting the whole pool.
- `GameEngine` split by responsibility. Self-contained parts became their own `LB.Engine` partitions, each owning its state and testable without an engine: `:Walls` (`WallSet`: placing wall pieces, fading those that hide the player), `:Torches` (`TorchSet`: placement, flicker, lights, fire), `:Minimap` (`Minimap`, `MinimapMarkers`), `:Effects` (`EffectsSystem`: the particle pool, the effect recipes including torch flames and embers, drawing), `:CombatText` (`CombatTextLog`, `FloatingText`), and `:Hud` (`Hud::Draw` from a `HudModel` snapshot). The remaining method bodies moved out of `GameEngine.ixx` (about 1,930 lines, now about 750) into implementation units by concern: `EngineSetup.cpp` (startup, shutdown, saves), `Simulation.cpp` (one step), `WorldView.cpp` (drawing). The public API is unchanged (constants such as `TorchLightRadius` and `FadedWallAlpha` are aliases of the parts' own).
- Model lookups by file go through one `GameEngine::ModelOf` with transparent hashing (no `std::string` built per lookup), replacing four copies of the same lambda.
- The renderer's `GpuRenderer` (one 1,100-line class in `DeviceResources.cpp`) split into internal partitions, one job each: `:DeviceResources` (the GDK device and swap chain template only), `:GpuResources` (upload memory, the texture heap and uploads, built-in textures, one-off command lists), `:ModelPipeline` (GPU models, frame constants and lights, the skin palette, instanced draws), `:PrimitivePipeline` (DirectXTK12 BasicEffect and unit shapes), `:OverlayPipeline`, `:ParticlePipeline`, and `:GpuRenderer` composing them (frame lifecycle, camera, device loss). The Win32/D3D12 include list is shared in `Direct3D.h`. Mapped upload buffers are unmapped by a scope guard. Nothing outside the module changes.

### Added
- Sound effects (Kenney Impact Sounds and RPG Audio, CC0, 59 clips in `Assets/Sounds`): footsteps on stone while walking, the sword's swing, a bony clatter where blows land (a heavier one for criticals), skeletons falling apart, rising out of the ground, and winding up their attacks, a thud when the player is hit, coins and items picked up, a full bag, chests creaking open, the trapdoor, potions, a bell on level-up, and the character screen's open, close, move, confirm, and equip. World sounds are heard from the player: full volume within 3 tiles, silent at 18, panned by how far to the screen's side they are; each sound picks a variant, never the same twice in a row. Interface sounds keep playing during the tactical pause (`:Sounds`: `GameSound`, `SoundBank`; `GameSounds.cpp`; `EngineOptions::soundDirectory`, copied next to the game by the build). There is no music or ambience yet.
- Audio playback in `LB.AudioSystem`: `LoadClip` decodes Ogg Vorbis (stb_vorbis), `PlaySound` takes a stereo pan, `Update` ends sounds with their clips, at most 48 play at once, and XAudio2 plays them through a submix per channel (channel volumes now reach the device). The backend initializes COM itself, so any host gets sound. `TakePlayedClips` reports what the game asked to hear.
- Removed: `LB::Dungeon::LootDrop` and `DungeonService::GenerateLoot`, placeholder loot ("Item_N" drops with a hidden random generator) that nothing in the game used; loot comes from loot.json through `LootRoller`.
- Particle effects across the game (`Effects.cpp`): sparks where blows land (more and bigger on criticals, on enemies and on the player), bone dust when a skeleton falls apart, earth stirred up by skeletons rising, dust puffs at the player's feet while walking, sparkles over gold and items on the floor, a golden burst when a chest opens, green sparkles when a potion is drunk, a golden burst and rising column on level-up, and blue mist over the trapdoor once the exit room is cleared. Fire, sparkles and mist keep moving during the pause; dust from movement only while the game runs. Each particle carries its `ParticleEffect` (`ParticleField::Count`).
- Dust particles in the renderer: `ParticleRenderData::glowing = false` blends with its opacity instead of adding light (`ParticleAlpha_PS.hlsl`); dust is drawn before the glowing particles, one draw each.
- Particle fire on the torches: each torch near the player feeds a stream of flame particles that rise and cool from white-yellow through orange to deep red as they shrink and fade, with embers drifting higher now and then. It keeps burning during the tactical pause (it is only a look). `LB.Engine:Particles` (`Particle`, `ParticleField`, bounded at 4000) and `GameEngine::GetParticles`.
- Particles in the renderer: `RenderingSystem::DrawParticle(ParticleRenderData)` draws camera-facing soft sprites (a glow generated in code) added to the frame after the models, depth-tested without writing depth, all in one instanced draw (`Particle_VS/PS.hlsl`).
- Torches: KayKit wall torches hang on room walls (one mid-side, more on long sides, facing in; they fade with their wall), each with a flickering warm point light. Dungeon floors dim the directional lights to 30% so the torches carry the light (`GameEngine::GetTorches`, `TorchLightRadius`, `DungeonDirectionalLight`).
- Point lights in the renderer: `RenderingSystem::DrawLight(PointLight)` queues lights per frame; the model shader adds the 16 nearest to the camera's target (wrapped diffuse, smooth falloff to the radius). `SetLighting(SceneLighting)` scales the directional lights and sets the ambient light for models and primitives alike. `MeshRenderData::emissive` lets primitives glow (the flames).
- Minimap: explored floor cells (revealed within four cells in sight, and whole rooms on entering) in the top right corner, centered on the player; M or the right stick click shows the whole floor large. Marks the player, the trapdoor, unopened chests, and enemies in sight (`IsCellExplored`, `IsMapExpanded`).
- Generated dungeon floors (design-dungeon-and-world.md 2-3), the game's world (`EngineOptions::dungeon`; the test arena remains without it). `DungeonService::GenerateFloor` lays out a floor reproducibly for a seed: a critical path of rooms (a self-avoiding walk on a grid of room slots) from the entrance to the exit, side branches of treasure, elite, and safe rooms by weight, rooms of random size in their slots, and one-cell corridors (`LB::Dungeon::FloorLayout`, `FloorGenerationParams`). Floors are built from KayKit pieces on a two-tile cell grid: a wall segment on every edge between floor and solid cells, pillars in the outer corners.
- Rooms come alive: walking into a combat, elite, or exit room for the first time raises its enemies (more and heavier with depth); treasure rooms hold a chest that opens when the player steps up to it (`loot_chest`); the exit room's trapdoor leads to the next, deeper floor once the room is cleared, with a full heal (`Descend`). Depth replaces the wave count for enemy scaling. A downed player gets back up at the floor's entrance; rooms not yet cleared raise their enemies again. Saves keep the run seed, depth, cleared rooms, and opened chests, so loading rebuilds the same floor (`GetWorld`, `GetDepth`, `GetRoomStates`).
- `LB.Engine:World` (`WorldMap`): collision that slides along walls and stops exactly at them, line of sight, and a flow field that leads enemies around corners; enemies only notice the player in sight. The player and enemies move through it in the arena too (the arena is a one-room map).
- Experience, loot, and gear. Kills award the enemy's experience (level-ups show "LEVEL UP!" and give attribute points); enemies drop gold and items from their loot table (`Assets/Data/loot.json`: gold chance and dice, item chance and weighted items) that lie on the floor as KayKit props until the player walks over them (`GetGroundItems`, `GetGold`, `DropLoot`). The bag holds 20 stacks (InventoryService; items are `ItemRef`s packed into item ids). Equipping a weapon changes the attack (and the weapon in the knight's hand), armour adds its physical armour rating and movement penalty; class and level requirements apply (`EquipItem`, `UnequipArmorSlot`, `GiveItem`, `CountItem`). Healing potions (`Assets/Data/consumables.json`) are drunk with R or gamepad X (`UseItem`); the player starts with the Rusted Shortsword and one potion.
- Character screen (I or C, gamepad View): stats, attributes (spend points), equipment (take armour off), and the bag (equip, drink); navigated with W/S, arrows, or the D-pad, Enter/Space/A to choose, Esc/B to close. The world waits while it is open. HUD: level, an experience bar, gold, potions, and a reminder when there are points to spend.
- Waves scale (design-combat-system.md 8.1, the wave standing in for depth): +15% enemy hit points and +10% experience per wave, +1 damage every second wave (`Enemy::maxHitPoints`, `bonusDamage`, `experience`).
- Saves keep level, experience, attributes, unspent points, gold, the weapon, worn armour, and the bag (`CharacterService::RestoreProgress`, `GetExperienceForLevel`).
- TrueType fonts: `RenderingSystem::LoadFont` bakes a .ttf with stb_truetype into a signed distance field atlas (printable ASCII and Latin-1, with kerning), so text stays sharp at any size and outlines come from the shader; `FindFont`, `GetFont`, `MeasureText` per font, UTF-8 text. The first font loaded is the default; `TextRenderData::fontId` picks another, or "pixel" for the built-in pixel font. The HUD and damage numbers use MedievalSharp (SIL Open Font License) from `Assets/Fonts` (`EngineOptions::fontDirectory`; without it text falls back to the pixel font; a missing font stops startup).
- Damage numbers: every resolved attack floats a number over the character it hit (white, gold and larger with "!" for criticals, red over the player) or a grey "MISS", rising and fading over a second and frozen during the tactical pause (`GameEngine::GetFloatingTexts`). The HUD labels the player's health bar ("HP 12/14") and shows the wave ("WAVE 2") instead of pips.
- Overlay text in the renderer: `DrawText` draws `TextRenderData` in a built-in 5x7 pixel font (`LB.RenderingSystem:BitmapFont`: space, digits, capitals, punctuation; lower case prints as capitals) scaled by whole pixels, with an optional dark outline; `MeasureText`, `GetQueuedText`. Rectangles and text share one alpha-blended overlay draw (`Overlay_VS/PS.hlsl`, font atlas in the texture heap), back to front by layer.
- Melee attack animations from the KayKit Character Animations pack (CC0): `Assets/Models/Animations/Rig_Medium_CombatMelee.glb`. Enemy types name their attack clip and its strike moment in enemies.json (`attackAnimation`, `attackImpactFraction`): the Skeleton Minion slices diagonally with its blade (hit at 43% of the attack), the Skeleton Warrior chops with its axe (56%), so damage lands when the weapon does.
- Enemies and real-time melee combat in the test arena (design-combat-system.md): waves of skeletons rise from the floor (wave 1: two Skeleton Minions; heavier waves add Skeleton Warriors, up to 4 + 2), notice the player within their aggro range, close in, and attack on a timer (Tier 1 AI); bodies push apart and never overlap the player. The player attacks with Left Click, Space, or gamepad A (held: repeated swings): each swing turns toward the nearest enemy in reach and strikes it part way through the weapon's attack time, rooting the player until then. Hits stagger enemies that are not mid-attack; the dead play their death and fade out. A cleared wave brings the next after 3 s and restores the player's health; a downed player gets back up after 3 s and the waves start over. The tactical pause freezes it all. `LB.Engine:Encounter` (`Encounter`, `Enemy`, `CombatEvent`), `GameEngine::GetEncounter`, `GetCombatEvents`, `SpawnEnemy`, `IsPlayerAlive`, `GetPlayerAttackProfile`, `GetPlayerDefense`; `EngineOptions::spawnEnemies` and `randomSeed`.
- Weapon attack resolution in `CombatService` (design-combat-system.md 4.1-4.3, 5.2): `ResolveWeaponAttack` (d20 + bonus against AC, natural 1 misses, natural 20 hits, crit range and multiplier), `RollDice`, `ApplyDefenses` (flat physical armour, then per-type resistance fractions; negative values are vulnerabilities; at least 1 damage unless immune). The player attacks with the starting Rusted Shortsword (class attack bonus + STR) and defends with AC 10 + DEX and the equipped pieces' physical armour.
- Enemy content `Assets/Data/enemies.json` (`LB::Content::EnemyDefinition`): hit points, AC, physical armour, resistances, basic attack (dice, type, crit, attack time, recovery, range), movement and aggro range, and the models and scale they are drawn with. Skeleton Minion and Skeleton Warrior; skeletons resist piercing and necrotic and are vulnerable to bludgeoning and radiant. Validated like the other content files.
- Skinned, textured, animated models in the renderer: glTF skins (joints, inverse bind matrices, `JOINTS_0`/`WEIGHTS_0`; rigid meshes under a joint, like the warrior's helmet, become parts skinned to it), base-color textures (PNG/JPEG decoded with stb_image, sRGB, with a mip chain; identical images shared between models), emissive colors (the skeletons' glowing eyes), and animation clips (`LoadAnimations`, `FindAnimation`, `GetAnimation`) that play on any model with the same joint names. `ModelRenderData` gains `animation` (current clip, cross-faded from the previous one), `transform` (e.g. a weapon at a hand slot, from `GetJointTransform`), and `partPoses` (rigid parts posed at their origin, like the knight's sword arm). Skinned copies are still instanced: their joint matrices go into a per-frame palette read by a skinned vertex shader (`ModelSkinned_VS.hlsl`). `LB.RenderingSystem:Animation` samples, blends, and skins poses on the CPU.
- KayKit Skeletons (CC0) in `Assets/Models`: Skeleton Minion and Warrior with their blade, axe, and shield, the shared texture atlas, and the Rig_Medium General and MovementBasic animation sets in `Assets/Models/Animations` (idle, walk, hit, death, spawn). The knight carries the Dungeon Pack's common sword and slashes with it.
- HUD: the player's health bar and bars over hurt enemies.
- `InputSystem::IsActionDown` (any binding of an action held).
- Walls fade when they hide the player (game-design.md occlusion handling): wall and corner pieces crossed by the camera's sight lines to the player's feet, chest, or head ease to 30% opacity over 0.2 s and back when clear, also during the tactical pause (camera rotation). `GameEngine::GetWallOpacities`.
- Instanced model rendering: the renderer groups each frame's `DrawModel` calls by model and draws every copy of a part in one call (custom Shader Model 6.0 shader compiled at build time by FXCompile/dxc, per-instance world matrix and color). The arena goes from 401 draw calls to 26. Copies with a tint alpha below 1 are drawn see-through after the opaque world (depth-only pass, then premultiplied-alpha blend).
- glTF 2.0 model loading with fastgltf: `RenderingSystem::LoadModel` (.gltf and .glb; triangles with position, normal, and texture coordinates; node hierarchy resolved to per-part transforms; flat base-color materials; bounds), `GetModel`, `FindModel`, `DrawModel`, `GetQueuedModels`, `RenderStats::modelsRendered`. Models load headless too and upload to default-heap GPU buffers with a device; they are uploaded again after a device loss.
- KayKit Dungeon Pack starter models (CC0) in `Assets/Models`, copied next to the game. The arena is built from floor tiles, wall segments, and corner pillars at `ModelScale` 0.5, and the player is the knight, turned to face the walk direction. `EngineOptions::modelDirectory`; a missing or invalid model stops startup with every problem listed.
- Full 3D rendering on DirectXTK12: a depth-tested world pass of lit primitives (`RenderingSystem::DrawMesh` with box, cylinder, sphere, cone) drawn before the 2D overlay; `SetCamera`, `GetQueuedMeshes`, `WorldToScreen`, `RenderStats::meshesRendered`.
- `IsometricCamera` (`LB.RenderingSystem:Camera`): perspective camera at a 40 degree pitch, 90 degree rotation steps with a 0.25 s ease, Close/Medium/Wide zoom with easing, soft follow with a 1.5-tile dead zone, instant refocus (`SnapTo`), screen-to-ground directions, and world-to-screen projection matching the GPU.
- The placeholder arena is drawn in 3D (floor slabs and a ring of walls) with the player as a body, head, and visor facing the walk direction. The camera follows the player and refocuses on spawn and load; Q/E and right stick flicks rotate it, the mouse wheel and right stick up/down zoom, also during the tactical pause.
- Class data file `Assets/Data/classes.json`: starting attributes (game-design.md), weapon adjustments, spell schools, resource, base movement speed, and the level 1-20 progression tables parsed from design-character-system.md. Loaded as `ClassDefinition`, validated, and cross-checked against spells.
- Class weapon adjustment model: every class has an attack adjustment for every weapon category (+3 signature, +1 trained, -1 untrained, -5 restricted; e.g. a sword is +3 for Warriors, +1 Rogues, -1 Mages, -5 Clerics). `CharacterService::GetAttackBonus` = class attack progression + STR (melee) or DEX (bows, crossbows) + adjustment.
- `LB::Character::ClassProgression` domain type shared by content loading and `CharacterService`.
- Attribute growth (classes.json `attributeGrowth`, `primaryAttributes`): per level 1 automatic point alternating between the class's primary attributes and 1 free point the player assigns with `CharacterService::SpendAttributePoint`; primary attributes cap at 25, others at 18. `CharacterProgressionResult::attributePointsGained`, `Character::GetUnspentAttributePoints`, `LB::AttributeType`.
- Class armour adjustment model (classes.json `armorAdjustments`): per class and armour category, scales on the pieces' movement and cast penalties (e.g. Warriors feel heavy armour at x0.5 on movement, Mages pay x2 on movement and casting beyond Light, Clerics cast in Light/Medium/Shield at x0.5). `CharacterService::EquipArmor`/`UnequipArmor`/`GetEquippedArmor`, armour-aware `GetMovementSpeed`, `GetCastTimeMultiplier`; `GameEngine::EquipPlayerArmor`.
- Audio wired into the engine: XAudio2 output device (`LB.AudioSystem:XAudio2Backend`), master volume, suspend/resume with the window, gameplay channels paused during the tactical pause (music keeps playing), volumes restored from and saved to preferences.
- Persistence backed by files: save slots (metadata + key/value `SaveData`) and preferences as JSON in `%LOCALAPPDATA%\LegendsBelow\Saves`, written atomically and discovered at startup. Quick save/load on F5/F9 (game clock and player position).
- Player movement: the engine owns a Warrior (CharacterService) moving in real time with WASD/arrows or the left stick at `ClassBaseSpeed + DEX_mod * 0.1` tiles/s (design-character-system.md 4.2), inside a placeholder test arena; stops while paused.
- Solid rectangle rendering (`RenderingSystem::DrawRectangle` now drawn on the GPU) for the placeholder arena and player.
- Content loading: `LB.Content` definitions for spells, weapons, and armour loaded strictly from `Assets/Data` (all errors reported, nothing loaded on error); `ContentSpellRepository` implements the Proxy spell facade; the build copies the data next to the executable; invalid content stops startup with the error list.
- Domain additions from the design documents: `SpellSchool::Generic`, `EquipmentSlot::Hands`, `DamageType::Acid` and `Psychic`.
- `EngineOptions` (save and content directories) for `GameEngine::Initialize`; `GetInitializationError`.
- Gamepad support through GameInput: `LB.InputSystem:GameInputBackend` reads the most recently used gamepad (radial dead zones, Xbox button mapping); the engine polls it once per simulation step while focused. Start toggles the tactical pause alongside Esc. Without the GameInput runtime the game still starts, without gamepad support.
- `InputSystem`: gamepad button transitions (`IsGamepadButtonPressed`/`Down`, `SetGamepadState`), gamepad action bindings, several bindings per action, `EnableDeviceInput`/`PollDevices`; `GameEngine::OnGamepadState` for hosts and tests.
- Input wired into the engine: the shell forwards keyboard and mouse messages to `GameEngine` (`OnKeyDown`, `OnKeyUp`, `OnMouseMove`, `OnMouseButton`, `OnMouseWheel`); `LB.Engine` re-exports `LB.InputSystem`. Each simulation step reads input, then ends the input frame; losing focus releases all keys and buttons.
- Tactical pause on Esc (design-ui-ux-and-audio.md): the simulation and game clock (`GetGameTimeSeconds`) stop while the loop keeps running; the background dims as placeholder feedback.
- `LB::Systems::Input::Keys` key codes (matching Windows virtual-key codes), mouse setters, `IsMouseButtonPressed`, and `ReleaseAll`.
- `LB.Engine` (LBEngineCore): `GameEngine` runs the Tick → Update → Render loop at a fixed 60 Hz; `StepTimer` ported to `std::chrono` with deterministic `Advance`.
- Direct3D 12 backend in LBRenderingSystem: `DeviceResources` moved from the executable into the internal partition `LB.RenderingSystem:DeviceResources`; `RenderingSystem::Initialize(void* window, …)` creates the device, `EndFrame` clears and presents. A null window runs headless.
- Engine tests (StepTimer, headless GameEngine) replacing the placeholder test; rendering tests for headless mode, resize, and device callbacks.
- Repository tests: `MockSpellRepository` rewritten against the current domain modules, with tests for every `SpellRepository` query and factory, replacing placeholder tests.

### Changed
- Content loading needs `enemies.json`, `consumables.json`, and `loot.json` next to the other data files; enemies name a loot table (`lootTable`), checked against loot.json, whose items are checked against the item files.
- Overlay rectangles are drawn as alpha-blended quads instead of render-target clears; the overlay (rectangles, text, sprites) counts as one draw call.
- `WorldPoint` moved from `LB.Engine:GameEngine` to `LB.Engine:World` (still exported by `LB.Engine`). `Encounter::Configure` takes the `WorldMap` instead of the arena size.
- The model shaders take the material color and emissive per part as root constants and sample a base-color texture (a white texel for untextured materials); the instance stream carries the copy's tint and its skin palette offset.
- `CharacterService` uses data-driven class rules: HP/MP come from the class progression tables (MaxHP = table HP + CON_mod x level; MaxMP = table MP + INT/WIS_mod x level for Mages/Clerics), replacing the hard-coded formulas. Characters can only be created for classes whose rules are configured; the engine configures them from classes.json and runs without a player when there is no content.
- Removed the hard-coded class attribute bonus on creation (classes.json starting attributes are the class template) and the fixed per-level attribute gains. Armour proficiency lists replaced by the armour adjustment model; armour.json `allowedClasses` is no longer cross-checked against them.
- The render target is sRGB (shading in linear space, matching glTF colors); the engine converts its display-space colors, and the default lighting is dimmed with almost no specular.
- fastgltf and simdjson are static (overlay triplets) and built with `/guard:cf /guard:ehcont`; LBRenderingSystem drops `/GL` in Release (corrupt module debug info, LNK4020).
- Movement is screen-relative through the camera: W/stick up walks away from the camera whatever its rotation. The background is a dim ambient color instead of cornflower blue.
- `LB.Engine` re-exports `LB.RenderingSystem`. DirectXTK12 added to `vcpkg.json` and linked statically into `LBRenderingSystem.lib` through overlay triplets (`triplets/`, `vcpkg-configuration.json`) that keep every other port dynamic; no DirectXTK12.dll ships.
- LegendsBelow is now a Win32/GDK shell: `Main.cpp` imports `LB.Engine` and forwards window events; `Game`, `DeviceResources`, and `StepTimer` removed from the executable.
- `vcpkg.json` moved to the repository root so all projects share one manifest.
- LBEngineCore references only the projects it imports (LBRenderingSystem, LBModel.Core).
- Executable compiler flags aligned with the module libraries (`/fp:precise`, `/sdl`; libraries gain `/guard:cf` in Release) and C++20 in all configurations.
- Release|x64 no longer expects a nonexistent `pch.h` in LBModel.Core, LBRenderingSystem, and LBEngineCore.

### Fixed
- Flaky combat tests (about 4% failure rate): `CombatService` takes an optional dice seed and keeps its RNG per instance; the tests use a fixed seed.
- Input edge detection no longer loses taps shorter than a frame: presses and releases are recorded as transitions instead of comparing current and previous key states. Mouse actions are now edge-triggered like keyboard actions.
- LBAssetPersistenceService builds: `proxy` added to the vcpkg manifest, included via the global module fragment instead of a header unit.
- Fresh clones build on the first try: `Directory.Build.targets` runs the vcpkg install before C++ module dependency scanning.
- Test projects build in Release|x64 (C++20 was only set for Debug); tests and libraries now share `/sdl` and, in Release, `/guard:cf`, removing the C5050 module-environment warnings.

### Removed
- Empty placeholder modules: `GameState`, `SystemManager` (LBEngineCore), `RenderContext`, `GraphicsTypes` (LBRenderingSystem), and `LBAssetPersistenceService` (LBAssetPersistenceService).

## 2026-10-09

### Changed
- Moved design and architecture documents from `.github/` to `Documentation/`; `copilot-instructions.md` stays in `.github/` so GitHub Copilot picks it up.
- Merged `architecture-current.md` and `design-architecture.md` into `Documentation/architecture.md`, which now reflects the C++20 module layout and actual implementation status.
- Updated `microsoft-proxy-implementation.md` to document the Proxy 4 facade that is actually in use.
- Converted design documents from Windows-1252 to UTF-8 and restored characters lost in earlier conversions.
- Solution: the Documentation folder now points at `Documentation/`; fixed the `/Components/MOdel/` folder name.
- Rewrote the README and this changelog to match the current state of the code.

### Removed
- Legacy `Components/LBModel` and `Components/Tests/LBModelTests`, superseded by `Components/Model/*`.
- `vcpkg_installed/` output from version control (now ignored).

## 2026-01-12

### Changed
- Split the services layer away from the old `LB.Domain` types onto the per-domain modules.
- Converted systems to C++20 modules and removed PCH usage from module projects.
- Laid out the modular game architecture (`LB.Engine`, rendering split); see `Documentation/modularized-game-architecture.md`.

### Added
- Unit tests for all Model, Service, and System components.
- Expanded `copilot-instructions.md` with module rules and common build errors.

## 2026-01-11

### Changed
- Split the domain model into separate C++20 module projects: `LBModel.Core`, `.Character`, `.Combat`, `.Inventory`, `.Dungeon`.
- Moved Progression Service DTOs into their own modules.

## 2026-01-10

### Added
- C++20 module services with tests: Character, Combat, Progression, Inventory.
- Module partitions for entities (Character, Spell, Weapon, Armor, StatusEffect).

### Changed
- Repository interfaces now use Microsoft Proxy 4.0 type erasure (`LB.Repositories`).
- `LBModel` became module-ready.

## 2026-01-09

### Added
- Initial project from the Microsoft GDK DirectX 12 template.
- D&D-inspired domain model (characters, items, spells, weapons, armour) with unit tests.
- Solution structure for Services, Systems, Tools, and GoogleTest test projects.
- Repository interfaces and asset persistence scaffolding.
- Game content: `spells.json`, `weapons.json`, `armour.json`.
