#pragma once
// RepositoryTestHelpers.h - Test utilities for repository implementations
// This file contains mock and example implementations for testing purposes

import LB.Domain;
import LB.Repositories;
import std;

namespace LB::Testing::Repositories {

	/// <summary>
	/// Mock spell repository for testing purposes
	/// Notice: No inheritance from any interface required!
	/// Microsoft Proxy uses duck typing to adapt this automatically
	/// </summary>
	class MockSpellRepository {
	private:
		std::vector<LB::Domain::Spell> spells_;

	public:
		// Constructor for test data
		MockSpellRepository() {
			// You can populate with test data here if needed
		}

		// Just implement methods with right signatures - no inheritance needed!
		std::optional<LB::Domain::Spell> GetSpell(LB::Domain::SpellId id) const {
			
			auto it = std::find_if(spells_.begin(), spells_.end(),
				[id](const LB::Domain::Spell& spell) { return spell.GetId() == id; });
			if (it != spells_.end()) {
				return *it	;
			}
			return std::nullopt;
		}

		std::vector<LB::Domain::SpellId> GetAllSpellIds() const {
			std::vector<LB::Domain::SpellId> ids;
			for (const auto& spell : spells_) ids.push_back(spell.GetId());
			return ids;
		}

		bool SpellExists(LB::Domain::SpellId id) const {
			return GetSpell(id).has_value();
		}

		std::vector<LB::Domain::SpellId> GetSpellsBySchool(LB::Domain::SpellSchool school) const {
			std::vector<LB::Domain::SpellId> ids;
			for (const auto& spell : spells_) {
				if (spell.GetSchool() == school) ids.push_back(spell.GetId());
			}
			return ids;
		}

		std::optional<LB::String> GetSchoolDescription(LB::Domain::SpellSchool school) const {
			return LB::String(LB::Domain::ToString(school));
		}

		std::vector<LB::Domain::SpellId> GetSpellsByClass(LB::Domain::CharacterClass characterClass) const {
			std::vector<LB::Domain::SpellId> ids;
			for (const auto& spell : spells_) {
				if (spell.CanBeCastBy(characterClass)) ids.push_back(spell.GetId());
			}
			return ids;
		}

		std::vector<LB::Domain::SpellId> GetSpellsByLevel(int32_t spellLevel) const {
			std::vector<LB::Domain::SpellId> ids;
			for (const auto& spell : spells_) {
				if (spell.GetLevel() == spellLevel) ids.push_back(spell.GetId());
			}
			return ids;
		}

		std::vector<LB::Domain::SpellId> GetSpellsByCategory(LB::Domain::SpellCategory category) const {
			std::vector<LB::Domain::SpellId> ids;
			for (const auto& spell : spells_) {
				const auto& categories = spell.GetCategories();
				if (std::find(categories.begin(), categories.end(), category) != categories.end()) {
					ids.push_back(spell.GetId());
				}
			}
			return ids;
		}

		std::vector<LB::Domain::SpellId> GetSpellsByManaCost(int32_t minCost, int32_t maxCost) const {
			std::vector<LB::Domain::SpellId> ids;
			for (const auto& spell : spells_) {
				int32_t cost = spell.GetManaCost();
				if (cost >= minCost && cost <= maxCost) ids.push_back(spell.GetId());
			}
			return ids;
		}

		std::vector<LB::Domain::SpellId> GetSpellsByCastTime(float maxCastTime) const {
			std::vector<LB::Domain::SpellId> ids;
			for (const auto& spell : spells_) {
				if (spell.GetCastTimeSeconds() <= maxCastTime) ids.push_back(spell.GetId());
			}
			return ids;
		}

		// Test-specific utilities
		void AddSpell(const LB::Domain::Spell& spell) {
			spells_.push_back(spell);
		}

		void Clear() {
			spells_.clear();
		}

		size_t Count() const {
			return spells_.size();
		}
	}; // End of MockSpellRepository class

	/// <summary>
	/// Test utilities for demonstrating Microsoft Proxy 4.0 usage
	/// </summary>
	namespace TestUtilities {

		/// <summary>
		/// Create a test spell repository for unit testing
		/// </summary>
		inline LB::Repositories::SpellRepository CreateTestRepository() {
			MockSpellRepository mockRepo;
			// Add some test data if needed
			return LB::Repositories::Factory::MakeSpellRepository(std::move(mockRepo));
		}

		/// <summary>
		/// Example usage demonstrating Microsoft Proxy 4.0 in tests
		/// Shows the power of professional-grade type erasure for testing
		/// </summary>
		inline void ExampleUsage() {
			// Create mock implementation for testing
			MockSpellRepository mockRepo;

			// Create type-erased proxy using Microsoft Proxy 4.0
			auto spellRepo = LB::Repositories::Factory::MakeSpellRepository(std::move(mockRepo));

			// Use the proxy in tests - full type safety, zero coupling, optimal performance
			auto allSpells = spellRepo->GetAllSpellIds();
			auto fireSpells = spellRepo->GetSpellsBySchool(LB::Domain::SpellSchool::Evocation);
			auto quickSpells = spellRepo->GetSpellsByCastTime(1.0f);

			// Microsoft Proxy 4.0 achieved: Professional type erasure for testing!
		}

	} // namespace TestUtilities

} // namespace LB::Testing::Repositories