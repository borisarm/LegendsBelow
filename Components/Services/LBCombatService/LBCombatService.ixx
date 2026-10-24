// LB.CombatService.ixx - Combat mechanics service using C++20 modules
export module LB.CombatService;

// Import only required domains
import LB.Core;
import LB.Character;
import LB.Combat;
import std;


/// <summary>
/// Combat service for LegendsBelow
/// Provides business logic for combat resolution, damage calculation, and spell effects
/// Follows D&D-inspired combat mechanics with modern game design
/// </summary>
export namespace LB::Services::Combat {
    // Re-export domain types for service convenience
    using LB::Combat::DamageType;
    using LB::Character::CharacterClass;
    using LB::Character::CharacterId;
    using LB::Attributes;

    /// <summary>
    /// Result of an attack roll
    /// Contains hit information and whether it was a critical hit
    /// </summary>
    struct AttackRollResult {
        bool hit{false};
        bool criticalHit{false};
        bool criticalMiss{false};
        
        std::int32_t attackRoll{0};
        std::int32_t targetArmorClass{0};
        std::int32_t attackBonus{0};
    };

    /// <summary>
    /// Result of damage calculation
    /// Contains actual damage dealt and type information
    /// </summary>
    struct DamageResult {
        std::int32_t totalDamage{0};
        std::int32_t baseDamage{0};
        std::int32_t bonusDamage{0};
        std::int32_t mitigatedDamage{0};
        DamageType damageType{DamageType::None};
        bool wasCritical{false};
        float damageReduction{0.0f};
    };

    /// <summary>
    /// Result of a complete combat action
    /// Combines attack and damage information
    /// </summary>
    struct CombatActionResult {
        bool success{false};
        AttackRollResult attackResult{};
        DamageResult damageResult{};
        std::int32_t targetRemainingHitPoints{0};
        bool targetDefeated{false};
    };

    /// <summary>
    /// Spell casting result
    /// Contains spell effect and mana consumption information
    /// </summary>
    struct SpellCastResult {
        bool success{false};
        bool insufficientMana{false};
        bool invalidTarget{false};
        std::int32_t manaCost{0};
        DamageResult damageResult{};
        std::int32_t healingAmount{0};
        std::vector<CharacterId> affectedTargets{};
    };

    /// <summary>
    /// Attack parameters for combat actions
    /// </summary>
    struct AttackParams {
        CharacterId attackerId;
        CharacterId targetId;
        DamageType damageType{DamageType::Slashing};
        std::int32_t baseDamage{0};
        std::int32_t bonusDamage{0};
        bool advantageOnRoll{false};
        bool disadvantageOnRoll{false};
    };

    /// <summary>
    /// What an attacker brings to a weapon attack (design-combat-system.md 4.1-4.3): the attack
    /// roll bonus and the weapon's damage profile
    /// </summary>
    struct WeaponAttackProfile {
        std::int32_t attackBonus{0};
        LB::Dice damageDice{1, 4, 0};
        std::int32_t damageBonus{0};                    // attribute and flat bonuses added to the roll
        DamageType damageType{DamageType::Slashing};
        std::int32_t critRangeMin{20};                  // natural rolls from this up to 20 crit (when they hit)
        float critMultiplier{1.5f};
        LB::Dice extraDice{0, 6, 0};                    // rolled on a hit and added (a rogue's sneak attack); count 0: none
    };

    /// <summary>
    /// How a target defends (design-combat-system.md 4.1 and 5.2)
    /// </summary>
    struct DefenseProfile {
        std::int32_t armorClass{10};
        std::int32_t physicalArmor{0};                  // flat reduction of physical damage
        std::map<DamageType, float> resistances;   // fraction removed; negative values are vulnerabilities
    };

    /// <summary>
    /// Outcome of a weapon attack
    /// </summary>
    struct WeaponAttackResult {
        bool hit{false};
        bool critical{false};
        std::int32_t naturalRoll{0};                    // the d20
        std::int32_t totalRoll{0};                      // d20 + attack bonus
        std::int32_t rawDamage{0};                      // dice + bonus (x crit multiplier), before defenses
        std::int32_t finalDamage{0};                    // after armour and resistance; 0 on a miss
        std::int32_t extraDamage{0};                    // of the raw damage, what the extra dice rolled
    };

    /// <summary>
    /// Core combat service
    /// Handles all combat-related calculations and resolutions
    /// </summary>
    class CombatService {
    public:
        /// <summary>
        /// Create a service with nondeterministic dice
        /// </summary>
        CombatService() : rng_(std::random_device{}()) {}

