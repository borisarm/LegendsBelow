// Projectiles.ixx - Arrows and bolts in flight: they fly straight at chest height, stick in the
// walls they reach, and hit the first body of the other side in their way
export module LB.Engine:Projectiles;

import std;
import LB.CombatService;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// Who shot a projectile; it only hits bodies of the other side
    /// </summary>
    enum class ProjectileSide : std::uint8_t {
        Player,
        Enemies
    };

    /// <summary>
    /// An arrow or bolt. It carries the shooter's attack as it was when it left, and is resolved
    /// against whoever it reaches (design-combat-system.md 7.4: a wall in the way stops it).
    /// </summary>
    struct Projectile {
        std::uint32_t id{0};
        ProjectileSide side{ProjectileSide::Enemies};
        std::uint32_t shooter{0};                      // the enemy that shot it (0: the player)
        WorldPoint position{};
        WorldPoint direction{0.0f, 1.0f};              // unit, on the ground
        float speed{10.0f};                            // tiles per second
        float rangeLeft{0.0f};                         // tiles it may still fly
        Services::Combat::WeaponAttackProfile attack{};
        std::string model;                             // drawn in flight
        float stuckSeconds{-1.0f};                     // >= 0: stuck in a wall for this long

        [[nodiscard]] bool InFlight() const noexcept { return stuckSeconds < 0.0f; }

        // Its heading around the vertical axis (0 faces +Z, like the characters)
        [[nodiscard]] float Yaw() const noexcept { return std::atan2(direction.x, direction.y); }
    };

    /// <summary>
    /// A body a projectile can hit this step: an enemy (by id) or the player (id 0)
    /// </summary>
    struct ProjectileTarget {
        std::uint32_t id{0};
        ProjectileSide side{ProjectileSide::Enemies};   // the side the body is on
        WorldPoint position{};
        float radius{0.3f};
    };

    /// <summary>
    /// What a projectile reached this step: a body, or a wall (no target)
    /// </summary>
    struct ProjectileImpact {
        Projectile projectile;
        std::optional<std::uint32_t> target;
        WorldPoint at{};
    };

    /// <summary>
    /// The projectiles of a floor. Each step they move along their heading, swept in short
    /// steps so fast ones cannot pass through a body; a body of the other side in the way is hit
    /// (the projectile is gone), a wall stops it (it stays stuck for a moment), and one that has
    /// flown its range drops out of play. They go where the target was when they left, so they
    /// can be dodged.
    /// </summary>
    class ProjectileSet {
    public:
        static constexpr float FlightHeight = 0.55f;    // tiles above the floor
        static constexpr float Radius = 0.08f;          // added to a body's radius
        static constexpr float StuckSeconds = 1.5f;     // in a wall before it is removed
        static constexpr float FadeSeconds = 0.5f;      // the last part of StuckSeconds
        static constexpr float SweepTiles = 0.1f;       // longest move between hit tests

        /// <summary>
        /// Start a projectile (its direction is normalized); returns its id
        /// </summary>
        std::uint32_t Launch(Projectile projectile) {
            const float length = std::hypot(projectile.direction.x, projectile.direction.y);
            projectile.direction = length > 1e-5f ? WorldPoint{ projectile.direction.x / length, projectile.direction.y / length } : WorldPoint{ 0.0f, 1.0f };
            projectile.id = nextId_++;
            projectile.stuckSeconds = -1.0f;
            projectiles_.push_back(std::move(projectile));
            return projectiles_.back().id;
        }

        /// <summary>
        /// One step: move every projectile in flight, age the stuck ones. `world` may be null
        /// (open ground). Returns what they reached, in launch order.
        /// </summary>
        std::vector<ProjectileImpact> Update(float elapsedSeconds, const WorldMap* world, std::span<const ProjectileTarget> targets) {
            std::vector<ProjectileImpact> impacts;
            std::vector<std::uint32_t> gone;
            for (auto& projectile : projectiles_) {
                if (!projectile.InFlight()) {
                    projectile.stuckSeconds += elapsedSeconds;
                    if (projectile.stuckSeconds >= StuckSeconds) gone.push_back(projectile.id);
                    continue;
                }

                const float travel = std::min(projectile.speed * elapsedSeconds, projectile.rangeLeft);
                const int steps = std::max(1, static_cast<int>(std::ceil(travel / SweepTiles)));
                const float step = travel / static_cast<float>(steps);
                bool done = false;
                for (int i = 0; i < steps && !done; ++i) {
                    const WorldPoint next{ projectile.position.x + projectile.direction.x * step, projectile.position.y + projectile.direction.y * step };
                    if (world && !world->IsOpen(next)) {
                        projectile.stuckSeconds = 0.0f;
                        impacts.push_back({ projectile, std::nullopt, projectile.position });
                        done = true;
                        break;
                    }
                    projectile.position = next;
                    projectile.rangeLeft -= step;
                    for (const auto& target : targets) {
                        if (target.side == projectile.side) continue;
                        if (Distance(projectile.position, target.position) <= target.radius + Radius) {
                            impacts.push_back({ projectile, target.id, projectile.position });
                            gone.push_back(projectile.id);
                            done = true;
                            break;
                        }
                    }
                }
                if (!done && projectile.rangeLeft <= 1e-4f) gone.push_back(projectile.id);   // spent
            }
            std::erase_if(projectiles_, [&](const Projectile& p) { return std::ranges::find(gone, p.id) != gone.end(); });
            return impacts;
        }

        [[nodiscard]] const std::vector<Projectile>& Get() const noexcept { return projectiles_; }

        void Clear() noexcept { projectiles_.clear(); }

        /// <summary>
        /// How visible a projectile is: 1 in flight, fading out at the end of its time in a wall
        /// </summary>
        [[nodiscard]] static float Opacity(const Projectile& projectile) noexcept {
            if (projectile.InFlight()) return 1.0f;
            return std::clamp((StuckSeconds - projectile.stuckSeconds) / FadeSeconds, 0.0f, 1.0f);
        }

    private:
        std::vector<Projectile> projectiles_;
        std::uint32_t nextId_{1};
    };

} // namespace LB::Engine
