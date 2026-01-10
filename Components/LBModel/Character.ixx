export module LB.Entities:Character;

import LB.Common;
import LB.Enums;
import LB.ValueObjects;
import :Entity;

export namespace LB::Domain {
	// Type aliases
	using SpellId = Id;
	using WeaponId = Id;
	using ArmorId = Id;
	using CharacterId = Id;
	using ItemId = Id;

	/// <summary>
   /// Character entity representing player characters and NPCs
   /// Core entity for character management, progression, and gameplay
   /// </summary>
	export class Character : public Entity {
	private:
		// Core character properties
		CharacterClass characterClass_;
		int32_t level_;
		int32_t experience_;
		Attributes attributes_;
		Position position_;

		// Health and mana
		int32_t currentHitPoints_;
		int32_t maxHitPoints_;
		int32_t currentManaPoints_;
		int32_t maxManaPoints_;

		// Status and condition
		bool isAlive_;
		bool isConscious_;

		// Character progression
		int32_t skillPoints_;
		std::vector<String> knownAbilities_;

	public:


		/// <summary>
		/// Create character with basic properties
		/// </summary>
		Character(Id id,
			String name,
			CharacterClass characterClass,
			const Attributes& attributes,
			int32_t level = 1,
			int32_t experience = 0,
			const Position& position = Position(0, 0))
			: Entity(id, std::move(name))
			, characterClass_(characterClass)
			, level_(level)
			, experience_(experience)
			, attributes_(attributes)
			, position_(position)
			, currentHitPoints_(0)
			, maxHitPoints_(0)
			, currentManaPoints_(0)
			, maxManaPoints_(0)
			, isAlive_(true)
			, isConscious_(true)
			, skillPoints_(0)
		{
			// Calculate starting hit points and mana based on class and attributes
			maxHitPoints_ = CalculateMaxHitPoints();
			maxManaPoints_ = CalculateMaxManaPoints();
			currentHitPoints_ = maxHitPoints_;
			currentManaPoints_ = maxManaPoints_;

			// Initialize class-specific starting abilities
			InitializeStartingAbilities();
		}

		/// <summary>
		/// Create character with explicit health/mana values
		/// </summary>
		Character(Id id,
			String name,
			CharacterClass characterClass,
			const Attributes& attributes,
			int32_t level,
			int32_t experience,
			int32_t maxHitPoints,
			int32_t maxManaPoints,
			const Position& position = Position(0, 0))
			: Entity(id, std::move(name))
			, characterClass_(characterClass)
			, level_(level)
			, experience_(experience)
			, attributes_(attributes)
			, position_(position)
			, currentHitPoints_(maxHitPoints)
			, maxHitPoints_(maxHitPoints)
			, currentManaPoints_(maxManaPoints)
			, maxManaPoints_(maxManaPoints)
			, isAlive_(true)
			, isConscious_(true)
			, skillPoints_(0)
		{
			InitializeStartingAbilities();
		}

		// Core Properties
		[[nodiscard]] CharacterClass GetCharacterClass() const noexcept { return characterClass_; }
		[[nodiscard]] int32_t GetLevel() const noexcept { return level_; }
		[[nodiscard]] int32_t GetExperience() const noexcept { return experience_; }
		[[nodiscard]] const Attributes& GetAttributes() const noexcept { return attributes_; }
		[[nodiscard]] const Position& GetPosition() const noexcept { return position_; }

		// Health and Mana
		[[nodiscard]] int32_t GetCurrentHitPoints() const noexcept { return currentHitPoints_; }
		[[nodiscard]] int32_t GetMaxHitPoints() const noexcept { return maxHitPoints_; }
		[[nodiscard]] int32_t GetCurrentManaPoints() const noexcept { return currentManaPoints_; }
		[[nodiscard]] int32_t GetMaxManaPoints() const noexcept { return maxManaPoints_; }

		// Status
		[[nodiscard]] bool IsAlive() const noexcept { return isAlive_; }
		[[nodiscard]] bool IsConscious() const noexcept { return isConscious_; }
		[[nodiscard]] bool IsDefeated() const noexcept { return !isAlive_ || !isConscious_; }

		// Progression
		[[nodiscard]] int32_t GetSkillPoints() const noexcept { return skillPoints_; }
		[[nodiscard]] const std::vector<String>& GetKnownAbilities() const noexcept { return knownAbilities_; }

		// Derived Properties
		[[nodiscard]] double GetHealthPercentage() const noexcept {
			return maxHitPoints_ > 0 ? static_cast<double>(currentHitPoints_) / maxHitPoints_ : 0.0;
		}