        /// <summary>
        /// Create a service whose dice are reproducible for a given seed (tests, replays)
        /// </summary>
        explicit CombatService(std::uint32_t seed) : rng_(seed) {}

        /// <summary>
        /// Perform a melee attack from attacker to target
        /// Uses D&D-style attack roll vs armor class
        /// </summary>
        AttackRollResult RollAttack(
            std::int32_t attackBonus,
            std::int32_t targetArmorClass,
            bool advantage = false,
            bool disadvantage = false) {

            AttackRollResult result{};
            result.attackBonus = attackBonus;
            result.targetArmorClass = targetArmorClass;

            // Roll d20 (1-20)
            std::int32_t roll = rollD20(advantage, disadvantage);
            result.attackRoll = roll;

            // Natural 20 is always a critical hit
            if (roll == 20) {
                result.criticalHit = true;
                result.hit = true;
                return result;
            }

            // Natural 1 is always a critical miss
            if (roll == 1) {
                result.criticalMiss = true;
                result.hit = false;
                return result;
            }

            // Normal hit check
            std::int32_t totalAttackRoll = roll + attackBonus;
            result.hit = totalAttackRoll >= targetArmorClass;

            return result;
        }

        /// <summary>
        /// Roll dice: count x d(sides) + bonus
        /// </summary>
        std::int32_t RollDice(const LB::Dice& dice) {
            std::uniform_int_distribution<std::int32_t> die(1, std::max(1, dice.sides));
            std::int32_t total = dice.bonus;
            for (std::int32_t i = 0; i < dice.count; ++i) {
                total += die(rng_);
            }
            return total;
        }

        /// <summary>
        /// Apply a target's defenses to raw damage (design-combat-system.md 5.2): physical damage
        /// loses the flat armour first, then the type's resistance takes its fraction (negative
        /// resistance adds damage). Damage that gets through deals at least 1, unless the target
        /// is immune (resistance 1) or the armour absorbs it all.
        /// </summary>
        static std::int32_t ApplyDefenses(std::int32_t rawDamage, DamageType type, const DefenseProfile& target) {
            if (rawDamage <= 0) return 0;

            const bool physical = type == DamageType::Slashing || type == DamageType::Piercing || type == DamageType::Bludgeoning;
            const std::int32_t afterArmor = physical ? std::max(0, rawDamage - std::max(0, target.physicalArmor)) : rawDamage;
            if (afterArmor == 0) return 0;

            const auto it = target.resistances.find(type);
            const float resist = std::clamp(it != target.resistances.end() ? it->second : 0.0f, -1.0f, 1.0f);
            if (resist >= 1.0f) return 0;

            const auto scaled = static_cast<std::int32_t>(std::lround(static_cast<float>(afterArmor) * (1.0f - resist)));
            return std::max(1, scaled);
        }

        /// <summary>
        /// A spell that reached its target (design-spells-system.md 5.1): no attack roll (it had to
        /// reach the target), its raw damage (base + coefficient x spell power, worked out when
        /// cast) through the target's defenses
        /// </summary>
        static WeaponAttackResult ResolveSpellHit(std::int32_t rawDamage, DamageType type, const DefenseProfile& target) {
            WeaponAttackResult result{};
            result.hit = true;
            result.rawDamage = std::max(0, rawDamage);
            result.finalDamage = ApplyDefenses(result.rawDamage, type, target);
            return result;
        }

        /// <summary>
        /// Resolve one weapon attack (design-combat-system.md 4.1-4.3): d20 + bonus against AC
        /// (natural 1 always misses, natural 20 always hits), crits on a hit within the crit
        /// range multiply the damage roll, then the target's defenses apply.
        /// </summary>
        WeaponAttackResult ResolveWeaponAttack(const WeaponAttackProfile& attack, const DefenseProfile& target) {
            WeaponAttackResult result{};
            result.naturalRoll = rollD20();
            result.totalRoll = result.naturalRoll + attack.attackBonus;
            result.hit = result.naturalRoll == 20 || (result.naturalRoll != 1 && result.totalRoll >= target.armorClass);
            if (!result.hit) return result;

            result.critical = result.naturalRoll >= std::clamp(attack.critRangeMin, 2, 20);
            std::int32_t damage = std::max(0, RollDice(attack.damageDice) + attack.damageBonus);
            if (attack.extraDice.count > 0) {
                result.extraDamage = RollDice(attack.extraDice);
                damage += result.extraDamage;
            }
            if (result.critical) {
                damage = static_cast<std::int32_t>(std::lround(static_cast<float>(damage) * std::max(1.0f, attack.critMultiplier)));
            }
            result.rawDamage = damage;
            result.finalDamage = ApplyDefenses(damage, attack.damageType, target);
            return result;
        }

