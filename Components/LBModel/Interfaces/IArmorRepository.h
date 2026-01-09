#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "../Entities/Armor.h"
#include "../Enums/ArmorCategory.h"
#include "../Enums/EquipmentSlot.h"
#include "../Enums/CharacterClass.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Armor repository interface for armor data access
    /// Clean interface without template inheritance complications
    /// </summary>
    class IArmorRepository {
    public:
        virtual ~IArmorRepository() = default;

        // Core armor operations
        virtual std::optional<Armor> GetArmor(ArmorId id) const = 0;
        virtual std::vector<ArmorId> GetArmorByCategory(ArmorCategory category) const = 0;
        virtual std::vector<ArmorId> GetArmorBySlot(EquipmentSlot slot) const = 0;
        virtual std::vector<ArmorId> GetArmorByClass(CharacterClass characterClass) const = 0;
        virtual std::vector<ArmorId> GetArmorByLevel(int32_t minLevel, int32_t maxLevel) const = 0;
        virtual std::vector<ArmorId> GetAllArmorIds() const = 0;
        virtual bool ArmorExists(ArmorId id) const = 0;
        virtual std::optional<String> GetCategoryDescription(ArmorCategory category) const = 0;
    };

} // namespace Domain
} // namespace LB
} // namespace LB

} // namespace Domain
} // namespace LB