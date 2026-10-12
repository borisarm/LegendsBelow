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
            bool usesMana{false};             // a mana (or stamina) bar
            std::string resource{"MP"};       // its label: "MP", or "ST" for stamina
            bool pathToChoose{false};
            std::int32_t mana{0};
            std::int32_t maxMana{0};
            std::vector<std::string> statuses;   // "Slowed", "Burning"
            std::string ammo;                 // "ARROWS 30" with a bow or crossbow; empty: none
        };
        // One slot of the spell bar
        struct SpellSlot {
            std::string key;                  // "1"
            std::string name;
            std::int32_t manaCost{0};
            float cooldownFraction{0.0f};     // of the cooldown still to run
            std::string blocker;              // why it cannot be cast ("level 5", "mana"), empty: ready
            Systems::Rendering::Color accent{1.0f, 1.0f, 1.0f, 1.0f};   // the element's color (linear)
        };
        // A health bar over a hurt enemy
        struct EnemyBar {
            Systems::Rendering::Vector3 overHead{};
            float fraction{1.0f};
        };

        std::optional<Player> player;
        // A companion's health (top left)
        struct AllyBar {
            std::string name;
            float fraction{1.0f};
            bool down{false};
        };
        std::vector<AllyBar> allies;
        std::vector<std::string> goals;      // the floor's objective and the bounty (top left, under the companions)
        std::vector<bool> goalsDone;
        float savedNotice{0.0f};              // "Saved" (top left) fading out: 1 just saved, 0 gone
        std::vector<EnemyBar> enemyBars;
        std::string banner;                   // "WAVE 2", "DEPTH 3" (top center); empty: none
        std::vector<SpellSlot> spellBar;      // bottom right; empty: none
        // A boss in the fight: its name and health across the top
        struct Boss {
            std::string name;
            float fraction{1.0f};
            bool phaseTwo{false};
        };
        std::optional<Boss> boss;
        std::string hint;                     // the tactical pause's help (bottom center)
        std::string order;                    // the order in hand (bottom center)
        std::string castName;                 // the spell being cast (a bar under the hero)
        std::optional<float> castFraction;
    };

    /// <summary>
    /// Health, mana, and experience bars, level, gold and potions, statuses (bottom left), the
    /// spell bar (bottom right), the cast bar, health bars over hurt enemies, and the banner (top
    /// center)
    /// </summary>
    class Hud {
    public:
        static constexpr float GoalWidth = 190.0f;   // the objective's and the bounty's lines, at most (UI units)

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
            renderer.GetUiSize(screenWidth, screenHeight);

            if (hud.player) {
                const auto& player = *hud.player;
                bar(24, screenHeight - 44, 240, 18, player.healthFraction, display(0.80f, 0.14f, 0.12f));
                label(std::format("HP {}/{}", player.hitPoints, player.maxHitPoints), 30, screenHeight - 42, 2.0f, display(1.0f, 1.0f, 1.0f));
                if (player.usesMana) {
                    const float fraction = player.maxMana > 0 ? static_cast<float>(player.mana) / static_cast<float>(player.maxMana) : 0.0f;
                    bar(24, screenHeight - 22, 240, 12, fraction, display(0.25f, 0.45f, 0.95f));
                    label(std::format("{} {}/{}", player.resource, player.mana, player.maxMana), 30, screenHeight - 22, 1.3f, display(1.0f, 1.0f, 1.0f));
                    bar(24, screenHeight - 6, 240, 3, player.experienceFraction, display(0.95f, 0.75f, 0.30f));
                }
                else {
                    bar(24, screenHeight - 20, 240, 5, player.experienceFraction, display(0.95f, 0.75f, 0.30f));
                }
                if (!player.statuses.empty()) {
                    std::string text;
                    for (const auto& status : player.statuses) text += (text.empty() ? "" : "  ") + status;
                    label(text, 130, screenHeight - 68, 1.8f, display(0.60f, 0.85f, 1.0f));
                }
                label(std::format("LEVEL {}", player.level), 24, screenHeight - 70, 2.0f, display(0.95f, 0.75f, 0.30f));
                label(std::format("GOLD {}", player.gold), 280, screenHeight - 42, 2.0f, display(0.95f, 0.80f, 0.35f));
                label(std::format("POTIONS {}", player.potions), 280, screenHeight - 22, 2.0f, display(0.95f, 0.45f, 0.40f));
                if (!player.ammo.empty()) label(player.ammo, 280, screenHeight - 62, 2.0f, display(0.85f, 0.80f, 0.65f));
                if (player.pointsToSpend || player.pathToChoose) {
                    label(player.pathToChoose ? "A PATH TO CHOOSE (I)" : "POINTS TO SPEND (I)", 24, screenHeight - 92, 2.0f, display(0.55f, 0.95f, 0.55f));
                }
            }

            for (const auto& enemy : hud.enemyBars) {
                const auto point = renderer.WorldToUi(enemy.overHead);
                if (!point.inFront) continue;
                bar(static_cast<std::int32_t>(point.x) - 22, static_cast<std::int32_t>(point.y) - 8, 44, 5, enemy.fraction, display(0.85f, 0.20f, 0.15f));
            }

            // The spell bar: a slot per spell with its key, name, and cost; the cooldown still to
            // run darkens it from the top, a blocked spell is dimmed with the reason
            if (!hud.spellBar.empty()) {
                constexpr std::int32_t size = 64;
                constexpr std::int32_t gap = 8;
                const auto count = static_cast<std::int32_t>(hud.spellBar.size());
                const std::int32_t left = screenWidth - 24 - count * size - (count - 1) * gap;
                const std::int32_t top = screenHeight - 24 - size;
                for (std::int32_t i = 0; i < count; ++i) {
                    const auto& slot = hud.spellBar[static_cast<std::size_t>(i)];
                    const std::int32_t x = left + i * (size + gap);
                    renderer.DrawRectangle(Rectangle{ x - 2, top - 2, size + 4, size + 4 }, display(0.05f, 0.04f, 0.04f), 10);
                    renderer.DrawRectangle(Rectangle{ x, top, size, size }, display(0.10f, 0.09f, 0.12f), 11);
                    renderer.DrawRectangle(Rectangle{ x, top + size - 5, size, 5 }, slot.accent, 11);
                    label(slot.key, x + 4, top + 2, 1.6f, display(0.95f, 0.75f, 0.30f));
                    // The name on up to two lines
                    const auto space = slot.name.find(' ');
                    const auto first = space == std::string::npos ? slot.name : slot.name.substr(0, space);
                    const auto second = space == std::string::npos ? std::string{} : slot.name.substr(space + 1);
                    const auto centered = [&](const std::string& text, std::int32_t y, float scale, Color color) {
                        const auto width = renderer.MeasureText(text, scale).width;
                        label(text, x + size / 2 - static_cast<std::int32_t>(width / 2.0f), y, scale, color);
                    };
                    centered(first, top + 18, 1.3f, display(0.92f, 0.90f, 0.85f));
                    if (!second.empty()) centered(second, top + 31, 1.3f, display(0.92f, 0.90f, 0.85f));
                    centered(std::format("{}", slot.manaCost), top + 44, 1.3f, display(0.55f, 0.70f, 1.0f));
                    if (slot.cooldownFraction > 0.0f) {
                        const auto shaded = static_cast<std::int32_t>(std::lround(static_cast<float>(size) * slot.cooldownFraction));
                        renderer.DrawRectangle(Rectangle{ x, top, size, shaded }, Color{ 0.0f, 0.0f, 0.0f, 0.65f }, 11);
                    }
                    else if (!slot.blocker.empty()) {
                        renderer.DrawRectangle(Rectangle{ x, top, size, size }, Color{ 0.0f, 0.0f, 0.0f, 0.55f }, 11);
                        // Why, in the top right corner: the level it opens at, or too little mana
                        const std::string reason = slot.blocker.starts_with("level") ? std::format("L{}", slot.blocker.substr(6)) : slot.blocker == "mana" ? "MP" : "";
                        if (!reason.empty()) {
                            const auto width = renderer.MeasureText(reason, 1.5f).width;
                            renderer.DrawText(Systems::Rendering::TextRenderData{ reason, LB::Position{ x + size - 4 - static_cast<std::int32_t>(width), top + 2 }, {},
                                display(0.95f, 0.45f, 0.40f), 1.5f, 13, true });
                        }
                    }
                }
            }

            // The cast bar, under the hero (the middle of the screen)
            if (hud.castFraction) {
                constexpr std::int32_t width = 140;
                const std::int32_t x = screenWidth / 2 - width / 2;
                const std::int32_t y = screenHeight / 2 + 48;
                bar(x, y, width, 7, *hud.castFraction, display(0.70f, 0.55f, 1.0f));
                const auto size = renderer.MeasureText(hud.castName, 1.6f);
                label(hud.castName, screenWidth / 2 - static_cast<std::int32_t>(size.width / 2.0f), y + 10, 1.6f, display(0.85f, 0.80f, 1.0f));
            }

            // An autosave just happened
            if (hud.savedNotice > 0.0f) {
                const auto y = 16 + static_cast<std::int32_t>(hud.allies.size()) * 34;
                label("Saved", 24, y, 1.6f, Color{ std::pow(0.95f, 2.2f), std::pow(0.80f, 2.2f), std::pow(0.40f, 2.2f), std::min(1.0f, hud.savedNotice * 2.0f) });
            }

            // The objective and the bounty, under the companions (dimmed once done)
            for (std::size_t i = 0; i < hud.goals.size(); ++i) {
                const auto y = 16 + static_cast<std::int32_t>(hud.allies.size()) * 34 + (hud.savedNotice > 0.0f ? 20 : 0) + static_cast<std::int32_t>(i) * 20;
                const bool done = i < hud.goalsDone.size() && hud.goalsDone[i];
                const auto text = done ? hud.goals[i] + " - done" : hud.goals[i];
                // Kept left of the banner (top center): smaller when long
                float scale = 1.5f;
                while (scale > 1.1f && renderer.MeasureText(text, scale).width > GoalWidth) scale -= 0.1f;
                label(text, 24, y, scale, done ? display(0.55f, 0.55f, 0.50f) : display(0.95f, 0.88f, 0.60f));
            }

            // Companions: a name and a health bar each (top left; grey while down)
            for (std::size_t i = 0; i < hud.allies.size(); ++i) {
                const auto& ally = hud.allies[i];
                const auto y = 16 + static_cast<std::int32_t>(i) * 34;
                label(ally.down ? ally.name + " - DOWN" : ally.name, 24, y, 1.6f, ally.down ? display(0.6f, 0.6f, 0.6f) : display(0.55f, 0.95f, 0.55f));
                bar(24, y + 16, 140, 8, ally.down ? 0.0f : ally.fraction, display(0.25f, 0.75f, 0.30f));
            }

            if (hud.boss) {
                constexpr std::int32_t width = 360;
                const std::int32_t x = screenWidth / 2 - width / 2;
                bar(x, 66, width, 12, hud.boss->fraction, hud.boss->phaseTwo ? display(0.75f, 0.20f, 0.85f) : display(0.80f, 0.14f, 0.12f));
                const auto name = hud.boss->phaseTwo ? hud.boss->name + " - ENRAGED" : hud.boss->name;
                const auto size = renderer.MeasureText(name, 2.0f);
                label(name, screenWidth / 2 - static_cast<std::int32_t>(size.width / 2.0f), 44, 2.0f, display(0.95f, 0.75f, 0.30f));
            }

            if (!hud.hint.empty()) {
                const auto size = renderer.MeasureText(hud.hint, 1.6f);
                label(hud.hint, screenWidth / 2 - static_cast<std::int32_t>(size.width / 2.0f), screenHeight - 120, 1.6f, display(0.95f, 0.90f, 0.70f));
            }
            if (!hud.order.empty()) {
                const auto size = renderer.MeasureText(hud.order, 1.8f);
                label(hud.order, screenWidth / 2 - static_cast<std::int32_t>(size.width / 2.0f), screenHeight - 100, 1.8f, display(1.0f, 0.55f, 0.40f));
            }

            if (!hud.banner.empty()) {
                const auto size = renderer.MeasureText(hud.banner, 3.0f);
                label(hud.banner, screenWidth / 2 - static_cast<std::int32_t>(size.width / 2.0f), 14, 3.0f, display(0.95f, 0.75f, 0.30f));
            }
        }
    };

} // namespace LB::Engine
