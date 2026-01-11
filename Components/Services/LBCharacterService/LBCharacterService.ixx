// LB.CharacterService.ixx - Character management service using C++20 modules
export module LB.CharacterService;

// Import only required domains
import LB.Core;
import LB.Character;
import std;

/// <summary>
/// Character management service for LegendsBelow
/// Provides business logic for character creation, progression, and management
/// Follows clean architecture principles with domain model separation
/// </summary>
export namespace LB::Services::Character {
	// Re-export domain types for service convenience
	using LB::Character::Character;
	using LB::Character::CharacterClass;
	using LB::Character::CharacterId;

    using LB::Attributes;
    using LB::Position;

    /// <summary>
    /// Character creation parameters
    /// Used for creating new characters with specified attributes and class
    /// </summary>
    struct CharacterCreationRequest {
        LB::String name;
        CharacterClass characterClass;
        Attributes baseAttributes;
        Position startingPosition{0, 0};

        CharacterCreationRequest(
            const LB::String& name, 
            CharacterClass charClass,
            const Attributes& attributes)
            : name(name), characterClass(charClass), baseAttributes(attributes) {}
    };

    /// <summary>
    /// Character progression result
    /// Contains information about level-up results and stat changes
    /// </summary>
    struct CharacterProgressionResult {
        bool levelIncreased{false};
        int32_t newLevel{1};
        Attributes attributeGains{};
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
        CharacterId id;
        LB::String name;
        CharacterClass characterClass;
        int32_t level;
        int32_t currentExperience;
        int32_t experienceToNextLevel;
        Attributes currentAttributes;
        int32_t currentHitPoints;
        int32_t maxHitPoints;
        int32_t currentManaPoints;
        int32_t maxManaPoints;
        Position position;
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
        std::optional<CharacterId> CreateCharacter(const CharacterCreationRequest& request) {
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
            Character character(
                characterId,
                request.name,
                request.characterClass,
                adjustedAttributes
            );

            // Set initial values
            character.SetLevel(1);
            character.SetExperience(0);
            character.SetHitPoints(startingHP, startingHP);
            character.SetManaPoints(startingMP, startingMP);
            character.SetPosition(request.startingPosition);

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
        std::optional<CharacterStatus> GetCharacterStatus(CharacterId id) const {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return std::nullopt;
            }

            const auto& character = it->second;
            CharacterStatus status{};

            status.id = character.GetId();
            status.name = character.GetName();
            status.characterClass = character.GetClass();
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
        std::optional<CharacterProgressionResult> AwardExperience(CharacterId id, int32_t experiencePoints) {
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
                result.attributeGains = calculateAttributeGains(character.GetClass(), levelsGained);

                // Apply attribute gains
                auto currentAttribs = character.GetAttributes();
                auto newAttributes = Attributes{
                    currentAttribs.strength + result.attributeGains.strength,
                    currentAttribs.dexterity + result.attributeGains.dexterity,
                    currentAttribs.constitution + result.attributeGains.constitution,
                    currentAttribs.intelligence + result.attributeGains.intelligence,
                    currentAttribs.wisdom + result.attributeGains.wisdom,
                    currentAttribs.charisma + result.attributeGains.charisma
                };

                // Calculate hit point and mana increases
                result.hitPointIncrease = calculateHitPointGain(character.GetClass(), result.attributeGains.constitution, levelsGained);
                result.manaPointIncrease = calculateManaPointGain(character.GetClass(), result.attributeGains.intelligence, levelsGained);

                // Apply changes to character
                character.SetLevel(newLevel);
                character.SetExperience(newExperience);
                character.SetAttributes(newAttributes);

                // Increase max HP and MP
                auto newMaxHP = character.GetMaxHitPoints() + result.hitPointIncrease;
                auto newMaxMP = character.GetMaxManaPoints() + result.manaPointIncrease;
                character.SetHitPoints(newMaxHP, newMaxHP); // Level up restores health
                character.SetManaPoints(newMaxMP, newMaxMP); // Level up restores mana

                // Determine unlocked abilities (simplified)
                result.unlockedAbilities = getAbilitiesUnlockedAtLevel(character.GetClass(), newLevel);
            } else {
                character.SetExperience(newExperience);
            }

            return result;
        }

