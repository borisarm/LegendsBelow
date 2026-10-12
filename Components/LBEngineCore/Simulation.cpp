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
        const bool screenWasOpen = ScreenOpen();
        if (menuWasOpen) {
            UpdateMenu();
        }
        else {
            // The merchant's screen first: the key that closes it must not open the character screen
            const bool tradeWasOpen = session_->trade.IsOpen();
            UpdateTradeScreen();
            if (!tradeWasOpen && !session_->trade.IsOpen()) UpdateCharacterScreen();
            if (!screenWasOpen && !ScreenOpen() && inputSystem_.IsActionTriggered(std::string(PauseAction))) {
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

        // The camera can be turned and zoomed during the tactical pause; orders are taken during it
        UpdateCameraControls();
        if (!menuWasOpen && !screenWasOpen && !menu_.IsOpen() && !ScreenOpen()) UpdateOrders();
        if (inputSystem_.IsActionTriggered(std::string(MapAction))) {
            session_->minimap.ToggleExpanded();
        }

        session_->combatEvents.clear();
        const bool running = !session_->paused && !menu_.IsOpen() && !screenWasOpen && !ScreenOpen();
        if (running) {
            session_->gameTimeSeconds += elapsedSeconds;
            // Attacks first: a swing that starts this step roots the player at once
            FollowOrder();
            UpdatePlayerCombat(static_cast<float>(elapsedSeconds));
            UpdatePlayerCasting(static_cast<float>(elapsedSeconds));
            const auto before = session_->playerPosition;
            MovePlayer(static_cast<float>(elapsedSeconds));
            const bool moving = Distance(before, session_->playerPosition) > 1e-4f;
            UpdateEncounter(static_cast<float>(elapsedSeconds));
            UpdatePlayerStatuses(static_cast<float>(elapsedSeconds));
            AnimatePlayer(static_cast<float>(elapsedSeconds), moving);
            UpdateQuickUse();
            if (session_->dungeonMode) session_->minimap.Reveal(session_->world, session_->playerPosition);
            UpdateGroundItems(static_cast<float>(elapsedSeconds));
            UpdateCombatTexts(static_cast<float>(elapsedSeconds));
            EmitCombatEffects();
            EmitProjectileEffects();
            EmitSpellEffects(static_cast<float>(elapsedSeconds));
            PlayCombatSounds();
            PlayProjectileSounds();
            PlaySpellSounds();
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
        session_->encounter.Configure(content_->FindEnemy(LightEnemy), content_->FindEnemy(HeavyEnemy), session_->world,
                                      content_->FindEnemy(RangedEnemy), content_->FindEnemy(CasterEnemy), content_->FindEnemy(BruteEnemy));
        session_->encounter.SetContent(content_.get());
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
        const bool free = !combat.swinging && !session_->casting.active && !session_->playerStatuses.PreventsActions();
        if (free && (inputSystem_.IsActionTriggered(attack) || inputSystem_.IsActionDown(attack))) {
            session_->order = {};   // the player's own attack replaces an order
            StartPlayerAttack();
        }
    }

    // ------------------------------------------------------------------------------------------
    // Orders and the tactical pause (design-combat-system.md 2.1)
    // ------------------------------------------------------------------------------------------

    /// <summary>
    /// Every step, paused or not: Tab toggles the tactical pause; the mouse picks the enemy under
    /// it, and a right click orders a move there or an attack on it. While paused, the d-pad
    /// cycles the picked enemy (nearest first), Attack orders an attack on the picked one, and a
    /// spell key orders a cast at it; they are carried out when the pause ends.
    /// </summary>
    void GameEngine::UpdateOrders() {
        if (!session_->playerId) return;
        const auto triggered = [this](std::string_view action) { return inputSystem_.IsActionTriggered(std::string(action)); };
        if (triggered(TacticalPauseAction)) SetPaused(!session_->paused);

        // The enemy under the mouse (when it moved), kept while it can be targeted
        std::optional<WorldPoint> floor;
        const auto& mouse = inputSystem_.GetMouseState();
        if (const auto point = renderingSystem_.ScreenToFloor(static_cast<float>(mouse.x), static_cast<float>(mouse.y))) floor = WorldPoint{ point->x, point->z };
        if (floor && (mouse.deltaX != 0 || mouse.deltaY != 0 || triggered(OrderAction))) {
            session_->picked.reset();
            float best = PickRadius;
            for (const auto& enemy : session_->encounter.GetEnemies()) {
                const float distance = Distance(enemy.position, *floor);
                if (enemy.IsTargetable() && distance <= best) {
                    best = distance;
                    session_->picked = enemy.id;
                }
            }
        }
        if (session_->picked) {
            const auto* enemy = session_->encounter.FindEnemy(*session_->picked);
            if (!enemy || !enemy->IsTargetable()) session_->picked.reset();
        }

        if (triggered(OrderAction) && floor && IsPlayerAlive()) {
            if (session_->picked) session_->order = PlayerOrder{ OrderKind::Attack, {}, session_->picked, 0 };
            else if (session_->world.IsOpen(*floor)) session_->order = PlayerOrder{ OrderKind::Move, *floor, std::nullopt, 0 };
        }
        if (!session_->paused || !IsPlayerAlive()) return;

        // The d-pad cycles through the enemies, nearest first
        const int cycle = (triggered(MenuRightAction) ? 1 : 0) - (triggered(MenuLeftAction) ? 1 : 0);
        if (cycle != 0) {
            std::vector<const Enemy*> targets;
            for (const auto& enemy : session_->encounter.GetEnemies()) {
                if (enemy.IsTargetable()) targets.push_back(&enemy);
            }
            std::ranges::sort(targets, {}, [this](const Enemy* enemy) { return Distance(enemy->position, session_->playerPosition); });
            if (!targets.empty()) {
                auto index = 0;
                for (std::size_t i = 0; i < targets.size(); ++i) {
                    if (session_->picked && targets[i]->id == *session_->picked) index = static_cast<int>(i) + cycle;
                }
                const auto count = static_cast<int>(targets.size());
                session_->picked = targets[static_cast<std::size_t>(((index % count) + count) % count)]->id;
            }
        }
        if (session_->picked && triggered(AttackAction)) session_->order = PlayerOrder{ OrderKind::Attack, {}, session_->picked, 0 };
        for (std::size_t slot = 0; slot < SpellSlots; ++slot) {
            if (triggered(SpellActions[slot]) && slot < GetSpellBar().size()) session_->order = PlayerOrder{ OrderKind::Cast, {}, session_->picked, slot };
        }
    }

    // Can the order's target be struck (or shot, or cast at) from here?
    bool GameEngine::OrderTargetInReach(const Enemy& target) const {
        const float distance = Distance(target.position, session_->playerPosition);
        const auto& order = session_->order;
        const bool sight = session_->world.LineOfSight(session_->playerPosition, target.position);
        if (order.kind == OrderKind::Cast) {
            const auto bar = GetSpellBar();
            if (order.slot >= bar.size()) return true;
            const auto& spell = *bar[order.slot];
            return spell.IsBurstAroundCaster() ? distance <= spell.radius : distance <= spell.range * 0.9f && sight;
        }
        const auto* weapon = session_->playerWeapon;
        if (weapon && weapon->IsRanged()) return distance <= weapon->range * 0.9f && sight;
        return distance <= PlayerReach;
    }

    // An order in hand: attack or cast once the target is in reach (the order then holds for the
    // next attack, a cast ends it); a target that is gone ends it
    void GameEngine::FollowOrder() {
        auto& order = session_->order;
        if (order.kind == OrderKind::None || !IsPlayerAlive()) return;
        const auto* target = order.target ? session_->encounter.FindEnemy(*order.target) : nullptr;
        if (order.target && (!target || !target->IsTargetable())) {
            order = {};
            return;
        }
        if (order.kind == OrderKind::Attack && target && OrderTargetInReach(*target)) {
            session_->playerFacingYaw = std::atan2(target->position.x - session_->playerPosition.x, target->position.y - session_->playerPosition.y);
            if (!session_->playerCombat.swinging && !session_->casting.active && !session_->playerStatuses.PreventsActions()) StartPlayerAttack(order.target);
        }
        if (order.kind == OrderKind::Cast && !session_->casting.active && !session_->playerCombat.swinging && (!target || OrderTargetInReach(*target))) {
            const auto slot = order.slot;
            const auto aim = order.target;
            order = {};
            StartCast(slot, aim);
        }
    }

    // Where an order walks the player this step (a ground direction), or nothing: arrived, in
    // reach, or busy
    std::optional<WorldPoint> GameEngine::OrderStep() const {
        const auto& order = session_->order;
        if (order.kind == OrderKind::None || session_->casting.active) return std::nullopt;
        WorldPoint goal = order.point;
        if (order.target) {
            const auto* target = session_->encounter.FindEnemy(*order.target);
            if (!target || OrderTargetInReach(*target)) return std::nullopt;
            goal = target->position;
        }
        else if (order.kind != OrderKind::Move) {
            return std::nullopt;
        }
        const float dx = goal.x - session_->playerPosition.x;
        const float dy = goal.y - session_->playerPosition.y;
        const float length = std::hypot(dx, dy);
        if (length <= ArriveTiles) return std::nullopt;
        return WorldPoint{ dx / length, dy / length };
    }

    // ------------------------------------------------------------------------------------------
    // Spells (design-spells-system.md 4): the bar's slots cast what classes.json puts on them
    // ------------------------------------------------------------------------------------------

    const Content::ClassDefinition* GameEngine::PlayerClassDefinition() const {
        return content_->FindClass(session_->playerClass);
    }

    // The player's attack on a target: a rogue's sneak attack dice join it against an enemy that
    // has not noticed them, is struck from behind (flanked), cannot act or move, or right after a
    // shadowstep (design-character-system.md 3.2.2)
    Services::Combat::WeaponAttackProfile GameEngine::PlayerAttackAgainst(const Enemy* target) const {
        auto attack = GetPlayerAttackProfile();
        const auto* classData = PlayerClassDefinition();
        if (!classData || classData->sneakAttackDice.count <= 0) return attack;
        const bool unaware = target && target->state == EnemyState::Idle;
        const bool helpless = target && (target->statuses.PreventsActions() || target->statuses.MovementMultiplier() <= 0.0f);
        bool behind = false;
        if (target) {
            const auto facing = Forward(target->facingYaw);
            const float dx = session_->playerPosition.x - target->position.x;
            const float dy = session_->playerPosition.y - target->position.y;
            const float length = std::hypot(dx, dy);
            behind = length > 1e-4f && (facing.x * dx + facing.y * dy) / length < FlankCosine;
        }
        if (unaware || helpless || behind || session_->sneakWindowSeconds > 0.0f) attack.extraDice = classData->sneakAttackDice;
        return attack;
    }

    void GameEngine::HealPlayer(std::int32_t amount) {
        const auto healed = session_->characterService.HealCharacter(*session_->playerId, amount);
        session_->combatText.Add(std::format("+{}", healed), { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.45f, 0.95f, 0.45f, 1.0f }, 3.0f);
        EmitHealingEffect();
    }

    std::vector<const Content::SpellDefinition*> GameEngine::GetSpellBar() const {
        std::vector<const Content::SpellDefinition*> bar;
        if (const auto* classData = PlayerClassDefinition()) {
            for (const auto& key : classData->spellBar) {
                if (const auto* spell = content_->FindSpell(key)) bar.push_back(spell);
            }
        }
        return bar;
    }

    std::int32_t GameEngine::GetPlayerSpellPower() const {
        const auto* player = PlayerCharacter();
        const auto* classData = PlayerClassDefinition();
        if (!player || !classData) return 0;
        const auto& attributes = player->GetAttributes();
        std::int32_t modifier = 0;
        switch (player->GetClass()) {
        case LB::Character::CharacterClass::Warrior: modifier = attributes.GetStrengthModifier(); break;   // a warrior's might
        case LB::Character::CharacterClass::Mage: modifier = attributes.GetIntelligenceModifier(); break;
        case LB::Character::CharacterClass::Cleric: modifier = attributes.GetWisdomModifier(); break;
        case LB::Character::CharacterClass::Rogue: modifier = attributes.GetIntelligenceModifier() / 2; break;
        default: break;
        }
        return std::max(0, classData->spellPowerBase + modifier * player->GetLevel() + EquippedBonuses().spellPower);
    }

    std::string GameEngine::SpellBlocker(std::size_t slot) const {
        const auto bar = GetSpellBar();
        if (slot >= bar.size()) return "empty";
        const auto* player = PlayerCharacter();
        if (!player || !IsPlayerAlive()) return "down";
        const auto& spell = *bar[slot];
        if (player->GetLevel() < spell.requiredLevel) return std::format("level {}", spell.requiredLevel);
        if (session_->spellCooldowns[slot] > 0.0f) return "cooldown";
        if (player->GetCurrentManaPoints() < spell.manaCost) return "mana";
        if (session_->playerStatuses.PreventsActions()) return "hindered";
        return {};
    }

    /// <summary>
    /// The spell bar each step: cooldowns run down and mana comes back (classes.json
    /// manaRegenPerSecond); a cast in progress completes after the spell's cast time; a slot's key
    /// starts one (not during an attack). Mana is spent as the spell leaves, so a cancelled cast
    /// costs nothing.
    /// </summary>
    void GameEngine::UpdatePlayerCasting(float elapsedSeconds) {
        if (!session_->playerId) return;
        for (auto& cooldown : session_->spellCooldowns) cooldown = std::max(0.0f, cooldown - elapsedSeconds);
        session_->evadeSeconds = std::max(0.0f, session_->evadeSeconds - elapsedSeconds);
        session_->sneakWindowSeconds = std::max(0.0f, session_->sneakWindowSeconds - elapsedSeconds);
        session_->casting.releaseSeconds = std::max(0.0f, session_->casting.releaseSeconds - elapsedSeconds);

        const auto* player = PlayerCharacter();
        const auto* classData = PlayerClassDefinition();
        if (player && classData && IsPlayerAlive()) {
            if (player->GetCurrentManaPoints() >= player->GetMaxManaPoints()) {
                session_->manaCarry = 0.0f;
            }
            else {
                session_->manaCarry += (classData->manaRegenPerSecond + EquippedBonuses().manaRegen) * elapsedSeconds;
                if (session_->manaCarry >= 1.0f) {
                    const auto whole = static_cast<std::int32_t>(session_->manaCarry);
                    session_->manaCarry -= static_cast<float>(whole);
                    session_->characterService.RestoreMana(*session_->playerId, whole);
                }
            }
        }

        auto& casting = session_->casting;
        if (!IsPlayerAlive() || session_->playerStatuses.PreventsActions()) {
            if (casting.active) CancelCast();
            return;
        }
        if (casting.active) {
            casting.seconds += elapsedSeconds;
            if (casting.seconds + 1e-4f >= casting.spell->castTimeSeconds) ReleaseCast();   // steps add up to just under whole times
            return;
        }
        if (session_->playerCombat.swinging) return;
        for (std::size_t slot = 0; slot < SpellSlots; ++slot) {
            if (inputSystem_.IsActionTriggered(std::string(SpellActions[slot]))) {
                session_->order = {};   // the player's own cast replaces an order
                StartCast(slot);
                break;
            }
        }
    }

    // A slot's spell begins: a missile turns toward the nearest enemy in its range and in sight;
    // a spell that cannot be cast says why over the player's head
    void GameEngine::StartCast(std::size_t slot, std::optional<std::uint32_t> aim) {
        const auto blocker = SpellBlocker(slot);
        if (!blocker.empty()) {
            const Systems::Rendering::Vector3 over{ session_->playerPosition.x, PlayerTop(), session_->playerPosition.y };
            if (blocker == "mana") session_->combatText.Add("NO MANA", over, { 0.35f, 0.55f, 1.0f, 1.0f }, 2.0f);
            else if (blocker.starts_with("level")) session_->combatText.Add(std::format("LEVEL {}", blocker.substr(6)), over, { 0.8f, 0.8f, 0.8f, 1.0f }, 2.0f);
            if (blocker != "cooldown") PlaySound(GameSound::NoMana);
            return;
        }

        const auto& spell = *GetSpellBar()[slot];
        std::optional<std::uint32_t> target;
        if (spell.IsMissile() || spell.NeedsTarget()) target = aim ? aim : session_->encounter.FindTarget(session_->playerPosition, spell.range, true);
        if (spell.NeedsTarget() && !target) {
            session_->combatText.Add("NO TARGET", { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.8f, 0.8f, 0.8f, 1.0f }, 2.0f);
            PlaySound(GameSound::NoMana);
            return;
        }
        auto& casting = session_->casting;
        casting = PlayerCasting{ true, slot, &spell, 0.0f, std::nullopt, 0.0f };
        casting.target = target;
        if (const auto* enemy = target ? session_->encounter.FindEnemy(*target) : nullptr) {
            session_->playerFacingYaw = std::atan2(enemy->position.x - session_->playerPosition.x, enemy->position.y - session_->playerPosition.y);
        }
        session_->playerCombat.hurtSeconds = 0.0f;
        session_->playerAnimator.Play(assets_.playerClips.castLoop.id, true, 1.0f, true, 0.1f);
        PlaySound(GameSound::SpellCast, session_->playerPosition);
        if (spell.castTimeSeconds <= 0.0f) ReleaseCast();
    }

    // The spell leaves: mana is spent and the cooldown starts; a missile flies at the target
    // where it stands now (straight ahead without one), a burst goes off around the player
    void GameEngine::ReleaseCast() {
        auto& casting = session_->casting;
        const auto& spell = *casting.spell;
        const auto slot = casting.slot;
        casting.active = false;
        if (spell.manaCost > 0 && !session_->characterService.ConsumeMana(*session_->playerId, spell.manaCost)) return;
        session_->spellCooldowns[slot] = spell.cooldownSeconds;

        const SpellPayload payload{ &spell, Encounter::SpellDamage(spell, GetPlayerSpellPower()) };
        const auto* target = casting.target ? session_->encounter.FindEnemy(*casting.target) : nullptr;
        if (target && !target->IsTargetable()) target = nullptr;
        const auto heal = static_cast<std::int32_t>(std::lround(static_cast<float>(spell.baseHeal) + spell.powerCoefficient * static_cast<float>(GetPlayerSpellPower())));
        const auto takeEvents = [this](const std::vector<CombatEvent>& events) {
            for (const auto& event : events) {
                session_->combatEvents.push_back(event);
                if (event.killed) {
                    if (const auto* enemy = session_->encounter.FindEnemy(event.enemyId)) OnEnemyKilled(*enemy);
                }
            }
        };

        if (spell.IsHeal()) {
            // design-spells-system.md 5.2: base + coefficient x the caster's power; a cure lifts hindrances
            HealPlayer(heal);
            session_->playerStatuses.RemoveHindrances(spell.cleansesStatuses);
        }
        else if (spell.IsDash()) {
            // A roll along the way the player faces (or is pushing the stick): untouchable meanwhile
            const auto forward = Forward(session_->playerFacingYaw);
            constexpr int Steps = 8;
            for (int i = 0; i < Steps; ++i) {
                session_->playerPosition = session_->world.Move(session_->playerPosition, { forward.x * spell.dashTiles / Steps, forward.y * spell.dashTiles / Steps }, PlayerRadius);
            }
            session_->characterService.PlaceCharacter(*session_->playerId, ToTile(session_->playerPosition));
            session_->evadeSeconds = spell.evadeSeconds;
            session_->effects.Emit(ParticleEffect::Footstep, { session_->playerPosition.x, 0.05f, session_->playerPosition.y }, 10);
            const auto& dodge = assets_.playerClips.dodge;
            casting.releaseSeconds = SpellReleaseSeconds;
            session_->playerAnimator.Play(dodge.id, false, dodge.seconds / SpellReleaseSeconds, true, 0.05f);
            return;
        }
        else if (spell.IsShadowstep()) {
            // Out of the shadows behind the target (beside it when a wall is behind it), facing it
            if (target) {
                const auto palette = ElementPalette(LB::Combat::DamageType::Shadow);
                session_->effects.Emit(ParticleEffect::SpellBurst, { session_->playerPosition.x, 0.5f, session_->playerPosition.y }, 16, 0.8f, &palette);
                const auto back = Forward(target->facingYaw);
                WorldPoint at{ target->position.x - back.x * 0.8f, target->position.y - back.y * 0.8f };
                if (!session_->world.CircleFits(at, PlayerRadius)) {
                    const float dx = session_->playerPosition.x - target->position.x;
                    const float dy = session_->playerPosition.y - target->position.y;
                    const float length = std::max(1e-4f, std::hypot(dx, dy));
                    at = { target->position.x + dx / length * 0.8f, target->position.y + dy / length * 0.8f };
                }
                if (session_->world.CircleFits(at, PlayerRadius)) {
                    session_->playerPosition = at;
                    session_->characterService.PlaceCharacter(*session_->playerId, ToTile(at));
                }
                session_->playerFacingYaw = std::atan2(target->position.x - session_->playerPosition.x, target->position.y - session_->playerPosition.y);
                session_->sneakWindowSeconds = SneakWindowSeconds;
                session_->effects.Emit(ParticleEffect::SpellBurst, { session_->playerPosition.x, 0.5f, session_->playerPosition.y }, 16, 0.8f, &palette);
            }
        }
        else if (spell.IsTargetedStrike()) {
            // An instant strike (and the caster healed, when the spell heals too)
            if (target) {
                if (auto event = session_->encounter.StrikeWithSpell(payload, target->id)) takeEvents({ *event });
                if (spell.baseHeal > 0) HealPlayer(heal);
            }
        }
        else if (spell.IsTargetedBurst()) {
            if (target) takeEvents(session_->encounter.CastBurst(payload, target->position));
        }
        else if (spell.IsMissile()) {
            auto direction = Forward(session_->playerFacingYaw);
            if (target) {
                direction = { target->position.x - session_->playerPosition.x, target->position.y - session_->playerPosition.y };
                session_->playerFacingYaw = std::atan2(direction.x, direction.y);
            }
            session_->encounter.CastMissile(payload, session_->playerPosition, direction, PlayerRadius);
        }
        else {
            for (const auto& event : session_->encounter.CastBurst(payload, session_->playerPosition)) {
                session_->combatEvents.push_back(event);
                if (event.killed) {
                    if (const auto* enemy = session_->encounter.FindEnemy(event.enemyId)) OnEnemyKilled(*enemy);
                }
            }
        }

        const auto& release = assets_.playerClips.castRelease;
        casting.releaseSeconds = SpellReleaseSeconds;
        session_->playerAnimator.Play(release.id, false, release.seconds / SpellReleaseSeconds, true, 0.05f);
    }

    void GameEngine::CancelCast() {
        session_->casting = PlayerCasting{};
    }

    // Burning and other damage over time on the player; a spell's statuses taking hold
    void GameEngine::UpdatePlayerStatuses(float elapsedSeconds) {
        if (!session_->playerId) return;
        if (!IsPlayerAlive()) {
            session_->playerStatuses.Clear();
            return;
        }
        for (const auto& tick : session_->playerStatuses.Update(elapsedSeconds)) {
            CombatEvent event{ CombatEventKind::EnemyAttack, 0, Services::Combat::CombatService::ResolveSpellHit(tick.damage, tick.damageType, GetPlayerDefense()), false };
            session_->characterService.DamageCharacter(*session_->playerId, event.result.finalDamage);
            session_->combatEvents.push_back(event);
            if (!IsPlayerAlive()) {
                session_->playerCombat.swinging = false;
                session_->playerCombat.deadSeconds = 0.0f;
                CancelCast();
                break;
            }
        }
    }

    void GameEngine::ApplySpellStatusesToPlayer(const Content::SpellDefinition& spell) {
        for (const auto& key : spell.appliedStatusIds) {
            if (const auto* status = content_->FindStatus(key)) session_->playerStatuses.Apply(*status);
        }
    }

    // A swing or a shot begins: pick the target, face it, and start the attack's animation
    void GameEngine::StartPlayerAttack(std::optional<std::uint32_t> aim) {
        auto& combat = session_->playerCombat;
        const auto* weapon = session_->playerWeapon;
        // Bows and crossbows need their ammunition (weapons.json ammoType)
        if (weapon && weapon->IsRanged() && !weapon->ammoType.empty() && AmmoCount(weapon->ammoType) == 0) {
            if (!session_->combatText.Texts().empty() && session_->combatText.Texts().back().text.starts_with("NO ")) return;   // said already
            const auto ammo = std::ranges::find(content_->GetConsumables(), weapon->ammoType, &Content::ConsumableDefinition::ammoType);
            std::string name = ammo != content_->GetConsumables().end() ? ammo->name : weapon->ammoType;
            std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            session_->combatText.Add(std::format("NO {}", name), { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.8f, 0.8f, 0.8f, 1.0f }, 2.0f);
            PlaySound(GameSound::NoMana);
            return;
        }
        combat.swinging = true;
        combat.swingSeconds = 0.0f;
        combat.impactDone = false;
        combat.hurtSeconds = 0.0f;
        combat.shooting = weapon && weapon->IsRanged();
        combat.target = aim ? aim
                      : combat.shooting ? session_->encounter.FindTarget(session_->playerPosition, weapon->range, true)
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
        else if (combat.shooting && weapon->category == LB::Combat::WeaponType::Wand) {
            clip = clips.castRelease;
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
            const auto attack = PlayerAttackAgainst(session_->encounter.FindEnemy(*combat.target));
            if (auto event = session_->encounter.StrikeEnemy(*combat.target, attack, session_->playerPosition, PlayerReach)) {
                if (event->result.hit) session_->sneakWindowSeconds = 0.0f;   // a shadowstep's sneak attack is spent on a hit
                session_->combatEvents.push_back(*event);
                if (event->killed) {
                    OnEnemyKilled(*session_->encounter.FindEnemy(event->enemyId));
                }
            }
            return;
        }

        const auto& weapon = *session_->playerWeapon;
        if (!weapon.ammoType.empty() && !SpendAmmo(weapon.ammoType)) return;   // out of arrows meanwhile
        auto direction = Forward(session_->playerFacingYaw);
        if (const auto* target = combat.target ? session_->encounter.FindEnemy(*combat.target) : nullptr; target && target->IsTargetable()) {
            direction = { target->position.x - session_->playerPosition.x, target->position.y - session_->playerPosition.y };
            session_->playerFacingYaw = std::atan2(direction.x, direction.y);
        }
        // An arrow, a bolt, or a wand's orb of its damage type (no model)
        const auto model = weapon.category == LB::Combat::WeaponType::Crossbow ? BoltModel
                         : weapon.category == LB::Combat::WeaponType::Bow ? ArrowModel : std::string_view{};
        session_->encounter.Shoot(PlayerAttackAgainst(combat.target ? session_->encounter.FindEnemy(*combat.target) : nullptr),
                                  session_->playerPosition, direction, weapon.projectileSpeed, weapon.range, std::string(model), PlayerRadius);
        session_->sneakWindowSeconds = 0.0f;
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
        else if (combat.swinging || session_->casting.active || session_->casting.releaseSeconds > 0.0f) {
            // the attack's or the spell's clip
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
            if (event.result.hit && session_->evadeSeconds > 0.0f && IsPlayerAlive()) {
                session_->combatText.Add("DODGE", { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.85f, 0.85f, 0.85f, 1.0f }, 2.0f);
                session_->combatEvents.back().result = {};   // shown as a miss
                continue;
            }
            if (event.spell && event.result.hit && IsPlayerAlive()) ApplySpellStatusesToPlayer(*event.spell);
            if (!event.result.hit || event.result.finalDamage <= 0 || !IsPlayerAlive()) continue;

            session_->characterService.DamageCharacter(*session_->playerId, event.result.finalDamage);
            session_->playerCombat.hitFlashSeconds = Encounter::HitFlashSeconds;
            if (!IsPlayerAlive()) {
                session_->playerCombat.swinging = false;
                session_->playerCombat.deadSeconds = 0.0f;
                CancelCast();
            }
            else if (!session_->playerCombat.swinging && !session_->casting.active) {
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
            if (event.spell && event.spell->baseDamage <= 0 && event.result.finalDamage <= 0) continue;   // a hindrance, not a blow
            Systems::Rendering::Vector3 at{ session_->playerPosition.x, PlayerTop(), session_->playerPosition.y };
            if (event.kind == CombatEventKind::PlayerAttack) {
                const auto* enemy = session_->encounter.FindEnemy(event.enemyId);
                if (!enemy) continue;
                at = { enemy->position.x, EnemyTop(*enemy), enemy->position.y };
            }
            session_->combatText.AddAttack(event, at);
            if (event.kind == CombatEventKind::PlayerAttack && event.result.hit && event.result.extraDamage > 0) {
                session_->combatText.Add("SNEAK ATTACK", { at.x, at.y + 0.35f, at.z }, { 1.0f, 0.82f, 0.30f, 1.0f }, 1.6f);
            }
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
        session_->casting = {};
        session_->evadeSeconds = 0.0f;
        session_->sneakWindowSeconds = 0.0f;
        session_->order = {};
        session_->picked.reset();
        session_->playerStatuses.Clear();
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
        if (!IsPlayerAlive() || session_->playerStatuses.PreventsActions()) return;
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

        // The player's own movement replaces an order; without it, an order may walk them
        WorldPoint direction{};
        if (dx != 0.0f || dy != 0.0f) {
            session_->order = {};
            // dy is screen-down; the camera maps (right, up) onto the ground
            const auto ground = session_->camera.ScreenToGround(dx, -dy);
            direction = { ground.x, ground.y };
        }
        else if (const auto step = OrderStep()) {
            direction = *step;
        }
        else {
            if (session_->order.kind == OrderKind::Move) session_->order = {};   // arrived
            return;
        }
        const float hindrance = session_->playerStatuses.MovementMultiplier();
        if (hindrance <= 0.0f) return;                      // rooted
        if (session_->casting.active) CancelCast();         // moving breaks a cast (design-spells-system.md 4.3)
        session_->playerFacingYaw = std::atan2(direction.x, direction.y);

        const float speed = session_->characterService.GetMovementSpeed(*session_->playerId).value_or(0.0f) * hindrance;
        session_->playerPosition = session_->world.Move(session_->playerPosition, { direction.x * speed * elapsedSeconds, direction.y * speed * elapsedSeconds }, PlayerRadius);

        // Keep the domain's tile position in sync (at most one tile per step at these speeds)
        const auto tile = ToTile(session_->playerPosition);
        if (!session_->characterService.MoveCharacter(*session_->playerId, tile)) {
            session_->characterService.PlaceCharacter(*session_->playerId, tile);
        }
    }

} // namespace LB::Engine