        /// <summary>
        /// Calculate damage with type-specific modifiers
        /// </summary>
        DamageResult CalculateDamage(
            std::int32_t baseDamage,
            std::int32_t bonusDamage,
            DamageType damageType,
            bool isCritical,
            float damageReduction = 0.0f) const {

            DamageResult result{};
            result.baseDamage = baseDamage;
            result.bonusDamage = bonusDamage;
            result.damageType = damageType;
            result.wasCritical = isCritical;
            result.damageReduction = std::clamp(damageReduction, 0.0f, 0.99f);

            // Critical hits double the base damage dice
            std::int32_t effectiveBaseDamage = isCritical ? (baseDamage * 2) : baseDamage;

            // Calculate total before mitigation
            std::int32_t totalBeforeMitigation = effectiveBaseDamage + bonusDamage;

            // Apply damage reduction
            result.mitigatedDamage = static_cast<std::int32_t>(totalBeforeMitigation * result.damageReduction);
            result.totalDamage = std::max(0, totalBeforeMitigation - result.mitigatedDamage);

            return result;
        }

        /// <summary>
        /// Calculate damage reduction based on armor and damage type
        /// Physical damage types are more affected by armor
        /// </summary>
        float CalculateDamageReduction(
            std::int32_t armorClass,
            DamageType damageType) const {

            // Base armor effectiveness (10 AC = 0%, 20 AC = 50%, 30 AC = 75%)
            float baseReduction = std::min(0.75f, (armorClass - 10.0f) / 40.0f);

            // Physical damage types are more affected by armor
            switch (damageType) {
                case DamageType::Slashing:
                case DamageType::Bludgeoning:
                    return baseReduction;

                case DamageType::Piercing:
                    // Piercing ignores some armor
                    return baseReduction * 0.75f;

                case DamageType::Fire:
                case DamageType::Cold:
                case DamageType::Lightning:
                case DamageType::Arcane:
                    // Elemental damage ignores most physical armor
                    return baseReduction * 0.25f;

                case DamageType::Holy:
                case DamageType::Shadow:
                    // Spiritual damage ignores physical armor entirely
                    return 0.0f;

                default:
                    return 0.0f;
            }
        }

        /// <summary>
        /// Calculate weapon damage based on strength/dexterity
        /// </summary>
        std::int32_t CalculateWeaponDamage(
            std::int32_t baseDamage,
            const Attributes& attributes,
            bool useStrength = true) const {

            std::int32_t modifier = useStrength ?
                attributes.GetStrengthModifier() :
                attributes.GetDexterityModifier();

            return std::max(1, baseDamage + modifier);
        }

        /// <summary>
        /// Calculate spell damage with intelligence/wisdom bonus
        /// </summary>
        std::int32_t CalculateSpellDamage(
            std::int32_t baseDamage,
            const Attributes& attributes,
            CharacterClass casterClass) const {

            std::int32_t spellBonus = 0;

            switch (casterClass) {
                case CharacterClass::Mage:
                case CharacterClass::Necromancer:
                    spellBonus = attributes.GetIntelligenceModifier();
                    break;
                case CharacterClass::Cleric:
                    spellBonus = attributes.GetWisdomModifier();
                    break;
                case CharacterClass::Rogue:
                    spellBonus = attributes.GetIntelligenceModifier() / 2;
                    break;
                default:
                    spellBonus = 0;
                    break;
            }

            return std::max(1, baseDamage + spellBonus);
        }

        /// <summary>
        /// Check if a spell hits the target (for attack spells)
        /// </summary>
        bool SpellAttackHits(
            std::int32_t spellAttackBonus,
            std::int32_t targetArmorClass) {

            std::int32_t roll = rollD20();
            return (roll + spellAttackBonus) >= targetArmorClass;
        }

        /// <summary>
        /// Calculate saving throw result
        /// Used for spells and abilities that allow saves
        /// </summary>
        bool MakesSavingThrow(
            std::int32_t saveDC,
            std::int32_t saveBonus,
            bool advantage = false) {

            std::int32_t roll = rollD20(advantage, false);
            return (roll + saveBonus) >= saveDC;
        }

