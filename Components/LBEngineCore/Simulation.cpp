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

        // An open menu takes the input; else the character screen does while open (Esc closes it
        // instead of opening the pause menu)
        const bool menuWasOpen = menu_.IsOpen();
        const bool screenWasOpen = session_->characterScreen.IsOpen();
        if (menuWasOpen) {
            UpdateMenu();
        }
        else {
            UpdateCharacterScreen();
            if (!screenWasOpen && !session_->characterScreen.IsOpen() && inputSystem_.IsActionTriggered(std::string(PauseAction))) {
                OpenMenu(MenuPage::Pause);
                PlaySound(GameSound::ScreenOpen);
            }
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
        const bool running = !session_->paused && !menu_.IsOpen() && !screenWasOpen && !session_->characterScreen.IsOpen();
        if (running) {
            session_->gameTimeSeconds += elapsedSeconds;
            // Attacks first: a swing that starts this step roots the player at once
            UpdatePlayerCombat(static_cast<float>(elapsedSeconds));
            const auto before = session_->playerPosition;
            MovePlayer(static_cast<float>(elapsedSeconds));
            const bool moving = Distance(before, session_->playerPosition) > 1e-4f;
            UpdateEncounter(static_cast<float>(elapsedSeconds));
            AnimatePlayer(static_cast<float>(elapsedSeconds), moving);
            UpdateQuickUse();
            if (session_->dungeonMode) session_->minimap.Reveal(session_->world, session_->playerPosition);
            UpdateGroundItems(static_cast<float>(elapsedSeconds));
            UpdateCombatTexts(static_cast<float>(elapsedSeconds));
            EmitCombatEffects();
            EmitProjectileEffects();
            PlayCombatSounds();
            PlayProjectileSounds();
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
        session_->encounter.Configure(content_->FindEnemy(LightEnemy), content_->FindEnemy(HeavyEnemy), session_->world, content_->FindEnemy(RangedEnemy));
        session_->encounter.SetClips(assets_.enemyClips);
        session_->encounter.Reset();
        if (session_->spawnEnemies && session_->playerId && !session_->dungeonMode) {
            session_->encounter.StartWave(1, session_->playerPosition);
        }
    }

    /// <summary>
    /// The player's attack: Attack (left mouse, Space, gamepad A) swings the weapon, or draws and
    /// looses a shot with a bow or crossbow, again and again while held. A swing turns toward the
    /// nearest enemy in reach and strikes it part way through (Encounter::AttackImpactFraction);
    /// a shot turns toward the nearest enemy in range and in sight (design-combat-system.md 7.4),
    /// or straight ahead without one, and leaves at ShotReleaseFraction as an arrow or bolt that
    /// hits whatever it reaches. The player stands still until the strike or the shot. A downed
    /// player gets back up after PlayerRespawnSeconds and the waves restart.
    /// </summary>
    void GameEngine::UpdatePlayerCombat(float elapsedSeconds) {
        if (!session_->playerId) return;
        auto& combat = session_->playerCombat;
        combat.hitFlashSeconds = std::max(0.0f, combat.hitFlashSeconds - elapsedSeconds);

        if (!IsPlayerAlive()) {
            combat.swinging = false;
            combat.deadSeconds += elapsedSeconds;
            if (combat.deadSeconds >= PlayerRespawnSeconds) {
                RespawnPlayer();
            }
            return;
        }

        const float attackSeconds = PlayerAttackSeconds();
        if (combat.swinging) {
            combat.swingSeconds += elapsedSeconds;
            const float impact = combat.shooting ? ShotReleaseFraction : Encounter::AttackImpactFraction;
            if (!combat.impactDone && combat.swingSeconds >= attackSeconds * impact) {
                combat.impactDone = true;
                ReleasePlayerAttack();
            }
            if (combat.swingSeconds >= attackSeconds) {
                combat.swinging = false;
            }
        }

        const std::string attack(AttackAction);
        if (!combat.swinging && (inputSystem_.IsActionTriggered(attack) || inputSystem_.IsActionDown(attack))) {
            StartPlayerAttack();
        }
    }

    // A swing or a shot begins: pick the target, face it, and start the attack's animation
    void GameEngine::StartPlayerAttack() {
        auto& combat = session_->playerCombat;
        const auto* weapon = session_->playerWeapon;
        combat.swinging = true;
        combat.swingSeconds = 0.0f;
        combat.impactDone = false;
        combat.hurtSeconds = 0.0f;
        combat.shooting = weapon && weapon->IsRanged();
        combat.target = combat.shooting ? session_->encounter.FindTarget(session_->playerPosition, weapon->range, true)
                                        : session_->encounter.FindTarget(session_->playerPosition, PlayerReach);
        if (const auto* target = combat.target ? session_->encounter.FindEnemy(*combat.target) : nullptr) {
            session_->playerFacingYaw = std::atan2(target->position.x - session_->playerPosition.x, target->position.y - session_->playerPosition.y);
        }
        if (!combat.shooting) PlaySound(GameSound::Swing, session_->playerPosition);

        // The whole clip spans the attack (a bow's draw spans it until the arrow leaves)
        const auto& clips = assets_.playerClips;
        const float attackSeconds = PlayerAttackSeconds();
        ClipInfo clip = clips.slash;
        float spanSeconds = attackSeconds;
        if (combat.shooting && weapon->category == LB::Combat::WeaponType::Bow) {
            clip = clips.bowDraw;
            spanSeconds = attackSeconds * ShotReleaseFraction;
        }
        else if (combat.shooting) {
            clip = clips.shoot;
        }
        else if (weapon && weapon->category == LB::Combat::WeaponType::Dagger) {
            clip = clips.stab;
        }
        else if (weapon && weapon->isTwoHanded) {
            clip = clips.slashTwoHanded;
        }
        session_->playerAnimator.Play(clip.id, false, clip.seconds / spanSeconds, true, 0.08f);
    }

    // The strike lands on the target picked at the start (if it is still in reach), or the shot
    // leaves toward the target where it stands now (straight ahead without one)
    void GameEngine::ReleasePlayerAttack() {
        auto& combat = session_->playerCombat;
        if (!combat.shooting) {
            if (!combat.target) return;
            if (auto event = session_->encounter.StrikeEnemy(*combat.target, GetPlayerAttackProfile(), session_->playerPosition, PlayerReach)) {
                session_->combatEvents.push_back(*event);
                if (event->killed) {
                    OnEnemyKilled(*session_->encounter.FindEnemy(event->enemyId));
                }
            }
            return;
        }

        const auto& weapon = *session_->playerWeapon;
        auto direction = Forward(session_->playerFacingYaw);
        if (const auto* target = combat.target ? session_->encounter.FindEnemy(*combat.target) : nullptr; target && target->IsTargetable()) {
            direction = { target->position.x - session_->playerPosition.x, target->position.y - session_->playerPosition.y };
            session_->playerFacingYaw = std::atan2(direction.x, direction.y);
        }
        const auto model = weapon.category == LB::Combat::WeaponType::Crossbow ? BoltModel : ArrowModel;
        session_->encounter.Shoot(GetPlayerAttackProfile(), session_->playerPosition, direction, weapon.projectileSpeed, weapon.range,
                                  std::string(model), PlayerRadius);
        PlaySound(GameSound::Shoot, session_->playerPosition);

        if (weapon.category == LB::Combat::WeaponType::Bow) {
            const auto& release = assets_.playerClips.bowRelease;
            const float rest = PlayerAttackSeconds() * (1.0f - ShotReleaseFraction);
            session_->playerAnimator.Play(release.id, false, release.seconds / rest, true, 0.05f);
        }
    }

    // The player's animation for this step: the attack plays out (started by StartPlayerAttack),
    // else running, a flinch from a hit, standing, or lying dead
    void GameEngine::AnimatePlayer(float elapsedSeconds, bool moving) {
        auto& combat = session_->playerCombat;
        auto& animator = session_->playerAnimator;
        const auto& clips = assets_.playerClips;
        combat.hurtSeconds = std::max(0.0f, combat.hurtSeconds - elapsedSeconds);

        if (!IsPlayerAlive()) {
            animator.Play(clips.death.id, false, 1.0f, false, 0.1f);
        }
        else if (combat.swinging) {
            // the attack's clip
        }
        else if (moving) {
            const float speed = session_->characterService.GetMovementSpeed(*session_->playerId).value_or(RunAnimationSpeed);
            animator.Play(clips.run.id, true, speed / RunAnimationSpeed);
        }
        else if (combat.hurtSeconds <= 0.0f) {
            const bool bow = session_->playerWeapon && session_->playerWeapon->category == LB::Combat::WeaponType::Bow;
            animator.Play(bow ? clips.bowIdle.id : clips.idle.id, true);
        }
        animator.Update(elapsedSeconds);
    }

    /// <summary>
    /// Enemies act; their hits are applied to the player. A new wave restores the player's health.
    /// </summary>
    void GameEngine::UpdateEncounter(float elapsedSeconds) {
        if (!session_->playerId) return;

        const PlayerTarget target{ session_->playerPosition, IsPlayerAlive(), GetPlayerDefense(), PlayerRadius };
        if (session_->dungeonMode) {
            UpdateFloorInteractions();
        }
        const auto wave = session_->encounter.GetWave();
        for (const auto& event : session_->encounter.Update(elapsedSeconds, target)) {
            session_->combatEvents.push_back(event);
            // The player's arrows and bolts arrive during the enemies' step
            if (event.kind == CombatEventKind::PlayerAttack) {
                if (event.killed) {
                    if (const auto* enemy = session_->encounter.FindEnemy(event.enemyId)) OnEnemyKilled(*enemy);
                }
                continue;
            }
            if (!event.result.hit || event.result.finalDamage <= 0 || !IsPlayerAlive()) continue;

            session_->characterService.DamageCharacter(*session_->playerId, event.result.finalDamage);
            session_->playerCombat.hitFlashSeconds = Encounter::HitFlashSeconds;
            if (!IsPlayerAlive()) {
                session_->playerCombat.swinging = false;
                session_->playerCombat.deadSeconds = 0.0f;
            }
            else if (!session_->playerCombat.swinging) {
                // A flinch (it does not interrupt anything: an attack keeps playing)
                const auto& hit = assets_.playerClips.hit;
                session_->playerAnimator.Play(hit.id, false, 1.0f, true, 0.05f);
                session_->playerCombat.hurtSeconds = hit.seconds;
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
