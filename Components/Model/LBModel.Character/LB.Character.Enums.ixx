// LB.Character.Enums.ixx - Character domain enumerations
export module LB.Character.Enums;

import std;


export namespace LB::Character {

    /// <summary>
    /// Character classes defining gameplay archetypes
    /// Based on classic RPG design principles
    /// </summary>
    enum class CharacterClass : std::uint32_t {
        None = 0,
        Warrior = 1,    // Melee combat specialist
        Rogue = 2,      // Stealth and precision
        Mage = 3,       // Arcane spellcaster
        Cleric = 4,     // Divine spellcaster and healer
        Necromancer = 5 // Shadow caster: curses and stolen life
    };

    /// <summary>
    /// Convert CharacterClass to string
    /// </summary>
    export constexpr const char* ToString(CharacterClass characterClass) noexcept {
        switch (characterClass) {
            case CharacterClass::None: return "None";
            case CharacterClass::Warrior: return "Warrior";
            case CharacterClass::Rogue: return "Rogue";
            case CharacterClass::Mage: return "Mage";
            case CharacterClass::Cleric: return "Cleric";
            case CharacterClass::Necromancer: return "Necromancer";
            default: return "Unknown";
        }
    }

} // namespace LB::Character