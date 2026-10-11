// Simulation.cpp - LB.Engine implementation unit: one GameEngine simulation step (input, the
// player's movement and attacks, the enemies, the camera controls)
module LB.Engine;

import std;
import LB.Core;
import LB.Character;
import LB.CharacterService;
import LB.Content;
import LB.InputSystem;
import LB.RenderingSystem;

namespace LB::Engine {

    /// <summary>
    /// Advance the simulation by one step.
    /// </summary>
    void GameEngine::Update(double elapsedSeconds) {
        if (active_) {
            inputSystem_.PollDevices();
        }

        // The character screen takes the input while open (Esc closes it instead of pausing)
        const bool screenWasOpen = session_->characterScreen.IsOpen();
        UpdateCharacterScreen();

        if (!screenWasOpen && !session_->characterScreen.IsOpen() && inputSystem_.IsActionTriggered(std::string(PauseAction))) {
            SetPaused(!session_->paused);
        }

        if (inputSystem_.IsActionTriggered(std::string(QuickSaveAction))) {
            SaveGame(QuickSaveSlot);
        }
        if (inputSystem_.IsActionTriggered(std::string(QuickLoadAction))) {
            LoadGame(QuickSaveSlot);
        }

        // The camera can be turned and zoomed during the tactical pause
        UpdateCameraControls();
        if (inputSystem_.IsActionTriggered(std::string(MapAction))) {
            session_->minimap.ToggleExpanded();
        }

        session_->combatEvents.clear();
        const bool running = !session_->paused && !screenWasOpen && !session_->characterScreen.IsOpen();
        if (running) {
            session_->gameTimeSeconds += elapsedSeconds;
            // Attacks first: a swing that starts this step roots the player at once
            UpdatePlayerCombat(static_cast<float>(elapsedSeconds));
            MovePlayer(static_cast<float>(elapsedSeconds));
            UpdateEncounter(static_cast<float>(elapsedSeconds));
            UpdateQuickUse();
            if (session_->dungeonMode) session_->minimap.Reveal(session_->world, session_->playerPosition);
            UpdateGroundItems(static_cast<float>(elapsedSeconds));
            UpdateCombatTexts(static_cast<float>(elapsedSeconds));
            EmitCombatEffects();
            PlayCombatSounds();
        }

        if (session_->playerId) {
            session_->camera.Follow({ session_->playerPosition.x, session_->playerPosition.y });
        }
        session_->camera.Update(static_cast<float>(elapsedSeconds));
        session_->walls.UpdateFade(session_->camera.GetView().eye,
            session_->playerId ? std::optional{ session_->playerPosition } : std::nullopt, static_cast<float>(elapsedSeconds));

        // Fire keeps burning during the pause and on the character screen: it is only a look
        UpdateEffects(static_cast<float>(elapsedSeconds), running);
        UpdateSounds(static_cast<float>(elapsedSeconds), running);

        // This step has consumed the input frame; presses are seen by exactly one step
        inputSystem_.Update();
    }

    // Waves start once there is a player to fight (and enemies are enabled)
    void GameEngine::StartEncounter() {
        session_->encounter.Configure(content_->FindEnemy(LightEnemy), content_->FindEnemy(HeavyEnemy), session_->world);
        session_->encounter.SetClips(assets_.enemyClips);
        session_->encounter.Reset();
        if (session_->spawnEnemies && session_->playerId && !session_->dungeonMode) {
            session_->encounter.StartWave(1, session_->playerPosition);
        }
    }

