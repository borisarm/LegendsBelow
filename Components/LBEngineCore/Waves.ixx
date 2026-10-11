// Waves.ixx - Who comes and where: wave composition and scaling, and finding spawn points away
// from the player and the other enemies
export module LB.Engine:Waves;

import std;
import :World;
import :Enemies;

export namespace LB::Engine {

    /// <summary>
    /// The rules of the test arena's waves (the wave standing in for dungeon depth) and where
    /// enemies may appear. Stateless: the encounter owns the roster and the random generator.
    /// </summary>
    struct Waves {
        static constexpr float DelaySeconds = 3.0f;           // between clearing a wave and the next
        static constexpr float MinimumSpawnDistance = 5.0f;   // from the player

        /// <summary>
        /// How much tougher a wave's enemies are (design-combat-system.md 8.1): +15% hit points
        /// and +10% experience per wave, +1 damage every second wave
        /// </summary>
        struct Scaling {
            float hitPoints{1.0f};
            std::int32_t damage{0};
            float experience{1.0f};
        };

        [[nodiscard]] static Scaling ScalingFor(std::int32_t wave) noexcept {
            const std::int32_t depth = std::max(0, wave - 1);
            return { 1.0f + 0.15f * static_cast<float>(depth), depth / 2, 1.0f + 0.1f * static_cast<float>(depth) };
        }

        /// <summary>
        /// Enemies in a wave: light ones grow from 2 to 4, heavy ones from 0 to 2
        /// (placeholder composition until dungeon rooms define their encounters)
        /// </summary>
        [[nodiscard]] static std::pair<std::int32_t, std::int32_t> Composition(std::int32_t wave) noexcept {
            const std::int32_t n = std::max(1, wave);
            return { std::min(2 + (n - 1) / 2, 4), std::min(n / 2, 2) };
        }

        /// <summary>
        /// Ranged enemies joining a wave: one from the second wave, two from the fifth
        /// </summary>
        [[nodiscard]] static std::int32_t Shooters(std::int32_t wave) noexcept {
            return wave < 2 ? 0 : std::min(1 + (wave - 2) / 3, 2);
        }

        /// <summary>
        /// Brutes joining a wave: one from the fourth
        /// </summary>
        [[nodiscard]] static std::int32_t Brutes(std::int32_t wave) noexcept {
            return wave < 4 ? 0 : 1;
        }

        /// <summary>
        /// Spellcasters joining a wave: one from the third wave, two from the sixth
        /// </summary>
        [[nodiscard]] static std::int32_t Casters(std::int32_t wave) noexcept {
            return wave < 3 ? 0 : std::min(1 + (wave - 3) / 3, 2);
        }

        /// <summary>
        /// A random spot in the world's first room away from the player and the other enemies (the
        /// farthest candidate when none qualifies); the player's spot without a world
        /// </summary>
        [[nodiscard]] static WorldPoint WaveSpawnPoint(const WorldMap* world, WorldPoint player, const std::vector<Enemy>& enemies, std::mt19937& rng) {
            if (!world || world->Layout().rooms.empty()) return player;

            WorldPoint best{};
            float bestScore = -1.0f;
            for (int attempt = 0; attempt < 32; ++attempt) {
                const auto candidate = world->RandomPointInRoom(0, 1.5f, rng);
                float nearestEnemy = std::numeric_limits<float>::max();
                for (const auto& enemy : enemies) {
                    if (!enemy.IsDead()) nearestEnemy = std::min(nearestEnemy, Distance(candidate, enemy.position));
                }
                const float fromPlayer = Distance(candidate, player);
                if (fromPlayer >= MinimumSpawnDistance && nearestEnemy >= 1.0f) return candidate;

                const float score = std::min(fromPlayer, nearestEnemy * MinimumSpawnDistance);
                if (score > bestScore) {
                    bestScore = score;
                    best = candidate;
                }
            }
            return best;
        }

        /// <summary>
        /// A random spot in a room where a body fits, as far from the player and the other enemies
        /// as the tries allow (the room's center when nothing fits)
        /// </summary>
        [[nodiscard]] static WorldPoint RoomSpawnPoint(const WorldMap& world, std::int32_t room, WorldPoint player, const std::vector<Enemy>& enemies,
                                                       float bodyRadius, std::mt19937& rng) {
            WorldPoint at = world.RoomCenter(room);
            float bestScore = -1.0f;
            for (int attempt = 0; attempt < 24; ++attempt) {
                const auto candidate = world.RandomPointInRoom(room, 1.0f, rng);
                if (!world.CircleFits(candidate, bodyRadius)) continue;
                float nearest = Distance(candidate, player);
                for (const auto& enemy : enemies) {
                    if (!enemy.IsDead()) nearest = std::min(nearest, Distance(candidate, enemy.position) * 2.5f);
                }
                if (nearest > bestScore) {
                    bestScore = nearest;
                    at = candidate;
                }
            }
            return at;
        }
    };

} // namespace LB::Engine
