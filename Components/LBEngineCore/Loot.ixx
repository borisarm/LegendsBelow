// Loot.ixx - Item references, rolled items (rarity and affixes), loot rolls, and items lying on
// the floor
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
        Consumable,   // consumables.json
        Rolled        // a weapon or armour piece with a rarity and affixes (ItemRolls: id is its index)
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
            if (kind < 1 || kind > 4) return {};
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
        case ItemKind::Rolled:   // named by ItemRolls
        case ItemKind::None:
            break;
        }
        return {};
    }

    /// <summary>
    /// What items add while worn or wielded: their affixes (design-loot-and-progression.md 3.2)
    /// </summary>
    struct ItemBonuses {
        std::int32_t damage{0};
        std::int32_t attackBonus{0};
        std::int32_t armorClass{0};
        std::int32_t armor{0};
        std::int32_t spellPower{0};
        float manaRegen{0.0f};

        ItemBonuses& operator+=(const Content::AffixDefinition& affix) noexcept {
            damage += affix.damage;
            attackBonus += affix.attackBonus;
            armorClass += affix.armorClass;
            armor += affix.armor;
            spellPower += affix.spellPower;
            manaRegen += affix.manaRegen;
            return *this;
        }

        ItemBonuses& operator+=(const ItemBonuses& other) noexcept {
            damage += other.damage;
            attackBonus += other.attackBonus;
            armorClass += other.armorClass;
            armor += other.armor;
            spellPower += other.spellPower;
            manaRegen += other.manaRegen;
            return *this;
        }
    };

    /// <summary>
    /// A weapon or armour piece dropped with a rarity and affixes (design-loot-and-progression.md 3)
    /// </summary>
    struct RolledItem {
        ItemRef base;                                         // the weapon or armour piece
        const Content::RarityDefinition* rarity{nullptr};     // into the content, which outlives the run
        std::vector<const Content::AffixDefinition*> affixes;
        std::string name;                                     // "Keen Iron Mace of the Bear"
    };

    /// <summary>
    /// The run's rolled items: a Rolled ItemRef is an index into them. Rolled items never stack
    /// (each has its own id).
    /// </summary>
    class ItemRolls {
    public:
        /// <summary>
        /// Keep a rolled item (named from its affixes and base) and return its reference
        /// </summary>
        ItemRef Add(RolledItem item, const Content::ContentDatabase& content) {
            if (item.name.empty()) item.name = NameOf(item, content);
            items_.push_back(std::move(item));
            return { ItemKind::Rolled, static_cast<Content::ContentId>(items_.size()) };
        }

        [[nodiscard]] const RolledItem* Find(ItemRef item) const noexcept {
            if (item.kind != ItemKind::Rolled || item.id == 0 || item.id > items_.size()) return nullptr;
            return &items_[item.id - 1];
        }

        /// <summary>
        /// The weapon or armour piece under a rolled item (anything else is itself)
        /// </summary>
        [[nodiscard]] ItemRef BaseOf(ItemRef item) const noexcept {
            const auto* rolled = Find(item);
            return rolled ? rolled->base : item;
        }

        [[nodiscard]] ItemBonuses BonusesOf(ItemRef item) const noexcept {
            ItemBonuses bonuses;
            if (const auto* rolled = Find(item)) {
                for (const auto* affix : rolled->affixes) bonuses += *affix;
            }
            return bonuses;
        }

        // Prefixes before the base name, suffixes after it
        static std::string NameOf(const RolledItem& item, const Content::ContentDatabase& content) {
            std::string name;
            for (const auto* affix : item.affixes) {
                if (affix->prefix) name += affix->name + " ";
            }
            name += DescribeItem(content, item.base).first;
            for (const auto* affix : item.affixes) {
                if (!affix->prefix) name += " " + affix->name;
            }
            return name;
        }

    private:
        std::vector<RolledItem> items_;
    };

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
                        drops.push_back(GroundItem{ 0, item, std::max(1, RollDice(entry.quantity)) });
                    }
                    break;
                }
            }
            return drops;
        }

        /// <summary>
        /// The rarity a weapon or armour piece drops at: by the rarities' weights at a depth
        /// (design-loot-and-progression.md 4.2-4.3); null without rarities
        /// </summary>
        const Content::RarityDefinition* RollRarity(const Content::ContentDatabase& content, std::int32_t depth) {
            float total = 0.0f;
            for (const auto& rarity : content.GetRarities()) total += rarity.WeightAt(depth);
            if (total <= 0.0f) return nullptr;
            float pick = std::uniform_real_distribution<float>(0.0f, total)(rng_);
            for (const auto& rarity : content.GetRarities()) {
                pick -= rarity.WeightAt(depth);
                if (pick <= 0.0f) return &rarity;
            }
            return &content.GetRarities().back();
        }

        /// <summary>
        /// Up to `count` different affixes for a weapon (or an armour piece): a prefix, then a
        /// suffix, then whichever kind is left
        /// </summary>
        std::vector<const Content::AffixDefinition*> RollAffixes(const Content::ContentDatabase& content, bool weapon, std::int32_t count) {
            std::vector<const Content::AffixDefinition*> chosen;
            for (std::int32_t i = 0; i < count; ++i) {
                const bool wantPrefix = i % 2 == 0;
                std::vector<const Content::AffixDefinition*> pool;
                for (const auto& affix : content.GetAffixes()) {
                    const bool fits = weapon ? affix.onWeapons : affix.onArmour;
                    if (fits && std::ranges::find(chosen, &affix) == chosen.end()) pool.push_back(&affix);
                }
                std::vector<const Content::AffixDefinition*> preferred;
                for (const auto* affix : pool) {
                    if (affix->prefix == wantPrefix) preferred.push_back(affix);
                }
                const auto& from = preferred.empty() ? pool : preferred;
                if (from.empty()) break;
                chosen.push_back(from[std::uniform_int_distribution<std::size_t>(0, from.size() - 1)(rng_)]);
            }
            return chosen;
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
