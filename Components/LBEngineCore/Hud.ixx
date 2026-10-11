// Hud.ixx - The heads-up display: drawn from a snapshot of what it shows, without reaching into
// the game
export module LB.Engine:Hud;

import std;
import LB.Core;
import LB.RenderingSystem;

export namespace LB::Engine {

    /// <summary>
    /// What the HUD shows this frame
    /// </summary>
    struct HudModel {
        struct Player {
            std::int32_t hitPoints{0};
            std::int32_t maxHitPoints{1};
            float healthFraction{1.0f};
            std::int32_t level{1};
            float experienceFraction{0.0f};   // toward the next level
            std::int32_t gold{0};
            std::int32_t potions{0};
            bool pointsToSpend{false};
        };
        // A health bar over a hurt enemy
        struct EnemyBar {
            Systems::Rendering::Vector3 overHead{};
            float fraction{1.0f};
        };

        std::optional<Player> player;
        std::vector<EnemyBar> enemyBars;
        std::string banner;                   // "WAVE 2", "DEPTH 3" (top center); empty: none
    };

    /// <summary>
    /// Health and experience bars, level, gold and potions (bottom left), health bars over hurt
    /// enemies, and the banner (top center)
    /// </summary>
    class Hud {
    public:
        static void Draw(Systems::Rendering::RenderingSystem& renderer, const HudModel& hud) {
            using Systems::Rendering::Color;
            using Systems::Rendering::Rectangle;
            const auto display = [](float r, float g, float b) {
                return Color{ std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), 1.0f };
            };
            const auto bar = [&](std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height, float fraction, Color fill) {
                renderer.DrawRectangle(Rectangle{ x - 2, y - 2, width + 4, height + 4 }, display(0.05f, 0.04f, 0.04f), 10);
                renderer.DrawRectangle(Rectangle{ x, y, width, height }, display(0.22f, 0.06f, 0.06f), 11);
                const auto filled = static_cast<std::int32_t>(std::lround(static_cast<float>(width) * std::clamp(fraction, 0.0f, 1.0f)));
                if (filled > 0) renderer.DrawRectangle(Rectangle{ x, y, filled, height }, fill, 12);
            };
            const auto label = [&](std::string text, std::int32_t x, std::int32_t y, float scale, Color color) {
                renderer.DrawText(Systems::Rendering::TextRenderData{ std::move(text), LB::Position{ x, y }, {}, color, scale, 13, true });
            };

            std::int32_t screenWidth = 0;
            std::int32_t screenHeight = 0;
            renderer.GetScreenSize(screenWidth, screenHeight);

            if (hud.player) {
                const auto& player = *hud.player;
                bar(24, screenHeight - 44, 240, 18, player.healthFraction, display(0.80f, 0.14f, 0.12f));
                label(std::format("HP {}/{}", player.hitPoints, player.maxHitPoints), 30, screenHeight - 42, 2.0f, display(1.0f, 1.0f, 1.0f));
                bar(24, screenHeight - 20, 240, 5, player.experienceFraction, display(0.95f, 0.75f, 0.30f));
                label(std::format("LEVEL {}", player.level), 24, screenHeight - 70, 2.0f, display(0.95f, 0.75f, 0.30f));
                label(std::format("GOLD {}", player.gold), 280, screenHeight - 42, 2.0f, display(0.95f, 0.80f, 0.35f));
                label(std::format("POTIONS {}", player.potions), 280, screenHeight - 22, 2.0f, display(0.95f, 0.45f, 0.40f));
                if (player.pointsToSpend) {
                    label("POINTS TO SPEND (I)", 24, screenHeight - 92, 2.0f, display(0.55f, 0.95f, 0.55f));
                }
            }

            for (const auto& enemy : hud.enemyBars) {
                const auto point = renderer.WorldToScreen(enemy.overHead);
                if (!point.inFront) continue;
                bar(static_cast<std::int32_t>(point.x) - 22, static_cast<std::int32_t>(point.y) - 8, 44, 5, enemy.fraction, display(0.85f, 0.20f, 0.15f));
            }

            if (!hud.banner.empty()) {
                const auto size = renderer.MeasureText(hud.banner, 3.0f);
                label(hud.banner, screenWidth / 2 - static_cast<std::int32_t>(size.width / 2.0f), 14, 3.0f, display(0.95f, 0.75f, 0.30f));
            }
        }
    };

} // namespace LB::Engine
