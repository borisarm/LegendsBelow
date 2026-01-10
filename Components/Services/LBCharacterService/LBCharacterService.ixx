// LB.CharacterService.ixx - Character management service using C++20 modules
export module LB.CharacterService;

import LB.Domain; // Import the pure domain model
import std;

/// <summary>
/// Character management service for LegendsBelow
/// Provides business logic for character creation, progression, and management
/// Follows clean architecture principles with domain model separation
/// </summary>
export namespace LB::Services::Character {

    /// <summary>
    /// Character creation parameters
    /// Used for creating new characters with specified attributes and class
    /// </summary>
    struct CharacterCreationRequest {
        LB::String name;
        LB::Domain::CharacterClass characterClass;
        LB::Domain::Attributes baseAttributes;
        LB::Domain::Position startingPosition{0, 0};

        CharacterCreationRequest(
            const LB::String& name, 
            LB::Domain::CharacterClass charClass,
            const LB::Domain::Attributes& attributes)
            : name(name), characterClass(charClass), baseAttributes(attributes) {}
    };

    /// <summary>
    /// Character progression result
    /// Contains information about level-up results and stat changes
    /// </summary>
    struct CharacterProgressionResult {
        bool levelIncreased{false};
        int32_t newLevel{1};
        LB::Domain::Attributes attributeGains{};
        int32_t hitPointIncrease{0};
        int32_t manaPointIncrease{0};
        std::vector<LB::String> unlockedAbilities;

        bool HasChanges() const {
            return levelIncreased || hitPointIncrease > 0 || manaPointIncrease > 0 || !unlockedAbilities.empty();
        }
    };

    /// <summary>
    /// Character status information
    /// Represents the current state and condition of a character
    /// </summary>
    struct CharacterStatus {
        LB::Domain::CharacterId id;
        LB::String name;
        LB::Domain::CharacterClass characterClass;
        int32_t level;
        int32_t currentExperience;
        int32_t experienceToNextLevel;
        LB::Domain::Attributes currentAttributes;
        int32_t currentHitPoints;
        int32_t maxHitPoints;
        int32_t currentManaPoints;
        int32_t maxManaPoints;
        LB::Domain::Position position;
        bool isAlive;

        // Derived properties
        double healthPercentage() const {
            return maxHitPoints > 0 ? static_cast<double>(currentHitPoints) / maxHitPoints : 0.0;
        }

        double manaPercentage() const {
            return maxManaPoints > 0 ? static_cast<double>(currentManaPoints) / maxManaPoints : 0.0;
        }

        double experiencePercentage() const {
            return experienceToNextLevel > 0 ? static_cast<double>(currentExperience) / experienceToNextLevel : 0.0;
        }
    };

    /// <summary>
    /// Core character management service
    /// Provides business logic for character operations
    /// </summary>
    class CharacterService {
    public:
        /// <summary>
        /// Create a new character with specified parameters
        /// Handles class-specific starting stats and abilities
        /// </summary>
        std::optional<LB::Domain::CharacterId> CreateCharacter(const CharacterCreationRequest& request) {
            // Validate input
            if (request.name.empty()) {
                return std::nullopt; // Invalid name
            }

            // Generate new character ID
            auto characterId = generateNextId();

            // Apply class-specific modifiers
            auto adjustedAttributes = applyClassModifiers(request.characterClass, request.baseAttributes);

            // Calculate starting hit points and mana
            auto startingHP = calculateBaseHitPoints(request.characterClass, adjustedAttributes);
            auto startingMP = calculateBaseManaPoints(request.characterClass, adjustedAttributes);

            // Create character domain entity
            LB::Domain::Character character(
                characterId,
                request.name,
                request.characterClass,
                adjustedAttributes,
                1, // Starting level
                0, // Starting experience
                startingHP,
                startingMP,
                request.startingPosition
            );

            // Store character (would integrate with repository)
            auto [it, success] = characters_.emplace(characterId, std::move(character));

            if (!success) {
                return std::nullopt; // Failed to insert (shouldn't happen with unique IDs)
            }

            return characterId;
        }

