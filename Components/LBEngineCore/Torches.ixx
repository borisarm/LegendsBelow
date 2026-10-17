// Torches.ixx - Torches on the room walls of a floor: where they hang, their flickering light,
// and the fire they feed into the effects
export module LB.Engine:Torches;

import std;
import LB.RenderingSystem;
import :World;
import :Walls;
import :Particles;
import :Effects;

export namespace LB::Engine {

    /// <summary>
    /// A torch on a room wall: where its flame burns, which way it faces, and the wall piece it
    /// hangs on (it fades with that wall; none without models)
    /// </summary>
    struct Torch {
        WorldPoint position{};       // under the flame, on the floor plane
        float yaw{0.0f};             // faces into the room
        std::optional<std::size_t> wall;
        float phase{0.0f};           // flicker offset
        float flameCarry{0.0f};      // fractions of particles owed to the next step
        float emberCarry{0.0f};
    };

    /// <summary>
    /// The torches of the current floor: one in the middle of each room side and more on long
    /// sides (one every four cells), hung on the wall's inner face and facing into the room. Each
    /// casts a flickering warm light; those near the player burn with flame and ember particles.
    /// </summary>
    class TorchSet {
    public:
        static constexpr float LightRadius = 6.0f;          // tiles
        static constexpr float LightIntensity = 1.6f;
        static constexpr float FlamesPerSecond = 34.0f;
        static constexpr float EmbersPerSecond = 2.5f;
        static constexpr float FireRange = 16.0f;           // tiles from the player
        static constexpr float GutteredBrightness = 0.2f;   // guttering: this share of their light and fire...
        static constexpr float GutterSeconds = 1.5f;        // ...reached (or back to full) over this long

        /// <summary>
        /// Place the torches for a world's rooms, each tied to the wall segment behind it
        /// </summary>
        void Build(const WorldMap& world, const WallSet& walls) {
            torches_.clear();
            brightness_ = 1.0f;
            guttered_ = false;
            const auto& layout = world.Layout();
            constexpr float cell = WorldMap::CellTiles;
            constexpr float pi = std::numbers::pi_v<float>;

            for (const auto& room : layout.rooms) {
                const auto& area = room.area;
                // Each side: its cells along the wall, the step outward, and the facing into the room
                struct Side { std::int32_t length; std::int32_t x0, y0, dx, dy, outX, outY; float yaw; };
                const Side sides[4] = {
                    { area.width, area.x, area.y, 1, 0, 0, -1, 0.0f },                          // north wall, faces south (+Z)
                    { area.width, area.x, area.y + area.height - 1, 1, 0, 0, 1, pi },           // south wall
                    { area.height, area.x, area.y, 0, 1, -1, 0, pi * 0.5f },                     // west wall, faces east
                    { area.height, area.x + area.width - 1, area.y, 0, 1, 1, 0, -pi * 0.5f },   // east wall
                };
                for (const auto& side : sides) {
                    std::vector<std::int32_t> spots{ side.length / 2 };
                    for (std::int32_t k = side.length / 2 + 4; k < side.length - 1; k += 4) spots.push_back(k);
                    for (std::int32_t k = side.length / 2 - 4; k > 0; k -= 4) spots.push_back(k);

                    for (const auto k : spots) {
                        const std::int32_t x = side.x0 + side.dx * k;
                        const std::int32_t y = side.y0 + side.dy * k;
                        if (layout.IsFloor(x + side.outX, y + side.outY)) continue;   // a doorway, not a wall

                        // The middle of the cell's edge on that side, just off the wall
                        const auto center = world.CellCenter(x, y);
                        const float outX = static_cast<float>(side.outX);
                        const float outY = static_cast<float>(side.outY);
                        const WorldPoint edge{ center.x + outX * cell * 0.5f, center.y + outY * cell * 0.5f };
                        Torch torch;
                        torch.position = { edge.x - outX * 0.05f, edge.y - outY * 0.05f };
                        torch.yaw = side.yaw;
                        torch.wall = walls.FindSegmentNear({ edge.x + outX * 0.4f, edge.y + outY * 0.4f }, 1.0f);
                        torch.phase = static_cast<float>(torches_.size()) * 1.7f;
                        torches_.push_back(torch);
                    }
                }
            }
        }

        void Clear() noexcept { torches_.clear(); }

