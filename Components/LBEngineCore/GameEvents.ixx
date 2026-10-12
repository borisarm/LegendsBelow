// GameEvents.ixx - What happens in a run, published on the run's EventBus for the parts that react to it
export module LB.Engine:GameEvents;

import std;
import :World;

export namespace LB::Engine {

    /// <summary>
    /// An enemy died: its experience and its loot table (progression reacts)
    /// </summary>
    struct EnemyKilledEvent {
        std::uint32_t enemyId{0};
        std::string enemyKey;
        WorldPoint at{};
        std::int32_t experience{0};
        std::string lootTable;
    };

    /// <summary>
    /// The hero reached a level (the fanfare, the abilities it opens, a path to choose)
    /// </summary>
    struct LevelGainedEvent {
        std::int32_t previous{1};   // the level before (several may be gained at once)
        std::int32_t level{1};
    };

    /// <summary>
    /// The hero took a path at SpecializationLevel
    /// </summary>
    struct PathChosenEvent {
        std::string key;
        std::string name;
    };

} // namespace LB::Engine
