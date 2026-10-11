// CombatText.ixx - Damage numbers, misses, and other short texts floating up from characters
export module LB.Engine:CombatText;

import std;
import LB.RenderingSystem;
import :Encounter;

export namespace LB::Engine {

    /// <summary>
    /// Combat feedback floating up from a character: damage dealt or taken, or a miss
    /// (game-design.md, UI feedback). Rises and fades over CombatTextLog::Seconds.
    /// </summary>
    struct FloatingText {
        std::string text;                       // "7", "12!" (critical), "MISS"
        Systems::Rendering::Vector3 position{}; // world point it starts from (over the character's head)
        Systems::Rendering::Color color{};      // display (sRGB) color
        float scale{3.0f};                      // text scale
        float ageSeconds{0.0f};
    };

    /// <summary>
    /// The texts floating over characters: started for attacks and game events, aged each step,
    /// and drawn rising with an ease-out and fading over the last 40% of their life
    /// </summary>
    class CombatTextLog {
    public:
        static constexpr float Seconds = 1.0f;
        static constexpr float Rise = 0.8f;     // tiles over the lifetime

        void Add(std::string text, Systems::Rendering::Vector3 at, Systems::Rendering::Color color, float scale) {
            texts_.push_back(FloatingText{ std::move(text), at, color, scale });
        }

        /// <summary>
        /// The text for an attack over its target's head: damage in white over enemies (gold and
        /// larger for criticals, marked "!"), red over the player, and a grey "MISS"
        /// </summary>
        void AddAttack(const CombatEvent& event, Systems::Rendering::Vector3 overTarget) {
            // Alternate sides a little so quick hits do not stack on top of each other
            overTarget.x += (attacks_++ % 2 == 0 ? -0.15f : 0.15f);
            const auto damage = std::format("{}{}", event.result.finalDamage, event.result.critical ? "!" : "");
            const float size = event.result.critical ? 4.0f : 3.0f;
            if (!event.result.hit) {
                Add("MISS", overTarget, { 0.75f, 0.75f, 0.75f, 1.0f }, 2.0f);
            }
            else if (event.kind == CombatEventKind::PlayerAttack) {
                Add(damage, overTarget, event.result.critical ? Systems::Rendering::Color{ 1.0f, 0.82f, 0.25f, 1.0f } : Systems::Rendering::Color{ 1.0f, 1.0f, 1.0f, 1.0f }, size);
            }
            else {
                Add(damage, overTarget, { 1.0f, 0.32f, 0.25f, 1.0f }, size);
            }
        }

        void Update(float elapsedSeconds) {
            for (auto& text : texts_) text.ageSeconds += elapsedSeconds;
            std::erase_if(texts_, [](const FloatingText& text) { return text.ageSeconds >= Seconds; });
        }

        void Draw(Systems::Rendering::RenderingSystem& renderer) const {
            for (const auto& floating : texts_) {
                const float t = std::clamp(floating.ageSeconds / Seconds, 0.0f, 1.0f);
                const float rise = Rise * (1.0f - (1.0f - t) * (1.0f - t));
                const auto point = renderer.WorldToUi({ floating.position.x, floating.position.y + rise, floating.position.z });
                if (!point.inFront) continue;

                const float fade = std::clamp((t - 0.6f) / 0.4f, 0.0f, 1.0f);
                Systems::Rendering::Color color{ std::pow(floating.color.r, 2.2f), std::pow(floating.color.g, 2.2f), std::pow(floating.color.b, 2.2f),
                                                 1.0f - fade * fade * (3.0f - 2.0f * fade) };
                const auto size = renderer.MeasureText(floating.text, floating.scale);
                renderer.DrawText(Systems::Rendering::TextRenderData{ floating.text,
                    LB::Position{ static_cast<std::int32_t>(point.x - size.width / 2.0f), static_cast<std::int32_t>(point.y - size.height) },
                    {}, color, floating.scale, 20, true });
            }
        }

        void Clear() noexcept { texts_.clear(); }

        [[nodiscard]] const std::vector<FloatingText>& Texts() const noexcept { return texts_; }

    private:
        std::vector<FloatingText> texts_;
        std::uint32_t attacks_{0};
    };

} // namespace LB::Engine
