// LB.Repositories.ixx - Repository interfaces using Microsoft Proxy 4.0
// Note: This belongs in the Services layer, not the Domain model
export module LB.Repositories;

import LB.Domain; // Import the pure domain model
import std;
import <proxy/proxy.h>; // Microsoft Proxy 4.0

export namespace LB::Repositories {

	// Define the repository facade using Microsoft Proxy 4.0
	PRO_DEF_MEM_DISPATCH(MemGetSpell, GetSpell);
	PRO_DEF_MEM_DISPATCH(MemGetAllSpellIds, GetAllSpellIds);
	PRO_DEF_MEM_DISPATCH(MemSpellExists, SpellExists);
	PRO_DEF_MEM_DISPATCH(MemGetSpellsBySchool, GetSpellsBySchool);
	PRO_DEF_MEM_DISPATCH(MemGetSchoolDescription, GetSchoolDescription);
	PRO_DEF_MEM_DISPATCH(MemGetSpellsByClass, GetSpellsByClass);
	PRO_DEF_MEM_DISPATCH(MemGetSpellsByLevel, GetSpellsByLevel);
	PRO_DEF_MEM_DISPATCH(MemGetSpellsByCategory, GetSpellsByCategory);
	PRO_DEF_MEM_DISPATCH(MemGetSpellsByManaCost, GetSpellsByManaCost);
	PRO_DEF_MEM_DISPATCH(MemGetSpellsByCastTime, GetSpellsByCastTime);

	/// <summary>
	/// Spell repository facade using Microsoft Proxy 4.0
	/// Any class with matching method signatures can be used with duck typing
	/// </summary>
	struct SpellRepositoryFacade : pro::facade_builder
		::add_convention<MemGetSpell, std::optional<LB::Domain::Spell>(LB::Domain::SpellId) const>
		::add_convention<MemGetAllSpellIds, std::vector<LB::Domain::SpellId>() const>
		::add_convention<MemSpellExists, bool(LB::Domain::SpellId) const>
		::add_convention<MemGetSpellsBySchool, std::vector<LB::Domain::SpellId>(LB::Domain::SpellSchool) const>
		::add_convention<MemGetSchoolDescription, std::optional<LB::String>(LB::Domain::SpellSchool) const>
		::add_convention<MemGetSpellsByClass, std::vector<LB::Domain::SpellId>(LB::Domain::CharacterClass) const>
		::add_convention<MemGetSpellsByLevel, std::vector<LB::Domain::SpellId>(int32_t) const>
		::add_convention<MemGetSpellsByCategory, std::vector<LB::Domain::SpellId>(LB::Domain::SpellCategory) const>
		::add_convention<MemGetSpellsByManaCost, std::vector<LB::Domain::SpellId>(int32_t, int32_t) const>
		::add_convention<MemGetSpellsByCastTime, std::vector<LB::Domain::SpellId>(float) const>
		::support_copy<pro::constraint_level::nontrivial>
		::build {};

	/// <summary>
	/// Type alias for spell repository using Microsoft Proxy 4.0
	/// This provides duck typing with zero-overhead abstractions
	/// </summary>
	using SpellRepository = pro::proxy<SpellRepositoryFacade>;

	/// <summary>
	/// Factory functions for creating repository proxies using Microsoft Proxy
	/// </summary>
	namespace Factory {

		/// <summary>
		/// Create spell repository proxy from any implementation
		/// Duck typing - no inheritance required!
		/// Uses Microsoft Proxy 4.0 for optimal performance and memory management
		/// </summary>
		template<typename T>
		SpellRepository MakeSpellRepository(T&& impl) {
			return pro::make_proxy<SpellRepositoryFacade>(std::forward<T>(impl));
		}

		/// <summary>
		/// Create spell repository with custom allocator
		/// </summary>
		template<typename Allocator, typename T>
		SpellRepository MakeSpellRepositoryWithAllocator(const Allocator& alloc, T&& impl) {
			return pro::allocate_proxy<SpellRepositoryFacade>(alloc, std::forward<T>(impl));
		}

		/// <summary>
		/// Create shared spell repository
		/// </summary>
		template<typename T>
		SpellRepository MakeSharedSpellRepository(T&& impl) {
			return pro::make_proxy_shared<SpellRepositoryFacade>(std::forward<T>(impl));
		}

	} // namespace Factory

} // namespace LB::Repositories