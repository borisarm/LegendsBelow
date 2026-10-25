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
        std::int32_t hitPoints{0};          // gained, over all the levels
        std::int32_t mana{0};
        std::int32_t attributePoints{0};
    };

    /// <summary>
    /// The hero took a path at SpecializationLevel
    /// </summary>
    struct PathChosenEvent {
        std::string key;
        std::string name;
    };

    /// <summary>
    /// What a dungeon's interactive element is (design-dungeon-and-world.md 6.3)
    /// </summary>
    enum class FeatureKind : std::uint8_t {
        Lever,       // pulled: its gate opens
        Waystone,    // awakened, or travelled by
        Portal       // opened (the Portal spell), or gone back through
    };

    /// <summary>
    /// An interactive element was activated (design-dungeon-and-world.md 6.3: OnActivated)
    /// </summary>
    struct FeatureActivatedEvent {
        FeatureKind kind{FeatureKind::Lever};
        WorldPoint at{};
    };

    /// <summary>
    /// The final boss fell (design-dungeon-and-world.md 7.4): the run is won (the victory, the
    /// legacy, the codex's last page react)
    /// </summary>
    struct RunWonEvent {
        WorldPoint at{};   // where it fell
    };

} // namespace LB::Engine