		[[nodiscard]] double GetManaPercentage() const noexcept {
			return maxManaPoints_ > 0 ? static_cast<double>(currentManaPoints_) / maxManaPoints_ : 0.0;
		}

		[[nodiscard]] int32_t GetArmorClass() const noexcept {
			return CalculateArmorClass();
		}

		[[nodiscard]] int32_t GetAttackBonus() const noexcept {
			return CalculateAttackBonus();
		}

		[[nodiscard]] int32_t GetSpellPowerBonus() const noexcept {
			return CalculateSpellPowerBonus();
		}

		// Mutators
		void SetLevel(int32_t level) {
			if (level > 0) {
				level_ = level;
				RecalculateMaxHitPoints();
				RecalculateMaxManaPoints();
			}
		}

		void SetExperience(int32_t experience) {
			if (experience >= 0) {
				experience_ = experience;
			}
		}

		void SetAttributes(const Attributes& attributes) {
			attributes_ = attributes;
			RecalculateMaxHitPoints();
			RecalculateMaxManaPoints();
		}

		void SetPosition(const Position& position) {
			position_ = position;
		}

		// Health Management
		void TakeDamage(int32_t damage) {
			if (damage <= 0 || !isAlive_) return;

			currentHitPoints_ = std::max(0, currentHitPoints_ - damage);

			if (currentHitPoints_ <= 0) {
				isAlive_ = false;
				isConscious_ = false;
				currentHitPoints_ = 0;
			}
			else if (currentHitPoints_ <= maxHitPoints_ / 4) {
				isConscious_ = false; // Unconscious when at 25% health or less
			}
		}

		void Heal(int32_t healAmount) {
			if (healAmount <= 0 || !isAlive_) return;

			currentHitPoints_ = std::min(maxHitPoints_, currentHitPoints_ + healAmount);

			// Regain consciousness if healed above 25% health
			if (currentHitPoints_ >= maxHitPoints_ / 4) {
				isConscious_ = true;
			}
		}

		void RestoreToMaxHealth() {
			if (isAlive_) {
				currentHitPoints_ = maxHitPoints_;
				isConscious_ = true;
			}
		}

		void IncreaseMaxHitPoints(int32_t increase) {
			if (increase > 0) {
				maxHitPoints_ += increase;
			}
		}

		// Mana Management
		bool SpendMana(int32_t manaCost) {
			if (manaCost <= 0 || currentManaPoints_ < manaCost) {
				return false;
			}
			currentManaPoints_ -= manaCost;
			return true;
		}

		void RestoreMana(int32_t manaAmount) {
			if (manaAmount <= 0) return;
			currentManaPoints_ = std::min(maxManaPoints_, currentManaPoints_ + manaAmount);
		}

		void RestoreToMaxMana() {
			currentManaPoints_ = maxManaPoints_;
		}

		void IncreaseMaxManaPoints(int32_t increase) {
			if (increase > 0) {
				maxManaPoints_ += increase;
			}
		}

		/// <summary>
		/// Consume mana for spell casting
		/// </summary>
		void ConsumeMana(int32_t amount) {
			if (amount <= 0) return;
			currentManaPoints_ = std::max(0, currentManaPoints_ - amount);
		}

		/// <summary>
		/// Revive a dead character
		/// </summary>
		void Revive() {
			isAlive_ = true;
			isConscious_ = true;
			currentHitPoints_ = 1; // Revive with minimal HP
		}

		// Progression
		void AwardExperience(int32_t experiencePoints) {
			if (experiencePoints > 0) {
				experience_ += experiencePoints;
			}
		}

		void AwardSkillPoints(int32_t skillPoints) {
			if (skillPoints > 0) {
				skillPoints_ += skillPoints;
			}
		}

		bool SpendSkillPoints(int32_t cost) {
			if (cost <= 0 || skillPoints_ < cost) {
				return false;
			}
			skillPoints_ -= cost;
			return true;
		}

		void LearnAbility(const String& abilityName) {
			// Avoid duplicates
			if (std::find(knownAbilities_.begin(), knownAbilities_.end(), abilityName) == knownAbilities_.end()) {
				knownAbilities_.push_back(abilityName);
			}
		}

		[[nodiscard]] bool KnowsAbility(const String& abilityName) const {
			return std::find(knownAbilities_.begin(), knownAbilities_.end(), abilityName) != knownAbilities_.end();
		}

		// Level Up (recalculates everything)
		void LevelUp() {
			level_++;
			RecalculateMaxHitPoints();
			RecalculateMaxManaPoints();

			// Award skill points based on class and intelligence
			int32_t skillPointsGained = CalculateSkillPointsPerLevel();
			skillPoints_ += skillPointsGained;

			// Learn class-specific abilities at certain levels
			auto newAbilities = GetAbilitiesForLevel(level_);
			for (const auto& ability : newAbilities) {
				LearnAbility(ability);
			}
		}

