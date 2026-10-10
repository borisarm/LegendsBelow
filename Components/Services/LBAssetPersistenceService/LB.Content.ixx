// LB.Content.ixx - Game content definitions loaded from Assets/Data JSON
//
// Definitions are the static, data-driven description of content (what a Fireball is);
// runtime state (a character's equipped sword) lives in the domain modules.
export module LB.Content;

import LB.Core;
import LB.Character;
import LB.Combat;
import std;

using namespace std;

export namespace LB::Content {
    using LB::String;
    using LB::Character::CharacterClass;
    using LB::Character::ArmorAdjustment;
    using LB::Character::AttributeGrowth;
    using LB::Character::ClassLevelTotals;
    using LB::Character::ClassProgression;
    using LB::Combat::ArmorCategory;
    using LB::Combat::DamageType;
    using LB::Combat::EquipmentSlot;
    using LB::Combat::SpellCategory;
    using LB::Combat::SpellSchool;
    using LB::Combat::WeaponType;

    /// <summary>
    /// Numeric id assigned at load time (1-based, in file order). Stable for a given data file;
    /// anything persisted (saves) should store the string key instead.
    /// </summary>
    using ContentId = LB::Id;

    /// <summary>
    /// Spell definition (Assets/Data/spells.json, design-spells-system.md)
    /// </summary>
    struct SpellDefinition {
        ContentId id{0};
        String key;                                   // e.g. "spell_evocation_arcane_bolt_l1"
        String name;
        SpellSchool school{SpellSchool::None};
        int32_t level{1};
        std::vector<SpellCategory> categories;
        std::optional<CharacterClass> requiredClass;  // empty: usable by any class
        int32_t requiredLevel{1};
        int32_t manaCost{0};
        float castTimeSeconds{0.0f};
        float cooldownSeconds{0.0f};
        float range{0.0f};
        float radius{0.0f};
        int32_t baseDamage{0};
        int32_t baseHeal{0};
        float powerCoefficient{0.0f};
        DamageType damageType{DamageType::None};
        float durationSeconds{0.0f};
        float lifestealFraction{0.0f};
        std::vector<String> appliedStatusIds;
        std::vector<String> scalingAttributes;
        String notes;

        [[nodiscard]] bool HasCategory(SpellCategory category) const {
            return std::ranges::find(categories, category) != categories.end();
        }

        [[nodiscard]] bool CanBeCastBy(CharacterClass characterClass) const {
            return !requiredClass || *requiredClass == characterClass;
        }
    };

    /// <summary>
    /// Weapon definition (Assets/Data/weapons.json, design-weapons-system.md)
    /// </summary>
    struct WeaponDefinition {
        ContentId id{0};
        String key;
        String name;
        WeaponType category{WeaponType::None};
        EquipmentSlot slot{EquipmentSlot::MainHand};
        int32_t requiredLevel{1};
        std::vector<CharacterClass> allowedClasses;
        LB::Dice damageDice{};
        int32_t damageBonus{0};
        std::vector<DamageType> damageTypes;
        float attackTimeSeconds{1.0f};
        int32_t critRangeMin{20};                     // "19-20" -> 19
        float critMultiplier{2.0f};
        String attributeScale;                        // "STR", "DEX", "INT", ...
        float scaleFactor{1.0f};
        bool isTwoHanded{false};
        std::vector<String> specialProperties;
        int32_t affixSlots{0};
        String ammoType;                              // ranged weapons only

        [[nodiscard]] bool IsUsableBy(CharacterClass characterClass) const {
            return std::ranges::find(allowedClasses, characterClass) != allowedClasses.end();
        }
    };

    /// <summary>
    /// Armour definition (Assets/Data/armour.json, design-armour-system.md)
    /// </summary>
    struct ArmorDefinition {
        ContentId id{0};
        String key;
        String name;
        EquipmentSlot slot{EquipmentSlot::None};
        ArmorCategory category{ArmorCategory::None};
        int32_t requiredLevel{1};
        std::vector<CharacterClass> allowedClasses;
        int32_t physicalAR{0};
        std::map<DamageType, int32_t> elementalAR;    // Fire, Cold, Lightning, Acid
        std::map<DamageType, int32_t> spiritualAR;    // Holy (radiant), Shadow (necrotic), Psychic
        int32_t armorBonus{0};                        // shields
        float blockChance{0.0f};                      // shields; percent (design-armour-system.md, 7)
        int32_t blockValue{0};                        // shields
        float weight{0.0f};
        float movementPenalty{0.0f};                  // fraction of speed, e.g. 0.10
        float castPenalty{0.0f};
        int32_t affixSlots{0};

