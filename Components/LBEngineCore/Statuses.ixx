// Statuses.ixx - Status effects on a character (design-character-system.md 6): slowed, rooted,
// frozen, burning; how long each lasts, how it hinders, and the damage it ticks
export module LB.Engine:Statuses;

import std;
import LB.Content;
import LB.CombatService;

export namespace LB::Engine {

    /// <summary>
    /// One status on a character and the time it has left
    /// </summary>
    struct ActiveStatus {
        const Content::StatusDefinition* definition{nullptr};   // into the content, which outlives it
        float secondsLeft{0.0f};
        float untilTick{0.0f};
    };

    /// <summary>
    /// Damage a status dealt this step (before the target's defenses)
    /// </summary>
    struct StatusTick {
        const Content::StatusDefinition* definition{nullptr};
        std::int32_t damage{0};
        Services::Combat::DamageType damageType{Services::Combat::DamageType::None};
    };

    /// <summary>
    /// The statuses on one character. A status applied again starts over (no stacking); the
    /// strongest hindrance wins (the lowest movement multiplier).
    /// </summary>
    class StatusSet {
    public:
        void Apply(const Content::StatusDefinition& status) {
            const auto it = std::ranges::find(active_, &status, &ActiveStatus::definition);
            if (it != active_.end()) {
                it->secondsLeft = status.durationSeconds;
                return;
            }
            active_.push_back({ &status, status.durationSeconds, status.tickIntervalSeconds });
        }

        /// <summary>
        /// Age the statuses; returns the damage ticks of this step. Expired ones are removed.
        /// </summary>
        std::vector<StatusTick> Update(float elapsedSeconds) {
            std::vector<StatusTick> ticks;
            for (auto& status : active_) {
                const auto& definition = *status.definition;
                status.secondsLeft -= elapsedSeconds;
                if (definition.damagePerTick <= 0) continue;
                status.untilTick -= elapsedSeconds;
                while (status.untilTick <= 0.0f && status.secondsLeft > -definition.tickIntervalSeconds) {
                    ticks.push_back({ &definition, definition.damagePerTick, definition.damageType });
                    status.untilTick += definition.tickIntervalSeconds;
                }
            }
            std::erase_if(active_, [](const ActiveStatus& status) { return status.secondsLeft <= 0.0f; });
            return ticks;
        }

        [[nodiscard]] float MovementMultiplier() const noexcept {
            float multiplier = 1.0f;
            for (const auto& status : active_) multiplier = std::min(multiplier, status.definition->movementMultiplier);
            return multiplier;
        }

        /// <summary>
        /// To hit from the statuses held (a blessing), and what they leave of a blow's damage
        /// </summary>
        [[nodiscard]] std::int32_t AttackBonus() const noexcept {
            std::int32_t bonus = 0;
            for (const auto& status : active_) bonus += status.definition->attackBonus;
            return bonus;
        }
        [[nodiscard]] float DamageDealtMultiplier() const noexcept {
            float multiplier = 1.0f;
            for (const auto& status : active_) multiplier *= status.definition->damageDealtMultiplier;
            return multiplier;
        }
        [[nodiscard]] std::int32_t Dealt(std::int32_t damage) const noexcept {
            if (damage <= 0) return damage;
            return std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(damage) * DamageDealtMultiplier())));
        }
        [[nodiscard]] float DamageTakenMultiplier() const noexcept {
            float multiplier = 1.0f;
            for (const auto& status : active_) multiplier *= status.definition->damageTakenMultiplier;
            return multiplier;
        }

        /// <summary>
        /// A blow's damage as the statuses leave it (at least 1 of a blow that did any)
        /// </summary>
        [[nodiscard]] std::int32_t Taken(std::int32_t damage) const noexcept {
            if (damage <= 0) return damage;
            return std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(damage) * DamageTakenMultiplier())));
        }

        [[nodiscard]] bool PreventsActions() const noexcept {
            return std::ranges::any_of(active_, [](const ActiveStatus& status) { return status.definition->preventsActions; });
        }

        [[nodiscard]] bool Has(std::string_view key) const noexcept {
            return std::ranges::any_of(active_, [key](const ActiveStatus& status) { return status.definition->key == key; });
        }

        // Damage over time of a type is on (for its look: burning)
        [[nodiscard]] bool HasDamageOverTime(Services::Combat::DamageType type) const noexcept {
            return std::ranges::any_of(active_, [type](const ActiveStatus& status) {
                return status.definition->damagePerTick > 0 && status.definition->damageType == type;
            });
        }

        /// <summary>
        /// Lift up to `count` hindrances (Debuff or Control statuses), oldest first
        /// </summary>
        void RemoveHindrances(std::int32_t count) {
            for (auto it = active_.begin(); it != active_.end() && count > 0;) {
                const auto& categories = it->definition->categories;
                const bool hindrance = std::ranges::find(categories, "Debuff") != categories.end() || std::ranges::find(categories, "Control") != categories.end();
                if (hindrance) {
                    it = active_.erase(it);
                    --count;
                }
                else {
                    ++it;
                }
            }
        }

        [[nodiscard]] const std::vector<ActiveStatus>& Active() const noexcept { return active_; }

        void Clear() noexcept { active_.clear(); }

    private:
        std::vector<ActiveStatus> active_;
    };

} // namespace LB::Engine
