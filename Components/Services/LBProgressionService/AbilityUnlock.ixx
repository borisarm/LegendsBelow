// AbilityUnlock.ixx - An ability a class opens at a level (from its spells in spells.json)
export module LB.ProgressionService.AbilityUnlock;

import LB.Core;
import LB.Character;
import std;


export namespace LB::Services::Progression {

    /// <summary>
    /// Ability unlock configuration
    /// Defines when abilities are unlocked per class
    /// </summary>
    struct AbilityUnlock {
        LB::String abilityName;
        std::int32_t requiredLevel;
        LB::Character::CharacterClass characterClass;
    };

} // namespace LB::Services::Progression
