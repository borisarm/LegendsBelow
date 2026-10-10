// Effects.cpp - GameEngine particle effects: what each gameplay moment emits, and drawing them
//
// Implementation unit of LB.Engine. Effects are only a look: nothing in the game reads them.
// Fire, sparkles and mist keep moving during the pause; dust from movement only appears while
// the game runs.

module LB.Engine;

import std;
import LB.RenderingSystem;

namespace LB::Engine {

    namespace {
        using Rgb = std::array<float, 3>;

        // How an effect's particles start and behave: spread around the emit point (horizontally
        // and upward), speeds outward and upward, then the Particle's own motion and looks
        struct Spray {
            float radius{0.0f};
            float height{0.0f};
            float outwardMin{0.0f}, outwardMax{0.0f};
            float upwardMin{0.0f}, upwardMax{0.0f};
            float buoyancy{0.0f};
            float drag{0.0f};
            float lifeMin{0.5f}, lifeMax{1.0f};
            float startSize{0.1f}, endSize{0.05f};
            Rgb start{}, middle{}, end{};
            float brightness{1.0f};
            bool glowing{true};
        };

        constexpr Rgb Gold{ 1.0f, 0.85f, 0.45f };
        constexpr Rgb DeepGold{ 1.0f, 0.62f, 0.18f };
        constexpr Rgb Amber{ 0.7f, 0.35f, 0.06f };

        Spray SprayFor(ParticleEffect effect) {
            switch (effect) {
            case ParticleEffect::HitSparks:
                return { .radius = 0.05f, .outwardMin = 1.0f, .outwardMax = 2.6f, .upwardMin = 0.4f, .upwardMax = 2.0f,
                         .buoyancy = -7.0f, .drag = 2.0f, .lifeMin = 0.18f, .lifeMax = 0.38f, .startSize = 0.07f, .endSize = 0.02f,
                         .start = { 1.0f, 0.95f, 0.8f }, .middle = { 1.0f, 0.62f, 0.2f }, .end = { 0.6f, 0.15f, 0.03f }, .brightness = 2.4f };
            case ParticleEffect::BoneDust:
                return { .radius = 0.3f, .height = 0.6f, .outwardMin = 0.15f, .outwardMax = 0.7f, .upwardMin = 0.1f, .upwardMax = 0.6f,
                         .buoyancy = 0.25f, .drag = 2.5f, .lifeMin = 0.8f, .lifeMax = 1.4f, .startSize = 0.25f, .endSize = 0.6f,
                         .start = { 0.62f, 0.6f, 0.55f }, .middle = { 0.5f, 0.48f, 0.44f }, .end = { 0.38f, 0.36f, 0.33f },
                         .brightness = 0.55f, .glowing = false };
            case ParticleEffect::RiseDust:
                return { .radius = 0.4f, .outwardMin = 0.1f, .outwardMax = 0.5f, .upwardMin = 0.25f, .upwardMax = 0.75f,
                         .buoyancy = 0.1f, .drag = 2.0f, .lifeMin = 0.6f, .lifeMax = 1.0f, .startSize = 0.18f, .endSize = 0.42f,
                         .start = { 0.45f, 0.4f, 0.34f }, .middle = { 0.4f, 0.36f, 0.31f }, .end = { 0.32f, 0.29f, 0.25f },
                         .brightness = 0.5f, .glowing = false };
            case ParticleEffect::Footstep:
                return { .radius = 0.12f, .outwardMin = 0.05f, .outwardMax = 0.25f, .upwardMin = 0.05f, .upwardMax = 0.25f,
                         .buoyancy = 0.0f, .drag = 2.5f, .lifeMin = 0.4f, .lifeMax = 0.7f, .startSize = 0.1f, .endSize = 0.26f,
                         .start = { 0.45f, 0.41f, 0.36f }, .middle = { 0.4f, 0.37f, 0.32f }, .end = { 0.34f, 0.31f, 0.27f },
                         .brightness = 0.42f, .glowing = false };
            case ParticleEffect::LootSparkle:
                return { .radius = 0.22f, .height = 0.15f, .outwardMin = 0.0f, .outwardMax = 0.04f, .upwardMin = 0.15f, .upwardMax = 0.35f,
                         .lifeMin = 0.6f, .lifeMax = 1.0f, .startSize = 0.07f, .endSize = 0.0f,
                         .start = Gold, .middle = DeepGold, .end = Amber, .brightness = 1.6f };
            case ParticleEffect::ChestBurst:
                return { .radius = 0.2f, .outwardMin = 0.3f, .outwardMax = 1.2f, .upwardMin = 1.2f, .upwardMax = 2.6f,
                         .buoyancy = -2.5f, .drag = 1.2f, .lifeMin = 0.8f, .lifeMax = 1.4f, .startSize = 0.09f, .endSize = 0.02f,
                         .start = Gold, .middle = DeepGold, .end = Amber, .brightness = 2.0f };
            case ParticleEffect::Healing:
                return { .radius = 0.45f, .height = 0.4f, .outwardMin = 0.0f, .outwardMax = 0.1f, .upwardMin = 0.6f, .upwardMax = 1.4f,
                         .buoyancy = 0.5f, .drag = 1.0f, .lifeMin = 0.7f, .lifeMax = 1.1f, .startSize = 0.09f, .endSize = 0.02f,
                         .start = { 0.75f, 1.0f, 0.75f }, .middle = { 0.3f, 0.95f, 0.4f }, .end = { 0.1f, 0.5f, 0.15f }, .brightness = 1.6f };
            case ParticleEffect::LevelUp:
                return { .radius = 0.3f, .height = 0.2f, .outwardMin = 1.2f, .outwardMax = 2.4f, .upwardMin = 1.5f, .upwardMax = 3.2f,
                         .buoyancy = -1.0f, .drag = 1.8f, .lifeMin = 1.0f, .lifeMax = 1.6f, .startSize = 0.11f, .endSize = 0.03f,
                         .start = { 1.0f, 0.95f, 0.7f }, .middle = Gold, .end = DeepGold, .brightness = 2.2f };
            case ParticleEffect::TrapdoorMist:
                return { .radius = 0.6f, .outwardMin = 0.0f, .outwardMax = 0.1f, .upwardMin = 0.2f, .upwardMax = 0.5f,
                         .buoyancy = 0.1f, .drag = 0.5f, .lifeMin = 1.2f, .lifeMax = 2.0f, .startSize = 0.2f, .endSize = 0.45f,
                         .start = { 0.4f, 0.7f, 1.0f }, .middle = { 0.25f, 0.5f, 1.0f }, .end = { 0.1f, 0.2f, 0.6f }, .brightness = 0.45f };
            default:
                return {};
            }
        }
    }