    /// <summary>
    /// The player's melee attack: Attack (left mouse, Space, gamepad A) swings the weapon,
    /// again and again while held. A swing turns toward the nearest enemy in reach and strikes
    /// it part way through (Encounter::AttackImpactFraction); the player stands still until
    /// then. A downed player gets back up after PlayerRespawnSeconds and the waves restart.
    /// </summary>
    void GameEngine::UpdatePlayerCombat(float elapsedSeconds) {
        if (!session_->playerId) return;
        session_->playerCombat.hitFlashSeconds = std::max(0.0f, session_->playerCombat.hitFlashSeconds - elapsedSeconds);

        if (!IsPlayerAlive()) {
            session_->playerCombat.swinging = false;
            session_->playerCombat.deadSeconds += elapsedSeconds;
            if (session_->playerCombat.deadSeconds >= PlayerRespawnSeconds) {
                RespawnPlayer();
            }
            return;
        }

        const float attackSeconds = PlayerAttackSeconds();
        if (session_->playerCombat.swinging) {
            session_->playerCombat.swingSeconds += elapsedSeconds;
            if (!session_->playerCombat.impactDone && session_->playerCombat.swingSeconds >= attackSeconds * Encounter::AttackImpactFraction) {
                session_->playerCombat.impactDone = true;
                if (session_->playerCombat.target) {
                    if (auto event = session_->encounter.StrikeEnemy(*session_->playerCombat.target, GetPlayerAttackProfile(), session_->playerPosition, PlayerReach)) {
                        session_->combatEvents.push_back(*event);
                        if (event->killed) {
                            OnEnemyKilled(*session_->encounter.FindEnemy(event->enemyId));
                        }
                    }
                }
            }
            if (session_->playerCombat.swingSeconds >= attackSeconds) {
                session_->playerCombat.swinging = false;
            }
        }

        const std::string attack(AttackAction);
        if (!session_->playerCombat.swinging && (inputSystem_.IsActionTriggered(attack) || inputSystem_.IsActionDown(attack))) {
            session_->playerCombat.swinging = true;
            session_->playerCombat.swingSeconds = 0.0f;
            PlaySound(GameSound::Swing, session_->playerPosition);
            session_->playerCombat.impactDone = false;
            session_->playerCombat.target = session_->encounter.FindTarget(session_->playerPosition, PlayerReach);
            if (const auto* target = session_->playerCombat.target ? session_->encounter.FindEnemy(*session_->playerCombat.target) : nullptr) {
                session_->playerFacingYaw = std::atan2(target->position.x - session_->playerPosition.x, target->position.y - session_->playerPosition.y);
            }
        }
    }

    /// <summary>
    /// Enemies act; their hits are applied to the player. A new wave restores the player's health.
    /// </summary>
    void GameEngine::UpdateEncounter(float elapsedSeconds) {
        if (!session_->playerId) return;

        const PlayerTarget target{ session_->playerPosition, IsPlayerAlive(), GetPlayerDefense() };
        if (session_->dungeonMode) {
            UpdateFloorInteractions();
        }
        const auto wave = session_->encounter.GetWave();
        for (const auto& event : session_->encounter.Update(elapsedSeconds, target)) {
            session_->combatEvents.push_back(event);
            if (!event.result.hit || event.result.finalDamage <= 0 || !IsPlayerAlive()) continue;

            session_->characterService.DamageCharacter(*session_->playerId, event.result.finalDamage);
            session_->playerCombat.hitFlashSeconds = Encounter::HitFlashSeconds;
            if (!IsPlayerAlive()) {
                session_->playerCombat.swinging = false;
                session_->playerCombat.deadSeconds = 0.0f;
            }
        }

        if (session_->encounter.GetWave() != wave) {
            session_->characterService.RestCharacter(*session_->playerId);
        }
        if (session_->dungeonMode) {
            UpdateRooms();
        }
    }

    // Age the combat texts and start one for each of this step's attacks, over the head of whoever
    // was attacked
    void GameEngine::UpdateCombatTexts(float elapsedSeconds) {
        session_->combatText.Update(elapsedSeconds);
        for (const auto& event : session_->combatEvents) {
            Systems::Rendering::Vector3 at{ session_->playerPosition.x, PlayerTop(), session_->playerPosition.y };
            if (event.kind == CombatEventKind::PlayerAttack) {
                const auto* enemy = session_->encounter.FindEnemy(event.enemyId);
                if (!enemy) continue;
                at = { enemy->position.x, EnemyTop(*enemy), enemy->position.y };
            }
            session_->combatText.AddAttack(event, at);
        }
    }

    // Back on the feet with full health: at the arena's center (the waves start over), or at
    // the floor's entrance (rooms not yet cleared raise their enemies again)
    void GameEngine::RespawnPlayer() {
        session_->characterService.ReviveCharacter(*session_->playerId, 1.0f);
        session_->playerPosition = session_->dungeonMode ? session_->world.RoomCenter(session_->world.Layout().entranceRoom) : WorldPoint{};
        for (auto& room : session_->rooms) {
            if (!room.cleared) {
                room.triggered = false;
                room.enemies.clear();
            }
        }
        session_->characterService.PlaceCharacter(*session_->playerId, ToTile(session_->playerPosition));
        session_->camera.SnapTo({ session_->playerPosition.x, session_->playerPosition.y });
        session_->playerCombat = {};
        session_->groundItems.clear();
        StartEncounter();
    }

