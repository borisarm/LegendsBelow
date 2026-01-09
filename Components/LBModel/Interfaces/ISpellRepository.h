#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "../Enums/SpellSchool.h"
#include "../Enums/CharacterClass.h"

namespace LB {
namespace Domain {

    // Forward declarations for spell-related types
    class Spell;
    using SpellId = Id;

    /// <summary>
    /// Repository interface for spell data access
    /// Defines contract for loading spell definitions and metadata
    /// </summary>
    class ISpellRepository {
    public:
        virtual ~ISpellRepository() = default;

        /// <summary>
        /// Get a specific spell by ID
        /// </summary>
        virtual std::optional<Spell> GetSpell(SpellId id) const = 0;

        /// <summary>
        /// Get all spells in a specific school
        /// </summary>
        virtual std::vector<SpellId> GetSpellsBySchool(SpellSchool school) const = 0;

        /// <summary>
        /// Get spells available to a character level
        /// </summary>
        virtual std::vector<SpellId> GetSpellsByLevel(int32_t spellLevel) const = 0;

        /// <summary>
        /// Get spells available to a character class
        /// </summary>
        virtual std::vector<SpellId> GetSpellsByClass(CharacterClass characterClass) const = 0;

        /// <summary>
        /// Load all available spell IDs
        /// </summary>
        virtual std::vector<SpellId> GetAllSpellIds() const = 0;

        /// <summary>
        /// Check if a spell exists
        /// </summary>
        virtual bool SpellExists(SpellId id) const = 0;

        /// <summary>
        /// Get spell school information
        /// </summary>
        virtual std::optional<String> GetSchoolDescription(SpellSchool school) const = 0;
    };

} // namespace Domain
} // namespace LB