    float GameEngine::EffectRandom(float lo, float hi) {
        return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(effectsRng_);
    }

    // `count` particles of an effect around a point, `strength` scaling their speed and size
    void GameEngine::EmitEffect(ParticleEffect effect, Systems::Rendering::Vector3 at, std::int32_t count, float strength) {
        const auto spray = SprayFor(effect);
        constexpr float tau = 2.0f * std::numbers::pi_v<float>;
        for (std::int32_t i = 0; i < count; ++i) {
            const float angle = EffectRandom(0.0f, tau);
            const float spread = spray.radius * std::sqrt(EffectRandom(0.0f, 1.0f));
            const float outward = EffectRandom(spray.outwardMin, spray.outwardMax) * strength;

            Particle p{};
            p.effect = effect;
            p.position = { at.x + std::sin(angle) * spread, at.y + EffectRandom(0.0f, spray.height), at.z + std::cos(angle) * spread };
            p.velocity = { std::sin(angle) * outward, EffectRandom(spray.upwardMin, spray.upwardMax) * strength, std::cos(angle) * outward };
            p.buoyancy = spray.buoyancy;
            p.drag = spray.drag;
            p.lifetimeSeconds = EffectRandom(spray.lifeMin, spray.lifeMax);
            p.startSize = spray.startSize * strength;
            p.endSize = spray.endSize * strength;
            p.startColor = spray.start;
            p.middleColor = spray.middle;
            p.endColor = spray.end;
            p.brightness = spray.brightness;
            p.glowing = spray.glowing;
            particles_.Spawn(p);
        }
    }

    // A steady stream: `perSecond` on average, whole particles each step
    void GameEngine::EmitStream(ParticleEffect effect, Systems::Rendering::Vector3 at, float perSecond, float elapsedSeconds) {
        const auto count = static_cast<std::int32_t>(std::floor(perSecond * elapsedSeconds + EffectRandom(0.0f, 1.0f)));
        if (count > 0) EmitEffect(effect, at, count);
    }