        [[nodiscard]] bool IsUsableBy(CharacterClass characterClass) const {
            return std::ranges::find(allowedClasses, characterClass) != allowedClasses.end();
        }
    };

    /// <summary>
    /// Class definition (Assets/Data/classes.json, design-character-system.md section 3,
    /// game-design.md Character Classes)
    /// </summary>
    struct ClassDefinition {
        ContentId id{0};
        String key;                                   // e.g. "class_warrior"
        CharacterClass characterClass{CharacterClass::None};
        String name;
        String role;
        std::vector<LB::AttributeType> primaryAttributes;   // "STR", "DEX", ... in the data
        LB::Attributes startingAttributes{};
        String resource;                              // "Mana" or "Stamina"
        float baseMovementSpeed{4.0f};                // tiles per second before DEX and armour
        AttributeGrowth attributeGrowth;
        // Movement/casting penalty scale per armour category; every category has a value
        std::map<ArmorCategory, ArmorAdjustment> armorAdjustments;
        // Attack roll adjustment per weapon category for this class: +3 signature, +1 trained,
        // -1 untrained, -5 restricted. Every category has a value.
        std::map<WeaponType, int32_t> weaponAdjustments;
        std::vector<SpellSchool> spellSchools;        // class spell access; Generic spells are open to all
        std::vector<String> signatureAbilities;
        ClassProgression progression;                 // levels 1..ClassProgression::MaxLevel

        [[nodiscard]] int32_t GetWeaponAdjustment(WeaponType weapon) const {
            auto it = weaponAdjustments.find(weapon);
            return it != weaponAdjustments.end() ? it->second : 0;
        }

        [[nodiscard]] ArmorAdjustment GetArmorAdjustment(ArmorCategory armor) const {
            auto it = armorAdjustments.find(armor);
            return it != armorAdjustments.end() ? it->second : ArmorAdjustment{};
        }

        [[nodiscard]] bool IsPrimaryAttribute(LB::AttributeType attribute) const {
            return std::ranges::find(primaryAttributes, attribute) != primaryAttributes.end();
        }

        [[nodiscard]] bool HasSpellSchool(SpellSchool school) const {
            return std::ranges::find(spellSchools, school) != spellSchools.end();
        }

        /// <summary>
        /// Progression totals at a level, clamped to [1, ClassProgression::MaxLevel]
        /// </summary>
        [[nodiscard]] ClassLevelTotals TotalsAt(int32_t level) const {
            return progression.TotalsAt(level);
        }
    };

    /// <summary>
    /// Enemy stat block (Assets/Data/enemies.json, design-combat-system.md): how tough it is, its
    /// basic attack, how it moves, and the models it is drawn with
    /// </summary>
    struct EnemyDefinition {
        ContentId id{0};
        String key;                                   // e.g. "enemy_skeleton_minion"
        String name;
        int32_t level{1};
        int32_t hitPoints{1};
        int32_t armorClass{10};                       // d20 attack rolls must reach this to hit
        int32_t physicalArmor{0};                     // flat reduction of physical damage (5.2)
        std::map<DamageType, float> resistances;      // fraction removed (0.25), negative: vulnerable
        int32_t experience{0};

        // Basic attack (a melee weapon profile)
        int32_t attackBonus{0};
        LB::Dice damageDice{};
        int32_t damageBonus{0};
        DamageType damageType{DamageType::Slashing};
        int32_t critRangeMin{20};
        float critMultiplier{1.5f};
        float attackTimeSeconds{1.0f};                // wind-up and swing
        float attackRecoverySeconds{0.0f};            // pause before the next attack
        float attackRange{1.0f};                      // tiles between centers
        String attackAnimation;                       // clip played for the attack (empty: a generic one)
        float attackImpactFraction{0.45f};            // when the hit lands, as a fraction of attackTimeSeconds

        // Movement and awareness
        float movementSpeed{3.0f};                    // tiles per second
        float aggroRange{8.0f};                       // tiles; notices the player within this distance

        // Presentation: model files (in the model directory) and scale
        String model;                                 // skinned glTF on the KayKit Rig_Medium
        String mainHandModel;                         // optional, held at the right hand slot
        String offHandModel;                          // optional, held at the left hand slot
        float modelScale{0.5f};

        [[nodiscard]] float GetResistance(DamageType type) const {
            auto it = resistances.find(type);
            return it != resistances.end() ? it->second : 0.0f;
        }
    };

    /// <summary>
    /// Outcome of loading content: every problem found, so data authors can fix them in one pass
    /// </summary>
    struct ContentLoadResult {
        std::vector<String> errors;

        [[nodiscard]] bool Succeeded() const noexcept { return errors.empty(); }
    };

