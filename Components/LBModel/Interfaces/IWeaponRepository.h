#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "../Entities/Weapon.h"
#include "../Enums/WeaponType.h"
#include "../Enums/DamageType.h"
#include "../Enums/CharacterClass.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Weapon repository interface for weapon data access
    /// Clean interface without template inheritance complications
    /// </summary>
    class IWeaponRepository {
    public:
        virtual ~IWeaponRepository() = default;

        // Core weapon operations
        virtual std::optional<Weapon> GetWeapon(WeaponId id) const = 0;
        virtual std::vector<WeaponId> GetWeaponsByType(WeaponType type) const = 0;
        virtual std::vector<WeaponId> GetWeaponsByDamageType(DamageType damageType) const = 0;
        virtual std::vector<WeaponId> GetWeaponsByClass(CharacterClass characterClass) const = 0;
        virtual std::vector<WeaponId> GetWeaponsByTier(int32_t minTier, int32_t maxTier) const = 0;
        virtual std::vector<WeaponId> GetAllWeaponIds() const = 0;
        virtual bool WeaponExists(WeaponId id) const = 0;
        virtual std::optional<String> GetWeaponTypeDescription(WeaponType type) const = 0;
    };

} // namespace Domain
} // namespace LB