#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "../Enums/CharacterClass.h"
#include "../Enums/SpellSchool.h"
#include "../Enums/WeaponType.h"
#include "../Enums/ArmorCategory.h"
#include "../Enums/EquipmentSlot.h"
#include "../Enums/DamageType.h"

namespace LB {
namespace Domain {

    // Forward declarations for proper dependency ordering
    class Spell;
    class Weapon;  
    class Armor;
    using SpellId = Id;
    using WeaponId = Id;
    using ArmorId = Id;

    /// <summary>
    /// Generic repository interface template for game entities
    /// Provides common CRUD and filtering operations
    /// </summary>
    template<typename TEntity, typename TEntityId, typename TPrimaryCategory>
    class IEntityRepository {
    public:
        virtual ~IEntityRepository() = default;

        /// <summary>
        /// Get a specific entity by ID
        /// </summary>
        virtual std::optional<TEntity> GetEntity(TEntityId id) const = 0;

        /// <summary>
        /// Get all entities of a specific primary category
        /// </summary>
        virtual std::vector<TEntityId> GetEntitiesByCategory(TPrimaryCategory category) const = 0;

        /// <summary>
        /// Get entities available to a character class
        /// </summary>
        virtual std::vector<TEntityId> GetEntitiesByClass(CharacterClass characterClass) const = 0;

        /// <summary>
        /// Get entities within a level/tier range
        /// </summary>
        virtual std::vector<TEntityId> GetEntitiesByLevelRange(int32_t minLevel, int32_t maxLevel) const = 0;

        /// <summary>
        /// Get all available entity IDs
        /// </summary>
        virtual std::vector<TEntityId> GetAllEntityIds() const = 0;

        /// <summary>
        /// Check if an entity exists
        /// </summary>
        virtual bool EntityExists(TEntityId id) const = 0;

        /// <summary>
        /// Get category description/metadata
        /// </summary>
        virtual std::optional<String> GetCategoryDescription(TPrimaryCategory category) const = 0;
    };

} // namespace Domain
} // namespace LB