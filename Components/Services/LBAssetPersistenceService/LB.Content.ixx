// LB.Content.ixx - Game content definitions loaded from Assets/Data JSON
//
// Definitions are the static, data-driven description of content (what a Fireball is);
// runtime state (a character's equipped sword) lives in the domain modules.
export module LB.Content;

import LB.Core;
import LB.Character;
import LB.Combat;
import std;


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
        std::int32_t level{1};
        std::vector<SpellCategory> categories;
        std::optional<CharacterClass> requiredClass;  // empty: usable by any class
        std::int32_t requiredLevel{1};
        std::int32_t manaCost{0};
        float castTimeSeconds{0.0f};
        float cooldownSeconds{0.0f};
        float range{0.0f};
        float radius{0.0f};
        std::int32_t baseDamage{0};
        std::int32_t baseHeal{0};
        float powerCoefficient{0.0f};
        DamageType damageType{DamageType::None};
        float durationSeconds{0.0f};
        float lifestealFraction{0.0f};
        std::vector<String> appliedStatusIds;
        std::vector<String> scalingAttributes;
        String notes;
        float projectileSpeed{0.0f};                  // tiles per second of a bolt; 0: not a missile
        float dashTiles{0.0f};                        // the caster rolls this far (a dodge)
        float evadeSeconds{0.0f};                     // ... and no attack touches them this long
        bool teleportBehindTarget{false};             // the caster steps behind the target (within range)
        std::int32_t cleansesStatuses{0};             // hindrances (Debuff, Control) it removes from the caster

        /// <summary>
        /// How it is cast: a missile (projectileSpeed, flying `range` tiles; with a radius it bursts
        /// where it lands), a burst around the caster (no range, a radius), an instant strike on
        /// a target in range (no speed, no radius), a burst at a target (range and radius), a heal
        /// of the caster (healing, no damage), a dodge, or a step behind a target
        /// </summary>
        [[nodiscard]] bool IsMissile() const noexcept { return projectileSpeed > 0.0f && range > 0.0f; }
        [[nodiscard]] bool IsBurstAroundCaster() const noexcept { return projectileSpeed <= 0.0f && range <= 0.0f && radius > 0.0f; }
        [[nodiscard]] bool IsTargetedStrike() const noexcept { return projectileSpeed <= 0.0f && range > 0.0f && radius <= 0.0f && baseDamage > 0 && !teleportBehindTarget; }
        [[nodiscard]] bool IsTargetedBurst() const noexcept { return projectileSpeed <= 0.0f && range > 0.0f && radius > 0.0f; }
        [[nodiscard]] bool IsHeal() const noexcept { return baseHeal > 0 && baseDamage <= 0; }
        [[nodiscard]] bool IsDash() const noexcept { return dashTiles > 0.0f; }
        [[nodiscard]] bool IsShadowstep() const noexcept { return teleportBehindTarget && range > 0.0f; }

        /// <summary>
        /// Needs an enemy to aim at (a strike, a burst at a target, a step behind one)
        /// </summary>
        [[nodiscard]] bool NeedsTarget() const noexcept { return !IsHeal() && (IsTargetedStrike() || IsTargetedBurst() || IsShadowstep()); }

        /// <summary>
        /// The game can cast it (buffs, summons, and the like are not built yet)
        /// </summary>
        [[nodiscard]] bool IsCastable() const noexcept {
            const bool affects = baseDamage > 0 || !appliedStatusIds.empty();
            return IsHeal() || IsDash() || IsShadowstep() || IsTargetedStrike()
                || ((IsMissile() || IsBurstAroundCaster() || IsTargetedBurst()) && affects);
        }

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
        std::int32_t requiredLevel{1};
        std::vector<CharacterClass> allowedClasses;
        LB::Dice damageDice{};
        std::int32_t damageBonus{0};
        std::vector<DamageType> damageTypes;
        float attackTimeSeconds{1.0f};
        std::int32_t critRangeMin{20};                     // "19-20" -> 19
        float critMultiplier{2.0f};
        String attributeScale;                        // "STR", "DEX", "INT", ...
        float scaleFactor{1.0f};
        bool isTwoHanded{false};
        std::vector<String> specialProperties;
        std::int32_t affixSlots{0};
        String ammoType;                              // ranged weapons only
        std::int32_t value{0};                        // gold a merchant asks for it (it buys at a fraction)
        float range{0.0f};                            // tiles a shot flies (bows, crossbows, wands); 0: melee
        float projectileSpeed{0.0f};                  // tiles per second of the arrow or bolt

        [[nodiscard]] bool IsRanged() const noexcept { return range > 0.0f; }

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
        std::int32_t requiredLevel{1};
        std::vector<CharacterClass> allowedClasses;
        std::int32_t physicalAR{0};
        std::map<DamageType, std::int32_t> elementalAR;    // Fire, Cold, Lightning, Acid
        std::map<DamageType, std::int32_t> spiritualAR;    // Holy (radiant), Shadow (necrotic), Psychic
        std::int32_t armorBonus{0};                        // shields
        float blockChance{0.0f};                      // shields; percent (design-armour-system.md, 7)
        std::int32_t blockValue{0};                        // shields
        float weight{0.0f};
        float movementPenalty{0.0f};                  // fraction of speed, e.g. 0.10
        float castPenalty{0.0f};
        std::int32_t affixSlots{0};
        std::int32_t value{0};                        // gold a merchant asks for it (it buys at a fraction)

        [[nodiscard]] bool IsUsableBy(CharacterClass characterClass) const {
            return std::ranges::find(allowedClasses, characterClass) != allowedClasses.end();
        }
    };

    /// <summary>
    /// A path a class chooses at SpecializationLevel (design-character-system.md 3, specialization
    /// paths): a name, what it means, and what it adds for good
    /// </summary>
    struct SpecializationDefinition {
        String key;                                   // e.g. "path_warrior_berserker"
        String name;
        String description;
        String flavor;                                // what the path is about (the choice's card)
        std::array<float, 3> color{ 0.95f, 0.75f, 0.30f };   // its color on the card and banner (display RGB)
        std::int32_t damage{0};
        std::int32_t attackBonus{0};
        std::int32_t armorClass{0};
        std::int32_t armor{0};
        std::int32_t spellPower{0};
        float manaRegen{0.0f};
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
        std::map<WeaponType, std::int32_t> weaponAdjustments;
        std::vector<SpellSchool> spellSchools;        // class spell access; Generic spells are open to all
        std::vector<String> signatureAbilities;
        std::vector<String> spellBar;                 // spell keys on the bar (keys 1-4), castable by the class
        float manaRegenPerSecond{0.0f};               // mana recovered per second (design-spells-system.md 4.1)
        std::int32_t spellPowerBase{0};               // SP_Base (design-character-system.md 4.2)
        String startingWeapon;                        // weapon key, equipped at the start (empty: none)
        LB::Dice sneakAttackDice{0, 6, 0};            // extra on blows against the unaware or helpless (count 0: none)
        std::vector<SpecializationDefinition> specializations;   // the paths to choose from at SpecializationLevel

        static constexpr std::int32_t SpecializationLevel = 5;

        [[nodiscard]] const SpecializationDefinition* FindSpecialization(std::string_view pathKey) const {
            const auto it = std::ranges::find(specializations, pathKey, &SpecializationDefinition::key);
            return it != specializations.end() ? &*it : nullptr;
        }
        std::vector<String> startingItems;            // item keys put in the bag at the start
        ClassProgression progression;                 // levels 1..ClassProgression::MaxLevel

        [[nodiscard]] std::int32_t GetWeaponAdjustment(WeaponType weapon) const {
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
        [[nodiscard]] ClassLevelTotals TotalsAt(std::int32_t level) const {
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
        std::int32_t level{1};
        std::int32_t hitPoints{1};
        std::int32_t armorClass{10};                       // d20 attack rolls must reach this to hit
        std::int32_t physicalArmor{0};                     // flat reduction of physical damage (5.2)
        std::map<DamageType, float> resistances;      // fraction removed (0.25), negative: vulnerable
        std::int32_t experience{0};

        // Basic attack (a melee weapon profile)
        std::int32_t attackBonus{0};
        LB::Dice damageDice{};
        std::int32_t damageBonus{0};
        DamageType damageType{DamageType::Slashing};
        std::int32_t critRangeMin{20};
        float critMultiplier{1.5f};
        float attackTimeSeconds{1.0f};                // wind-up and swing
        float attackRecoverySeconds{0.0f};            // pause before the next attack
        float attackRange{1.0f};                      // tiles between centers
        String attackAnimation;                       // clip played for the attack (empty: a generic one)
        float attackImpactFraction{0.45f};            // when the hit lands (or the shot leaves), as a fraction of attackTimeSeconds

        // Ranged attackers (projectileSpeed > 0) shoot a projectile that flies attackRange tiles
        // instead of striking, only at a player in sight, and back away from one closer than
        // keepAwayRange (design-combat-system.md 9.1, Kite)
        float projectileSpeed{0.0f};                  // tiles per second; 0: melee
        String projectileModel;                       // the arrow or bolt drawn in flight
        float keepAwayRange{0.0f};                    // tiles; 0: holds its ground

        // Casters (a spell key) cast that missile instead of shooting: its speed, damage type and
        // statuses; its damage is the spell's base plus its coefficient times spellPower
        String spell;
        std::int32_t spellPower{0};

        // Size, toughness, and boss mechanics
        String rig{"Medium"};                         // KayKit rig its model and clips use: Medium or Large
        bool staggerImmune{false};                    // hits do not make it reel
        bool boss{false};                             // a floor's boss: its health bar tops the screen
        String summon;                                // enemy key it raises while fighting (empty: none)
        std::int32_t summonCount{2};                  // raised at a time
        std::int32_t maxSummons{4};                   // of its raised enemies alive at once
        float summonCooldownSeconds{8.0f};
        float phaseTwoHealthFraction{0.0f};           // below this share of its health: raises and attacks twice as often (0: one phase)

        // A telegraphed attack (design-combat-system.md 9.2, boss patterns): a ring on the ground
        // for slamWindupSeconds, around itself or at the player's spot, then everyone inside is struck
        float slamRadius{0.0f};                       // tiles; 0: none
        float slamWindupSeconds{1.0f};
        float slamCooldownSeconds{8.0f};
        float slamTriggerRange{3.0f};                 // starts one at a player this close (and in sight)
        bool slamAtTarget{false};                     // at the player's spot (else around itself)
        LB::Dice slamDamage{};
        DamageType slamDamageType{DamageType::Bludgeoning};

        // Group tactics (design-combat-system.md 9.1): its group's other fighters do not pass a
        // tank until it closes in, and its shooters keep behind the front line
        bool tank{false};

        // Movement and awareness
        float movementSpeed{3.0f};                    // tiles per second
        float aggroRange{8.0f};                       // tiles; notices the player within this distance

        // Presentation: model files (in the model directory) and scale
        String lootTable;                             // loot.json table rolled when it dies (empty: drops nothing)

        String model;                                 // skinned glTF on the KayKit Rig_Medium
        String mainHandModel;                         // optional, held at the right hand slot
        String offHandModel;                          // optional, held at the left hand slot
        float modelScale{0.5f};

        [[nodiscard]] bool IsRanged() const noexcept { return projectileSpeed > 0.0f || !spell.empty(); }
        [[nodiscard]] bool IsCaster() const noexcept { return !spell.empty(); }
        [[nodiscard]] bool HasSlam() const noexcept { return slamRadius > 0.0f; }

        [[nodiscard]] float GetResistance(DamageType type) const {
            auto it = resistances.find(type);
            return it != resistances.end() ? it->second : 0.0f;
        }
    };

    /// <summary>
    /// A companion who fights beside the hero (Assets/Data/allies.json): hired from the merchant,
    /// growing with the hero's level (design-character-system.md 7, companions)
    /// </summary>
    struct AllyDefinition {
        ContentId id{0};
        String key;                                   // e.g. "ally_barbarian"
        String name;
        String description;
        std::int32_t hitPoints{20};                   // at the hero's level 1
        std::int32_t hitPointsPerLevel{4};            // more for each level of the hero's above 1
        std::int32_t armorClass{12};
        std::int32_t physicalArmor{0};
        std::int32_t attackBonus{3};                  // and +1 for every two levels of the hero's above 1
        LB::Dice damageDice{1, 8, 0};
        std::int32_t damageBonus{0};                  // and +1 for every three levels of the hero's above 1
        DamageType damageType{DamageType::Slashing};
        float attackTimeSeconds{1.0f};                // the swing or the draw, its blow halfway
        float attackRecoverySeconds{0.5f};
        float attackRange{1.3f};                      // tiles between centers
        float projectileSpeed{0.0f};                  // tiles per second: a shooter; 0: melee
        String projectileModel;
        float movementSpeed{3.2f};
        std::int32_t price{60};                       // gold the merchant asks to hire them
        String model;                                 // a KayKit Adventurers body (Rig_Medium)
        String mainHandModel;
        float modelScale{0.5f};

        [[nodiscard]] bool IsRanged() const noexcept { return projectileSpeed > 0.0f; }
    };

    /// <summary>
    /// A consumable item (Assets/Data/consumables.json, design-loot-and-progression.md 2.1): used
    /// from the inventory or the quick-use button
    /// </summary>
    struct ConsumableDefinition {
        ContentId id{0};
        String key;                                   // e.g. "consumable_potion_healing_minor"
        String name;
        LB::Dice healing{};                           // hit points restored (0d0: none)
        std::int32_t maxStack{10};
        String model;                                 // how it lies on the floor
        String ammoType;                              // ammunition for weapons of this ammoType ("Arrow", "Bolt"); empty: not ammunition
        bool isKey{false};                            // opens a locked gate (design-dungeon-and-world.md 4.3); not used from the bag
        std::int32_t value{0};                        // gold a merchant asks for one

        [[nodiscard]] bool IsAmmunition() const noexcept { return !ammoType.empty(); }
    };

    /// <summary>
    /// A rarity tier (Assets/Data/rarities.json, design-loot-and-progression.md 3.1, 4.2-4.3): how
    /// many affixes it brings, its color, and its drop weight, which changes with depth
    /// </summary>
    struct RarityDefinition {
        ContentId id{0};
        String key;                                   // e.g. "rarity_rare"
        String name;                                  // "Rare"
        std::int32_t affixes{0};
        std::array<float, 3> color{1.0f, 1.0f, 1.0f}; // display (sRGB) color of its names
        float weight{0.0f};                           // at depth 1
        float weightPerDepth{0.0f};                   // added for each floor deeper (never below 0)
        float priceMultiplier{1.0f};                  // on the item's value
        bool legendary{false};                        // carries a legendary power besides its affixes

        [[nodiscard]] float WeightAt(std::int32_t depth) const noexcept {
            return std::max(0.0f, weight + weightPerDepth * static_cast<float>(std::max(0, depth - 1)));
        }
    };

    /// <summary>
    /// An affix (Assets/Data/affixes.json, design-loot-and-progression.md 3.2): a prefix or suffix
    /// of an item's name and what it adds while the item is worn or wielded
    /// </summary>
    struct AffixDefinition {
        ContentId id{0};
        String key;                                   // e.g. "affix_prefix_keen"
        String name;                                  // "Keen", "of the Bear"
        bool prefix{true};                            // a prefix (before the name) or a suffix (after)
        bool onWeapons{false};
        bool onArmour{false};
        std::int32_t damage{0};                       // added to the weapon's damage
        std::int32_t attackBonus{0};
        std::int32_t armorClass{0};
        std::int32_t armor{0};                        // flat physical armour
        std::int32_t spellPower{0};
        float manaRegen{0.0f};                        // mana per second
        std::int32_t strength{0};                     // attributes (design-loot-and-progression.md 3.2)
        std::int32_t dexterity{0};
        std::int32_t intelligence{0};
        std::int32_t wisdom{0};
        std::int32_t hitPoints{0};                    // added to the maximum
        std::int32_t mana{0};
        // A legendary power (type "Power": only on legendary items, design-loot-and-progression.md
        // 3.3): ChainLightning, LifeSteal, Frostbite, Ignite (weapons), Thorns (armour)
        String power;
        float chance{0.0f};                           // of a hit setting it off (LifeSteal, Thorns: the share of the damage)
        std::int32_t powerDamage{0};                  // ChainLightning's damage
        String description;                           // what it does, for the character screen

        [[nodiscard]] bool IsPower() const noexcept { return !power.empty(); }
    };

    /// <summary>
    /// One possible item drop of a loot table, with its relative weight
    /// </summary>
    struct LootEntry {
        String item;                                  // weapon, armour, or consumable key
        std::int32_t weight{1};
        LB::Dice quantity{0, 1, 1};                   // how many drop (ammunition comes in handfuls); 1 by default
    };

    /// <summary>
    /// What a source drops (Assets/Data/loot.json, design-loot-and-progression.md 4.2): a chance of
    /// gold (dice), and a chance of one item picked by weight
    /// </summary>
    struct LootTableDefinition {
        ContentId id{0};
        String key;                                   // e.g. "loot_skeleton_minion"
        float goldChance{0.0f};                       // 0..1
        LB::Dice gold{1, 1, 0};
        float itemChance{0.0f};                       // 0..1
        std::vector<LootEntry> items;

        [[nodiscard]] std::int32_t TotalWeight() const noexcept {
            std::int32_t total = 0;
            for (const auto& entry : items) total += entry.weight;
            return total;
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
    /// A status effect (Assets/Data/statuses.json, design-character-system.md 6.1, a flat subset of
    /// its effect lists): how long it lasts, how it hinders movement and actions, and the damage it
    /// deals every tick
    /// </summary>
    struct StatusDefinition {
        ContentId id{0};
        String key;                                   // e.g. "status_slow_basic"
        String displayName;
        std::vector<String> categories;               // Buff, Debuff, Control, Elemental, ...
        float durationSeconds{1.0f};
        float tickIntervalSeconds{1.0f};
        float movementMultiplier{1.0f};               // 0.5 slows to half speed, 0 roots
        bool preventsActions{false};                  // no moving, attacking, or casting (frozen, charmed)
        std::int32_t damagePerTick{0};
        DamageType damageType{DamageType::None};
    };

    /// <summary>
    /// All loaded game content, with lookups by key and id
    /// </summary>
    class ContentDatabase {
    public:
        /// <summary>
        /// Load spells.json, weapons.json, armour.json, classes.json, enemies.json, consumables.json,
        /// loot.json, statuses.json, rarities.json, affixes.json, and allies.json from a directory (UTF-8 path). On any error the database is left empty and
        /// every error is reported.
        /// </summary>
        ContentLoadResult LoadFromDirectory(const String& directory);

        /// <summary>
        /// Load from JSON text (one array per content type). Used by tests and tools; source
        /// names appear in error messages. With an empty class array, class rules are not checked;
        /// otherwise every character class must be defined and items/spells must agree with them.
        /// </summary>
        ContentLoadResult LoadFromJson(const String& spellsJson, const String& weaponsJson, const String& armourJson,
                                       const String& classesJson = "[]", const String& enemiesJson = "[]",
                                       const String& consumablesJson = "[]", const String& lootJson = "[]",
                                       const String& statusesJson = "[]", const String& raritiesJson = "[]", const String& affixesJson = "[]",
                                       const String& alliesJson = "[]");

        void Clear() {
            spells_.clear();
            weapons_.clear();
            armour_.clear();
            classes_.clear();
            enemies_.clear();
            consumables_.clear();
            lootTables_.clear();
            statuses_.clear();
            rarities_.clear();
            affixes_.clear();
            allies_.clear();
        }

        [[nodiscard]] bool IsEmpty() const noexcept {
            return spells_.empty() && weapons_.empty() && armour_.empty() && classes_.empty() && enemies_.empty()
                && consumables_.empty() && lootTables_.empty() && statuses_.empty();
        }

        [[nodiscard]] const std::vector<SpellDefinition>& GetSpells() const noexcept { return spells_; }
        [[nodiscard]] const std::vector<WeaponDefinition>& GetWeapons() const noexcept { return weapons_; }
        [[nodiscard]] const std::vector<ArmorDefinition>& GetArmour() const noexcept { return armour_; }
        [[nodiscard]] const std::vector<ClassDefinition>& GetClasses() const noexcept { return classes_; }
        [[nodiscard]] const std::vector<EnemyDefinition>& GetEnemies() const noexcept { return enemies_; }
        [[nodiscard]] const std::vector<ConsumableDefinition>& GetConsumables() const noexcept { return consumables_; }
        [[nodiscard]] const std::vector<LootTableDefinition>& GetLootTables() const noexcept { return lootTables_; }
        [[nodiscard]] const std::vector<StatusDefinition>& GetStatuses() const noexcept { return statuses_; }
        [[nodiscard]] const std::vector<RarityDefinition>& GetRarities() const noexcept { return rarities_; }
        [[nodiscard]] const std::vector<AffixDefinition>& GetAffixes() const noexcept { return affixes_; }
        [[nodiscard]] const RarityDefinition* FindRarity(std::string_view key) const { return FindByKey(rarities_, key); }
        [[nodiscard]] const AffixDefinition* FindAffix(std::string_view key) const { return FindByKey(affixes_, key); }
        [[nodiscard]] const std::vector<AllyDefinition>& GetAllies() const noexcept { return allies_; }
        [[nodiscard]] const AllyDefinition* FindAlly(std::string_view key) const { return FindByKey(allies_, key); }

        [[nodiscard]] const SpellDefinition* FindSpell(ContentId id) const { return FindById(spells_, id); }
        [[nodiscard]] const SpellDefinition* FindSpell(std::string_view key) const { return FindByKey(spells_, key); }
        [[nodiscard]] const WeaponDefinition* FindWeapon(ContentId id) const { return FindById(weapons_, id); }
        [[nodiscard]] const WeaponDefinition* FindWeapon(std::string_view key) const { return FindByKey(weapons_, key); }
        [[nodiscard]] const ArmorDefinition* FindArmor(ContentId id) const { return FindById(armour_, id); }
        [[nodiscard]] const ArmorDefinition* FindArmor(std::string_view key) const { return FindByKey(armour_, key); }
        [[nodiscard]] const ClassDefinition* FindClass(std::string_view key) const { return FindByKey(classes_, key); }
        [[nodiscard]] const EnemyDefinition* FindEnemy(ContentId id) const { return FindById(enemies_, id); }
        [[nodiscard]] const EnemyDefinition* FindEnemy(std::string_view key) const { return FindByKey(enemies_, key); }
        [[nodiscard]] const ConsumableDefinition* FindConsumable(ContentId id) const { return FindById(consumables_, id); }
        [[nodiscard]] const ConsumableDefinition* FindConsumable(std::string_view key) const { return FindByKey(consumables_, key); }
        [[nodiscard]] const LootTableDefinition* FindLootTable(std::string_view key) const { return FindByKey(lootTables_, key); }
        [[nodiscard]] const StatusDefinition* FindStatus(std::string_view key) const { return FindByKey(statuses_, key); }
        [[nodiscard]] const ClassDefinition* FindClass(CharacterClass characterClass) const {
            auto it = std::ranges::find(classes_, characterClass, &ClassDefinition::characterClass);
            return it != classes_.end() ? &*it : nullptr;
        }

        /// <summary>
        /// Weapons a class may wield at a given character level
        /// </summary>
        [[nodiscard]] std::vector<const WeaponDefinition*> GetWeaponsFor(CharacterClass characterClass, std::int32_t characterLevel) const {
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
        std::vector<ConsumableDefinition> consumables_;
        std::vector<LootTableDefinition> lootTables_;
        std::vector<StatusDefinition> statuses_;
        std::vector<RarityDefinition> rarities_;
        std::vector<AffixDefinition> affixes_;
        std::vector<AllyDefinition> allies_;
    };

} // namespace LB::Content