		// Class-specific ability checks
		[[nodiscard]] bool CanCastSpells() const noexcept {
			return characterClass_ == CharacterClass::Mage ||
				characterClass_ == CharacterClass::Cleric;
		}

		[[nodiscard]] bool CanWearHeavyArmor() const noexcept {
			return characterClass_ == CharacterClass::Warrior;
		}

		[[nodiscard]] bool CanUseStealthAttacks() const noexcept {
			return characterClass_ == CharacterClass::Rogue;
		}

		[[nodiscard]] bool CanChannelDivineEnergy() const noexcept {
			return characterClass_ == CharacterClass::Cleric;
		}

	private:
		[[nodiscard]] int32_t CalculateMaxHitPoints() const {
			int32_t baseHP = 10; // Base hit points
			int32_t constitutionBonus = attributes_.GetConstitutionModifier();
			int32_t levelBonus = (level_ - 1) * GetHitPointsPerLevel();

			// Class-specific HP modifiers
			int32_t classBonus = 0;
			switch (characterClass_) {
			case CharacterClass::Warrior: classBonus = 8; break;  // Hardy fighters
			case CharacterClass::Rogue: classBonus = 4; break;    // Agile but not as tough
			case CharacterClass::Mage: classBonus = 0; break;     // Frail spellcasters
			case CharacterClass::Cleric: classBonus = 6; break;   // Moderately tough
			default: break;
			}

			return std::max(1, baseHP + classBonus + constitutionBonus + levelBonus);
		}

		[[nodiscard]] int32_t CalculateMaxManaPoints() const {
			int32_t baseMP = 0;
			int32_t intelligenceBonus = attributes_.GetIntelligenceModifier();
			int32_t wisdomBonus = attributes_.GetWisdomModifier();
			int32_t levelBonus = (level_ - 1) * GetManaPointsPerLevel();

			// Class-specific mana
			switch (characterClass_) {
			case CharacterClass::Warrior:
				baseMP = 0;
				break;
			case CharacterClass::Rogue:
				baseMP = 10 + intelligenceBonus;
				break;
			case CharacterClass::Mage:
				baseMP = 20 + (intelligenceBonus * 3) + levelBonus;
				break;
			case CharacterClass::Cleric:
				baseMP = 15 + (wisdomBonus * 2) + levelBonus;
				break;
			default:
				break;
			}

			return std::max(0, baseMP);
		}

		[[nodiscard]] int32_t GetHitPointsPerLevel() const {
			switch (characterClass_) {
			case CharacterClass::Warrior: return 6; // d8 + 2 average
			case CharacterClass::Rogue: return 4;   // d6 + 1 average
			case CharacterClass::Mage: return 2;    // d4 average
			case CharacterClass::Cleric: return 4;  // d6 + 1 average
			default: return 3;
			}
		}

		[[nodiscard]] int32_t GetManaPointsPerLevel() const {
			switch (characterClass_) {
			case CharacterClass::Warrior: return 0;
			case CharacterClass::Rogue: return 1;
			case CharacterClass::Mage: return 8;
			case CharacterClass::Cleric: return 5;
			default: return 0;
			}
		}

		void RecalculateMaxHitPoints() {
			int32_t oldMax = maxHitPoints_;
			maxHitPoints_ = CalculateMaxHitPoints();

			// Adjust current HP proportionally if max changed
			if (oldMax > 0 && maxHitPoints_ != oldMax) {
				double ratio = static_cast<double>(currentHitPoints_) / oldMax;
				currentHitPoints_ = static_cast<int32_t>(maxHitPoints_ * ratio);
			}
		}

		void RecalculateMaxManaPoints() {
			int32_t oldMax = maxManaPoints_;
			maxManaPoints_ = CalculateMaxManaPoints();

			// Adjust current mana proportionally if max changed
			if (oldMax > 0 && maxManaPoints_ != oldMax) {
				double ratio = static_cast<double>(currentManaPoints_) / oldMax;
				currentManaPoints_ = static_cast<int32_t>(maxManaPoints_ * ratio);
			}
		}

		[[nodiscard]] int32_t CalculateArmorClass() const {
			int32_t baseAC = 10; // Base armor class
			int32_t dexterityBonus = attributes_.GetDexterityModifier();

			// Class-specific AC modifiers (assuming no equipment)
			int32_t classBonus = 0;
			switch (characterClass_) {
			case CharacterClass::Warrior: classBonus = 2; break; // Natural fighting stance
			case CharacterClass::Rogue: classBonus = 1; break;   // Agile dodging
			case CharacterClass::Mage: classBonus = 0; break;    // No natural armor
			case CharacterClass::Cleric: classBonus = 1; break;  // Divine protection
			default: break;
			}

			return baseAC + dexterityBonus + classBonus;
		}

