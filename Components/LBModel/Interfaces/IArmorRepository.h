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
    /// Repository interface for armor data access
    /// Defines contract for loading armor definitions and metadata
    /// </summary>
    class IArmorRepository {
    public:
        virtual ~IArmorRepository() = default;

        /// <summary>
        /// Get a specific armor piece by ID
        /// </summary>
        virtual std::optional<Armor> GetArmor(ArmorId id) const = 0;

        /// <summary>
        /// Get all armor of a specific category
        /// </summary>
        virtual std::vector<ArmorId> GetArmorByCategory(ArmorCategory category) const = 0;

        /// <summary>
        /// Get armor for a specific equipment slot
        /// </summary>
        virtual std::vector<ArmorId> GetArmorBySlot(EquipmentSlot slot) const = 0;

        /// <summary>
        /// Get armor available to a character class
        /// </summary>
        virtual std::vector<ArmorId> GetArmorByClass(CharacterClass characterClass) const = 0;

        /// <summary>
        /// Get armor within a level range
        /// </summary>
        virtual std::vector<ArmorId> GetArmorByLevel(int32_t minLevel, int32_t maxLevel) const = 0;

        /// <summary>
        /// Load all available armor IDs
        /// </summary>
        virtual std::vector<ArmorId> GetAllArmorIds() const = 0;

        /// <summary>
        /// Check if an armor piece exists
        /// </summary>
        virtual bool ArmorExists(ArmorId id) const = 0;

        /// <summary>
        /// Get armor category information
        /// </summary>
        virtual std::optional<String> GetCategoryDescription(ArmorCategory category) const = 0;
    };

} // namespace Domain
} // namespace LB