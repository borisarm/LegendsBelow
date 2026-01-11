// LB.Dungeon.ValueObjects.ixx - Dungeon domain value objects
export module LB.Dungeon.ValueObjects;

import LB.Core;
import LB.Character;
import std;

export namespace LB::Dungeon {
    using LB::Character::ItemId;

    /// <summary>
    /// Loot drop configuration
    /// </summary>
    struct LootDrop {
        ItemId itemId;
        LB::String itemName;
        float dropChance{1.0f}; // 0.0 to 1.0
        int32_t minQuantity{1};
        int32_t maxQuantity{1};

        [[nodiscard]] bool ShouldDrop() const {
            static thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(gen) <= dropChance;
        }

        [[nodiscard]] int32_t RollQuantity() const {
            if (minQuantity == maxQuantity) return minQuantity;
            static thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_int_distribution<int32_t> dist(minQuantity, maxQuantity);
            return dist(gen);
        }
    };

    /// <summary>
    /// Dungeon generation parameters
    /// </summary>
    struct DungeonGenerationParams {
        int32_t minRooms{5};
        int32_t maxRooms{15};
        int32_t recommendedLevel{1};
        float combatRoomChance{0.6f};
        float treasureRoomChance{0.2f};
        float eliteRoomChance{0.1f};
        bool guaranteeBossRoom{true};
    };

} // namespace LB::Dungeon