		[[nodiscard]] int32_t CalculateAttackBonus() const {
			int32_t strengthBonus = attributes_.GetStrengthModifier();
			int32_t dexterityBonus = attributes_.GetDexterityModifier();
			int32_t levelBonus = level_ / 2; // Half level bonus

			// Use best attribute for attack (STR for melee, DEX for ranged/finesse)
			int32_t attributeBonus = std::max(strengthBonus, dexterityBonus);

			// Class-specific attack bonuses
			int32_t classBonus = 0;
			switch (characterClass_) {
			case CharacterClass::Warrior: classBonus = 2; break; // Combat training
			case CharacterClass::Rogue: classBonus = 1; break;   // Precision attacks
			case CharacterClass::Mage: classBonus = 0; break;    // Poor physical combat
			case CharacterClass::Cleric: classBonus = 1; break;  // Moderate training
			default: break;
			}

			return attributeBonus + levelBonus + classBonus;
		}

		[[nodiscard]] int32_t CalculateSpellPowerBonus() const {
			int32_t intelligenceBonus = attributes_.GetIntelligenceModifier();
			int32_t wisdomBonus = attributes_.GetWisdomModifier();
			int32_t levelBonus = level_ / 3; // Third level bonus

			// Use best casting attribute based on class
			int32_t attributeBonus = 0;
			switch (characterClass_) {
			case CharacterClass::Mage:
				attributeBonus = intelligenceBonus;
				break;
			case CharacterClass::Cleric:
				attributeBonus = wisdomBonus;
				break;
			case CharacterClass::Rogue:
				attributeBonus = intelligenceBonus / 2; // Limited spell ability
				break;
			default:
				attributeBonus = 0;
				break;
			}

			return attributeBonus + levelBonus;
		}

		[[nodiscard]] int32_t CalculateSkillPointsPerLevel() const {
			int32_t intelligenceBonus = std::max(0, attributes_.GetIntelligenceModifier());
			int32_t baseSkillPoints = 2; // Base skill points per level

			// Class-specific skill point modifiers
			int32_t classBonus = 0;
			switch (characterClass_) {
			case CharacterClass::Warrior: classBonus = 1; break; // Combat skills
			case CharacterClass::Rogue: classBonus = 4; break;   // Skill specialist
			case CharacterClass::Mage: classBonus = 2; break;    // Academic knowledge
			case CharacterClass::Cleric: classBonus = 2; break;  // Divine wisdom
			default: break;
			}

			return baseSkillPoints + classBonus + intelligenceBonus;
		}

		void InitializeStartingAbilities() {
			// Class-specific starting abilities
			switch (characterClass_) {
			case CharacterClass::Warrior:
				LearnAbility("Weapon Proficiency");
				LearnAbility("Heavy Armor Proficiency");
				break;
			case CharacterClass::Rogue:
				LearnAbility("Stealth");
				LearnAbility("Lock Picking");
				LearnAbility("Trap Detection");
				break;
			case CharacterClass::Mage:
				LearnAbility("Arcane Magic");
				LearnAbility("Spell Focus");
				LearnAbility("Cantrips");
				break;
			case CharacterClass::Cleric:
				LearnAbility("Divine Magic");
				LearnAbility("Channel Divinity");
				LearnAbility("Healing Touch");
				break;
			default:
				break;
			}
		}

		[[nodiscard]] std::vector<String> GetAbilitiesForLevel(int32_t level) const {
			std::vector<String> abilities;

			switch (characterClass_) {
			case CharacterClass::Warrior:
				if (level == 2) abilities.push_back("Power Attack");
				if (level == 3) abilities.push_back("Cleave");
				if (level == 5) abilities.push_back("Extra Attack");
				break;
			case CharacterClass::Rogue:
				if (level == 2) abilities.push_back("Sneak Attack");
				if (level == 3) abilities.push_back("Backstab");
				if (level == 5) abilities.push_back("Evasion");
				break;
			case CharacterClass::Mage:
				if (level == 2) abilities.push_back("Magic Missile");
				if (level == 3) abilities.push_back("Shield");
				if (level == 5) abilities.push_back("Fireball");
				break;
			case CharacterClass::Cleric:
				if (level == 2) abilities.push_back("Turn Undead");
				if (level == 3) abilities.push_back("Cure Wounds");
				if (level == 5) abilities.push_back("Divine Favor");
				break;
			default:
				break;
			}

						return abilities;
					}
				};

			}