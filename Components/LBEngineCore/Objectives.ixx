// Objectives.ixx - Each floor's secondary objective (a named champion, a relic, a captive, or every
// room cleared) and the merchant's bounties (design-dungeon-and-world.md 8.2)
export module LB.Engine:Objectives;

import std;
import LB.Core;
import LB.Content;
import LB.Dungeon;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// What a floor asks of the hero besides the way down
    /// </summary>
    enum class ObjectiveKind : std::uint8_t {
        None,           // a resting place
        SlayChampion,   // a named elite leads one room's enemies
        Retrieve,       // a relic lies in one room
        Rescue,         // a captive waits in one room, freed once the room is clear
        ClearFloor      // every room with enemies cleared
    };

    /// <summary>
    /// One floor's objective. Chosen from the floor's own seed (the same floor, the same
    /// objective), so a save keeps only whether it is done.
    /// </summary>
    struct FloorObjective {
        ObjectiveKind kind{ObjectiveKind::None};
        std::int32_t room{-1};                    // the champion's, relic's, or captive's room
        WorldPoint at{};                          // where the relic or the captive is
        std::string name;                         // the champion's ("Grishnak the Cruel") or the relic's
        std::string model;                        // the relic's
        std::optional<std::uint32_t> champion;    // its enemy, once its room woke
        bool done{false};
        std::int32_t cleared{0};                  // ClearFloor: rooms cleared...
        std::int32_t rooms{0};                    // ...of these

        /// <summary>
        /// The HUD's line: "Slay Grishnak the Cruel", "Find the Lost Tome", "Free a lost scout",
        /// "Clear the floor (3/7)"
        /// </summary>
        [[nodiscard]] std::string Title() const {
            switch (kind) {
            case ObjectiveKind::SlayChampion: return std::format("Slay {}", name);
            case ObjectiveKind::Retrieve: return std::format("Find {}", name);
            case ObjectiveKind::Rescue: return std::format("Free {}", name);
            case ObjectiveKind::ClearFloor: return std::format("Clear the floor ({}/{})", cleared, rooms);
            default: return {};
            }
        }
    };

    /// <summary>
    /// A floor's objective: one of the four kinds, its room the combat room farthest from the
    /// entrance (a floor without one clears its rooms instead); none on a resting place
    /// </summary>
    [[nodiscard]] inline FloorObjective ChooseObjective(const WorldMap& world, std::uint32_t seed, const Content::ObjectiveSettings& settings,
                                                        bool restFloor) {
        FloorObjective objective;
        if (restFloor) return objective;
        std::mt19937 rng(seed ^ 0x0B1EC7u);
        const auto& layout = world.Layout();
        const auto entrance = world.RoomCenter(layout.entranceRoom);
        float farthest = -1.0f;
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            const auto type = layout.rooms[i].type;
            if (type == Dungeon::RoomType::Combat || type == Dungeon::RoomType::Elite || type == Dungeon::RoomType::Exit) ++objective.rooms;
            if (type != Dungeon::RoomType::Combat || static_cast<std::int32_t>(i) == layout.exitRoom) continue;
            const float distance = Distance(world.RoomCenter(static_cast<std::int32_t>(i)), entrance);
            if (distance > farthest) {
                farthest = distance;
                objective.room = static_cast<std::int32_t>(i);
            }
        }
        const auto pick = [&](std::size_t count) { return static_cast<std::size_t>(rng() % std::max<std::size_t>(count, 1)); };
        objective.kind = static_cast<ObjectiveKind>(1 + rng() % 4);
        if (objective.room < 0) objective.kind = ObjectiveKind::ClearFloor;
        switch (objective.kind) {
        case ObjectiveKind::SlayChampion:
            objective.name = std::format("{} {}", settings.championNames[pick(settings.championNames.size())],
                                         settings.championTitles[pick(settings.championTitles.size())]);
            break;
        case ObjectiveKind::Retrieve: {
            const auto& relic = settings.relics[pick(settings.relics.size())];
            objective.name = relic.name;
            objective.model = relic.model;
            break;
        }
        case ObjectiveKind::Rescue:
            objective.name = settings.captiveName;
            break;
        default:
            break;
        }
        if (objective.room >= 0) objective.at = world.RoomCenter(objective.room);
        return objective;
    }

    /// <summary>
    /// A floor's book of lore (design-dungeon-and-world.md 8.3): where it lies and its codex entry
    /// </summary>
    struct LoreBook {
        WorldPoint at{};
        std::string key;                          // lore.json
        bool read{false};                         // in the codex already
    };

    /// <summary>
    /// Text broken into lines of at most `width` characters, at spaces (the codex, a book's banner)
    /// </summary>
    [[nodiscard]] inline std::vector<std::string> WrapText(std::string_view text, std::size_t width) {
        std::vector<std::string> lines;
        std::string line;
        std::size_t start = 0;
        while (start < text.size()) {
            auto end = text.find(' ', start);
            if (end == std::string_view::npos) end = text.size();
            const auto word = text.substr(start, end - start);
            if (!line.empty() && line.size() + 1 + word.size() > width) {
                lines.push_back(line);
                line.clear();
            }
            line += (line.empty() ? "" : " ") + std::string(word);
            start = end + 1;
        }
        if (!line.empty()) lines.push_back(line);
        return lines;
    }

    /// <summary>
    /// "Werewolves", "Witches", "Orc Raiders": a kind's name for many
    /// </summary>
    [[nodiscard]] inline std::string Plural(std::string_view name) {
        std::string many(name);
        if (many.ends_with("f")) return many.substr(0, many.size() - 1) + "ves";
        if (many.ends_with("ch") || many.ends_with("sh") || many.ends_with("s") || many.ends_with("x")) return many + "es";
        return many + "s";
    }

    /// <summary>
    /// The merchant's bounty: so many of one kind slain below, for gold (posted at a resting
    /// place for the tier beneath it; paid the moment it is done)
    /// </summary>
    struct Bounty {
        std::string enemyKey;
        std::string plural;                       // "Werewolves"
        std::int32_t needed{0};
        std::int32_t killed{0};
        std::int32_t gold{0};

        [[nodiscard]] bool Active() const noexcept { return needed > 0; }

        [[nodiscard]] std::string Title() const { return std::format("Bounty: {} {}/{}", plural, killed, needed); }
    };

} // namespace LB::Engine
