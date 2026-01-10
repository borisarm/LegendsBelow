# Microsoft Proxy Pattern Implementation

## Overview

The LegendsBelow project now uses the **Microsoft Proxy pattern** for repository architecture, providing type erasure and polymorphic behavior without traditional inheritance.

## Benefits of Microsoft Proxy Pattern

### ? **Type Erasure Without Inheritance**
- **No virtual inheritance trees**
- **Duck typing** - implementations don't need to inherit from interfaces
- **Zero-overhead abstractions** when using the actual Microsoft Proxy library

### ? **Clean Service Dependencies**
```cpp
class CombatService {
    CombatService(
        Domain::ProxyRepositories::SpellRepositoryProxy spells,
        Domain::ProxyRepositories::WeaponRepositoryProxy weapons)
        : spells_(std::move(spells)), weapons_(std::move(weapons)) {}
        
    void ProcessCombat() {
        auto damageSpells = spells_->GetSpellsByCategory(Domain::SpellCategory::Damage);
        auto quickSpells = spells_->GetSpellsByCastTime(1.0f);
        // Clean, type-safe usage
    }
};
```

### ? **Implementation Freedom**
```cpp
class ConcreteSpellRepository {
    // Just implement the methods with right signatures
    // NO inheritance required!
    
    std::optional<Spell> GetSpell(SpellId id) const { /* your code */ }
    std::vector<SpellId> GetAllSpellIds() const { /* your code */ }
    // ... other methods
};

// Create proxy automatically
auto proxy = RepositoryFactory::MakeSpellRepository(ConcreteSpellRepository{});
```

## Current Implementation

### **Facade Interfaces**
Located in `Components/LBModel/Interfaces/RepositoryComposition.h`:

- **`SpellRepositoryFacade`** - Complete spell operations
- **`WeaponRepositoryFacade`** - Complete weapon operations  
- **`ArmorRepositoryFacade`** - Complete armor operations

### **Proxy Types**
```cpp
using SpellRepositoryProxy = std::unique_ptr<SpellRepositoryFacade>;
using WeaponRepositoryProxy = std::unique_ptr<WeaponRepositoryFacade>;
using ArmorRepositoryProxy = std::unique_ptr<ArmorRepositoryFacade>;
```

### **Factory Functions**
```cpp
template<typename TImpl>
auto MakeSpellRepository(TImpl&& impl) -> SpellRepositoryProxy;

template<typename TImpl>  
auto MakeWeaponRepository(TImpl&& impl) -> WeaponRepositoryProxy;

template<typename TImpl>
auto MakeArmorRepository(TImpl&& impl) -> ArmorRepositoryProxy;
```

## Integration with Microsoft Proxy Library

### **When Microsoft Proxy is Available**

Replace the current facade interfaces with actual Microsoft Proxy definitions:

```cpp
PRO_DEF_FACADE(SpellRepository,
    PRO_MAKE_DISPATCH_PACK(
        PRO_FUNC(GetSpell, std::optional<Spell>(SpellId), const),
        PRO_FUNC(GetAllSpellIds, std::vector<SpellId>(), const),
        PRO_FUNC(GetSpellsBySchool, std::vector<SpellId>(SpellSchool), const),
        // ... all spell operations
    )
);

using SpellRepositoryProxy = pro::proxy<SpellRepository>;
```

### **Factory Functions with Proxy**
```cpp
template<typename TImpl>
auto MakeSpellRepository(TImpl&& impl) {
    return pro::make_proxy<SpellRepository>(std::forward<TImpl>(impl));
}
```

## Architecture Benefits

### **1. Eliminates Template Complexity**
- **Before**: Complex template inheritance chains
- **After**: Simple facade interfaces with type erasure

### **2. Duck Typing Support**  
- **Before**: Implementations must inherit from specific interfaces
- **After**: Implementations just need matching method signatures

### **3. Zero Runtime Overhead**
- **Before**: Virtual function calls with inheritance
- **After**: Microsoft Proxy optimizes to direct calls

### **4. Flexible Implementation**
```cpp
// Implementation can be:
class DatabaseSpellRepository { /* database access */ };
class FileSpellRepository { /* file-based storage */ };  
class CachedSpellRepository { /* in-memory cache */ };

// All work with the same proxy interface automatically!
```

## Migration Path

### **Phase 1: Current State** ?
- Facade interfaces using traditional inheritance
- Factory functions for type erasure
- Full compatibility with existing code

### **Phase 2: Microsoft Proxy Integration** ??
1. Add Microsoft Proxy to vcpkg dependencies
2. Replace facade interfaces with `PRO_DEF_FACADE` definitions
3. Update proxy types to use `pro::proxy<T>`
4. Update factory functions to use `pro::make_proxy<T>`

### **Phase 3: Performance Optimization** ?
- Remove virtual function overhead
- Enable compile-time optimizations
- Zero-cost abstractions achieved

## Usage Examples

### **Creating Repository Proxies**
```cpp
// Any implementation works
ConcreteSpellRepository concrete;
auto proxy = RepositoryFactory::MakeSpellRepository(std::move(concrete));

// Use through type-erased interface
auto spells = proxy->GetAllSpellIds();
auto fireSpells = proxy->GetSpellsBySchool(SpellSchool::Evocation);
```

### **Service Injection**
```cpp
class GameEngine {
    void Initialize() {
        auto spellRepo = RepositoryFactory::MakeSpellRepository(FileSpellRepository{});
        auto weaponRepo = RepositoryFactory::MakeWeaponRepository(DatabaseWeaponRepository{});
        
        auto combatService = std::make_unique<CombatService>(
            std::move(spellRepo), 
            std::move(weaponRepo)
        );
    }
};
```

The Microsoft Proxy pattern provides **clean architecture** with **zero coupling** between services and specific repository implementations, while maintaining **full type safety** and **performance**.

---

*Type erasure without inheritance. Polymorphism without overhead.* ??