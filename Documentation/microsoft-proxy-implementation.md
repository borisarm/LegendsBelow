# Microsoft Proxy Pattern Implementation

## Overview

LegendsBelow uses [Microsoft Proxy](https://github.com/microsoft/proxy) (v4) for repository abstractions: type erasure and polymorphic behavior without inheritance. A repository implementation only needs methods with matching signatures; no base class is required.

## Benefits

### Type Erasure Without Inheritance
- No virtual inheritance trees
- Duck typing: implementations don't inherit from an interface
- Dispatch overhead comparable to or lower than virtual calls, with value semantics

### Clean Service Dependencies
```cpp
class CombatService {
public:
    explicit CombatService(LB::Repositories::SpellRepository spells)
        : spells_(std::move(spells)) {}

    void ProcessCombat() {
        auto damageSpells = spells_->GetSpellsByCategory(LB::Combat::SpellCategory::Damage);
        auto quickSpells  = spells_->GetSpellsByCastTime(1.0f);
    }

private:
    LB::Repositories::SpellRepository spells_;
};
```

### Implementation Freedom
```cpp
class JsonSpellRepository {
public:
    // Just implement the methods with the right signatures - no inheritance.
    std::optional<LB::Character::Spell> GetSpell(SpellId id) const;
    std::vector<SpellId> GetAllSpellIds() const;
    // ... remaining facade methods
};

auto repo = LB::Repositories::Factory::MakeSpellRepository(JsonSpellRepository{});
```

## Current Implementation

Module `LB.Repositories` in `Components/Services/LBAssetPersistenceService/LB.Repositories.ixx` (Services layer, not Domain):

- **`SpellRepositoryFacade`**: built with `pro::facade_builder`, one `PRO_DEF_MEM_DISPATCH` convention per method:
  `GetSpell`, `GetAllSpellIds`, `SpellExists`, `GetSpellsBySchool`, `GetSchoolDescription`, `GetSpellsByClass`, `GetSpellsByLevel`, `GetSpellsByCategory`, `GetSpellsByManaCost`, `GetSpellsByCastTime`. Supports copy.
- **`SpellRepository`**: `pro::proxy<SpellRepositoryFacade>`.
- **Factory functions** (`LB::Repositories::Factory`):
  - `MakeSpellRepository(impl)` → `pro::make_proxy`
  - `MakeSpellRepositoryWithAllocator(alloc, impl)` → `pro::allocate_proxy`
  - `MakeSharedSpellRepository(impl)` → `pro::make_proxy_shared`

Consumers: `Components/Tests/Services/LBAssetPersistenceServiceTests/` (`RepositoryTestHelpers.h` holds the mock).

Note: the domain `LB::Character::Spell` is still minimal (id, name, mana cost, base damage). School, class, level, category, and cast time are not on the entity yet, so implementations must keep that metadata alongside it, as the test mock does.

Facade definition pattern:
```cpp
PRO_DEF_MEM_DISPATCH(MemGetSpell, GetSpell);
// ...

struct SpellRepositoryFacade : pro::facade_builder
    ::add_convention<MemGetSpell, std::optional<LB::Character::Spell>(SpellId) const>
    // ...
    ::support_copy<pro::constraint_level::nontrivial>
    ::build {};

using SpellRepository = pro::proxy<SpellRepositoryFacade>;
```

## Status and Next Steps

- [x] Microsoft Proxy v4 facade for spells
- [x] `proxy` (4.1.0) declared in the root `vcpkg.json`; `LB.Repositories.ixx` includes `<proxy/proxy.h>` in its global module fragment
- [x] Tests through the type-erased proxy with a duck-typed `MockSpellRepository` (all three factory functions, every query)
- [ ] Weapon and armor facades (`WeaponRepository`, `ArmorRepository`) following the spell pattern
- [x] Content-backed spell repository: `LB::Content::ContentSpellRepository` / `MakeContentSpellRepository` (`LB.ContentRepositories`) over the loaded `ContentDatabase`
- [ ] Inject repositories into services from `LB.Engine` (see `modularized-game-architecture.md`)

## Usage Examples

### Creating Repository Proxies
```cpp
auto repo = LB::Repositories::Factory::MakeSpellRepository(JsonSpellRepository{});

auto all     = repo->GetAllSpellIds();
auto evokers = repo->GetSpellsBySchool(LB::Combat::SpellSchool::Evocation);
```

### Service Injection
```cpp
void GameEngine::Initialize() {
    auto spells = LB::Repositories::Factory::MakeSpellRepository(JsonSpellRepository{});
    m_combatService = std::make_unique<CombatService>(std::move(spells));
}
```
