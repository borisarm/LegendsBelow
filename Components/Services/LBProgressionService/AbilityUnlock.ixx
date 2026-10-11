// AbilityUnlock.ixx - Ability unlock configuration per class
export module LB.ProgressionService.AbilityUnlock;

import LB.Core;
import LB.Character;
import std;


export namespace LB::Services::Progression {

    /// <summary>
    /// Ability unlock configuration
    /// Defines when abilities are unlocked per class
    /// </summary>
    export struct AbilityUnlock {
        LB::String abilityName;
        std::int32_t requiredLevel;
        LB::Character::CharacterClass characterClass;
    };

} // namespace LB::Services::Progression
