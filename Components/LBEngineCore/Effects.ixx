// Effects.ixx - The particle effects system: a pool of particles, the recipe of each effect
// (spread, speeds, motion, colors), and drawing them. Effects are only a look: nothing in the
// game reads them. The game decides when and where (Effects.cpp).
export module LB.Engine:Effects;

import std;
import LB.Combat;
import LB.RenderingSystem;
import :Particles;

namespace LB::Engine::EffectDetail {
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
            float sizeJitter{0.0f};          // start size varies by up to this fraction
            Rgb start{}, middle{}, end{};
            float brightness{1.0f};
            bool glowing{true};
        };

        inline constexpr Rgb Gold{ 1.0f, 0.85f, 0.45f };
        inline constexpr Rgb DeepGold{ 1.0f, 0.62f, 0.18f };
        inline constexpr Rgb Amber{ 0.7f, 0.35f, 0.06f };

        inline Spray SprayFor(ParticleEffect effect) {
            switch (effect) {
            case ParticleEffect::TorchFlame:
                return { .radius = 0.05f, .outwardMin = 0.0f, .outwardMax = 0.1f, .upwardMin = 0.55f, .upwardMax = 0.9f,
                         .buoyancy = 0.6f, .drag = 1.5f, .lifeMin = 0.35f, .lifeMax = 0.6f, .startSize = 0.15f, .endSize = 0.04f,
                         .sizeJitter = 0.2f, .start = { 1.0f, 0.85f, 0.45f }, .middle = { 1.0f, 0.42f, 0.08f }, .end = { 0.5f, 0.08f, 0.02f },
                         .brightness = 1.15f };
            case ParticleEffect::TorchEmber:
                return { .radius = 0.06f, .outwardMin = 0.0f, .outwardMax = 0.3f, .upwardMin = 0.6f, .upwardMax = 1.1f,
                         .buoyancy = 0.2f, .drag = 0.6f, .lifeMin = 1.0f, .lifeMax = 1.8f, .startSize = 0.04f, .endSize = 0.02f,
                         .start = { 1.0f, 0.7f, 0.3f }, .middle = { 1.0f, 0.4f, 0.1f }, .end = { 0.7f, 0.15f, 0.03f }, .brightness = 2.0f };
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
            case ParticleEffect::SpellTrail:
                return { .radius = 0.05f, .height = 0.05f, .outwardMin = 0.0f, .outwardMax = 0.15f, .upwardMin = -0.1f, .upwardMax = 0.2f,
                         .buoyancy = 0.2f, .drag = 2.0f, .lifeMin = 0.25f, .lifeMax = 0.45f, .startSize = 0.14f, .endSize = 0.02f,
                         .sizeJitter = 0.3f, .start = { 1.0f, 1.0f, 1.0f }, .middle = { 0.8f, 0.8f, 0.8f }, .end = { 0.4f, 0.4f, 0.4f },
                         .brightness = 1.8f };
            case ParticleEffect::SpellBurst:
                return { .radius = 0.15f, .height = 0.2f, .outwardMin = 1.2f, .outwardMax = 3.2f, .upwardMin = 0.2f, .upwardMax = 1.4f,
                         .buoyancy = -1.5f, .drag = 2.2f, .lifeMin = 0.3f, .lifeMax = 0.6f, .startSize = 0.16f, .endSize = 0.03f,
                         .sizeJitter = 0.3f, .start = { 1.0f, 1.0f, 1.0f }, .middle = { 0.8f, 0.8f, 0.8f }, .end = { 0.4f, 0.4f, 0.4f },
                         .brightness = 2.2f };
            case ParticleEffect::SpellCharge:
                return { .radius = 0.25f, .height = 0.3f, .outwardMin = -0.6f, .outwardMax = -0.3f, .upwardMin = 0.1f, .upwardMax = 0.4f,
                         .buoyancy = 0.0f, .drag = 1.0f, .lifeMin = 0.25f, .lifeMax = 0.4f, .startSize = 0.07f, .endSize = 0.02f,
                         .start = { 1.0f, 1.0f, 1.0f }, .middle = { 0.8f, 0.8f, 0.8f }, .end = { 0.4f, 0.4f, 0.4f }, .brightness = 1.8f };
            case ParticleEffect::CryptDust:
                return { .outwardMin = 0.02f, .outwardMax = 0.12f, .upwardMin = -0.04f, .upwardMax = 0.04f, .buoyancy = -0.01f, .drag = 0.05f,
                         .lifeMin = 4.0f, .lifeMax = 7.0f, .startSize = 0.035f, .endSize = 0.03f, .sizeJitter = 0.4f,
                         .start = { 0.80f, 0.76f, 0.68f }, .middle = { 0.72f, 0.68f, 0.60f }, .end = { 0.60f, 0.57f, 0.50f },
                         .brightness = 0.4f, .glowing = false };
            case ParticleEffect::CaveMist:
                return { .outwardMin = 0.04f, .outwardMax = 0.14f, .upwardMin = 0.0f, .upwardMax = 0.02f, .drag = 0.05f,
                         .lifeMin = 5.0f, .lifeMax = 8.0f, .startSize = 0.9f, .endSize = 1.7f, .sizeJitter = 0.3f,
                         .start = { 0.52f, 0.60f, 0.68f }, .middle = { 0.48f, 0.56f, 0.64f }, .end = { 0.42f, 0.50f, 0.58f },
                         .brightness = 0.09f, .glowing = false };
            case ParticleEffect::RuinEmbers:
                return { .outwardMin = 0.05f, .outwardMax = 0.2f, .upwardMin = 0.15f, .upwardMax = 0.45f, .buoyancy = 0.04f, .drag = 0.2f,
                         .lifeMin = 3.0f, .lifeMax = 5.0f, .startSize = 0.045f, .endSize = 0.0f, .sizeJitter = 0.3f,
                         .start = { 1.0f, 0.62f, 0.22f }, .middle = { 0.95f, 0.35f, 0.08f }, .end = { 0.5f, 0.1f, 0.02f },
                         .brightness = 1.1f };
            case ParticleEffect::ArchiveMotes:
                return { .outwardMin = 0.02f, .outwardMax = 0.08f, .upwardMin = 0.02f, .upwardMax = 0.12f, .buoyancy = 0.01f, .drag = 0.1f,
                         .lifeMin = 4.0f, .lifeMax = 7.0f, .startSize = 0.05f, .endSize = 0.0f, .sizeJitter = 0.3f,
                         .start = { 0.65f, 0.75f, 1.0f }, .middle = { 0.55f, 0.55f, 1.0f }, .end = { 0.35f, 0.25f, 0.7f }, .brightness = 0.9f };
            case ParticleEffect::TrapdoorMist:
                return { .radius = 0.6f, .outwardMin = 0.0f, .outwardMax = 0.1f, .upwardMin = 0.2f, .upwardMax = 0.5f,
                         .buoyancy = 0.1f, .drag = 0.5f, .lifeMin = 1.2f, .lifeMax = 2.0f, .startSize = 0.2f, .endSize = 0.45f,
                         .start = { 0.4f, 0.7f, 1.0f }, .middle = { 0.25f, 0.5f, 1.0f }, .end = { 0.1f, 0.2f, 0.6f }, .brightness = 0.45f };
            default:
                return {};
            }
        }
}

