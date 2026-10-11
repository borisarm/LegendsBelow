// LB.CharacterService.ixx - Character management service using C++20 modules
export module LB.CharacterService;

// Import only required domains
import LB.Core;
import LB.Character;
import LB.Combat;
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
    using LB::AttributeType;
    using LB::Character::ArmorAdjustment;
    using LB::Character::AttributeGrowth;
    using LB::Character::ClassProgression;
    using LB::Combat::ArmorCategory;
    using LB::Combat::EquipmentSlot;
    using LB::Combat::WeaponType;

    /// <summary>
    /// A class's rules, normally loaded from classes.json: movement, the level 1-20
    /// progression table, and attack adjustments per weapon category
    /// </summary>
    struct ClassRules {
        float baseMovementSpeed{4.0f};                       // tiles per second before DEX and armour
        ClassProgression progression;                        // HP/MP gains, attack and save bonuses
        std::map<WeaponType, std::int32_t> weaponAdjustments;     // e.g. Sword: +3 Warrior, -5 Cleric
        std::vector<AttributeType> primaryAttributes;        // receive the automatic level-up points
        AttributeGrowth attributeGrowth;                     // points per level and attribute caps
        std::map<ArmorCategory, ArmorAdjustment> armorAdjustments;  // scales armour penalties
    };

    /// <summary>
    /// An equipped armour piece (from armour.json): what it costs in movement and casting
    /// </summary>
    struct ArmorPiece {
        LB::Id armorId{LB::InvalidId};
        EquipmentSlot slot{EquipmentSlot::None};
        ArmorCategory category{ArmorCategory::None};
        float movementPenalty{0.0f};   // fraction of speed, e.g. 0.15
        float castPenalty{0.0f};       // fraction of cast time, e.g. 0.05
    };
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
        std::int32_t newLevel{1};
        Attributes attributeGains{0, 0, 0, 0, 0, 0};  // automatic class points applied (Attributes default to 10)
        std::int32_t attributePointsGained{0};     // free points to assign (SpendAttributePoint)
        std::int32_t hitPointIncrease{0};
        std::int32_t manaPointIncrease{0};
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
        std::int32_t level;
        std::int32_t currentExperience;
        std::int32_t experienceToNextLevel;
        Attributes currentAttributes;
        std::int32_t currentHitPoints;
        std::int32_t maxHitPoints;
        std::int32_t currentManaPoints;
        std::int32_t maxManaPoints;
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

            // Classes are data driven: no rules, no character
            const auto* rules = GetClassRules(request.characterClass);
            if (!rules) {
                return std::nullopt;
            }

            // Generate new character ID
            auto characterId = generateNextId();

            // Apply class-specific modifiers
            // Starting attributes come from the class template / background; only the class caps apply
            auto adjustedAttributes = clampToCaps(*rules, request.baseAttributes);

            // Starting hit points and mana from the class progression table at level 1
            auto startingHP = calculateMaxHitPoints(*rules, 1, adjustedAttributes);
            auto startingMP = calculateMaxManaPoints(request.characterClass, *rules, 1, adjustedAttributes);

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
        std::optional<CharacterProgressionResult> AwardExperience(CharacterId id, std::int32_t experiencePoints) {
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
                const auto& rules = *GetClassRules(character.GetClass());

                // Attribute growth: automatic class points into the primary attributes, free points
                // for the player (design-character-system.md 5.2)
                auto newAttributes = character.GetAttributes();
                std::int32_t freePoints = 0;
                for (std::int32_t level = oldLevel + 1; level <= newLevel; ++level) {
                    for (std::int32_t point = 0; point < rules.attributeGrowth.autoPointsPerLevel; ++point) {
                        if (!grantAutoPoint(rules, level + point, newAttributes, result.attributeGains)) {
                            ++freePoints;  // every primary at its cap: the point becomes a free one
                        }
                    }
                    freePoints += rules.attributeGrowth.freePointsPerLevel;
                }
                result.attributePointsGained = freePoints;
                character.SetUnspentAttributePoints(character.GetUnspentAttributePoints() + freePoints);

                // Recompute max HP and MP from the progression table at the new level
                const auto newMaxHP = calculateMaxHitPoints(rules, newLevel, newAttributes);
                const auto newMaxMP = calculateMaxManaPoints(character.GetClass(), rules, newLevel, newAttributes);
                result.hitPointIncrease = newMaxHP - character.GetMaxHitPoints();
                result.manaPointIncrease = newMaxMP - character.GetMaxManaPoints();

                // Apply changes to character
                character.SetLevel(newLevel);
                character.SetExperience(newExperience);
                character.SetAttributes(newAttributes);
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
        bool DamageCharacter(CharacterId id, std::int32_t damage, float damageReduction = 0.0f) {
            auto it = characters_.find(id);
            if (it == characters_.end() || damage <= 0) {
                return false;
            }

            auto& character = it->second;
            if (!character.IsAlive()) {
                return false;
            }

            // Apply damage reduction
            auto actualDamage = static_cast<std::int32_t>(damage * (1.0f - std::clamp(damageReduction, 0.0f, 0.99f)));

            // Apply damage
            auto newHP = std::max(0, character.GetCurrentHitPoints() - actualDamage);
            character.SetHitPoints(newHP, character.GetMaxHitPoints());

            return true;
        }

        /// <summary>
        /// Heal character by specified amount
        /// Cannot exceed maximum hit points
        /// </summary>
        std::int32_t HealCharacter(CharacterId id, std::int32_t healAmount) {
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
        /// Real-time movement speed in tiles per second (design-character-system.md, 4.2):
        /// TilesPerSecond = ClassBaseSpeed + DEX_mod * 0.1
        /// Armour penalties (design-armour-system.md, 4.2) apply once equipment data is modelled.
        /// </summary>
        std::optional<float> GetMovementSpeed(CharacterId id) const {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return std::nullopt;
            }

            const auto& character = it->second;
            const auto* rules = GetClassRules(character.GetClass());
            if (!rules) {
                return std::nullopt;
            }

            // design-armour-system.md 4.2: BaseSpeed * (1 - sum of armour penalties), each penalty
            // scaled by how well the class handles that armour category
            const float baseSpeed = rules->baseMovementSpeed
                + static_cast<float>(character.GetAttributes().GetDexterityModifier()) * 0.1f;
            float penalty = 0.0f;
            for (const auto& piece : equippedArmorOf(id)) {
                penalty += piece.movementPenalty * armorAdjustmentFor(*rules, piece.category).movementScale;
            }
            return std::max(MinimumMovementSpeed, baseSpeed * std::max(0.0f, 1.0f - penalty));
        }

        /// <summary>
        /// Cast time multiplier from worn armour (design-armour-system.md 4.3): 1 + sum of the pieces'
        /// cast penalties, each scaled by the class's casting adjustment for that armour category
        /// </summary>
        std::optional<float> GetCastTimeMultiplier(CharacterId id) const {
            const auto* rules = GetRulesFor(id);
            if (!rules) {
                return std::nullopt;
            }

            float penalty = 0.0f;
            for (const auto& piece : equippedArmorOf(id)) {
                penalty += piece.castPenalty * armorAdjustmentFor(*rules, piece.category).castingScale;
            }
            return 1.0f + penalty;
        }

        /// <summary>
        /// Wear an armour piece in its slot, replacing whatever was there
        /// </summary>
        bool EquipArmor(CharacterId id, const ArmorPiece& piece) {
            if (!characters_.contains(id) || piece.slot == EquipmentSlot::None) {
                return false;
            }
            equippedArmor_[id][piece.slot] = piece;
            return true;
        }

        /// <summary>
        /// Remove the armour piece in a slot
        /// </summary>
        bool UnequipArmor(CharacterId id, EquipmentSlot slot) {
            auto it = equippedArmor_.find(id);
            return it != equippedArmor_.end() && it->second.erase(slot) > 0;
        }

        /// <summary>
        /// Armour pieces a character is wearing
        /// </summary>
        std::vector<ArmorPiece> GetEquippedArmor(CharacterId id) const {
            return equippedArmorOf(id);
        }

        /// <summary>
        /// Highest value an attribute can reach for this character's class
        /// </summary>
        std::optional<std::int32_t> GetAttributeCap(CharacterId id, AttributeType attribute) const {
            const auto* rules = GetRulesFor(id);
            if (!rules) {
                return std::nullopt;
            }
            return attributeCap(*rules, attribute);
        }

        /// <summary>
        /// Assign one free attribute point. Fails without unspent points or at the class cap.
        /// Max HP/MP follow the new attribute; current HP/MP rise by the same amount.
        /// </summary>
        bool SpendAttributePoint(CharacterId id, AttributeType attribute) {
            auto it = characters_.find(id);
            const auto* rules = GetRulesFor(id);
            if (it == characters_.end() || !rules) {
                return false;
            }

            auto& character = it->second;
            auto attributes = character.GetAttributes();
            if (character.GetUnspentAttributePoints() <= 0 || attributes.Get(attribute) >= attributeCap(*rules, attribute)) {
                return false;
            }

            attributes.Set(attribute, attributes.Get(attribute) + 1);
            character.SetAttributes(attributes);
            character.SetUnspentAttributePoints(character.GetUnspentAttributePoints() - 1);

            const auto level = character.GetLevel();
            const auto newMaxHP = calculateMaxHitPoints(*rules, level, attributes);
            const auto newMaxMP = calculateMaxManaPoints(character.GetClass(), *rules, level, attributes);
            character.SetHitPoints(std::clamp(character.GetCurrentHitPoints() + newMaxHP - character.GetMaxHitPoints(), 0, newMaxHP), newMaxHP);
            character.SetManaPoints(std::clamp(character.GetCurrentManaPoints() + newMaxMP - character.GetMaxManaPoints(), 0, newMaxMP), newMaxMP);
            return true;
        }

        /// <summary>
        /// Put back saved progression: level, experience, attributes, and unassigned points. Max
        /// HP/MP follow from the class tables; current HP is kept as a fraction of max. Returns false
        /// for an unknown character or values outside the class rules (level 1..MaxLevel,
        /// attributes within the class caps, no negative experience or points).
        /// </summary>
        bool RestoreProgress(CharacterId id, std::int32_t level, std::int32_t experience, const Attributes& attributes, std::int32_t unspentPoints) {
            auto it = characters_.find(id);
            const auto* rules = GetRulesFor(id);
            if (it == characters_.end() || !rules) {
                return false;
            }
            if (level < 1 || level > ClassProgression::MaxLevel || experience < 0 || unspentPoints < 0 || !attributes.IsValid()) {
                return false;
            }
            for (auto attribute : { AttributeType::Strength, AttributeType::Dexterity, AttributeType::Constitution,
                                    AttributeType::Intelligence, AttributeType::Wisdom, AttributeType::Charisma }) {
                if (attributes.Get(attribute) > attributeCap(*rules, attribute)) return false;
            }

            auto& character = it->second;
            const double health = character.GetMaxHitPoints() > 0
                ? static_cast<double>(character.GetCurrentHitPoints()) / character.GetMaxHitPoints() : 1.0;
            character.SetLevel(level);
            character.SetExperience(experience);
            character.SetAttributes(attributes);
            character.SetUnspentAttributePoints(unspentPoints);

            const auto maxHP = calculateMaxHitPoints(*rules, level, attributes);
            const auto maxMP = calculateMaxManaPoints(character.GetClass(), *rules, level, attributes);
            character.SetHitPoints(std::clamp(static_cast<std::int32_t>(std::lround(health * maxHP)), character.IsAlive() ? 1 : 0, maxHP), maxHP);
            character.SetManaPoints(maxMP, maxMP);
            return true;
        }

        /// <summary>
        /// Total experience a character needs to reach a level (level 1: 0)
        /// </summary>
        std::int32_t GetExperienceForLevel(std::int32_t level) const {
            return calculateExperienceForLevel(level);
        }

        /// <summary>
        /// Class adjustment for attacking with a weapon category (classes.json weaponAdjustments)
        /// </summary>
        std::optional<std::int32_t> GetWeaponAdjustment(CharacterId id, WeaponType weapon) const {
            const auto* rules = GetRulesFor(id);
            if (!rules) {
                return std::nullopt;
            }

            auto it = rules->weaponAdjustments.find(weapon);
            return it != rules->weaponAdjustments.end() ? it->second : 0;
        }

        /// <summary>
        /// Attack bonus with a weapon category (design-character-system.md 4.2 plus class adjustment):
        /// melee = class melee bonus + STR_mod, ranged (bows, crossbows) = class ranged bonus + DEX_mod,
        /// then + the class adjustment for that weapon category
        /// </summary>
        std::optional<std::int32_t> GetAttackBonus(CharacterId id, WeaponType weapon) const {
            auto it = characters_.find(id);
            const auto* rules = GetRulesFor(id);
            if (it == characters_.end() || !rules) {
                return std::nullopt;
            }

            const auto& character = it->second;
            const auto totals = rules->progression.TotalsAt(character.GetLevel());
            const bool ranged = weapon == WeaponType::Bow || weapon == WeaponType::Crossbow;
            const std::int32_t classBonus = ranged ? totals.rangedAttackBonus : totals.meleeAttackBonus;
            const std::int32_t attributeBonus = ranged
                ? character.GetAttributes().GetDexterityModifier()
                : character.GetAttributes().GetStrengthModifier();

            return classBonus + attributeBonus + *GetWeaponAdjustment(id, weapon);
        }

        /// <summary>
        /// Configure a class's rules, normally from classes.json. Characters of a class can only
        /// be created once its rules are configured.
        /// </summary>
        void SetClassRules(CharacterClass charClass, ClassRules rules) {
            rules.baseMovementSpeed = std::max(MinimumMovementSpeed, rules.baseMovementSpeed);
            classRules_[charClass] = std::move(rules);
        }

        /// <summary>
        /// Rules for a class, or null if not configured
        /// </summary>
        const ClassRules* GetClassRules(CharacterClass charClass) const {
            auto it = classRules_.find(charClass);
            return it != classRules_.end() ? &it->second : nullptr;
        }

        /// <summary>
        /// Place a character at a position without movement validation
        /// (spawning, teleports, restoring a saved game)
        /// </summary>
        bool PlaceCharacter(CharacterId id, const Position& position) {
            auto it = characters_.find(id);
            if (it == characters_.end()) {
                return false;
            }

            it->second.SetPosition(position);
            return true;
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
        bool ConsumeMana(CharacterId id, std::int32_t manaCost) {
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
        std::int32_t RestoreMana(CharacterId id, std::int32_t manaAmount) {
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
            equippedArmor_.erase(id);
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
        std::size_t GetCharacterCount() const noexcept {
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
        std::vector<CharacterId> GetCharactersByLevelRange(std::int32_t minLevel, std::int32_t maxLevel) const {
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
        bool EquipWeapon(CharacterId characterId, [[maybe_unused]] LB::Id weaponId) {
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
            auto healAmount = static_cast<std::int32_t>(character.GetMaxHitPoints() * std::clamp(healthPercentage, 0.0f, 1.0f));
            character.SetHitPoints(healAmount, character.GetMaxHitPoints());

            return true;
        }

        /// <summary>
        /// Clear all characters from service
        /// Useful for testing or resetting game state
        /// </summary>
        void ClearAll() {
            characters_.clear();
            equippedArmor_.clear();
            nextId_ = 1;
        }

    private:
        std::unordered_map<CharacterId, Character> characters_;
        std::unordered_map<CharacterClass, ClassRules> classRules_;
        std::unordered_map<CharacterId, std::map<EquipmentSlot, ArmorPiece>> equippedArmor_;
        CharacterId nextId_{1};

        CharacterId generateNextId() {
            return nextId_++;
        }

        std::int32_t calculateExperienceForLevel(std::int32_t level) const {
            // Simple exponential XP curve
            if (level <= 1) return 0;
            return static_cast<std::int32_t>(100 * std::pow(level - 1, 1.5));
        }

        std::int32_t calculateLevelForExperience(std::int32_t experience) const {
            std::int32_t level = 1;
            while (calculateExperienceForLevel(level + 1) <= experience) {
                level++;
                if (level >= 100) break; // Cap at level 100
            }
            return level;
        }

        // Slowest a character can move, however low its DEX
        static constexpr float MinimumMovementSpeed = 1.0f;

        std::vector<ArmorPiece> equippedArmorOf(CharacterId id) const {
            std::vector<ArmorPiece> pieces;
            if (auto it = equippedArmor_.find(id); it != equippedArmor_.end()) {
                for (const auto& [slot, piece] : it->second) pieces.push_back(piece);
            }
            return pieces;
        }

        static ArmorAdjustment armorAdjustmentFor(const ClassRules& rules, ArmorCategory category) {
            auto it = rules.armorAdjustments.find(category);
            return it != rules.armorAdjustments.end() ? it->second : ArmorAdjustment{};
        }

        static bool isPrimary(const ClassRules& rules, AttributeType attribute) {
            return std::ranges::find(rules.primaryAttributes, attribute) != rules.primaryAttributes.end();
        }

        static std::int32_t attributeCap(const ClassRules& rules, AttributeType attribute) {
            return isPrimary(rules, attribute) ? rules.attributeGrowth.primaryAttributeCap : rules.attributeGrowth.otherAttributeCap;
        }

        static Attributes clampToCaps(const ClassRules& rules, Attributes attributes) {
            for (auto type : { AttributeType::Strength, AttributeType::Dexterity, AttributeType::Constitution,
                               AttributeType::Intelligence, AttributeType::Wisdom, AttributeType::Charisma }) {
                attributes.Set(type, std::min(attributes.Get(type), attributeCap(rules, type)));
            }
            return attributes;
        }

        // Give one automatic point to a primary attribute, rotating through the primaries by
        // slot (level 2 -> first primary, level 3 -> second, ...). Primaries at their cap are
        // skipped. Returns false when every primary is capped.
        static bool grantAutoPoint(const ClassRules& rules, std::int32_t slot, Attributes& attributes, Attributes& gains) {
            const auto count = static_cast<std::int32_t>(rules.primaryAttributes.size());
            for (std::int32_t offset = 0; offset < count; ++offset) {
                const auto attribute = rules.primaryAttributes[static_cast<std::size_t>((slot - 2 + offset) % count)];
                if (attributes.Get(attribute) < attributeCap(rules, attribute)) {
                    attributes.Set(attribute, attributes.Get(attribute) + 1);
                    gains.Set(attribute, gains.Get(attribute) + 1);
                    return true;
                }
            }
            return false;
        }

        const ClassRules* GetRulesFor(CharacterId id) const {
            auto it = characters_.find(id);
            return it != characters_.end() ? GetClassRules(it->second.GetClass()) : nullptr;
        }

        // design-character-system.md 4.2: MaxHP = class table HP + CON_mod * level
        static std::int32_t calculateMaxHitPoints(const ClassRules& rules, std::int32_t level, const Attributes& attributes) {
            const auto totals = rules.progression.TotalsAt(level);
            return std::max(1, totals.maxHitPoints + attributes.GetConstitutionModifier() * level);
        }

        // design-character-system.md 4.2: MaxMP = class table MP + CasterPrimaryMod * level,
        // CasterPrimaryMod = INT_mod (Mage), WIS_mod (Cleric), 0 for others
        static std::int32_t calculateMaxManaPoints(CharacterClass charClass, const ClassRules& rules, std::int32_t level, const Attributes& attributes) {
            const auto totals = rules.progression.TotalsAt(level);
            std::int32_t casterModifier = 0;
            if (charClass == CharacterClass::Mage) casterModifier = attributes.GetIntelligenceModifier();
            if (charClass == CharacterClass::Cleric) casterModifier = attributes.GetWisdomModifier();
            return std::max(0, totals.maxManaPoints + casterModifier * level);
        }

        std::int32_t getMaxMoveDistance(CharacterClass charClass, const Attributes& attributes) const {
            std::int32_t baseMoveDistance = 5;
            std::int32_t dexterityBonus = attributes.GetDexterityModifier();

            switch (charClass) {
                case CharacterClass::Warrior: baseMoveDistance = 4; break;
                case CharacterClass::Rogue: baseMoveDistance = 7; break;
                case CharacterClass::Mage: baseMoveDistance = 4; break;
                case CharacterClass::Cleric: baseMoveDistance = 5; break;
                default: break;
            }

            return std::max(1, baseMoveDistance + dexterityBonus);
        }

        std::vector<LB::String> getAbilitiesUnlockedAtLevel(CharacterClass charClass, std::int32_t level) const {
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