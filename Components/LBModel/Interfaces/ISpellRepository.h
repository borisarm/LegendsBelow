#pragma once
#include <vector>
#include <optional>
#include "../Common/Types.h"
#include "../Entities/Spell.h"
#include "../Enums/SpellSchool.h"
#include "../Enums/CharacterClass.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Spell repository interface for spell data access
    /// Clean interface without template inheritance complications
    /// </summary>
    class ISpellRepository {
    public:
        virtual ~ISpellRepository() = default;

        // Core spell operations
        virtual std::optional<Spell> GetSpell(SpellId id) const = 0;
        virtual std::vector<SpellId> GetSpellsBySchool(SpellSchool school) const = 0;
        virtual std::vector<SpellId> GetSpellsByLevel(int32_t spellLevel) const = 0;
        virtual std::vector<SpellId> GetSpellsByClass(CharacterClass characterClass) const = 0;
        virtual std::vector<SpellId> GetAllSpellIds() const = 0;
        virtual bool SpellExists(SpellId id) const = 0;
        virtual std::optional<String> GetSchoolDescription(SpellSchool school) const = 0;
    };

} // namespace Domain
} // namespace LB