        /// <summary>
        /// Calculate spell save DC for a caster
        /// </summary>
        std::int32_t CalculateSpellSaveDC(
            std::int32_t casterLevel,
            const Attributes& attributes,
            CharacterClass casterClass) const {

            std::int32_t baseDC = 8 + (casterLevel / 2);
            std::int32_t abilityBonus = 0;

            switch (casterClass) {
                case CharacterClass::Mage:
                case CharacterClass::Necromancer:
                    abilityBonus = attributes.GetIntelligenceModifier();
                    break;
                case CharacterClass::Cleric:
                    abilityBonus = attributes.GetWisdomModifier();
                    break;
                default:
                    abilityBonus = 0;
                    break;
            }

            return baseDC + abilityBonus;
        }

        /// <summary>
        /// Execute a full combat action with all calculations
        /// This is a convenience method that combines attack roll and damage
        /// </summary>
        CombatActionResult ExecuteAttack([[maybe_unused]] const AttackParams& params) const {
            CombatActionResult result{};

            // This would integrate with CharacterService to get actual character data
            // For now, returning basic structure
            result.success = true;

            return result;
        }

        /// <summary>
        /// Calculate area of effect damage for multi-target spells
        /// </summary>
        std::vector<DamageResult> CalculateAreaDamage(
            std::int32_t baseDamage,
            std::int32_t spellBonus,
            DamageType damageType,
            const std::vector<std::int32_t>& targetArmorClasses) const {

            std::vector<DamageResult> results;
            results.reserve(targetArmorClasses.size());

            for (auto ac : targetArmorClasses) {
                float reduction = CalculateDamageReduction(ac, damageType);
                auto damageResult = CalculateDamage(
                    baseDamage,
                    spellBonus,
                    damageType,
                    false, // AoE spells don't crit
                    reduction
                );
                results.push_back(damageResult);
            }

            return results;
        }

        /// <summary>
        /// Calculate healing amount with wisdom bonus
        /// </summary>
        std::int32_t CalculateHealing(
            std::int32_t baseHealing,
            const Attributes& attributes) const {

            std::int32_t wisdomBonus = attributes.GetWisdomModifier();
            return std::max(1, baseHealing + wisdomBonus);
        }

        /// <summary>
        /// Determine if attack has advantage based on conditions
        /// </summary>
        bool HasAdvantage(
            bool attackerHidden,
            bool targetProne,
            bool targetBlinded) const {

            return attackerHidden || targetProne || targetBlinded;
        }

        /// <summary>
        /// Determine if attack has disadvantage based on conditions
        /// </summary>
        bool HasDisadvantage(
            bool attackerBlinded,
            bool attackerProne,
            bool targetHidden) const {

            return attackerBlinded || attackerProne || targetHidden;
        }

        /// <summary>
        /// Calculate critical hit multiplier
        /// Some classes or weapons may have different multipliers
        /// </summary>
        float GetCriticalMultiplier(CharacterClass attackerClass) const {
            switch (attackerClass) {
                case CharacterClass::Rogue:
                    return 3.0f; // Rogues get better crits
                case CharacterClass::Warrior:
                    return 2.5f; // Warriors get enhanced crits
                default:
                    return 2.0f; // Standard double damage
            }
        }

        /// <summary>
        /// Calculate critical hit chance
        /// Base is 5% (natural 20), some classes get higher
        /// </summary>
        std::int32_t GetCriticalHitThreshold(CharacterClass attackerClass) const {
            switch (attackerClass) {
                case CharacterClass::Rogue:
                    return 19; // Crits on 19-20 (10% chance)
                case CharacterClass::Warrior:
                    return 19; // Crits on 19-20 (10% chance)
                default:
                    return 20; // Crits only on natural 20 (5% chance)
            }
        }

    private:
        /// <summary>
        /// Roll a d20 with optional advantage or disadvantage
        /// </summary>
        std::int32_t rollD20(bool advantage = false, bool disadvantage = false) {
            auto& gen = rng_;
            std::uniform_int_distribution<std::int32_t> dist(1, 20);

            if (advantage && !disadvantage) {
                // Roll twice, take higher
                return std::max(dist(gen), dist(gen));
            }
            else if (disadvantage && !advantage) {
                // Roll twice, take lower
                return std::min(dist(gen), dist(gen));
            }
            else {
                // Normal roll (or advantage + disadvantage = normal)
                return dist(gen);
            }
        }

        // Dice state; mutable so rolls stay available from const queries.
        // One instance must not be used from several threads at once.
        std::mt19937 rng_;   // rolling dice changes it: the methods that roll are not const
    };

} // namespace LB::Services::Combat