        /// <summary>
        /// Get current status of a character
        /// Returns comprehensive character information
        /// </summary>
        std::optional<CharacterStatus> GetCharacterStatus(LB::Domain::CharacterId id) const {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return std::nullopt;
            }

            const auto& character = it->second;
            CharacterStatus status{};

            status.id = character.GetId();
            status.name = character.GetName();
            status.characterClass = character.GetCharacterClass();
            status.level = character.GetLevel();
            status.currentExperience = character.GetExperience();
            status.experienceToNextLevel = calculateExperienceForLevel(character.GetLevel() + 1);
            status.currentAttributes = character.GetAttributes();
            status.currentHitPoints = character.GetCurrentHitPoints();
            status.maxHitPoints = character.GetMaxHitPoints();
            status.currentManaPoints = character.GetCurrentManaPoints();
            status.maxManaPoints = character.GetMaxManaPoints();
            status.position = character.GetPosition();
            status.isAlive = character.IsAlive();

            return status;
        }

        /// <summary>
        /// Award experience points and handle potential level-up
        /// Returns progression information if character levels up
        /// </summary>
        std::optional<CharacterProgressionResult> AwardExperience(LB::Domain::CharacterId id, int32_t experiencePoints) {
            auto it = characters_.find(id);
            if (it == characters_.end() || experiencePoints <= 0) {
                return std::nullopt;
            }

            auto& character = it->second;
            auto oldLevel = character.GetLevel();
            auto oldExperience = character.GetExperience();

            // Add experience
            auto newExperience = oldExperience + experiencePoints;
            auto newLevel = calculateLevelForExperience(newExperience);

            CharacterProgressionResult result{};
            result.levelIncreased = newLevel > oldLevel;
            result.newLevel = newLevel;

            if (result.levelIncreased) {
                // Calculate attribute gains per level
                auto levelsGained = newLevel - oldLevel;
                result.attributeGains = calculateAttributeGains(character.GetCharacterClass(), levelsGained);

                // Apply attribute gains
                auto currentAttribs = character.GetAttributes();
                auto newAttributes = LB::Domain::Attributes{
                    currentAttribs.strength + result.attributeGains.strength,
                    currentAttribs.dexterity + result.attributeGains.dexterity,
                    currentAttribs.constitution + result.attributeGains.constitution,
                    currentAttribs.intelligence + result.attributeGains.intelligence,
                    currentAttribs.wisdom + result.attributeGains.wisdom,
                    currentAttribs.charisma + result.attributeGains.charisma
                };

                // Calculate hit point and mana increases
                result.hitPointIncrease = calculateHitPointGain(character.GetCharacterClass(), result.attributeGains.constitution, levelsGained);
                result.manaPointIncrease = calculateManaPointGain(character.GetCharacterClass(), result.attributeGains.intelligence, levelsGained);

                // Apply changes to character
                character.SetLevel(newLevel);
                character.SetExperience(newExperience);
                character.SetAttributes(newAttributes);
                character.IncreaseMaxHitPoints(result.hitPointIncrease);
                character.IncreaseMaxManaPoints(result.manaPointIncrease);
                character.RestoreToMaxHealth(); // Level up restores health

                // Determine unlocked abilities (simplified)
                result.unlockedAbilities = getAbilitiesUnlockedAtLevel(character.GetCharacterClass(), newLevel);
            } else {
                character.SetExperience(newExperience);
            }

            return result;
        }

        /// <summary>
        /// Move character to new position
        /// Validates movement rules and updates position
        /// </summary>
        bool MoveCharacter(LB::Domain::CharacterId id, const LB::Domain::Position& newPosition) {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return false;
            }

            auto& character = it->second;

            // Validate movement (basic distance check)
            auto currentPos = character.GetPosition();
            auto distance = currentPos.ManhattanDistanceTo(newPosition);
            auto maxMoveDistance = getMaxMoveDistance(character.GetCharacterClass(), character.GetAttributes());

