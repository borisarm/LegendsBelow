#pragma once
// RepositoryTestHelpers.h - Test doubles for repository facades
//
// MockSpellRepository satisfies LB::Repositories::SpellRepositoryFacade purely by having
// methods with matching signatures: no inheritance, Microsoft Proxy adapts it.

import LB.Core;
import LB.Character;
import LB.Combat;
import LB.Repositories;
import std;

namespace LB::Testing::Repositories {

	using LB::Character::CharacterClass;
	using LB::Character::Spell;
	using LB::Character::SpellId;
	using LB::Combat::SpellCategory;
	using LB::Combat::SpellSchool;

	/// <summary>
	/// A spell plus the metadata the repository queries filter on.
	/// The domain Spell entity is still minimal, so the mock carries this alongside it.
	/// </summary>
	struct SpellRecord {
		Spell spell;
		SpellSchool school{ SpellSchool::None };
		std::vector<CharacterClass> classes;
		std::int32_t level{ 1 };
		std::vector<SpellCategory> categories;
		float castTimeSeconds{ 0.0f };
	};

	/// <summary>
	/// In-memory spell repository for tests
	/// </summary>
	class MockSpellRepository {
	public:
		std::optional<Spell> GetSpell(SpellId id) const {
			if (const auto* record = Find(id)) {
				return record->spell;
			}
			return std::nullopt;
		}

		std::vector<SpellId> GetAllSpellIds() const {
			return Select([](const SpellRecord&) { return true; });
		}

		bool SpellExists(SpellId id) const {
			return Find(id) != nullptr;
		}

		std::vector<SpellId> GetSpellsBySchool(SpellSchool school) const {
			return Select([school](const SpellRecord& r) { return r.school == school; });
		}

		std::optional<LB::String> GetSchoolDescription(SpellSchool school) const {
			switch (school) {
				case SpellSchool::Evocation: return LB::String("Direct damage and energy manipulation");
				case SpellSchool::Frost: return LB::String("Cold-based magic with slowing effects");
				case SpellSchool::Holy: return LB::String("Divine magic for healing and protection");
				case SpellSchool::Shadow: return LB::String("Dark magic affecting life force");
				case SpellSchool::Utility: return LB::String("Non-combat spells for exploration");
				default: return std::nullopt;
			}
		}

		std::vector<SpellId> GetSpellsByClass(CharacterClass characterClass) const {
			return Select([characterClass](const SpellRecord& r) {
				return std::ranges::find(r.classes, characterClass) != r.classes.end();
			});
		}

		std::vector<SpellId> GetSpellsByLevel(std::int32_t spellLevel) const {
			return Select([spellLevel](const SpellRecord& r) { return r.level == spellLevel; });
		}

		std::vector<SpellId> GetSpellsByCategory(SpellCategory category) const {
			return Select([category](const SpellRecord& r) {
				return std::ranges::find(r.categories, category) != r.categories.end();
			});
		}

		std::vector<SpellId> GetSpellsByManaCost(std::int32_t minCost, std::int32_t maxCost) const {
			return Select([minCost, maxCost](const SpellRecord& r) {
				return r.spell.manaCost >= minCost && r.spell.manaCost <= maxCost;
			});
		}

		std::vector<SpellId> GetSpellsByCastTime(float maxCastTime) const {
			return Select([maxCastTime](const SpellRecord& r) { return r.castTimeSeconds <= maxCastTime; });
		}

		// Test-specific utilities
		void Add(SpellRecord record) {
			records_.push_back(std::move(record));
		}

		std::size_t Count() const {
			return records_.size();
		}

	private:
		const SpellRecord* Find(SpellId id) const {
			auto it = std::ranges::find_if(records_, [id](const SpellRecord& r) { return r.spell.id == id; });
			return it != records_.end() ? &*it : nullptr;
		}

		template<typename Predicate>
		std::vector<SpellId> Select(Predicate predicate) const {
			std::vector<SpellId> ids;
			for (const auto& record : records_) {
				if (predicate(record)) ids.push_back(record.spell.id);
			}
			return ids;
		}

		std::vector<SpellRecord> records_;
	};

	/// <summary>
	/// Known test spell IDs
	/// </summary>
	namespace TestSpells {
		inline constexpr SpellId MagicMissile = 1;
		inline constexpr SpellId Fireball = 2;
		inline constexpr SpellId FrostNova = 3;
		inline constexpr SpellId DivineLight = 4;
	}

	/// <summary>
	/// A small spell book covering several schools, classes, levels, and categories
	/// </summary>
	inline MockSpellRepository CreateSampleRepository() {
		MockSpellRepository repo;
		repo.Add({ Spell{ TestSpells::MagicMissile, "Magic Missile", 5, 4 }, SpellSchool::Evocation,
			{ CharacterClass::Mage }, 1, { SpellCategory::Damage }, 0.0f });
		repo.Add({ Spell{ TestSpells::Fireball, "Fireball", 30, 28 }, SpellSchool::Evocation,
			{ CharacterClass::Mage }, 3, { SpellCategory::Damage }, 1.5f });
		repo.Add({ Spell{ TestSpells::FrostNova, "Frost Nova", 20, 8 }, SpellSchool::Frost,
			{ CharacterClass::Mage }, 2, { SpellCategory::Damage, SpellCategory::Control }, 0.5f });
		repo.Add({ Spell{ TestSpells::DivineLight, "Divine Light", 15, 0 }, SpellSchool::Holy,
			{ CharacterClass::Cleric }, 1, { SpellCategory::Heal }, 1.0f });
		return repo;
	}

	/// <summary>
	/// Allocator that counts allocations, to verify MakeSpellRepositoryWithAllocator uses it
	/// </summary>
	template<typename T>
	struct CountingAllocator {
		using value_type = T;

		std::shared_ptr<std::size_t> allocations = std::make_shared<std::size_t>(0);

		CountingAllocator() = default;
		template<typename U>
		CountingAllocator(const CountingAllocator<U>& other) noexcept : allocations(other.allocations) {}

		T* allocate(std::size_t n) {
			++*allocations;
			return std::allocator<T>{}.allocate(n);
		}

		void deallocate(T* p, std::size_t n) noexcept {
			std::allocator<T>{}.deallocate(p, n);
		}

		template<typename U>
		bool operator==(const CountingAllocator<U>& other) const noexcept { return allocations == other.allocations; }
	};

} // namespace LB::Testing::Repositories
