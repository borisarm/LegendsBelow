// Effects.cpp - LB.Engine implementation unit: when and where GameEngine plays effects (the
// EffectsSystem holds the particles and the recipes). Fire, sparkles and mist keep moving during
// the pause; dust from movement only appears while the game runs.

module LB.Engine;

import std;
import LB.RenderingSystem;

namespace LB::Engine {

    // Sparks where this step's blows landed, and bone dust where skeletons fell apart
    void GameEngine::EmitCombatEffects() {
        for (const auto& event : session_->combatEvents) {
            if (!event.result.hit || event.result.finalDamage <= 0) continue;
            const float strength = event.result.critical ? 1.4f : 1.0f;
            const std::int32_t sparks = event.result.critical ? 28 : 14;

            if (event.kind == CombatEventKind::PlayerAttack) {
                const auto* enemy = session_->encounter.FindEnemy(event.enemyId);
                if (!enemy) continue;
                const float top = EnemyTop(*enemy);
                session_->effects.Emit(ParticleEffect::HitSparks, { enemy->position.x, top * 0.55f, enemy->position.y }, sparks, strength);
                if (event.killed) {
                    session_->effects.Emit(ParticleEffect::BoneDust, { enemy->position.x, 0.05f, enemy->position.y }, 24);
                }
            }
            else {
                session_->effects.Emit(ParticleEffect::HitSparks, { session_->playerPosition.x, PlayerTop() * 0.55f, session_->playerPosition.y }, sparks * 2 / 3, strength);
            }
        }
    }

    void GameEngine::EmitHealingEffect() {
        session_->effects.Emit(ParticleEffect::Healing, { session_->playerPosition.x, 0.1f, session_->playerPosition.y }, 34);
    }

    void GameEngine::EmitLevelUpEffect() {
        const Systems::Rendering::Vector3 feet{ session_->playerPosition.x, 0.1f, session_->playerPosition.y };
        session_->effects.Emit(ParticleEffect::LevelUp, feet, 60);
        session_->effects.Emit(ParticleEffect::Healing, feet, 20, 1.3f);   // a rising green-gold column as well
    }

    void GameEngine::EmitChestEffect(WorldPoint chest) {
        session_->effects.Emit(ParticleEffect::ChestBurst, { chest.x, 0.5f, chest.y }, 44);
    }

    // Each step: move the particles, then feed the streams. Fire, loot sparkles and the mist of
    // an open trapdoor keep going while paused; dust from rising skeletons and the player's feet
    // only while the game runs.
    void GameEngine::UpdateEffects(float elapsedSeconds, bool running) {
        session_->effects.Update(elapsedSeconds);
        session_->torches.UpdateFire(elapsedSeconds, session_->playerPosition, session_->walls, session_->effects);

        for (const auto& item : session_->groundItems) {
            session_->effects.Stream(ParticleEffect::LootSparkle, { item.position.x, 0.08f, item.position.y }, item.IsGold() ? 3.0f : 2.0f, elapsedSeconds);
        }

        if (session_->dungeonMode && session_->world.Layout().exitRoom >= 0 && static_cast<std::size_t>(session_->world.Layout().exitRoom) < session_->rooms.size()
            && session_->rooms[static_cast<std::size_t>(session_->world.Layout().exitRoom)].cleared) {
            const auto exit = session_->world.RoomCenter(session_->world.Layout().exitRoom);
            session_->effects.Stream(ParticleEffect::TrapdoorMist, { exit.x, 0.05f, exit.y }, 10.0f, elapsedSeconds);
        }

        if (!running) {
            session_->effectsPlayerPosition = session_->playerPosition;
            return;
        }

        for (const auto& enemy : session_->encounter.GetEnemies()) {
            if (enemy.state == EnemyState::Spawning) {
                session_->effects.Stream(ParticleEffect::RiseDust, { enemy.position.x, 0.03f, enemy.position.y }, 28.0f, elapsedSeconds);
            }
        }

        // Walking (not drifting a hair): puffs at the feet
        const float moved = Encounter::Distance(session_->playerPosition, session_->effectsPlayerPosition);
        if (elapsedSeconds > 0.0f && moved / elapsedSeconds > 0.5f && IsPlayerAlive()) {
            session_->effects.Stream(ParticleEffect::Footstep, { session_->playerPosition.x, 0.03f, session_->playerPosition.y }, 9.0f, elapsedSeconds);
        }
        session_->effectsPlayerPosition = session_->playerPosition;
    }

} // namespace LB::Engine