    // Sparks where this step's blows landed, and bone dust where skeletons fell apart
    void GameEngine::EmitCombatEffects() {
        for (const auto& event : combatEvents_) {
            if (!event.result.hit || event.result.finalDamage <= 0) continue;
            const float strength = event.result.critical ? 1.4f : 1.0f;
            const std::int32_t sparks = event.result.critical ? 28 : 14;

            if (event.kind == CombatEventKind::PlayerAttack) {
                const auto* enemy = encounter_.FindEnemy(event.enemyId);
                if (!enemy) continue;
                const float top = EnemyTop(*enemy);
                EmitEffect(ParticleEffect::HitSparks, { enemy->position.x, top * 0.55f, enemy->position.y }, sparks, strength);
                if (event.killed) {
                    EmitEffect(ParticleEffect::BoneDust, { enemy->position.x, 0.05f, enemy->position.y }, 24);
                }
            }
            else {
                EmitEffect(ParticleEffect::HitSparks, { playerPosition_.x, PlayerTop() * 0.55f, playerPosition_.y }, sparks * 2 / 3, strength);
            }
        }
    }

    void GameEngine::EmitHealingEffect() {
        EmitEffect(ParticleEffect::Healing, { playerPosition_.x, 0.1f, playerPosition_.y }, 34);
    }

    void GameEngine::EmitLevelUpEffect() {
        const Systems::Rendering::Vector3 feet{ playerPosition_.x, 0.1f, playerPosition_.y };
        EmitEffect(ParticleEffect::LevelUp, feet, 60);
        EmitEffect(ParticleEffect::Healing, feet, 20, 1.3f);   // a rising green-gold column as well
    }

    void GameEngine::EmitChestEffect(WorldPoint chest) {
        EmitEffect(ParticleEffect::ChestBurst, { chest.x, 0.5f, chest.y }, 44);
    }

    // Each step: move the particles, then feed the streams. Fire, loot sparkles and the mist of
    // an open trapdoor keep going while paused; dust from rising skeletons and the player's feet
    // only while the game runs.
    void GameEngine::UpdateEffects(float elapsedSeconds, bool running) {
        particles_.Update(elapsedSeconds);
        UpdateTorchFire(elapsedSeconds);

        for (const auto& item : groundItems_) {
            EmitStream(ParticleEffect::LootSparkle, { item.position.x, 0.08f, item.position.y }, item.IsGold() ? 3.0f : 2.0f, elapsedSeconds);
        }

        if (dungeonMode_ && world_.Layout().exitRoom >= 0 && static_cast<std::size_t>(world_.Layout().exitRoom) < rooms_.size()
            && rooms_[static_cast<std::size_t>(world_.Layout().exitRoom)].cleared) {
            const auto exit = world_.RoomCenter(world_.Layout().exitRoom);
            EmitStream(ParticleEffect::TrapdoorMist, { exit.x, 0.05f, exit.y }, 10.0f, elapsedSeconds);
        }

        if (!running) {
            effectsPlayerPosition_ = playerPosition_;
            return;
        }

        for (const auto& enemy : encounter_.GetEnemies()) {
            if (enemy.state == EnemyState::Spawning) {
                EmitStream(ParticleEffect::RiseDust, { enemy.position.x, 0.03f, enemy.position.y }, 28.0f, elapsedSeconds);
            }
        }

        // Walking (not drifting a hair): puffs at the feet
        const float moved = Encounter::Distance(playerPosition_, effectsPlayerPosition_);
        if (elapsedSeconds > 0.0f && moved / elapsedSeconds > 0.5f && IsPlayerAlive()) {
            EmitStream(ParticleEffect::Footstep, { playerPosition_.x, 0.03f, playerPosition_.y }, 9.0f, elapsedSeconds);
        }
        effectsPlayerPosition_ = playerPosition_;
    }

    // Every particle as a sprite: glowing ones dimmed with the scene during the pause (their
    // light), dust with its color dimmed but its opacity kept
    void GameEngine::DrawParticles(const Systems::Rendering::Color& tint) {
        for (const auto& particle : particles_.GetParticles()) {
            auto color = particle.CurrentColor();
            color = { color.r * tint.r, color.g * tint.g, color.b * tint.b, color.a };
            renderingSystem_.DrawParticle(Systems::Rendering::ParticleRenderData{ particle.position, particle.Size(), color, particle.glowing });
        }
    }

} // namespace LB::Engine