    /// <summary>
    /// Q/E and right stick flicks rotate the camera; the mouse wheel and right stick up/down zoom
    /// </summary>
    void GameEngine::UpdateCameraControls() {
        if (inputSystem_.IsActionTriggered(std::string(RotateCameraLeftAction))) session_->camera.RotateCounterClockwise();
        if (inputSystem_.IsActionTriggered(std::string(RotateCameraRightAction))) session_->camera.RotateClockwise();

        const auto wheel = inputSystem_.GetMouseState().wheelDelta;
        if (wheel > 0) session_->camera.ZoomIn();
        if (wheel < 0) session_->camera.ZoomOut();

        // Flicks act once when the stick crosses the threshold
        const auto& pad = inputSystem_.GetGamepadState(0);
        const auto flick = [](float now, float before) {
            if (now >= CameraStickThreshold && before < CameraStickThreshold) return 1;
            if (now <= -CameraStickThreshold && before > -CameraStickThreshold) return -1;
            return 0;
        };
        const int turn = flick(pad.rightThumbX, session_->previousRightStick.x);
        const int zoom = flick(pad.rightThumbY, session_->previousRightStick.y);
        if (turn > 0) session_->camera.RotateClockwise();
        if (turn < 0) session_->camera.RotateCounterClockwise();
        if (zoom > 0) session_->camera.ZoomIn();
        if (zoom < 0) session_->camera.ZoomOut();
        session_->previousRightStick = WorldPoint{ pad.rightThumbX, pad.rightThumbY };
    }

    /// <summary>
    /// Real-time movement for one simulation step: WASD/arrow keys (screen directions,
    /// diagonals normalized) or the left stick (analog), turned to the ground through the
    /// camera so "up" always walks away from it. Speed comes from CharacterService.
    /// </summary>
    void GameEngine::MovePlayer(float elapsedSeconds) {
        if (!IsPlayerAlive()) return;
        if (session_->playerCombat.swinging && !session_->playerCombat.impactDone) return;   // committed to the swing

        namespace Keys = Systems::Input::Keys;
        const auto held = [this](std::int32_t a, std::int32_t b) {
            return inputSystem_.IsKeyDown(a) || inputSystem_.IsKeyDown(b);
        };

        float dx = 0.0f;
        float dy = 0.0f;
        if (held(Keys::FromChar('D'), Keys::Right)) dx += 1.0f;
        if (held(Keys::FromChar('A'), Keys::Left)) dx -= 1.0f;
        if (held(Keys::FromChar('S'), Keys::Down)) dy += 1.0f;
        if (held(Keys::FromChar('W'), Keys::Up)) dy -= 1.0f;

        if (dx != 0.0f || dy != 0.0f) {
            const float length = std::sqrt(dx * dx + dy * dy);
            dx /= length;
            dy /= length;
        }
        else {
            // Stick up is +Y; screen up is -Y. Dead zones are applied by the input backend.
            const auto& pad = inputSystem_.GetGamepadState(0);
            dx = pad.leftThumbX;
            dy = -pad.leftThumbY;
            const float magnitude = std::sqrt(dx * dx + dy * dy);
            if (magnitude > 1.0f) {
                dx /= magnitude;
                dy /= magnitude;
            }
        }

        if (dx == 0.0f && dy == 0.0f) return;

        // dy is screen-down; the camera maps (right, up) onto the ground
        const auto direction = session_->camera.ScreenToGround(dx, -dy);
        session_->playerFacingYaw = std::atan2(direction.x, direction.y);

        const float speed = session_->characterService.GetMovementSpeed(*session_->playerId).value_or(0.0f);
        session_->playerPosition = session_->world.Move(session_->playerPosition, { direction.x * speed * elapsedSeconds, direction.y * speed * elapsedSeconds }, PlayerRadius);

        // Keep the domain's tile position in sync (at most one tile per step at these speeds)
        const auto tile = ToTile(session_->playerPosition);
        if (!session_->characterService.MoveCharacter(*session_->playerId, tile)) {
            session_->characterService.PlaceCharacter(*session_->playerId, tile);
        }
    }

} // namespace LB::Engine
