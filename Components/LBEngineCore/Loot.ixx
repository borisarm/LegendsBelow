// Loot.ixx - Item references, loot rolls, and items lying on the floor
export module LB.Engine:Loot;

import std;
import LB.Core;
import LB.Content;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// Which content file an item comes from
    /// </summary>
    enum class ItemKind : std::uint8_t {
        None = 0,
        Weapon,       // weapons.json
        Armor,        // armour.json
        Consumable    // consumables.json
    };

    /// <summary>
    /// An item by content file and id. Packs into one LB::Id (kind in the high bits) so the
    /// inventory, which stores plain item ids, can hold weapons, armour, and consumables together.
    /// </summary>
    struct ItemRef {
        ItemKind kind{ItemKind::None};
        Content::ContentId id{0};

        [[nodiscard]] bool IsValid() const noexcept { return kind != ItemKind::None && id > 0; }

        [[nodiscard]] LB::Id Pack() const noexcept {
            return (static_cast<LB::Id>(kind) << 24) | (id & 0xFFFFFF);
        }

        [[nodiscard]] static ItemRef Unpack(LB::Id packed) noexcept {
            const auto kind = static_cast<std::uint8_t>(packed >> 24);
            if (kind < 1 || kind > 3) return {};
            return { static_cast<ItemKind>(kind), packed & 0xFFFFFF };
        }

        /// <summary>
        /// The item a content key names (weapon, armour, or consumable), or an invalid reference
        /// </summary>
        [[nodiscard]] static ItemRef FromKey(const Content::ContentDatabase& content, std::string_view key) {
            if (const auto* weapon = content.FindWeapon(key)) return { ItemKind::Weapon, weapon->id };
            if (const auto* armor = content.FindArmor(key)) return { ItemKind::Armor, armor->id };
            if (const auto* consumable = content.FindConsumable(key)) return { ItemKind::Consumable, consumable->id };
            return {};
        }

        friend bool operator==(const ItemRef&, const ItemRef&) = default;
    };

    /// <summary>
    /// Display name and content key of an item (empty for invalid references)
    /// </summary>
    [[nodiscard]] inline std::pair<std::string, std::string> DescribeItem(const Content::ContentDatabase& content, ItemRef item) {
        switch (item.kind) {
        case ItemKind::Weapon:
            if (const auto* weapon = content.FindWeapon(item.id)) return { weapon->name, weapon->key };
            break;
        case ItemKind::Armor:
            if (const auto* armor = content.FindArmor(item.id)) return { armor->name, armor->key };
            break;
        case ItemKind::Consumable:
            if (const auto* consumable = content.FindConsumable(item.id)) return { consumable->name, consumable->key };
            break;
        case ItemKind::None:
            break;
        }
        return {};
    }

    /// <summary>
    /// Something lying on the floor: gold (item invalid) or one item stack
    /// </summary>
    struct GroundItem {
        std::uint32_t id{0};
        ItemRef item{};
        std::int32_t quantity{1};       // gold amount for gold
        WorldPoint position{};
        float ageSeconds{0.0f};
        bool fullWarningShown{false};   // "inventory full" once per item, not every step

        [[nodiscard]] bool IsGold() const noexcept { return !item.IsValid(); }
    };

    /// <summary>
    /// Rolls loot tables (design-loot-and-progression.md 4.2): gold with the table's chance and
    /// dice, and an item with its chance, picked by weight. Reproducible for a seed.
    /// </summary>
    class LootRoller {
    public:
        LootRoller() : rng_(std::random_device{}()) {}
        explicit LootRoller(std::uint32_t seed) : rng_(seed) {}

        /// <summary>
        /// What a table drops (gold first, then the item); empty when nothing drops
        /// </summary>
        std::vector<GroundItem> Roll(const Content::LootTableDefinition& table, const Content::ContentDatabase& content) {
            std::vector<GroundItem> drops;
            if (Chance(table.goldChance)) {
                const auto gold = RollDice(table.gold);
                if (gold > 0) drops.push_back(GroundItem{ 0, {}, gold });
            }

            const auto total = table.TotalWeight();
            if (total > 0 && Chance(table.itemChance)) {
                auto pick = std::uniform_int_distribution<std::int32_t>(1, total)(rng_);
                for (const auto& entry : table.items) {
                    pick -= entry.weight;
                    if (pick > 0) continue;
                    if (const auto item = ItemRef::FromKey(content, entry.item); item.IsValid()) {
                        drops.push_back(GroundItem{ 0, item, 1 });
                    }
                    break;
                }
            }
            return drops;
        }

        /// <summary>
        /// count x d(sides) + bonus
        /// </summary>
        std::int32_t RollDice(const LB::Dice& dice) {
            std::int32_t total = dice.bonus;
            if (dice.sides < 1) return total;
            std::uniform_int_distribution<std::int32_t> die(1, dice.sides);
            for (std::int32_t i = 0; i < dice.count; ++i) total += die(rng_);
            return total;
        }

        /// <summary>
        /// A small random offset (tiles) so several drops do not land on one spot
        /// </summary>
        WorldPoint Scatter(float radius) {
            std::uniform_real_distribution<float> angle(0.0f, 2.0f * std::numbers::pi_v<float>);
            std::uniform_real_distribution<float> distance(0.0f, radius);
            const float a = angle(rng_);
            const float d = distance(rng_);
            return { std::cos(a) * d, std::sin(a) * d };
        }

    private:
        bool Chance(float probability) {
            if (probability <= 0.0f) return false;
            if (probability >= 1.0f) return true;
            return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_) < probability;
        }

        std::mt19937 rng_;
    };

} // namespace LB::Engine
