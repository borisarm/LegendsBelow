// LB.ContentRepositories.ixx - Repository facades backed by loaded content definitions
//
// ContentSpellRepository satisfies LB::Repositories::SpellRepositoryFacade by duck typing
// (see microsoft-proxy-implementation.md); it shares the immutable ContentDatabase.
export module LB.ContentRepositories;

import LB.Core;
import LB.Character;
import LB.Combat;
import LB.Content;
import LB.Repositories;
import std;


export namespace LB::Content {

    /// <summary>
    /// Spell repository over loaded spell definitions. Copies share the same database.
    /// </summary>
    class ContentSpellRepository {
    public:
        explicit ContentSpellRepository(std::shared_ptr<const ContentDatabase> content)
            : content_(std::move(content)) {}

        std::optional<LB::Character::Spell> GetSpell(LB::Character::SpellId id) const {
            const auto* definition = content_->FindSpell(id);
            if (!definition) return std::nullopt;
            return LB::Character::Spell{ definition->id, definition->name, definition->manaCost, definition->baseDamage };
        }

        std::vector<LB::Character::SpellId> GetAllSpellIds() const {
            return Select([](const SpellDefinition&) { return true; });
        }

        bool SpellExists(LB::Character::SpellId id) const {
            return content_->FindSpell(id) != nullptr;
        }

        std::vector<LB::Character::SpellId> GetSpellsBySchool(SpellSchool school) const {
            return Select([school](const SpellDefinition& s) { return s.school == school; });
        }

        // School descriptions from design-spells-system.md, 2.1
        std::optional<LB::String> GetSchoolDescription(SpellSchool school) const {
            switch (school) {
                case SpellSchool::Evocation: return LB::String("Raw damage: fire, lightning, force.");
                case SpellSchool::Frost: return LB::String("Slows, freezes, and control.");
                case SpellSchool::Holy: return LB::String("Healing, protection, radiant damage.");
                case SpellSchool::Shadow: return LB::String("Curses, drains, necrotic damage.");
                case SpellSchool::Utility: return LB::String("Teleport, detection, and vision.");
                case SpellSchool::Generic: return LB::String("Buffs and wards any class can learn.");
                default: return std::nullopt;
            }
        }

        std::vector<LB::Character::SpellId> GetSpellsByClass(CharacterClass characterClass) const {
            return Select([characterClass](const SpellDefinition& s) { return s.CanBeCastBy(characterClass); });
        }

        std::vector<LB::Character::SpellId> GetSpellsByLevel(std::int32_t spellLevel) const {
            return Select([spellLevel](const SpellDefinition& s) { return s.level == spellLevel; });
        }

        std::vector<LB::Character::SpellId> GetSpellsByCategory(SpellCategory category) const {
            return Select([category](const SpellDefinition& s) { return s.HasCategory(category); });
        }

        std::vector<LB::Character::SpellId> GetSpellsByManaCost(std::int32_t minCost, std::int32_t maxCost) const {
            return Select([minCost, maxCost](const SpellDefinition& s) { return s.manaCost >= minCost && s.manaCost <= maxCost; });
        }

        std::vector<LB::Character::SpellId> GetSpellsByCastTime(float maxCastTime) const {
            return Select([maxCastTime](const SpellDefinition& s) { return s.castTimeSeconds <= maxCastTime; });
        }

    private:
        template<typename Predicate>
        std::vector<LB::Character::SpellId> Select(Predicate predicate) const {
            std::vector<LB::Character::SpellId> ids;
            for (const auto& spell : content_->GetSpells()) {
                if (predicate(spell)) ids.push_back(spell.id);
            }
            return ids;
        }

        std::shared_ptr<const ContentDatabase> content_;
    };

    /// <summary>
    /// Type-erased spell repository over loaded content
    /// </summary>
    inline LB::Repositories::SpellRepository MakeContentSpellRepository(std::shared_ptr<const ContentDatabase> content) {
        return LB::Repositories::Factory::MakeSpellRepository(ContentSpellRepository(std::move(content)));
    }

} // namespace LB::Content
