#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "../Enums/WeaponType.h"
#include "../Enums/DamageType.h"
#include "../Enums/CharacterClass.h"

namespace LB {
namespace Domain {

    // Forward declarations for weapon-related types
    class Weapon;
    enum class WeaponId : Id;

    /// <summary>
    /// Repository interface for weapon data access
    /// Defines contract for loading weapon definitions and metadata
    /// </summary>
    class IWeaponRepository {
    public:
        virtual ~IWeaponRepository() = default;

        /// <summary>
        /// Get a specific weapon by ID
        /// </summary>
        virtual std::optional<Weapon> GetWeapon(WeaponId id) const = 0;

        /// <summary>
        /// Get all weapons of a specific type
        /// </summary>
        virtual std::vector<WeaponId> GetWeaponsByType(WeaponType type) const = 0;

        /// <summary>
        /// Get weapons that deal a specific damage type
        /// </summary>
        virtual std::vector<WeaponId> GetWeaponsByDamageType(DamageType damageType) const = 0;

        /// <summary>
        /// Get weapons available to a character class
        /// </summary>
        virtual std::vector<WeaponId> GetWeaponsByClass(CharacterClass characterClass) const = 0;

        /// <summary>
        /// Get weapons within a level/tier range
        /// </summary>
        virtual std::vector<WeaponId> GetWeaponsByTier(int32_t minTier, int32_t maxTier) const = 0;

        /// <summary>
        /// Load all available weapon IDs
        /// </summary>
        virtual std::vector<WeaponId> GetAllWeaponIds() const = 0;

        /// <summary>
        /// Check if a weapon exists
        /// </summary>
        virtual bool WeaponExists(WeaponId id) const = 0;

        /// <summary>
        /// Get weapon type information
        /// </summary>
        virtual std::optional<String> GetWeaponTypeDescription(WeaponType type) const = 0;
    };

} // namespace Domain
} // namespace LB