        /// <summary>
        /// Gutter every torch (a boss's last phase darkens the hall) or let them burn up again;
        /// the light fades over GutterSeconds as the fire is fed
        /// </summary>
        void Gutter(bool guttered) noexcept { guttered_ = guttered; }
        [[nodiscard]] bool IsGuttered() const noexcept { return guttered_; }

        /// <summary>
        /// How bright the torches burn now: 1 in full, down to GutteredBrightness
        /// </summary>
        [[nodiscard]] float Brightness() const noexcept { return brightness_; }

        /// <summary>
        /// Feed the fire of the torches near the player (not those on a faded wall): a steady
        /// stream of flames, and now and then an ember drifting higher
        /// </summary>
        void UpdateFire(float elapsedSeconds, WorldPoint player, const WallSet& walls, EffectsSystem& effects) {
            const float target = guttered_ ? GutteredBrightness : 1.0f;
            const float change = (1.0f - GutteredBrightness) * elapsedSeconds / GutterSeconds;
            brightness_ = brightness_ < target ? std::min(target, brightness_ + change) : std::max(target, brightness_ - change);
            for (auto& torch : torches_) {
                if (walls.Opacity(torch.wall) < 0.5f) continue;
                if (std::hypot(torch.position.x - player.x, torch.position.y - player.y) > FireRange) continue;
                const auto base = FlameOf(torch);

                torch.flameCarry += FlamesPerSecond * brightness_ * elapsedSeconds;
                const auto flames = static_cast<std::int32_t>(torch.flameCarry);
                torch.flameCarry -= static_cast<float>(flames);
                effects.Emit(ParticleEffect::TorchFlame, { base.x, base.y - 0.06f, base.z }, flames);

                torch.emberCarry += EmbersPerSecond * brightness_ * elapsedSeconds;
                const auto embers = static_cast<std::int32_t>(torch.emberCarry);
                torch.emberCarry -= static_cast<float>(embers);
                effects.Emit(ParticleEffect::TorchEmber, { base.x, base.y + 0.05f, base.z }, embers);
            }
        }

        /// <summary>
        /// The torch props (when there is a model; they fade with their wall) and a flickering
        /// light at each flame
        /// </summary>
        void Draw(Systems::Rendering::RenderingSystem& renderer, const WallSet& walls, Systems::Rendering::ModelId model,
                  float modelScale, Systems::Rendering::Color tint, double timeSeconds, const ViewArea& view = {}) const {
            for (const auto& torch : torches_) {
                if (!view.Contains(torch.position, LightRadius)) continue;   // its light may still reach the view
                const auto flame = FlameOf(torch);
                if (model != Systems::Rendering::InvalidModelId) {
                    auto color = tint;
                    color.a = walls.Opacity(torch.wall);
                    renderer.DrawModel(Systems::Rendering::ModelRenderData{ model, { torch.position.x, 1.0f, torch.position.y }, torch.yaw, modelScale, color });
                }
                Systems::Rendering::PointLight light{};
                light.position = { flame.x + std::sin(torch.yaw) * 0.25f, flame.y, flame.z + std::cos(torch.yaw) * 0.25f };
                light.color = { 1.0f, 0.55f, 0.22f, 1.0f };
                light.radius = LightRadius;
                light.intensity = LightIntensity * brightness_ * Flicker(timeSeconds, torch.phase);
                renderer.DrawLight(light);
            }
        }

        [[nodiscard]] const std::vector<Torch>& Torches() const noexcept { return torches_; }

        /// <summary>
        /// Where a torch's flame burns: above and in front of its mount
        /// </summary>
        [[nodiscard]] static Systems::Rendering::Vector3 FlameOf(const Torch& torch) noexcept {
            constexpr float out = 0.285f;   // the torch head, in front of the wall plate
            return { torch.position.x + std::sin(torch.yaw) * out, 1.42f, torch.position.y + std::cos(torch.yaw) * out };
        }

        /// <summary>
        /// A torch's flicker: a slow sway and a quicker shiver, between about 80% and 100%
        /// </summary>
        [[nodiscard]] static float Flicker(double timeSeconds, float phase) noexcept {
            const auto t = static_cast<float>(timeSeconds);
            return 0.9f + 0.06f * std::sin(t * 5.3f + phase) + 0.04f * std::sin(t * 13.7f + phase * 2.3f);
        }

    private:
        std::vector<Torch> torches_;
        float brightness_{1.0f};
        bool guttered_{false};
    };

} // namespace LB::Engine