        /// <summary>
        /// Move character to new position
        /// Validates movement rules and updates position
        /// </summary>
        bool MoveCharacter(CharacterId id, const Position& newPosition) {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return false;
            }

            auto& character = it->second;

            // Validate movement (basic distance check)
            auto currentPos = character.GetPosition();
            auto distance = currentPos.ManhattanDistance(newPosition);
            auto maxMoveDistance = getMaxMoveDistance(character.GetClass(), character.GetAttributes());

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
        bool DamageCharacter(CharacterId id, int32_t damage, float damageReduction = 0.0f) {
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

            // Apply damage
            auto newHP = std::max(0, character.GetCurrentHitPoints() - actualDamage);
            character.SetHitPoints(newHP, character.GetMaxHitPoints());

            return true;
        }

        /// <summary>
        /// Heal character by specified amount
        /// Cannot exceed maximum hit points
        /// </summary>
        int32_t HealCharacter(CharacterId id, int32_t healAmount) {
            auto it = characters_.find(id);
            if (it == characters_.end() || healAmount <= 0) {
                return 0;
            }

            auto& character = it->second;
            auto currentHP = character.GetCurrentHitPoints();
            auto maxHP = character.GetMaxHitPoints();
            auto actualHeal = std::min(healAmount, maxHP - currentHP);

            if (actualHeal > 0) {
                character.SetHitPoints(currentHP + actualHeal, maxHP);
            }

            return actualHeal;
        }