    /// <summary>
    /// All loaded game content, with lookups by key and id
    /// </summary>
    class ContentDatabase {
    public:
        /// <summary>
        /// Load spells.json, weapons.json, armour.json, classes.json and enemies.json from a
        /// directory (UTF-8 path). On any error the database is left empty and every error is reported.
        /// </summary>
        ContentLoadResult LoadFromDirectory(const String& directory);

        /// <summary>
        /// Load from JSON text (one array per content type). Used by tests and tools; source
        /// names appear in error messages. With an empty class array, class rules are not checked;
        /// otherwise every character class must be defined and items/spells must agree with them.
        /// </summary>
        ContentLoadResult LoadFromJson(const String& spellsJson, const String& weaponsJson, const String& armourJson,
                                       const String& classesJson = "[]", const String& enemiesJson = "[]");

        void Clear() {
            spells_.clear();
            weapons_.clear();
            armour_.clear();
            classes_.clear();
            enemies_.clear();
        }

        [[nodiscard]] bool IsEmpty() const noexcept {
            return spells_.empty() && weapons_.empty() && armour_.empty() && classes_.empty() && enemies_.empty();
        }

        [[nodiscard]] const std::vector<SpellDefinition>& GetSpells() const noexcept { return spells_; }
        [[nodiscard]] const std::vector<WeaponDefinition>& GetWeapons() const noexcept { return weapons_; }
        [[nodiscard]] const std::vector<ArmorDefinition>& GetArmour() const noexcept { return armour_; }
        [[nodiscard]] const std::vector<ClassDefinition>& GetClasses() const noexcept { return classes_; }
        [[nodiscard]] const std::vector<EnemyDefinition>& GetEnemies() const noexcept { return enemies_; }

        [[nodiscard]] const SpellDefinition* FindSpell(ContentId id) const { return FindById(spells_, id); }
        [[nodiscard]] const SpellDefinition* FindSpell(std::string_view key) const { return FindByKey(spells_, key); }
        [[nodiscard]] const WeaponDefinition* FindWeapon(ContentId id) const { return FindById(weapons_, id); }
        [[nodiscard]] const WeaponDefinition* FindWeapon(std::string_view key) const { return FindByKey(weapons_, key); }
        [[nodiscard]] const ArmorDefinition* FindArmor(ContentId id) const { return FindById(armour_, id); }
        [[nodiscard]] const ArmorDefinition* FindArmor(std::string_view key) const { return FindByKey(armour_, key); }
        [[nodiscard]] const ClassDefinition* FindClass(std::string_view key) const { return FindByKey(classes_, key); }
        [[nodiscard]] const EnemyDefinition* FindEnemy(ContentId id) const { return FindById(enemies_, id); }
        [[nodiscard]] const EnemyDefinition* FindEnemy(std::string_view key) const { return FindByKey(enemies_, key); }
        [[nodiscard]] const ClassDefinition* FindClass(CharacterClass characterClass) const {
            auto it = std::ranges::find(classes_, characterClass, &ClassDefinition::characterClass);
            return it != classes_.end() ? &*it : nullptr;
        }

        /// <summary>
        /// Weapons a class may wield at a given character level
        /// </summary>
        [[nodiscard]] std::vector<const WeaponDefinition*> GetWeaponsFor(CharacterClass characterClass, int32_t characterLevel) const {
            std::vector<const WeaponDefinition*> result;
            for (const auto& weapon : weapons_) {
                if (weapon.IsUsableBy(characterClass) && weapon.requiredLevel <= characterLevel) result.push_back(&weapon);
            }
            return result;
        }

        /// <summary>
        /// Armour pieces a class may wear in a slot
        /// </summary>
        [[nodiscard]] std::vector<const ArmorDefinition*> GetArmourFor(CharacterClass characterClass, EquipmentSlot slot) const {
            std::vector<const ArmorDefinition*> result;
            for (const auto& armor : armour_) {
                if (armor.IsUsableBy(characterClass) && armor.slot == slot) result.push_back(&armor);
            }
            return result;
        }

    private:
        // ids are 1-based positions, so lookups by id are direct
        template<typename T>
        static const T* FindById(const std::vector<T>& items, ContentId id) {
            if (id < 1 || static_cast<std::size_t>(id) > items.size()) return nullptr;
            return &items[static_cast<std::size_t>(id - 1)];
        }

        template<typename T>
        static const T* FindByKey(const std::vector<T>& items, std::string_view key) {
            auto it = std::ranges::find(items, key, &T::key);
            return it != items.end() ? &*it : nullptr;
        }

        std::vector<SpellDefinition> spells_;
        std::vector<WeaponDefinition> weapons_;
        std::vector<ArmorDefinition> armour_;
        std::vector<ClassDefinition> classes_;
        std::vector<EnemyDefinition> enemies_;
    };

} // namespace LB::Content
