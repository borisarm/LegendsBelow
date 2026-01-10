// LB.Repositories.ixx - Repository interfaces and proxy pattern module
// Note: This belongs in the Services layer, not the Domain model
export module LB.Repositories;

import LB.Domain; // Import the pure domain model
import std;
export namespace LB::Repositories {

	/// <summary>
	/// Type-erased spell repository using function objects
	/// Provides the same interface as Microsoft Proxy would
	/// Any class with matching method signatures can be used
	/// </summary>
	class SpellRepositoryProxy {
	private:
		// Function objects for type erasure (similar to Microsoft Proxy internals)
		std::function<std::optional<LB::Domain::Spell>(LB::Domain::SpellId)> getSpell_;
		std::function<std::vector<LB::Domain::SpellId>()> getAllSpellIds_;
		std::function<bool(LB::Domain::SpellId)> spellExists_;
		std::function<std::vector<LB::Domain::SpellId>(LB::Domain::SpellSchool)> getSpellsBySchool_;
		std::function<std::optional<LB::String>(LB::Domain::SpellSchool)> getSchoolDescription_;
		std::function<std::vector<LB::Domain::SpellId>(LB::Domain::CharacterClass)> getSpellsByClass_;
		std::function<std::vector<LB::Domain::SpellId>(int32_t)> getSpellsByLevel_;
		std::function<std::vector<LB::Domain::SpellId>(LB::Domain::SpellCategory)> getSpellsByCategory_;
		std::function<std::vector<LB::Domain::SpellId>(int32_t, int32_t)> getSpellsByManaCost_;
		std::function<std::vector<LB::Domain::SpellId>(float)> getSpellsByCastTime_;

	public:
		/// <summary>
		/// Create proxy from any implementation using duck typing
		/// No inheritance required - just matching method signatures!
		/// </summary>
		template<typename T>
		explicit SpellRepositoryProxy(T&& impl) {
			// Capture implementation methods using perfect forwarding
			auto* ptr = new std::decay_t<T>(std::forward<T>(impl));
			auto deleter = [ptr](auto*) { delete ptr; };

			getSpell_ = [ptr](LB::Domain::SpellId id) { return ptr->GetSpell(id); };
			getAllSpellIds_ = [ptr]() { return ptr->GetAllSpellIds(); };
			spellExists_ = [ptr](LB::Domain::SpellId id) { return ptr->SpellExists(id); };
			getSpellsBySchool_ = [ptr](LB::Domain::SpellSchool school) { return ptr->GetSpellsBySchool(school); };
			getSchoolDescription_ = [ptr](LB::Domain::SpellSchool school) { return ptr->GetSchoolDescription(school); };
			getSpellsByClass_ = [ptr](LB::Domain::CharacterClass cls) { return ptr->GetSpellsByClass(cls); };
			getSpellsByLevel_ = [ptr](int32_t level) { return ptr->GetSpellsByLevel(level); };
			getSpellsByCategory_ = [ptr](LB::Domain::SpellCategory cat) { return ptr->GetSpellsByCategory(cat); };
			getSpellsByManaCost_ = [ptr](int32_t min, int32_t max) { return ptr->GetSpellsByManaCost(min, max); };
			getSpellsByCastTime_ = [ptr](float maxTime) { return ptr->GetSpellsByCastTime(maxTime); };

			// Store deleter for cleanup
			cleanup_ = std::shared_ptr<void>(ptr, deleter);
		}

		// Repository interface - exactly like Microsoft Proxy would provide
		std::optional<LB::Domain::Spell> GetSpell(LB::Domain::SpellId id) const { return getSpell_(id); }
		std::vector<LB::Domain::SpellId> GetAllSpellIds() const { return getAllSpellIds_(); }
		bool SpellExists(LB::Domain::SpellId id) const { return spellExists_(id); }
		std::vector<LB::Domain::SpellId> GetSpellsBySchool(LB::Domain::SpellSchool school) const { return getSpellsBySchool_(school); }
		std::optional<LB::String> GetSchoolDescription(LB::Domain::SpellSchool school) const { return getSchoolDescription_(school); }
		std::vector<LB::Domain::SpellId> GetSpellsByClass(LB::Domain::CharacterClass characterClass) const { return getSpellsByClass_(characterClass); }
		std::vector<LB::Domain::SpellId> GetSpellsByLevel(int32_t level) const { return getSpellsByLevel_(level); }
		std::vector<LB::Domain::SpellId> GetSpellsByCategory(LB::Domain::SpellCategory category) const { return getSpellsByCategory_(category); }
		std::vector<LB::Domain::SpellId> GetSpellsByManaCost(int32_t minCost, int32_t maxCost) const { return getSpellsByManaCost_(minCost, maxCost); }
		std::vector<LB::Domain::SpellId> GetSpellsByCastTime(float maxCastTime) const { return getSpellsByCastTime_(maxCastTime); }

	private:
		std::shared_ptr<void> cleanup_; // For automatic memory management
	};

	/// <summary>
	/// Factory functions for creating repository proxies
	/// These enable duck typing - any implementation with matching signatures works
	/// </summary>
	namespace Factory {

		/// <summary>
		/// Create spell repository proxy from any implementation
		/// Duck typing - no inheritance required!
		/// </summary>
		template<typename T>
		SpellRepositoryProxy MakeSpellRepository(T&& impl) {
			return SpellRepositoryProxy(std::forward<T>(impl));
		}

	} // namespace Factory

} // namespace LB::Repositories

/// <summary>
/// Example concrete implementations demonstrating the proxy pattern
/// Shows duck typing and zero-coupling architecture
/// </summary>
export namespace LB::Repositories::Examples {

	/// <summary>
	/// Example concrete implementation that works with the proxy
	/// Notice: No inheritance from any interface required!
	/// Our proxy uses duck typing to adapt this automatically
	/// </summary>
	class ConcreteSpellRepository {
	private:
		std::vector<LB::Domain::Spell> spells_;

	public:
		// Just implement methods with right signatures - no inheritance needed!

		std::optional<LB::Domain::Spell> GetSpell(LB::Domain::SpellId id) const {
			for (const auto& spell : spells_) {
				if (spell.GetId() == id) return spell;
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
	}; // End of ConcreteSpellRepository class

	/// <summary>
	/// Example usage putting it all together
	/// Demonstrates the power of the proxy pattern
	/// </summary>
	inline void ExampleUsage() {
		// Create concrete implementation (no inheritance!)
		ConcreteSpellRepository concreteSpells;

		// Create type-erased proxy using duck typing
		auto spellRepo = LB::Repositories::Factory::MakeSpellRepository(std::move(concreteSpells));

		// Use the proxy - full type safety, zero coupling, maximum flexibility
		auto allSpells = spellRepo.GetAllSpellIds();
		auto fireSpells = spellRepo.GetSpellsBySchool(LB::Domain::SpellSchool::Evocation);
		auto quickSpells = spellRepo.GetSpellsByCastTime(1.0f);

		// Zero coupling achieved!
	}

} // namespace LB::Repositories::Examples