        /// <summary>
        /// Get all managed characters
        /// Returns character IDs for iteration
        /// </summary>
        std::vector<CharacterId> GetAllCharacterIds() const {
            std::vector<CharacterId> ids;
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
        std::optional<std::reference_wrapper<const Character>> GetCharacter(CharacterId id) const {
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
        bool RestCharacter(CharacterId id, bool fullRest = true) {
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
                character.SetHitPoints(character.GetMaxHitPoints(), character.GetMaxHitPoints());
                character.SetManaPoints(character.GetMaxManaPoints(), character.GetMaxManaPoints());
            } else {
                // Short rest: partial recovery
                auto healAmount = character.GetMaxHitPoints() / 2;
                auto manaAmount = character.GetMaxManaPoints() / 2;
                auto newHP = std::min(character.GetCurrentHitPoints() + healAmount, character.GetMaxHitPoints());
                auto newMP = std::min(character.GetCurrentManaPoints() + manaAmount, character.GetMaxManaPoints());
                character.SetHitPoints(newHP, character.GetMaxHitPoints());
                character.SetManaPoints(newMP, character.GetMaxManaPoints());
            }

            return true;
        }

        /// <summary>
        /// Consume mana for spell casting or abilities
        /// Returns true if character had sufficient mana
        /// </summary>
        bool ConsumeMana(CharacterId id, int32_t manaCost) {
            auto it = characters_.find(id);
            if (it == characters_.end() || manaCost <= 0) {
                return false;
            }

            auto& character = it->second;
            if (character.GetCurrentManaPoints() < manaCost) {
                return false; // Insufficient mana
            }

            // Consume mana
            auto newMP = character.GetCurrentManaPoints() - manaCost;
            character.SetManaPoints(newMP, character.GetMaxManaPoints());
            return true;
        }

        /// <summary>
        /// Restore mana to character
        /// Returns actual amount restored
        /// </summary>
        int32_t RestoreMana(CharacterId id, int32_t manaAmount) {
            auto it = characters_.find(id);
            if (it == characters_.end() || manaAmount <= 0) {
                return 0;
            }

            auto& character = it->second;
            auto currentMP = character.GetCurrentManaPoints();
            auto maxMP = character.GetMaxManaPoints();
            auto actualRestore = std::min(manaAmount, maxMP - currentMP);

            if (actualRestore > 0) {
                character.SetManaPoints(currentMP + actualRestore, maxMP);
            }

            return actualRestore;
        }

        /// <summary>
        /// Remove character from service management
        /// Returns true if character was found and removed
        /// </summary>
        bool RemoveCharacter(CharacterId id) {
            return characters_.erase(id) > 0;
        }

        /// <summary>
        /// Check if character exists in service
        /// </summary>
        bool HasCharacter(CharacterId id) const {
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
        std::vector<CharacterId> GetCharactersByClass(CharacterClass charClass) const {
            std::vector<CharacterId> ids;

            for (const auto& [id, character] : characters_) {
                if (character.GetClass() == charClass) {
                    ids.push_back(id);
                }
            }

            return ids;
        }

        /// <summary>
        /// Get all alive characters
        /// </summary>
        std::vector<CharacterId> GetAliveCharacters() const {
            std::vector<CharacterId> ids;

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
        std::vector<CharacterId> GetCharactersByLevelRange(int32_t minLevel, int32_t maxLevel) const {
            std::vector<CharacterId> ids;

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
        bool EquipWeapon(CharacterId characterId, LB::Id weaponId) {
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
        bool EquipArmor(CharacterId characterId, LB::Id armorId) {
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
        /// TODO: Implement ability tracking system
        /// </summary>
        bool LearnAbility(CharacterId id, const LB::String& abilityName) {
            auto it = characters_.find(id);
            if (it == characters_.end() || abilityName.empty()) {
                return false;
            }

            // TODO: Integrate with ability/spell system
            // For now, just validate the character exists
            return true;
        }

        /// <summary>
        /// Revive a dead character with specified HP percentage
        /// </summary>
        bool ReviveCharacter(CharacterId id, float healthPercentage = 0.5f) {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return false;
            }

            auto& character = it->second;
            if (character.IsAlive()) {
                return false; // Already alive
            }

            // Revive character with specified HP percentage
            auto healAmount = static_cast<int32_t>(character.GetMaxHitPoints() * std::clamp(healthPercentage, 0.0f, 1.0f));
            character.SetHitPoints(healAmount, character.GetMaxHitPoints());

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
        std::unordered_map<CharacterId, Character> characters_;
        CharacterId nextId_{1};

        CharacterId generateNextId() {
            return nextId_++;
        }

        Attributes applyClassModifiers(CharacterClass charClass, const Attributes& base) const {
            // Apply class-specific attribute modifiers
            switch (charClass) {
                case CharacterClass::Warrior:
                    return Attributes{base.strength + 2, base.dexterity, base.constitution + 1, base.intelligence, base.wisdom, base.charisma};
                case CharacterClass::Rogue:
                    return Attributes{base.strength, base.dexterity + 2, base.constitution, base.intelligence, base.wisdom, base.charisma + 1};
                case CharacterClass::Mage:
                    return Attributes{base.strength, base.dexterity, base.constitution, base.intelligence + 2, base.wisdom + 1, base.charisma};
                case CharacterClass::Cleric:
                    return Attributes{base.strength, base.dexterity, base.constitution + 1, base.intelligence, base.wisdom + 2, base.charisma};
                default:
                    return base;
            }
        }

        int32_t calculateBaseHitPoints(CharacterClass charClass, const Attributes& attributes) const {
            int32_t baseHP = 10; // Base hit points
            int32_t constitutionBonus = attributes.GetConstitutionModifier();

            // Class-specific HP modifiers
            switch (charClass) {
                case CharacterClass::Warrior: baseHP += 5; break;
                case CharacterClass::Rogue: baseHP += 2; break;
                case CharacterClass::Mage: baseHP += 0; break;
                case CharacterClass::Cleric: baseHP += 3; break;
                default: break;
            }

            return std::max(1, baseHP + constitutionBonus);
        }

        int32_t calculateBaseManaPoints(CharacterClass charClass, const Attributes& attributes) const {
            int32_t baseMP = 0; // Base mana points
            int32_t intelligenceBonus = attributes.GetIntelligenceModifier();

            // Class-specific MP modifiers
            switch (charClass) {
                case CharacterClass::Warrior: baseMP = 0; break;
                case CharacterClass::Rogue: baseMP = 5; break;
                case CharacterClass::Mage: baseMP = 20; break;
                case CharacterClass::Cleric: baseMP = 15; break;
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

        Attributes calculateAttributeGains(CharacterClass charClass, int32_t levels) const {
            // Per-level attribute gains based on class
            Attributes gainsPerLevel{};

            switch (charClass) {
                case CharacterClass::Warrior:
                    gainsPerLevel = Attributes{1, 0, 1, 0, 0, 0};
                    break;
                case CharacterClass::Rogue:
                    gainsPerLevel = Attributes{0, 1, 0, 0, 0, 1};
                    break;
                case CharacterClass::Mage:
                    gainsPerLevel = Attributes{0, 0, 0, 1, 1, 0};
                    break;
                case CharacterClass::Cleric:
                    gainsPerLevel = Attributes{0, 0, 1, 0, 1, 0};
                    break;
                default:
                    gainsPerLevel = Attributes{0, 0, 0, 0, 0, 0};
                    break;
            }

            return Attributes{
                gainsPerLevel.strength * levels,
                gainsPerLevel.dexterity * levels,
                gainsPerLevel.constitution * levels,
                gainsPerLevel.intelligence * levels,
                gainsPerLevel.wisdom * levels,
                gainsPerLevel.charisma * levels
            };
        }

        int32_t calculateHitPointGain(CharacterClass charClass, int32_t constitutionGain, int32_t levels) const {
            int32_t baseGainPerLevel = 3; // Base HP gain per level

            switch (charClass) {
                case CharacterClass::Warrior: baseGainPerLevel = 5; break;
                case CharacterClass::Rogue: baseGainPerLevel = 4; break;
                case CharacterClass::Mage: baseGainPerLevel = 2; break;
                case CharacterClass::Cleric: baseGainPerLevel = 3; break;
                default: break;
            }

            return (baseGainPerLevel + constitutionGain) * levels;
        }

        int32_t calculateManaPointGain(CharacterClass charClass, int32_t intelligenceGain, int32_t levels) const {
            int32_t baseGainPerLevel = 0;

            switch (charClass) {
                case CharacterClass::Warrior: baseGainPerLevel = 0; break;
                case CharacterClass::Rogue: baseGainPerLevel = 1; break;
                case CharacterClass::Mage: baseGainPerLevel = 5; break;
                case CharacterClass::Cleric: baseGainPerLevel = 3; break;
                default: break;
            }

            return (baseGainPerLevel + intelligenceGain * 2) * levels;
        }

        int32_t getMaxMoveDistance(CharacterClass charClass, const Attributes& attributes) const {
            int32_t baseMoveDistance = 5;
            int32_t dexterityBonus = attributes.GetDexterityModifier();

            switch (charClass) {
                case CharacterClass::Warrior: baseMoveDistance = 4; break;
                case CharacterClass::Rogue: baseMoveDistance = 7; break;
                case CharacterClass::Mage: baseMoveDistance = 4; break;
                case CharacterClass::Cleric: baseMoveDistance = 5; break;
                default: break;
            }

            return std::max(1, baseMoveDistance + dexterityBonus);
        }

        std::vector<LB::String> getAbilitiesUnlockedAtLevel(CharacterClass charClass, int32_t level) const {
            std::vector<LB::String> abilities;

            // Simplified ability unlocking system
            if (level == 2) {
                switch (charClass) {
                    case CharacterClass::Warrior: abilities.push_back("Power Attack"); break;
                    case CharacterClass::Rogue: abilities.push_back("Sneak Attack"); break;
                    case CharacterClass::Mage: abilities.push_back("Magic Missile"); break;
                    case CharacterClass::Cleric: abilities.push_back("Heal"); break;
                    default: break;
                }
            } else if (level == 5) {
                switch (charClass) {
                    case CharacterClass::Warrior: abilities.push_back("Cleave"); break;
                    case CharacterClass::Rogue: abilities.push_back("Backstab"); break;
                    case CharacterClass::Mage: abilities.push_back("Fireball"); break;
                    case CharacterClass::Cleric: abilities.push_back("Turn Undead"); break;
                    default: break;
                }
            }

            return abilities;
        }
    };

} // namespace LB::Services::Character