export namespace LB::Engine {

    /// <summary>
    /// Colors an effect takes instead of its recipe's (start, middle, end; linear RGB): a spell's
    /// element
    /// </summary>
    struct EffectPalette {
        std::array<float, 3> start{};
        std::array<float, 3> middle{};
        std::array<float, 3> end{};
    };

    /// <summary>
    /// The colors of a damage type's magic (missiles, bursts, their light)
    /// </summary>
    inline EffectPalette ElementPalette(LB::Combat::DamageType type) noexcept {
        using LB::Combat::DamageType;
        switch (type) {
        case DamageType::Arcane: return { { 0.95f, 0.75f, 1.0f }, { 0.65f, 0.30f, 1.0f }, { 0.25f, 0.05f, 0.50f } };
        case DamageType::Cold: return { { 0.85f, 0.97f, 1.0f }, { 0.40f, 0.75f, 1.0f }, { 0.10f, 0.30f, 0.70f } };
        case DamageType::Fire: return { { 1.0f, 0.90f, 0.50f }, { 1.0f, 0.45f, 0.10f }, { 0.50f, 0.10f, 0.02f } };
        case DamageType::Lightning: return { { 1.0f, 1.0f, 0.85f }, { 0.80f, 0.85f, 1.0f }, { 0.30f, 0.35f, 0.80f } };
        case DamageType::Shadow: return { { 0.70f, 1.0f, 0.60f }, { 0.30f, 0.70f, 0.20f }, { 0.10f, 0.25f, 0.05f } };
        default: return { { 1.0f, 1.0f, 0.95f }, { 0.85f, 0.85f, 0.80f }, { 0.40f, 0.40f, 0.40f } };
        }
    }

