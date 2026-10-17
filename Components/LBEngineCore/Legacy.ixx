// Legacy.ixx - What carries over from one game to the next: the records of all runs, and the boons
// they unlock for new heroes (design-loot-and-progression.md 7.1, account-level unlocks)
export module LB.Engine:Legacy;

import std;

export namespace LB::Engine {

    /// <summary>
    /// A boon for every new hero, unlocked by the records of past runs
    /// </summary>
    enum class Boon : std::uint8_t {
        Provisions,   // an extra healing potion
        OldCoin,      // gold to start with
        Heirloom,     // the class's weapon, rolled uncommon
        Veteran,      // more experience
        Laurels       // the heirloom rolled rare
    };

    struct BoonInfo {
        Boon boon;
        std::string_view name;
        std::string_view gives;          // what a new hero gets
        std::string_view requirement;    // what unlocks it
    };

    inline constexpr std::array<BoonInfo, 5> Boons{ {
        { Boon::Provisions, "Provisions", "start with an extra healing potion", "reach depth 3" },
        { Boon::OldCoin, "Old Coin", "start with 30 gold", "defeat a boss" },
        { Boon::Heirloom, "Heirloom", "start with an uncommon weapon of your class", "reach depth 7" },
        { Boon::Veteran, "Veteran", "gain a tenth more experience", "defeat three bosses" },
        { Boon::Laurels, "Laurels", "start with a rare weapon of your class", "win a run" },
    } };

    /// <summary>
    /// The records of every run (the profile), and which boons they unlock
    /// </summary>
    class Legacy {
    public:
        static constexpr std::int32_t ProvisionsDepth = 3;
        static constexpr std::int32_t HeirloomDepth = 7;
        static constexpr std::int32_t OldCoinBosses = 1;
        static constexpr std::int32_t VeteranBosses = 3;
        static constexpr std::int32_t OldCoinGold = 30;
        static constexpr float VeteranExperience = 1.1f;

        std::int32_t deepest{0};
        std::int32_t bosses{0};
        std::int32_t runs{0};
        std::int32_t kills{0};
        std::int32_t victories{0};       // runs won (the final boss fallen)
        std::int32_t fastestVictory{0};  // seconds of game time of the quickest (0: none yet)
        std::set<std::string> lore;   // codex entries read (lore.json keys)

        [[nodiscard]] bool Has(Boon boon) const noexcept {
            switch (boon) {
            case Boon::Provisions: return deepest >= ProvisionsDepth;
            case Boon::OldCoin: return bosses >= OldCoinBosses;
            case Boon::Heirloom: return deepest >= HeirloomDepth;
            case Boon::Veteran: return bosses >= VeteranBosses;
            case Boon::Laurels: return victories >= 1;
            }
            return false;
        }

        /// <summary>
        /// The boons unlocked now and not before (from an earlier copy of the records)
        /// </summary>
        [[nodiscard]] std::vector<const BoonInfo*> NewSince(const Legacy& before) const {
            std::vector<const BoonInfo*> unlocked;
            for (const auto& info : Boons) {
                if (Has(info.boon) && !before.Has(info.boon)) unlocked.push_back(&info);
            }
            return unlocked;
        }

        /// <summary>
        /// As the profile keeps it ("legacy.deepest", ...), and back (unreadable values: 0)
        /// </summary>
        [[nodiscard]] std::map<std::string, std::string> ToProfile() const {
            return { { "legacy.deepest", std::to_string(deepest) }, { "legacy.bosses", std::to_string(bosses) },
                     { "legacy.runs", std::to_string(runs) }, { "legacy.kills", std::to_string(kills) },
                     { "legacy.victories", std::to_string(victories) }, { "legacy.fastestVictory", std::to_string(fastestVictory) },
                     { "legacy.lore", JoinLore() } };
        }

        // "a,b,c"
        [[nodiscard]] std::string JoinLore() const {
            std::string text;
            for (const auto& key : lore) text += (text.empty() ? "" : ",") + key;
            return text;
        }

        static Legacy FromProfile(const std::map<std::string, std::string>& profile) {
            const auto read = [&](const char* key) {
                const auto it = profile.find(key);
                std::int32_t value = 0;
                if (it != profile.end()) std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
                return std::max(0, value);
            };
            Legacy legacy;
            legacy.deepest = read("legacy.deepest");
            legacy.bosses = read("legacy.bosses");
            legacy.runs = read("legacy.runs");
            legacy.kills = read("legacy.kills");
            legacy.victories = read("legacy.victories");
            legacy.fastestVictory = read("legacy.fastestVictory");
            if (const auto it = profile.find("legacy.lore"); it != profile.end()) {
                std::string key;
                for (const char c : it->second + ',') {
                    if (c != ',') { key += c; continue; }
                    if (!key.empty()) legacy.lore.insert(key);
                    key.clear();
                }
            }
            return legacy;
        }
    };

} // namespace LB::Engine