            if (distance > maxMoveDistance) {
                return false;
            }

            character.SetPosition(newPosition);
            return true;
        }

        /// <summary>
        /// Apply damage to character
        /// Handles damage reduction and death state
        /// </summary>
        bool DamageCharacter(LB::Domain::CharacterId id, int32_t damage, float damageReduction = 0.0f) {
            auto it = characters_.find(id);
            if (it == characters_.end() || damage <= 0) {
                return false;
            }

            auto& character = it->second;
            if (!character.IsAlive()) {
                return false;
            }

            // Apply damage reduction
            auto actualDamage = static_cast<int32_t>(damage * (1.0f - std::clamp(damageReduction, 0.0f, 0.99f)));
            character.TakeDamage(actualDamage);

            return true;
        }

        /// <summary>
        /// Heal character by specified amount
        /// Cannot exceed maximum hit points
        /// </summary>
        int32_t HealCharacter(LB::Domain::CharacterId id, int32_t healAmount) {
            auto it = characters_.find(id);
            if (it == characters_.end() || healAmount <= 0) {
                return 0;
            }

            auto& character = it->second;
            auto currentHP = character.GetCurrentHitPoints();
            auto maxHP = character.GetMaxHitPoints();
            auto actualHeal = std::min(healAmount, maxHP - currentHP);

            if (actualHeal > 0) {
                character.Heal(actualHeal);
            }

            return actualHeal;
        }

        /// <summary>
        /// Get all managed characters
        /// Returns character IDs for iteration
        /// </summary>
        std::vector<LB::Domain::CharacterId> GetAllCharacterIds() const {
            std::vector<LB::Domain::CharacterId> ids;
            ids.reserve(characters_.size());

            for (const auto& [id, character] : characters_) {
                ids.push_back(id);
            }

            return ids;
        }

        /// <summary>
        /// Get character entity by ID
        /// Returns const reference to character for read-only operations
        /// </summary>
        std::optional<std::reference_wrapper<const LB::Domain::Character>> GetCharacter(LB::Domain::CharacterId id) const {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return std::nullopt;
            }
            return std::cref(it->second);
        }

        /// <summary>
        /// Rest character to restore health and mana
        /// Simulates short or long rest mechanics
        /// </summary>
        bool RestCharacter(LB::Domain::CharacterId id, bool fullRest = true) {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return false;
            }

            auto& character = it->second;
            if (!character.IsAlive()) {
                return false; // Cannot rest if dead
            }

            if (fullRest) {
                // Long rest: full recovery
                character.RestoreToMaxHealth();
                character.RestoreToMaxMana();
            } else {
                // Short rest: partial recovery
                auto healAmount = character.GetMaxHitPoints() / 2;
                auto manaAmount = character.GetMaxManaPoints() / 2;
                character.Heal(healAmount);
                character.RestoreMana(manaAmount);
            }

            return true;
        }

        /// <summary>
        /// Consume mana for spell casting or abilities
        /// Returns true if character had sufficient mana
        /// </summary>
        bool ConsumeMana(LB::Domain::CharacterId id, int32_t manaCost) {
            auto it = characters_.find(id);
            if (it == characters_.end() || manaCost <= 0) {
                return false;
            }

            auto& character = it->second;
            if (character.GetCurrentManaPoints() < manaCost) {
                return false; // Insufficient mana
            }

            character.ConsumeMana(manaCost);
            return true;
        }

        /// <summary>
        /// Restore mana to character
        /// Returns actual amount restored
        /// </summary>
        int32_t RestoreMana(LB::Domain::CharacterId id, int32_t manaAmount) {
            auto it = characters_.find(id);
            if (it == characters_.end() || manaAmount <= 0) {
                return 0;
            }

            auto& character = it->second;
            auto currentMP = character.GetCurrentManaPoints();
            auto maxMP = character.GetMaxManaPoints();
            auto actualRestore = std::min(manaAmount, maxMP - currentMP);

            if (actualRestore > 0) {
                character.RestoreMana(actualRestore);
            }

            return actualRestore;
        }

        /// <summary>
        /// Remove character from service management
        /// Returns true if character was found and removed
        /// </summary>
        bool RemoveCharacter(LB::Domain::CharacterId id) {
            return characters_.erase(id) > 0;
        }

        /// <summary>
        /// Check if character exists in service
        /// </summary>
        bool HasCharacter(LB::Domain::CharacterId id) const {
            return characters_.contains(id);
        }

        /// <summary>
        /// Get count of managed characters
        /// </summary>
        size_t GetCharacterCount() const noexcept {
            return characters_.size();
        }

        /// <summary>
        /// Get all characters of a specific class
        /// </summary>
        std::vector<LB::Domain::CharacterId> GetCharactersByClass(LB::Domain::CharacterClass charClass) const {
            std::vector<LB::Domain::CharacterId> ids;

            for (const auto& [id, character] : characters_) {
                if (character.GetCharacterClass() == charClass) {
                    ids.push_back(id);
                }
            }

            return ids;
        }

        /// <summary>
        /// Get all alive characters
        /// </summary>
        std::vector<LB::Domain::CharacterId> GetAliveCharacters() const {
            std::vector<LB::Domain::CharacterId> ids;

            for (const auto& [id, character] : characters_) {
                if (character.IsAlive()) {
                    ids.push_back(id);
                }
            }

            return ids;
        }

        /// <summary>
        /// Get all characters in a level range
        /// </summary>
        std::vector<LB::Domain::CharacterId> GetCharactersByLevelRange(int32_t minLevel, int32_t maxLevel) const {
            std::vector<LB::Domain::CharacterId> ids;

            for (const auto& [id, character] : characters_) {
                auto level = character.GetLevel();
                if (level >= minLevel && level <= maxLevel) {
                    ids.push_back(id);
                }
            }

            return ids;
        }

        /// <summary>
        /// Equip weapon to character
        /// Placeholder for equipment system integration
        /// </summary>
        bool EquipWeapon(LB::Domain::CharacterId characterId, LB::Domain::WeaponId weaponId) {
            auto it = characters_.find(characterId);
            if (it == characters_.end()) {
                return false;
            }

            // TODO: Integrate with inventory/equipment system
            // For now, just validate the character exists
            return true;
        }

        /// <summary>
        /// Equip armor to character
        /// Placeholder for equipment system integration
        /// </summary>
        bool EquipArmor(LB::Domain::CharacterId characterId, LB::Domain::ArmorId armorId) {
            auto it = characters_.find(characterId);
            if (it == characters_.end()) {
                return false;
            }

            // TODO: Integrate with inventory/equipment system
            // For now, just validate the character exists
            return true;
        }

        /// <summary>
        /// Learn new ability for character
        /// </summary>
        bool LearnAbility(LB::Domain::CharacterId id, const LB::String& abilityName) {
            auto it = characters_.find(id);
            if (it == characters_.end() || abilityName.empty()) {
                return false;
            }

            auto& character = it->second;
            character.LearnAbility(abilityName);
            return true;
        }

        /// <summary>
        /// Revive a dead character with specified HP percentage
        /// </summary>
        bool ReviveCharacter(LB::Domain::CharacterId id, float healthPercentage = 0.5f) {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return false;
            }

            auto& character = it->second;
            if (character.IsAlive()) {
                return false; // Already alive
            }

            character.Revive();
            auto healAmount = static_cast<int32_t>(character.GetMaxHitPoints() * std::clamp(healthPercentage, 0.0f, 1.0f));
            character.Heal(healAmount);

            return true;
        }

        /// <summary>
        /// Clear all characters from service
        /// Useful for testing or resetting game state
        /// </summary>
        void ClearAll() {
            characters_.clear();
            nextId_ = 1;
        }

    private:
        std::unordered_map<LB::Domain::CharacterId, LB::Domain::Character> characters_;
        LB::Domain::CharacterId nextId_{1};

        LB::Domain::CharacterId generateNextId() {
            return nextId_++;
        }

        LB::Domain::Attributes applyClassModifiers(LB::Domain::CharacterClass charClass, const LB::Domain::Attributes& base) const {
            // Apply class-specific attribute modifiers
            switch (charClass) {
                case LB::Domain::CharacterClass::Warrior:
                    return LB::Domain::Attributes{base.strength + 2, base.dexterity, base.constitution + 1, base.intelligence, base.wisdom, base.charisma};
                case LB::Domain::CharacterClass::Rogue:
                    return LB::Domain::Attributes{base.strength, base.dexterity + 2, base.constitution, base.intelligence, base.wisdom, base.charisma + 1};
                case LB::Domain::CharacterClass::Mage:
                    return LB::Domain::Attributes{base.strength, base.dexterity, base.constitution, base.intelligence + 2, base.wisdom + 1, base.charisma};
                case LB::Domain::CharacterClass::Cleric:
                    return LB::Domain::Attributes{base.strength, base.dexterity, base.constitution + 1, base.intelligence, base.wisdom + 2, base.charisma};
                default:
                    return base;
            }
        }

        int32_t calculateBaseHitPoints(LB::Domain::CharacterClass charClass, const LB::Domain::Attributes& attributes) const {
            int32_t baseHP = 10; // Base hit points
            int32_t constitutionBonus = attributes.GetConstitutionModifier();

            // Class-specific HP modifiers
            switch (charClass) {
                case LB::Domain::CharacterClass::Warrior: baseHP += 5; break;
                case LB::Domain::CharacterClass::Rogue: baseHP += 2; break;
                case LB::Domain::CharacterClass::Mage: baseHP += 0; break;
                case LB::Domain::CharacterClass::Cleric: baseHP += 3; break;
                default: break;
            }

            return std::max(1, baseHP + constitutionBonus);
        }

        int32_t calculateBaseManaPoints(LB::Domain::CharacterClass charClass, const LB::Domain::Attributes& attributes) const {
            int32_t baseMP = 0; // Base mana points
            int32_t intelligenceBonus = attributes.GetIntelligenceModifier();

            // Class-specific MP modifiers
            switch (charClass) {
                case LB::Domain::CharacterClass::Warrior: baseMP = 0; break;
                case LB::Domain::CharacterClass::Rogue: baseMP = 5; break;
                case LB::Domain::CharacterClass::Mage: baseMP = 20; break;
                case LB::Domain::CharacterClass::Cleric: baseMP = 15; break;
                default: break;
            }

            return std::max(0, baseMP + intelligenceBonus * 2);
        }

        int32_t calculateExperienceForLevel(int32_t level) const {
            // Simple exponential XP curve
            if (level <= 1) return 0;
            return static_cast<int32_t>(100 * std::pow(level - 1, 1.5));
        }

        int32_t calculateLevelForExperience(int32_t experience) const {
            int32_t level = 1;
            while (calculateExperienceForLevel(level + 1) <= experience) {
                level++;
                if (level >= 100) break; // Cap at level 100
            }
            return level;
        }

        LB::Domain::Attributes calculateAttributeGains(LB::Domain::CharacterClass charClass, int32_t levels) const {
            // Per-level attribute gains based on class
            LB::Domain::Attributes gainsPerLevel{};

            switch (charClass) {
                case LB::Domain::CharacterClass::Warrior:
                    gainsPerLevel = LB::Domain::Attributes{1, 0, 1, 0, 0, 0};
                    break;
                case LB::Domain::CharacterClass::Rogue:
                    gainsPerLevel = LB::Domain::Attributes{0, 1, 0, 0, 0, 1};
                    break;
                case LB::Domain::CharacterClass::Mage:
                    gainsPerLevel = LB::Domain::Attributes{0, 0, 0, 1, 1, 0};
                    break;
                case LB::Domain::CharacterClass::Cleric:
                    gainsPerLevel = LB::Domain::Attributes{0, 0, 1, 0, 1, 0};
                    break;
                default:
                    gainsPerLevel = LB::Domain::Attributes{0, 0, 0, 0, 0, 0};
                    break;
            }

            return LB::Domain::Attributes{
                gainsPerLevel.strength * levels,
                gainsPerLevel.dexterity * levels,
                gainsPerLevel.constitution * levels,
                gainsPerLevel.intelligence * levels,
                gainsPerLevel.wisdom * levels,
                gainsPerLevel.charisma * levels
            };
        }

        int32_t calculateHitPointGain(LB::Domain::CharacterClass charClass, int32_t constitutionGain, int32_t levels) const {
            int32_t baseGainPerLevel = 3; // Base HP gain per level

            switch (charClass) {
                case LB::Domain::CharacterClass::Warrior: baseGainPerLevel = 5; break;
                case LB::Domain::CharacterClass::Rogue: baseGainPerLevel = 4; break;
                case LB::Domain::CharacterClass::Mage: baseGainPerLevel = 2; break;
                case LB::Domain::CharacterClass::Cleric: baseGainPerLevel = 3; break;
                default: break;
            }

            return (baseGainPerLevel + constitutionGain) * levels;
        }

        int32_t calculateManaPointGain(LB::Domain::CharacterClass charClass, int32_t intelligenceGain, int32_t levels) const {
            int32_t baseGainPerLevel = 0;

            switch (charClass) {
                case LB::Domain::CharacterClass::Warrior: baseGainPerLevel = 0; break;
                case LB::Domain::CharacterClass::Rogue: baseGainPerLevel = 1; break;
                case LB::Domain::CharacterClass::Mage: baseGainPerLevel = 5; break;
                case LB::Domain::CharacterClass::Cleric: baseGainPerLevel = 3; break;
                default: break;
            }

            return (baseGainPerLevel + intelligenceGain * 2) * levels;
        }

        int32_t getMaxMoveDistance(LB::Domain::CharacterClass charClass, const LB::Domain::Attributes& attributes) const {
            int32_t baseMoveDistance = 5;
            int32_t dexterityBonus = attributes.GetDexterityModifier();

            switch (charClass) {
                case LB::Domain::CharacterClass::Warrior: baseMoveDistance = 4; break;
                case LB::Domain::CharacterClass::Rogue: baseMoveDistance = 7; break;
                case LB::Domain::CharacterClass::Mage: baseMoveDistance = 4; break;
                case LB::Domain::CharacterClass::Cleric: baseMoveDistance = 5; break;
                default: break;
            }

            return std::max(1, baseMoveDistance + dexterityBonus);
        }

        std::vector<LB::String> getAbilitiesUnlockedAtLevel(LB::Domain::CharacterClass charClass, int32_t level) const {
            std::vector<LB::String> abilities;

            // Simplified ability unlocking system
            if (level == 2) {
                switch (charClass) {
                    case LB::Domain::CharacterClass::Warrior: abilities.push_back("Power Attack"); break;
                    case LB::Domain::CharacterClass::Rogue: abilities.push_back("Sneak Attack"); break;
                    case LB::Domain::CharacterClass::Mage: abilities.push_back("Magic Missile"); break;
                    case LB::Domain::CharacterClass::Cleric: abilities.push_back("Heal"); break;
                    default: break;
                }
            } else if (level == 5) {
                switch (charClass) {
                    case LB::Domain::CharacterClass::Warrior: abilities.push_back("Cleave"); break;
                    case LB::Domain::CharacterClass::Rogue: abilities.push_back("Backstab"); break;
                    case LB::Domain::CharacterClass::Mage: abilities.push_back("Fireball"); break;
                    case LB::Domain::CharacterClass::Cleric: abilities.push_back("Turn Undead"); break;
                    default: break;
                }
            }

            return abilities;
        }
    };

} // namespace LB::Services::Character