// Particles.ixx - Short-lived particles (torch fire, sparks, dust, sparkles): simulation only
export module LB.Engine:Particles;

import std;
import LB.RenderingSystem;

export namespace LB::Engine {

    /// <summary>
    /// What a particle belongs to (each effect has its own look; see Effects.cpp)
    /// </summary>
    enum class ParticleEffect : std::uint8_t {
        None,
        TorchFlame,     // torch fire: flames and embers
        TorchEmber,
        HitSparks,      // a weapon connecting (more for criticals)
        BoneDust,       // a skeleton falling apart
        RiseDust,       // earth stirred by a skeleton rising
        Footstep,       // dust kicked up by the player walking
        LootSparkle,    // gold and items waiting on the floor
        ChestBurst,     // a chest opening
        Healing,        // a potion drunk
        LevelUp,        // a new level
        TrapdoorMist,   // the way down, open
    };

    /// <summary>
    /// One particle: it moves, rises with its buoyancy (falls when negative), slows with drag, and
    /// over its lifetime shrinks or grows and changes color (through a middle color), fading out
    /// at the end. Glowing particles add light; the others (dust) blend with their opacity.
    /// </summary>
    struct Particle {
        Systems::Rendering::Vector3 position{};
        Systems::Rendering::Vector3 velocity{};   // tiles per second
        float buoyancy{0.0f};                      // upward acceleration
        float drag{0.0f};                          // fraction of velocity lost per second
        float ageSeconds{0.0f};
        float lifetimeSeconds{1.0f};
        float startSize{0.1f};
        float endSize{0.05f};
        std::array<float, 3> startColor{ 1.0f, 0.9f, 0.6f };
        std::array<float, 3> middleColor{ 1.0f, 0.5f, 0.1f };
        std::array<float, 3> endColor{ 0.6f, 0.1f, 0.02f };
        float brightness{1.0f};                    // glowing: light added; dust: opacity
        bool glowing{true};
        ParticleEffect effect{ParticleEffect::None};

        [[nodiscard]] float Progress() const noexcept {
            return std::clamp(ageSeconds / std::max(lifetimeSeconds, 1e-3f), 0.0f, 1.0f);
        }

        [[nodiscard]] float Size() const noexcept {
            return startSize + (endSize - startSize) * Progress();
        }

        /// <summary>
        /// Color now (linear), with alpha: full for the first half of the life, fading to nothing
        /// </summary>
        [[nodiscard]] Systems::Rendering::Color CurrentColor() const noexcept {
            const float t = Progress();
            const auto mix = [](const std::array<float, 3>& a, const std::array<float, 3>& b, float k) {
                return std::array<float, 3>{ a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k };
            };
            const auto c = t < 0.5f ? mix(startColor, middleColor, t * 2.0f) : mix(middleColor, endColor, t * 2.0f - 1.0f);
            const float fade = t < 0.5f ? 1.0f : 1.0f - (t - 0.5f) * 2.0f;
            return { c[0], c[1], c[2], brightness * fade };
        }
    };

    /// <summary>
    /// A pool of particles: spawned by the game, moved each step, removed when their lifetime
    /// ends. Bounded: past MaxParticles the oldest make room (in constant time: a deque, oldest
    /// first).
    /// </summary>
    class ParticleField {
    public:
        static constexpr std::size_t MaxParticles = 4000;

        void Spawn(const Particle& particle) {
            if (particles_.size() >= MaxParticles) {
                particles_.pop_front();
            }
            particles_.push_back(particle);
        }

        void Update(float elapsedSeconds) {
            for (auto& p : particles_) {
                p.ageSeconds += elapsedSeconds;
                p.velocity.y += p.buoyancy * elapsedSeconds;
                const float keep = std::max(0.0f, 1.0f - p.drag * elapsedSeconds);
                p.velocity = { p.velocity.x * keep, p.velocity.y * keep, p.velocity.z * keep };
                p.position = { p.position.x + p.velocity.x * elapsedSeconds, p.position.y + p.velocity.y * elapsedSeconds,
                               p.position.z + p.velocity.z * elapsedSeconds };
            }
            std::erase_if(particles_, [](const Particle& p) { return p.ageSeconds >= p.lifetimeSeconds; });
        }

        void Clear() noexcept { particles_.clear(); }

        [[nodiscard]] const std::deque<Particle>& GetParticles() const noexcept { return particles_; }

        [[nodiscard]] std::size_t Count(ParticleEffect effect) const noexcept {
            return static_cast<std::size_t>(std::ranges::count(particles_, effect, &Particle::effect));
        }

    private:
        std::deque<Particle> particles_;
    };

} // namespace LB::Engine
