#pragma once
#include <vector>
#include <optional>
#include <string>
#include "../Entities/Character.h"
#include "../Enums/CharacterClass.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Repository interface for character data access
    /// Defines contract for loading character templates and player data
    /// </summary>
    class ICharacterRepository {
    public:
        virtual ~ICharacterRepository() = default;

        /// <summary>
        /// Get a character template for a specific class (for character creation)
        /// </summary>
        virtual std::optional<Character> GetCharacterTemplate(CharacterClass characterClass) const = 0;

        /// <summary>
        /// Load all available character class templates
        /// </summary>
        virtual std::vector<CharacterClass> GetAvailableClasses() const = 0;

        /// <summary>
        /// Get starting attributes for a character class
        /// </summary>
        virtual std::optional<Attributes> GetStartingAttributes(CharacterClass characterClass) const = 0;

        /// <summary>
        /// Get class-specific information
        /// </summary>
        virtual std::optional<String> GetClassDescription(CharacterClass characterClass) const = 0;
        virtual std::optional<String> GetClassName(CharacterClass characterClass) const = 0;
    };

} // namespace Domain
} // namespace LB