    class EffectsSystem {
    public:
        EffectsSystem() = default;
        explicit EffectsSystem(std::uint32_t seed) : rng_(seed) {}

        /// <summary>
        /// `count` particles of an effect around a point, `strength` scaling their speed and size
        /// </summary>
        void Emit(ParticleEffect effect, Systems::Rendering::Vector3 at, std::int32_t count, float strength = 1.0f,
                  const EffectPalette* palette = nullptr) {
            auto spray = EffectDetail::SprayFor(effect);
            if (palette) {
                spray.start = palette->start;
                spray.middle = palette->middle;
                spray.end = palette->end;
            }
            constexpr float tau = 2.0f * std::numbers::pi_v<float>;
            for (std::int32_t i = 0; i < count; ++i) {
                const float angle = Random(0.0f, tau);
                const float spread = spray.radius * std::sqrt(Random(0.0f, 1.0f));
                const float outward = Random(spray.outwardMin, spray.outwardMax) * strength;

                Particle p{};
                p.effect = effect;
                p.position = { at.x + std::sin(angle) * spread, at.y + Random(0.0f, spray.height), at.z + std::cos(angle) * spread };
                p.velocity = { std::sin(angle) * outward, Random(spray.upwardMin, spray.upwardMax) * strength, std::cos(angle) * outward };
                p.buoyancy = spray.buoyancy;
                p.drag = spray.drag;
                p.lifetimeSeconds = Random(spray.lifeMin, spray.lifeMax);
                p.startSize = spray.startSize * strength * (1.0f + Random(-spray.sizeJitter, spray.sizeJitter));
                p.endSize = spray.endSize * strength;
                p.startColor = spray.start;
                p.middleColor = spray.middle;
                p.endColor = spray.end;
                p.brightness = spray.brightness;
                p.glowing = spray.glowing;
                particles_.Spawn(p);
            }
        }

        /// <summary>
        /// A steady stream: `perSecond` on average, whole particles each step
        /// </summary>
        void Stream(ParticleEffect effect, Systems::Rendering::Vector3 at, float perSecond, float elapsedSeconds,
                    const EffectPalette* palette = nullptr) {
            const auto count = static_cast<std::int32_t>(std::floor(perSecond * elapsedSeconds + Random(0.0f, 1.0f)));
            if (count > 0) Emit(effect, at, count, 1.0f, palette);
        }

        /// <summary>
        /// Spread over a square (`halfExtent` tiles around `center`, between two heights): a
        /// steady `perSecond` on average, each particle somewhere new (a tier's air)
        /// </summary>
        void Scatter(ParticleEffect effect, Systems::Rendering::Vector3 center, float halfExtent, float lowest, float highest,
                     float perSecond, float elapsedSeconds) {
            const auto count = static_cast<std::int32_t>(std::floor(perSecond * elapsedSeconds + Random(0.0f, 1.0f)));
            for (std::int32_t i = 0; i < count; ++i) {
                Emit(effect, { center.x + Random(-halfExtent, halfExtent), Random(lowest, highest), center.z + Random(-halfExtent, halfExtent) }, 1);
            }
        }

        void Update(float elapsedSeconds) { particles_.Update(elapsedSeconds); }

        /// <summary>
        /// Every particle as a sprite. The tint dims glowing ones with the scene (their light);
        /// dust gets its color dimmed but keeps its opacity.
        /// </summary>
        void Draw(Systems::Rendering::RenderingSystem& renderer, Systems::Rendering::Color tint) const {
            for (const auto& particle : particles_.GetParticles()) {
                auto color = particle.CurrentColor();
                color = { color.r * tint.r, color.g * tint.g, color.b * tint.b, color.a };
                renderer.DrawParticle(Systems::Rendering::ParticleRenderData{ particle.position, particle.Size(), color, particle.glowing });
            }
        }

        void Clear() noexcept { particles_.Clear(); }

        [[nodiscard]] const ParticleField& Particles() const noexcept { return particles_; }

    private:
        float Random(float lo, float hi) {
            return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_);
        }

        ParticleField particles_;
        std::mt19937 rng_{ 12345u };   // effects look the same run to run
    };

} // namespace LB::Engine
