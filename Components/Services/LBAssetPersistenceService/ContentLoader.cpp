// ContentLoader.cpp - LB.Content implementation unit: strict JSON loading of content definitions
//
// Validation follows asset-preprocesing-strategy-instructions.md: fail fast with clear messages.
// Every problem is collected (not just the first) and nothing is loaded if any is found.
// Unknown fields are ignored so data can gain fields before code uses them.
module;

#include <nlohmann/json.hpp>

module LB.Content;

import std;

namespace LB::Content {

    namespace {
        using Json = nlohmann::json;

        // An item's tier from its key's "_tN" suffix (1 without one)
        std::int32_t TierOfKey(std::string_view key) {
            const auto mark = key.rfind("_t");
            std::int32_t tier = 0;
            if (mark == std::string_view::npos || mark + 2 >= key.size()) return 1;
            const auto [end, ec] = std::from_chars(key.data() + mark + 2, key.data() + key.size(), tier);
            return ec == std::errc{} && end == key.data() + key.size() && tier >= 1 ? tier : 1;
        }

        template<typename E>
        struct EnumName {
            std::string_view name;
            E value;
        };

        constexpr EnumName<SpellSchool> SpellSchools[] = {
            {"Evocation", SpellSchool::Evocation}, {"Frost", SpellSchool::Frost}, {"Holy", SpellSchool::Holy},
            {"Shadow", SpellSchool::Shadow}, {"Utility", SpellSchool::Utility}, {"Generic", SpellSchool::Generic},
        };

        constexpr EnumName<SpellCategory> SpellCategories[] = {
            {"Damage", SpellCategory::Damage}, {"Heal", SpellCategory::Heal}, {"Buff", SpellCategory::Buff},
            {"Debuff", SpellCategory::Debuff}, {"Control", SpellCategory::Control}, {"Utility", SpellCategory::Utility},
        };

        constexpr EnumName<CharacterClass> CharacterClasses[] = {
            {"Warrior", CharacterClass::Warrior}, {"Rogue", CharacterClass::Rogue},
            {"Mage", CharacterClass::Mage}, {"Cleric", CharacterClass::Cleric},
        };

        // Radiant/Necrotic are the design documents' names for the domain's Holy/Shadow
        // (design-spells-system.md, 2.1)
        constexpr EnumName<DamageType> DamageTypes[] = {
            {"Slashing", DamageType::Slashing}, {"Piercing", DamageType::Piercing}, {"Bludgeoning", DamageType::Bludgeoning},
            {"Fire", DamageType::Fire}, {"Cold", DamageType::Cold}, {"Lightning", DamageType::Lightning},
            {"Acid", DamageType::Acid}, {"Holy", DamageType::Holy}, {"Radiant", DamageType::Holy},
            {"Shadow", DamageType::Shadow}, {"Necrotic", DamageType::Shadow}, {"Psychic", DamageType::Psychic},
            {"Arcane", DamageType::Arcane},
        };

        // "Polearm" and "Spear" are one category (design-weapons-system.md, 2.2)
        constexpr EnumName<WeaponType> WeaponTypes[] = {
            {"Sword", WeaponType::Sword}, {"Axe", WeaponType::Axe}, {"Mace", WeaponType::Mace},
            {"Dagger", WeaponType::Dagger}, {"Spear", WeaponType::Spear}, {"Polearm", WeaponType::Spear},
            {"Bow", WeaponType::Bow}, {"Crossbow", WeaponType::Crossbow}, {"Staff", WeaponType::Staff},
            {"Wand", WeaponType::Wand},
        };

        constexpr EnumName<ArmorCategory> ArmorCategories[] = {
            {"Cloth", ArmorCategory::Cloth}, {"Light", ArmorCategory::Light}, {"Medium", ArmorCategory::Medium},
            {"Heavy", ArmorCategory::Heavy}, {"Shield", ArmorCategory::Shield},
        };

        constexpr EnumName<EquipmentSlot> EquipmentSlots[] = {
            {"Head", EquipmentSlot::Head}, {"Chest", EquipmentSlot::Chest}, {"Legs", EquipmentSlot::Legs},
            {"Feet", EquipmentSlot::Feet}, {"Hands", EquipmentSlot::Hands}, {"MainHand", EquipmentSlot::MainHand},
            {"OffHand", EquipmentSlot::OffHand}, {"Ring1", EquipmentSlot::Ring1}, {"Ring2", EquipmentSlot::Ring2},
            {"Necklace", EquipmentSlot::Necklace},
        };

        template<typename E, std::size_t N>
        std::optional<E> FindEnum(const EnumName<E> (&table)[N], std::string_view name) {
            for (const auto& entry : table) {
                if (entry.name == name) return entry.value;
            }
            return std::nullopt;
        }

        // First (canonical) name of a value, for error messages
        template<typename E, std::size_t N>
        std::string_view NameOf(const EnumName<E> (&table)[N], E value) {
            for (const auto& entry : table) {
                if (entry.value == value) return entry.name;
            }
            return "?";
        }

        constexpr std::string_view AttributeNames[] = { "STR", "DEX", "CON", "INT", "WIS", "CHA" };

        constexpr EnumName<LB::AttributeType> AttributeTypes[] = {
            {"STR", LB::AttributeType::Strength}, {"DEX", LB::AttributeType::Dexterity},
            {"CON", LB::AttributeType::Constitution}, {"INT", LB::AttributeType::Intelligence},
            {"WIS", LB::AttributeType::Wisdom}, {"CHA", LB::AttributeType::Charisma},
        };

        /// <summary>
        /// Reads typed fields from one JSON object, recording errors with file and entry context
        /// </summary>
        class EntryReader {
        public:
            EntryReader(const Json& object, std::string context, std::vector<String>& errors)
                : object_(object), context_(std::move(context)), errors_(errors) {}

            void Error(const std::string& message) {
                errors_.push_back(std::format("{}: {}", context_, message));
            }

            bool Has(const char* field) const {
                return object_.contains(field) && !object_.at(field).is_null();
            }

            const std::string& Context() const noexcept { return context_; }
            std::vector<String>& Errors() noexcept { return errors_; }

            // A nested value, or null after reporting a missing required field / wrong type
            const Json* Nested(const char* field, bool required, bool isArray) {
                if (!Present(field, required)) return nullptr;
                const auto& value = object_.at(field);
                if (isArray ? !value.is_array() : !value.is_object()) {
                    TypeError(field, isArray ? "an array" : "an object");
                    return nullptr;
                }
                return &value;
            }

            void ReadString(const char* field, String& out, bool required) {
                if (!Present(field, required)) return;
                const auto& value = object_.at(field);
                if (!value.is_string()) { TypeError(field, "a string"); return; }
                out = value.get<String>();
                if (required && out.empty()) Error(std::format("'{}' must not be empty", field));
            }

            void ReadInt(const char* field, std::int32_t& out, bool required) {
                if (!Present(field, required)) return;
                const auto& value = object_.at(field);
                if (!value.is_number_integer()) { TypeError(field, "an integer"); return; }
                out = value.get<std::int32_t>();
            }

            void ReadFloat(const char* field, float& out, bool required) {
                if (!Present(field, required)) return;
                const auto& value = object_.at(field);
                if (!value.is_number()) { TypeError(field, "a number"); return; }
                out = value.get<float>();
            }

            void ReadBool(const char* field, bool& out) {
                if (!Present(field, false)) return;
                const auto& value = object_.at(field);
                if (!value.is_boolean()) { TypeError(field, "true or false"); return; }
                out = value.get<bool>();
            }

            void ReadStringArray(const char* field, std::vector<String>& out) {
                if (!Present(field, false)) return;
                const auto& value = object_.at(field);
                if (!value.is_array()) { TypeError(field, "an array of strings"); return; }
                for (const auto& item : value) {
                    if (!item.is_string()) { TypeError(field, "an array of strings"); return; }
                    out.push_back(item.get<String>());
                }
            }

            template<typename E, std::size_t N>
            void ReadEnum(const char* field, E& out, const EnumName<E> (&table)[N], bool required) {
                if (!Present(field, required)) return;
                const auto& value = object_.at(field);
                if (!value.is_string()) { TypeError(field, "a string"); return; }
                ParseEnum(field, value.get<String>(), out, table);
            }

            template<typename E, std::size_t N>
            void ReadOptionalEnum(const char* field, std::optional<E>& out, const EnumName<E> (&table)[N]) {
                if (!Present(field, false)) return;
                E value{};
                const auto& json = object_.at(field);
                if (!json.is_string()) { TypeError(field, "a string"); return; }
                if (ParseEnum(field, json.get<String>(), value, table)) out = value;
            }

            // Accepts an array of names, or a single name when allowSingle is true
            template<typename E, std::size_t N>
            void ReadEnumArray(const char* field, std::vector<E>& out, const EnumName<E> (&table)[N], bool required, bool allowSingle = false) {
                if (!Present(field, required)) return;
                const auto& value = object_.at(field);

                if (allowSingle && value.is_string()) {
                    E parsed{};
                    if (ParseEnum(field, value.get<String>(), parsed, table)) out.push_back(parsed);
                    return;
                }
                if (!value.is_array()) { TypeError(field, "an array of names"); return; }
                for (const auto& item : value) {
                    if (!item.is_string()) { TypeError(field, "an array of names"); return; }
                    E parsed{};
                    if (ParseEnum(field, item.get<String>(), parsed, table)) out.push_back(parsed);
                }
                if (required && out.empty()) Error(std::format("'{}' must list at least one value", field));
            }

            // { "Fire": 2, "Cold": 0 } -> map keyed by damage type
            void ReadResistances(const char* field, std::map<DamageType, std::int32_t>& out) {
                if (!Present(field, false)) return;
                const auto& value = object_.at(field);
                if (!value.is_object()) { TypeError(field, "an object of damage type to rating"); return; }
                for (const auto& [name, rating] : value.items()) {
                    DamageType type{};
                    if (!ParseEnum(field, name, type, DamageTypes)) continue;
                    if (!rating.is_number_integer()) { TypeError(field, "integer ratings"); continue; }
                    out[type] = rating.get<std::int32_t>();
                }
            }

            // { "Piercing": 0.5, "Bludgeoning": -0.25 } -> fraction of damage removed per type
            void ReadResistanceFractions(const char* field, std::map<DamageType, float>& out) {
                if (!Present(field, false)) return;
                const auto& value = object_.at(field);
                if (!value.is_object()) { TypeError(field, "an object of damage type to fraction"); return; }
                for (const auto& [name, fraction] : value.items()) {
                    DamageType type{};
                    if (!ParseEnum(field, name, type, DamageTypes)) continue;
                    if (!fraction.is_number()) { TypeError(field, "numeric fractions"); continue; }
                    const float resist = fraction.get<float>();
                    if (resist < -1.0f || resist > 1.0f) {
                        Error(std::format("'{}' value for '{}' must be in [-1, 1]", field, name));
                        continue;
                    }
                    out[type] = resist;
                }
            }

            // "2d8" or "1d6+2"
            void ReadDice(const char* field, LB::Dice& out, bool required) {
                if (!Present(field, required)) return;
                const auto& value = object_.at(field);
                if (!value.is_string()) { TypeError(field, "dice like \"2d8\""); return; }

                const auto text = value.get<String>();
                if (auto dice = ParseDice(text)) out = *dice;
                else Error(std::format("'{}' has invalid dice '{}' (expected NdS or NdS+B)", field, text));
            }

            // "20" -> 20, "19-20" -> 19
            void ReadCritRange(const char* field, std::int32_t& out) {
                if (!Present(field, false)) return;
                const auto& value = object_.at(field);
                if (!value.is_string()) { TypeError(field, "a range like \"19-20\""); return; }

                const auto text = value.get<String>();
                std::int32_t low = 0;
                std::int32_t high = 0;
                const auto dash = text.find('-');
                const bool ok = dash == std::string::npos
                    ? ParseInt(text, low) && (high = low, true)
                    : ParseInt(text.substr(0, dash), low) && ParseInt(text.substr(dash + 1), high);
                if (!ok || high != 20 || low < 2 || low > 20) {
                    Error(std::format("'{}' has invalid crit range '{}' (expected \"20\" or \"N-20\")", field, text));
                    return;
                }
                out = low;
            }

        private:
            bool Present(const char* field, bool required) {
                if (Has(field)) return true;
                if (required) Error(std::format("missing required field '{}'", field));
                return false;
            }

            void TypeError(const char* field, const char* expected) {
                Error(std::format("'{}' must be {}", field, expected));
            }

            template<typename E, std::size_t N>
            bool ParseEnum(const char* field, const String& name, E& out, const EnumName<E> (&table)[N]) {
                if (auto value = FindEnum(table, name)) {
                    out = *value;
                    return true;
                }
                Error(std::format("'{}' has unknown value '{}'", field, name));
                return false;
            }

            static bool ParseInt(std::string_view text, std::int32_t& out) {
                const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
                return ec == std::errc{} && end == text.data() + text.size();
            }

            static std::optional<LB::Dice> ParseDice(std::string_view text) {
                const auto d = text.find('d');
                if (d == std::string_view::npos) return std::nullopt;
                const auto plus = text.find('+', d);

                LB::Dice dice{};
                if (!ParseInt(text.substr(0, d), dice.count)) return std::nullopt;
                if (!ParseInt(text.substr(d + 1, plus == std::string_view::npos ? std::string_view::npos : plus - d - 1), dice.sides)) return std::nullopt;
                if (plus != std::string_view::npos && !ParseInt(text.substr(plus + 1), dice.bonus)) return std::nullopt;
                if (dice.count < 1 || dice.sides < 2) return std::nullopt;
                return dice;
            }

            const Json& object_;
            std::string context_;
            std::vector<String>& errors_;
        };

        SpellDefinition ReadSpell(EntryReader& reader) {
            SpellDefinition spell{};
            reader.ReadString("id", spell.key, true);
            reader.ReadString("name", spell.name, true);
            reader.ReadEnum("school", spell.school, SpellSchools, true);
            reader.ReadInt("level", spell.level, true);
            reader.ReadEnumArray("categories", spell.categories, SpellCategories, true);
            reader.ReadOptionalEnum("requiredClass", spell.requiredClass, CharacterClasses);
            reader.ReadInt("requiredLevel", spell.requiredLevel, false);
            reader.ReadInt("manaCost", spell.manaCost, true);
            reader.ReadFloat("castTimeSeconds", spell.castTimeSeconds, true);
            reader.ReadFloat("cooldownSeconds", spell.cooldownSeconds, false);
            reader.ReadFloat("range", spell.range, false);
            reader.ReadFloat("radius", spell.radius, false);
            reader.ReadInt("baseDamage", spell.baseDamage, false);
            reader.ReadInt("baseHeal", spell.baseHeal, false);
            reader.ReadFloat("powerCoefficient", spell.powerCoefficient, false);
            reader.ReadEnum("damageType", spell.damageType, DamageTypes, false);
            reader.ReadFloat("durationSeconds", spell.durationSeconds, false);
            reader.ReadFloat("lifestealFraction", spell.lifestealFraction, false);
            reader.ReadStringArray("appliedStatusIds", spell.appliedStatusIds);
            reader.ReadStringArray("scalingAttributes", spell.scalingAttributes);
            reader.ReadString("notes", spell.notes, false);
            reader.ReadFloat("projectileSpeed", spell.projectileSpeed, false);
            reader.ReadFloat("dashTiles", spell.dashTiles, false);
            reader.ReadFloat("knockbackTiles", spell.knockbackTiles, false);
            if (spell.knockbackTiles < 0.0f) reader.Error("'knockbackTiles' must not be negative");
            reader.ReadFloat("evadeSeconds", spell.evadeSeconds, false);
            reader.ReadBool("teleportBehindTarget", spell.teleportBehindTarget);
            reader.ReadBool("canCrit", spell.canCrit);
            reader.ReadString("effect", spell.effect, false);
            reader.ReadFloat("blinkTiles", spell.blinkTiles, false);
            reader.ReadInt("cleansesStatuses", spell.cleansesStatuses, false);
            if (spell.dashTiles < 0.0f || spell.evadeSeconds < 0.0f || spell.cleansesStatuses < 0) reader.Error("'dashTiles', 'evadeSeconds', and 'cleansesStatuses' must not be negative");
            if (spell.teleportBehindTarget && spell.range <= 0.0f) reader.Error("'teleportBehindTarget' needs a 'range'");

            if (!spell.effect.empty() && std::ranges::find(SpellDefinition::UtilityEffects, std::string_view(spell.effect)) == SpellDefinition::UtilityEffects.end()) {
                reader.Error(std::format("unknown 'effect' '{}'", spell.effect));
            }
            if (spell.effect == "Blink" && spell.blinkTiles <= 0.0f) reader.Error("a Blink needs a positive 'blinkTiles'");
            if (spell.effect == "Buff" && spell.appliedStatusIds.empty()) reader.Error("a Buff needs 'appliedStatusIds'");
            if (spell.effect == "Cleanse" && spell.cleansesStatuses <= 0) reader.Error("a Cleanse needs 'cleansesStatuses'");
            if ((spell.effect == "RemoteInteract" || spell.effect == "IceWall") && spell.range <= 0.0f) reader.Error(std::format("a {} needs a 'range'", spell.effect));
            if ((spell.effect == "IceWall" || spell.effect == "RevealSecrets") && spell.durationSeconds <= 0.0f) reader.Error(std::format("a {} needs a 'durationSeconds'", spell.effect));
            if (spell.level < 1) reader.Error("'level' must be at least 1");
            if (spell.projectileSpeed < 0.0f) reader.Error("'projectileSpeed' must not be negative");
            if (spell.projectileSpeed > 0.0f && spell.range <= 0.0f) reader.Error("a missile ('projectileSpeed') needs a 'range'");
            if (spell.manaCost < 0) reader.Error("'manaCost' must not be negative");
            if (spell.castTimeSeconds < 0.0f) reader.Error("'castTimeSeconds' must not be negative");
            return spell;
        }

        WeaponDefinition ReadWeapon(EntryReader& reader) {
            WeaponDefinition weapon{};
            reader.ReadString("id", weapon.key, true);
            weapon.tier = TierOfKey(weapon.key);
            reader.ReadString("name", weapon.name, true);
            reader.ReadEnum("category", weapon.category, WeaponTypes, true);
            reader.ReadEnum("slot", weapon.slot, EquipmentSlots, false);
            reader.ReadInt("requiredLevel", weapon.requiredLevel, false);
            reader.ReadEnumArray("allowedClasses", weapon.allowedClasses, CharacterClasses, true);
            reader.ReadDice("damageDice", weapon.damageDice, true);
            reader.ReadInt("damageBonus", weapon.damageBonus, false);
            reader.ReadEnumArray("damageType", weapon.damageTypes, DamageTypes, true, /*allowSingle*/ true);
            reader.ReadFloat("attackTimeSeconds", weapon.attackTimeSeconds, true);
            reader.ReadCritRange("critRange", weapon.critRangeMin);
            reader.ReadFloat("critMultiplier", weapon.critMultiplier, false);
            reader.ReadString("attributeScale", weapon.attributeScale, false);
            reader.ReadFloat("scaleFactor", weapon.scaleFactor, false);
            reader.ReadBool("isTwoHanded", weapon.isTwoHanded);
            reader.ReadStringArray("specialProperties", weapon.specialProperties);
            reader.ReadInt("affixSlots", weapon.affixSlots, false);
            reader.ReadString("ammoType", weapon.ammoType, false);
            reader.ReadInt("value", weapon.value, false);
            reader.ReadFloat("weight", weapon.weight, false);
            if (weapon.weight < 0.0f) reader.Error("'weight' must not be negative");
            reader.ReadFloat("range", weapon.range, false);
            reader.ReadFloat("projectileSpeed", weapon.projectileSpeed, false);

            if (weapon.attackTimeSeconds <= 0.0f) reader.Error("'attackTimeSeconds' must be positive");
            if (weapon.critMultiplier < 1.0f) reader.Error("'critMultiplier' must be at least 1");
            // Bows, crossbows, and wands shoot: they need a range and a projectile speed; nothing else does
            const bool shoots = weapon.category == WeaponType::Bow || weapon.category == WeaponType::Crossbow || weapon.category == WeaponType::Wand;
            if (shoots && (weapon.range <= 0.0f || weapon.projectileSpeed <= 0.0f)) reader.Error("bows, crossbows, and wands need a positive 'range' and 'projectileSpeed'");
            if (!shoots && (weapon.range != 0.0f || weapon.projectileSpeed != 0.0f)) reader.Error("only bows, crossbows, and wands have a 'range' or 'projectileSpeed'");
            return weapon;
        }

        ArmorDefinition ReadArmor(EntryReader& reader) {
            ArmorDefinition armor{};
            reader.ReadString("id", armor.key, true);
            armor.tier = TierOfKey(armor.key);
            reader.ReadString("name", armor.name, true);
            reader.ReadEnum("slot", armor.slot, EquipmentSlots, true);
            reader.ReadEnum("category", armor.category, ArmorCategories, true);
            reader.ReadInt("value", armor.value, false);
            reader.ReadInt("requiredLevel", armor.requiredLevel, false);
            reader.ReadEnumArray("allowedClasses", armor.allowedClasses, CharacterClasses, true);
            reader.ReadInt("physicalAR", armor.physicalAR, false);
            reader.ReadResistances("elementalAR", armor.elementalAR);
            reader.ReadResistances("spiritualAR", armor.spiritualAR);
            reader.ReadInt("armorBonus", armor.armorBonus, false);
            reader.ReadFloat("blockChance", armor.blockChance, false);
            reader.ReadInt("blockValue", armor.blockValue, false);
            reader.ReadFloat("weight", armor.weight, false);
            reader.ReadFloat("movementPenalty", armor.movementPenalty, false);
            reader.ReadFloat("castPenalty", armor.castPenalty, false);
            reader.ReadInt("affixSlots", armor.affixSlots, false);

            if (armor.movementPenalty < 0.0f || armor.movementPenalty >= 1.0f) reader.Error("'movementPenalty' must be in [0, 1)");
            if (armor.blockChance < 0.0f || armor.blockChance > 100.0f) reader.Error("'blockChance' must be a percentage in [0, 100]");
            return armor;
        }

        ClassDefinition ReadClass(EntryReader& reader) {
            ClassDefinition definition{};
            reader.ReadString("id", definition.key, true);
            reader.ReadEnum("class", definition.characterClass, CharacterClasses, true);
            reader.ReadString("name", definition.name, true);
            reader.ReadString("role", definition.role, false);
            reader.ReadEnumArray("primaryAttributes", definition.primaryAttributes, AttributeTypes, true);
            reader.ReadString("resource", definition.resource, true);
            reader.ReadFloat("baseMovementSpeed", definition.baseMovementSpeed, true);
            reader.ReadEnumArray("spellSchools", definition.spellSchools, SpellSchools, false);
            reader.ReadStringArray("signatureAbilities", definition.signatureAbilities);
            reader.ReadStringArray("spellBar", definition.spellBar);
            reader.ReadFloat("manaRegenPerSecond", definition.manaRegenPerSecond, false);
            reader.ReadInt("spellPowerBase", definition.spellPowerBase, false);
            reader.ReadString("startingWeapon", definition.startingWeapon, false);
            reader.ReadDice("sneakAttackDice", definition.sneakAttackDice, false);
            if (const Json* paths = reader.Nested("specializations", false, true)) {
                std::size_t index = 0;
                for (const auto& path : *paths) {
                    const std::string context = std::format("{}: specializations[{}]", reader.Context(), index++);
                    if (!path.is_object()) {
                        reader.Errors().push_back(std::format("{}: must be a JSON object", context));
                        continue;
                    }
                    EntryReader pathReader(path, context, reader.Errors());
                    SpecializationDefinition specialization{};
                    pathReader.ReadString("id", specialization.key, true);
                    pathReader.ReadString("name", specialization.name, true);
                    pathReader.ReadString("description", specialization.description, false);
                    pathReader.ReadString("flavor", specialization.flavor, false);
                    if (const Json* color = pathReader.Nested("color", false, true)) {
                        if (color->size() != 3) pathReader.Error("'color' must be three numbers (display RGB, 0..1)");
                        else {
                            for (std::size_t i = 0; i < 3; ++i) {
                                if (!(*color)[i].is_number()) pathReader.Error("'color' must be three numbers (display RGB, 0..1)");
                                else specialization.color[i] = (*color)[i].get<float>();
                            }
                        }
                    }
                    pathReader.ReadInt("damage", specialization.damage, false);
                    pathReader.ReadInt("attackBonus", specialization.attackBonus, false);
                    pathReader.ReadInt("armorClass", specialization.armorClass, false);
                    pathReader.ReadInt("armor", specialization.armor, false);
                    pathReader.ReadInt("spellPower", specialization.spellPower, false);
                    pathReader.ReadFloat("manaRegen", specialization.manaRegen, false);
                    definition.specializations.push_back(std::move(specialization));
                }
            }
            reader.ReadStringArray("startingItems", definition.startingItems);
            if (definition.spellBar.size() > 4) reader.Error("'spellBar' holds at most 4 spells");
            if (definition.manaRegenPerSecond < 0.0f) reader.Error("'manaRegenPerSecond' must not be negative");

            if (definition.resource != "Mana" && definition.resource != "Stamina") {
                if (!definition.resource.empty()) reader.Error(std::format("'resource' must be Mana or Stamina, not '{}'", definition.resource));
            }
            if (definition.baseMovementSpeed <= 0.0f) reader.Error("'baseMovementSpeed' must be positive");

            // "startingAttributes": { "STR": 16, ... } - all six, each in the domain's [1, 30]
            if (const Json* attributes = reader.Nested("startingAttributes", true, false)) {
                std::int32_t* targets[] = {
                    &definition.startingAttributes.strength, &definition.startingAttributes.dexterity,
                    &definition.startingAttributes.constitution, &definition.startingAttributes.intelligence,
                    &definition.startingAttributes.wisdom, &definition.startingAttributes.charisma,
                };
                for (std::size_t i = 0; i < std::size(AttributeNames); ++i) {
                    const std::string name(AttributeNames[i]);
                    if (!attributes->contains(name) || !(*attributes)[name].is_number_integer()) {
                        reader.Error(std::format("'startingAttributes' needs an integer '{}'", name));
                        continue;
                    }
                    *targets[i] = (*attributes)[name].get<std::int32_t>();
                }
                if (!definition.startingAttributes.IsValid()) reader.Error("'startingAttributes' values must be in [1, 30]");

            }

            // "attributeGrowth": { "autoPointsPerLevel": 1, "freePointsPerLevel": 1, caps }
            if (const Json* growth = reader.Nested("attributeGrowth", true, false)) {
                EntryReader growthReader(*growth, reader.Context() + ": attributeGrowth", reader.Errors());
                auto& g = definition.attributeGrowth;
                growthReader.ReadInt("autoPointsPerLevel", g.autoPointsPerLevel, true);
                growthReader.ReadInt("freePointsPerLevel", g.freePointsPerLevel, true);
                growthReader.ReadInt("primaryAttributeCap", g.primaryAttributeCap, true);
                growthReader.ReadInt("otherAttributeCap", g.otherAttributeCap, true);
                if (g.autoPointsPerLevel < 0 || g.freePointsPerLevel < 0) growthReader.Error("points per level must not be negative");
                if (g.otherAttributeCap < 1 || g.primaryAttributeCap > 30) growthReader.Error("caps must be in [1, 30]");
                if (g.primaryAttributeCap < g.otherAttributeCap) growthReader.Error("'primaryAttributeCap' must be at least 'otherAttributeCap'");
            }

            // "armorAdjustments": { "Heavy": { "movement": 0.5, "casting": 1.0 }, ... } - every category
            if (const Json* adjustments = reader.Nested("armorAdjustments", true, false)) {
                for (const auto& [name, value] : adjustments->items()) {
                    const auto category = FindEnum(ArmorCategories, name);
                    if (!category) {
                        reader.Error(std::format("'armorAdjustments' has unknown armour category '{}'", name));
                        continue;
                    }
                    if (!value.is_object()) {
                        reader.Error(std::format("'armorAdjustments' value for '{}' must be an object", name));
                        continue;
                    }
                    EntryReader adjustmentReader(value, std::format("{}: armorAdjustments.{}", reader.Context(), name), reader.Errors());
                    ArmorAdjustment adjustment{};
                    adjustmentReader.ReadFloat("movement", adjustment.movementScale, true);
                    adjustmentReader.ReadFloat("casting", adjustment.castingScale, true);
                    if (adjustment.movementScale < 0.0f || adjustment.castingScale < 0.0f) adjustmentReader.Error("scales must not be negative");
                    definition.armorAdjustments[*category] = adjustment;
                }
                for (const auto& entry : ArmorCategories) {
                    if (!definition.armorAdjustments.contains(entry.value)) {
                        reader.Error(std::format("'armorAdjustments' has no value for '{}'", entry.name));
                    }
                }
            }

            // "weaponAdjustments": { "Sword": 3, ... } - one value for every weapon category
            if (const Json* adjustments = reader.Nested("weaponAdjustments", true, false)) {
                for (const auto& [name, value] : adjustments->items()) {
                    const auto weapon = FindEnum(WeaponTypes, name);
                    if (!weapon) {
                        reader.Error(std::format("'weaponAdjustments' has unknown weapon category '{}'", name));
                        continue;
                    }
                    if (!value.is_number_integer()) {
                        reader.Error(std::format("'weaponAdjustments' value for '{}' must be an integer", name));
                        continue;
                    }
                    if (!definition.weaponAdjustments.emplace(*weapon, value.get<std::int32_t>()).second) {
                        reader.Error(std::format("'weaponAdjustments' lists '{}' more than once (Polearm is Spear)", NameOf(WeaponTypes, *weapon)));
                    }
                }
                for (const auto& entry : WeaponTypes) {
                    if (!definition.weaponAdjustments.contains(entry.value) && entry.name == NameOf(WeaponTypes, entry.value)) {
                        reader.Error(std::format("'weaponAdjustments' has no value for '{}'", entry.name));
                    }
                }
            }

            // "progression": levels 1..MaxLevel in order
            if (const Json* rows = reader.Nested("progression", true, true)) {
                std::size_t index = 0;
                for (const auto& row : *rows) {
                    const std::string context = std::format("{}: progression[{}]", reader.Context(), index++);
                    if (!row.is_object()) {
                        reader.Errors().push_back(std::format("{}: must be a JSON object", context));
                        continue;
                    }
                    EntryReader rowReader(row, context, reader.Errors());
                    LB::Character::ClassLevelGains level{};
                    rowReader.ReadInt("level", level.level, true);
                    rowReader.ReadInt("hpGain", level.hpGain, true);
                    rowReader.ReadInt("mpGain", level.mpGain, true);
                    rowReader.ReadInt("meleeAttackBonus", level.meleeAttackBonus, true);
                    rowReader.ReadInt("rangedAttackBonus", level.rangedAttackBonus, true);
                    rowReader.ReadInt("physicalSaveBonus", level.physicalSaveBonus, true);
                    rowReader.ReadInt("mentalSaveBonus", level.mentalSaveBonus, true);
                    if (level.hpGain < 0 || level.mpGain < 0) rowReader.Error("gains must not be negative");
                    definition.progression.levels.push_back(level);
                }

                if (!definition.progression.IsComplete()) {
                    reader.Error(std::format("'progression' must list levels 1 to {} in order", ClassProgression::MaxLevel));
                }
            }

            // Starting attributes must respect the class caps (read above)
            for (const auto& entry : AttributeTypes) {
                const std::int32_t cap = definition.IsPrimaryAttribute(entry.value) ? definition.attributeGrowth.primaryAttributeCap : definition.attributeGrowth.otherAttributeCap;
                if (definition.startingAttributes.Get(entry.value) > cap) {
                    reader.Error(std::format("starting {} {} is above the class cap {}", entry.name, definition.startingAttributes.Get(entry.value), cap));
                }
            }
            return definition;
        }

        EnemyDefinition ReadEnemy(EntryReader& reader) {
            EnemyDefinition enemy{};
            reader.ReadString("id", enemy.key, true);
            reader.ReadString("name", enemy.name, true);
            reader.ReadInt("level", enemy.level, false);
            reader.ReadInt("hitPoints", enemy.hitPoints, true);
            reader.ReadInt("armorClass", enemy.armorClass, true);
            reader.ReadInt("physicalArmor", enemy.physicalArmor, false);
            reader.ReadResistanceFractions("resistances", enemy.resistances);
            reader.ReadInt("experience", enemy.experience, false);

            reader.ReadInt("attackBonus", enemy.attackBonus, true);
            reader.ReadDice("damageDice", enemy.damageDice, true);
            reader.ReadInt("damageBonus", enemy.damageBonus, false);
            reader.ReadEnum("damageType", enemy.damageType, DamageTypes, true);
            reader.ReadCritRange("critRange", enemy.critRangeMin);
            reader.ReadFloat("critMultiplier", enemy.critMultiplier, false);
            reader.ReadFloat("attackTimeSeconds", enemy.attackTimeSeconds, true);
            reader.ReadFloat("attackRecoverySeconds", enemy.attackRecoverySeconds, false);
            reader.ReadFloat("attackRange", enemy.attackRange, true);
            reader.ReadString("attackAnimation", enemy.attackAnimation, false);
            reader.ReadFloat("attackImpactFraction", enemy.attackImpactFraction, false);
            reader.ReadFloat("movementSpeed", enemy.movementSpeed, true);
            reader.ReadFloat("aggroRange", enemy.aggroRange, false);
            reader.ReadFloat("projectileSpeed", enemy.projectileSpeed, false);
            reader.ReadString("projectileModel", enemy.projectileModel, false);
            reader.ReadFloat("keepAwayRange", enemy.keepAwayRange, false);
            reader.ReadString("spell", enemy.spell, false);
            reader.ReadInt("spellPower", enemy.spellPower, false);
            reader.ReadString("rig", enemy.rig, false);
            reader.ReadBool("staggerImmune", enemy.staggerImmune);
            reader.ReadFloat("lungeRange", enemy.lungeRange, false);
            reader.ReadFloat("lungeSpeed", enemy.lungeSpeed, false);
            reader.ReadFloat("lungeCooldownSeconds", enemy.lungeCooldownSeconds, false);
            reader.ReadFloat("lifeSteal", enemy.lifeSteal, false);
            reader.ReadBool("rises", enemy.rises);
            reader.ReadString("voice", enemy.voice, false);
            if (!enemy.voice.empty() && enemy.voice != "beast" && enemy.voice != "brute" && enemy.voice != "shriek" && enemy.voice != "giant") {
                reader.Error("'voice' must be beast, brute, shriek, or giant");
            }
            if (enemy.lungeRange < 0.0f || enemy.lungeSpeed <= 0.0f) reader.Error("'lungeRange' must not be negative and 'lungeSpeed' must be positive");
            if (enemy.lifeSteal < 0.0f || enemy.lifeSteal > 1.0f) reader.Error("'lifeSteal' must be in [0, 1]");
            reader.ReadBool("boss", enemy.boss);
            reader.ReadString("summon", enemy.summon, false);
            reader.ReadStringArray("summons", enemy.summons);
            reader.ReadInt("summonFromPhase", enemy.summonFromPhase, false);
            reader.ReadInt("summonCount", enemy.summonCount, false);
            reader.ReadInt("maxSummons", enemy.maxSummons, false);
            reader.ReadFloat("summonCooldownSeconds", enemy.summonCooldownSeconds, false);
            reader.ReadFloat("phaseTwoHealthFraction", enemy.phaseTwoHealthFraction, false);
            reader.ReadFloat("phaseThreeHealthFraction", enemy.phaseThreeHealthFraction, false);
            reader.ReadFloat("slamRadius", enemy.slamRadius, false);
            reader.ReadFloat("slamWindupSeconds", enemy.slamWindupSeconds, false);
            reader.ReadFloat("slamCooldownSeconds", enemy.slamCooldownSeconds, false);
            reader.ReadFloat("slamTriggerRange", enemy.slamTriggerRange, false);
            reader.ReadBool("slamAtTarget", enemy.slamAtTarget);
            reader.ReadDice("slamDamage", enemy.slamDamage, false);
            reader.ReadEnum("slamDamageType", enemy.slamDamageType, DamageTypes, false);
            reader.ReadBool("tank", enemy.tank);
            if (const auto* slam = reader.Nested("phaseThreeSlam", false, false)) {
                EntryReader nested(*slam, reader.Context() + ": phaseThreeSlam", reader.Errors());
                TelegraphDefinition telegraph{};
                nested.ReadFloat("radius", telegraph.radius, true);
                nested.ReadFloat("windupSeconds", telegraph.windupSeconds, true);
                nested.ReadFloat("cooldownSeconds", telegraph.cooldownSeconds, true);
                nested.ReadFloat("triggerRange", telegraph.triggerRange, true);
                nested.ReadBool("atTarget", telegraph.atTarget);
                nested.ReadDice("damage", telegraph.damage, true);
                nested.ReadEnum("damageType", telegraph.damageType, DamageTypes, true);
                if (telegraph.radius <= 0.0f || telegraph.windupSeconds <= 0.0f || telegraph.cooldownSeconds <= 0.0f) {
                    nested.Error("'radius', 'windupSeconds', and 'cooldownSeconds' must be positive");
                }
                enemy.phaseThreeSlam = telegraph;
            }
            if (enemy.slamRadius < 0.0f) reader.Error("'slamRadius' must not be negative");
            if (enemy.slamRadius > 0.0f && enemy.slamWindupSeconds <= 0.0f) reader.Error("'slamWindupSeconds' must be positive");
            if (enemy.rig != "Medium" && enemy.rig != "Large") reader.Error(std::format("'rig' must be Medium or Large, not '{}'", enemy.rig));
            if (enemy.IsSummoner() && (enemy.summonCount < 1 || enemy.maxSummons < 1 || enemy.summonCooldownSeconds <= 0.0f)) {
                reader.Error("a summoner needs a positive 'summonCount', 'maxSummons', and 'summonCooldownSeconds'");
            }
            if (enemy.phaseTwoHealthFraction < 0.0f || enemy.phaseTwoHealthFraction >= 1.0f) reader.Error("'phaseTwoHealthFraction' must be in [0, 1)");
            if (enemy.phaseThreeHealthFraction < 0.0f || (enemy.phaseThreeHealthFraction > 0.0f && enemy.phaseThreeHealthFraction >= enemy.phaseTwoHealthFraction)) {
                reader.Error("'phaseThreeHealthFraction' must be below 'phaseTwoHealthFraction' (0: no third phase)");
            }
            if (enemy.summonFromPhase < 1 || enemy.summonFromPhase > 3) reader.Error("'summonFromPhase' must be 1, 2, or 3");

            reader.ReadString("lootTable", enemy.lootTable, false);
            reader.ReadString("model", enemy.model, true);
            reader.ReadString("mainHandModel", enemy.mainHandModel, false);
            reader.ReadString("offHandModel", enemy.offHandModel, false);
            reader.ReadFloat("modelScale", enemy.modelScale, false);
            if (const auto* tint = reader.Nested("tint", false, true)) {
                if (tint->size() != 3 || !std::ranges::all_of(*tint, [](const Json& c) { return c.is_number() && c.get<float>() >= 0.0f; })) {
                    reader.Error("'tint' must be three numbers (red, green, blue), none negative");
                }
                else {
                    for (std::size_t i = 0; i < 3; ++i) enemy.tint[i] = (*tint)[i].get<float>();
                }
            }

            if (enemy.level < 1) reader.Error("'level' must be at least 1");
            if (enemy.hitPoints < 1) reader.Error("'hitPoints' must be at least 1");
            if (enemy.armorClass < 0) reader.Error("'armorClass' must not be negative");
            if (enemy.physicalArmor < 0) reader.Error("'physicalArmor' must not be negative");
            if (enemy.experience < 0) reader.Error("'experience' must not be negative");
            if (enemy.critMultiplier < 1.0f) reader.Error("'critMultiplier' must be at least 1");
            if (enemy.attackTimeSeconds <= 0.0f) reader.Error("'attackTimeSeconds' must be positive");
            if (enemy.attackRecoverySeconds < 0.0f) reader.Error("'attackRecoverySeconds' must not be negative");
            if (enemy.attackRange <= 0.0f) reader.Error("'attackRange' must be positive");
            if (enemy.attackImpactFraction <= 0.0f || enemy.attackImpactFraction >= 1.0f) reader.Error("'attackImpactFraction' must be between 0 and 1");
            if (enemy.movementSpeed <= 0.0f) reader.Error("'movementSpeed' must be positive");
            if (enemy.aggroRange < 0.0f) reader.Error("'aggroRange' must not be negative");
            if (enemy.modelScale <= 0.0f) reader.Error("'modelScale' must be positive");
            if (enemy.projectileSpeed < 0.0f) reader.Error("'projectileSpeed' must not be negative");
            if (enemy.projectileSpeed > 0.0f && enemy.projectileModel.empty()) reader.Error("a ranged enemy ('projectileSpeed') needs a 'projectileModel'");
            if (enemy.projectileSpeed <= 0.0f && !enemy.projectileModel.empty()) reader.Error("'projectileModel' needs a 'projectileSpeed'");
            if (enemy.IsCaster() && enemy.projectileSpeed > 0.0f) reader.Error("a caster ('spell') casts its spell's missile: no 'projectileSpeed'");
            if (enemy.spellPower < 0) reader.Error("'spellPower' must not be negative");
            if (enemy.keepAwayRange < 0.0f || (enemy.keepAwayRange > 0.0f && enemy.keepAwayRange >= enemy.attackRange)) {
                reader.Error("'keepAwayRange' must be between 0 and 'attackRange'");
            }
            return enemy;
        }

        LoreDefinition ReadLore(EntryReader& reader) {
            LoreDefinition lore;
            reader.ReadString("id", lore.key, true);
            reader.ReadInt("tier", lore.tier, true);
            reader.ReadString("title", lore.title, true);
            reader.ReadString("text", lore.text, true);
            if (lore.tier < 1) reader.Error("'tier' must be at least 1");
            return lore;
        }

        MutatorDefinition ReadMutator(EntryReader& reader) {
            MutatorDefinition mutator;
            reader.ReadString("id", mutator.key, true);
            reader.ReadString("name", mutator.name, true);
            reader.ReadString("description", mutator.description, true);
            reader.ReadFloat("enemyHitPoints", mutator.enemyHitPoints, false);
            reader.ReadInt("enemyArmorClass", mutator.enemyArmorClass, false);
            reader.ReadInt("enemyAttackBonus", mutator.enemyAttackBonus, false);
            reader.ReadInt("enemyDamageBonus", mutator.enemyDamageBonus, false);
            reader.ReadFloat("enemyLifeSteal", mutator.enemyLifeSteal, false);
            reader.ReadInt("extraEnemies", mutator.extraEnemies, false);
            reader.ReadFloat("goldMultiplier", mutator.goldMultiplier, false);
            if (mutator.enemyHitPoints < 0.0f || mutator.enemyLifeSteal < 0.0f || mutator.enemyLifeSteal > 1.0f || mutator.extraEnemies < 0 || mutator.goldMultiplier <= 0.0f) {
                reader.Error("'enemyHitPoints' and 'extraEnemies' must not be negative, 'enemyLifeSteal' must be 0 to 1, and 'goldMultiplier' positive");
            }
            return mutator;
        }

        // objectives.json: one object of settings
        ObjectiveSettings ReadObjectives(const String& text, std::vector<String>& errors) {
            ObjectiveSettings settings;
            const Json json = Json::parse(text, nullptr, /*allow_exceptions*/ false);
            if (json.is_discarded() || !json.is_object()) {
                errors.push_back("objectives.json: expected one JSON object");
                return settings;
            }
            EntryReader reader(json, "objectives.json", errors);
            reader.ReadInt("rewardGoldPerDepth", settings.rewardGoldPerDepth, true);
            reader.ReadInt("rewardExperiencePerDepth", settings.rewardExperiencePerDepth, true);
            reader.ReadFloat("championHitPointsMultiplier", settings.championHitPointsMultiplier, true);
            reader.ReadInt("championDamageBonus", settings.championDamageBonus, true);
            reader.ReadFloat("championExperienceMultiplier", settings.championExperienceMultiplier, true);
            reader.ReadStringArray("championNames", settings.championNames);
            reader.ReadStringArray("championTitles", settings.championTitles);
            reader.ReadString("captiveName", settings.captiveName, true);
            reader.ReadString("captiveModel", settings.captiveModel, true);
            reader.ReadString("cageModel", settings.cageModel, true);
            reader.ReadInt("bountyKills", settings.bountyKills, true);
            reader.ReadInt("bountyGoldPerDepth", settings.bountyGoldPerDepth, true);
            if (settings.championNames.empty() || settings.championTitles.empty()) reader.Error("'championNames' and 'championTitles' need at least one each");
            if (settings.championHitPointsMultiplier < 1.0f || settings.championExperienceMultiplier < 1.0f) reader.Error("the champion's multipliers must be at least 1");
            if (settings.bountyKills <= 0) reader.Error("'bountyKills' must be positive");
            if (!json.contains("relics") || !json["relics"].is_array() || json["relics"].empty()) {
                reader.Error("'relics' must be a non-empty array");
            }
            else {
                std::size_t index = 0;
                for (const auto& entry : json["relics"]) {
                    ++index;
                    if (!entry.is_object()) {
                        reader.Error(std::format("relic {} must be an object", index));
                        continue;
                    }
                    EntryReader relicReader(entry, std::format("objectives.json: relic {}", index), errors);
                    RelicDefinition relic;
                    relicReader.ReadString("name", relic.name, true);
                    relicReader.ReadString("model", relic.model, true);
                    settings.relics.push_back(std::move(relic));
                }
            }
            return settings;
        }

        AllyDefinition ReadAlly(EntryReader& reader) {
            AllyDefinition ally{};
            reader.ReadString("id", ally.key, true);
            reader.ReadString("name", ally.name, true);
            reader.ReadString("description", ally.description, false);
            reader.ReadInt("hitPoints", ally.hitPoints, true);
            reader.ReadInt("hitPointsPerLevel", ally.hitPointsPerLevel, false);
            reader.ReadInt("armorClass", ally.armorClass, true);
            reader.ReadInt("physicalArmor", ally.physicalArmor, false);
            reader.ReadInt("attackBonus", ally.attackBonus, true);
            reader.ReadDice("damageDice", ally.damageDice, true);
            reader.ReadInt("damageBonus", ally.damageBonus, false);
            reader.ReadEnum("damageType", ally.damageType, DamageTypes, true);
            reader.ReadFloat("attackTimeSeconds", ally.attackTimeSeconds, true);
            reader.ReadFloat("attackRecoverySeconds", ally.attackRecoverySeconds, false);
            reader.ReadFloat("attackRange", ally.attackRange, true);
            reader.ReadFloat("projectileSpeed", ally.projectileSpeed, false);
            reader.ReadString("projectileModel", ally.projectileModel, false);
            reader.ReadFloat("movementSpeed", ally.movementSpeed, true);
            reader.ReadInt("price", ally.price, true);
            reader.ReadInt("healAmount", ally.healAmount, false);
            reader.ReadFloat("healCooldownSeconds", ally.healCooldownSeconds, false);
            reader.ReadFloat("healRange", ally.healRange, false);
            reader.ReadString("model", ally.model, true);
            reader.ReadString("mainHandModel", ally.mainHandModel, false);
            reader.ReadFloat("modelScale", ally.modelScale, false);
            if (ally.hitPoints <= 0) reader.Error("'hitPoints' must be positive");
            if (ally.attackTimeSeconds <= 0.0f) reader.Error("'attackTimeSeconds' must be positive");
            if (ally.attackRange <= 0.0f) reader.Error("'attackRange' must be positive");
            if (ally.movementSpeed <= 0.0f) reader.Error("'movementSpeed' must be positive");
            if (ally.price < 0) reader.Error("'price' must not be negative");
            if (ally.projectileSpeed < 0.0f) reader.Error("'projectileSpeed' must not be negative");
            if (ally.projectileSpeed > 0.0f && ally.projectileModel.empty()) reader.Error("a shooter ('projectileSpeed') needs a 'projectileModel'");
            return ally;
        }

        RarityDefinition ReadRarity(EntryReader& reader) {
            RarityDefinition rarity{};
            reader.ReadString("id", rarity.key, true);
            reader.ReadString("name", rarity.name, true);
            reader.ReadInt("affixes", rarity.affixes, true);
            reader.ReadFloat("weight", rarity.weight, true);
            reader.ReadFloat("weightPerDepth", rarity.weightPerDepth, false);
            reader.ReadFloat("priceMultiplier", rarity.priceMultiplier, false);
            reader.ReadBool("legendary", rarity.legendary);
            if (const Json* color = reader.Nested("color", true, true)) {
                if (color->size() != 3) reader.Error("'color' must be three numbers (display RGB, 0..1)");
                else {
                    for (std::size_t i = 0; i < 3; ++i) {
                        if (!(*color)[i].is_number()) reader.Error("'color' must be three numbers (display RGB, 0..1)");
                        else rarity.color[i] = (*color)[i].get<float>();
                    }
                }
            }
            if (rarity.affixes < 0 || rarity.affixes > 3) reader.Error("'affixes' must be in [0, 3]");
            if (rarity.weight < 0.0f) reader.Error("'weight' must not be negative");
            return rarity;
        }

        AffixDefinition ReadAffix(EntryReader& reader) {
            AffixDefinition affix{};
            String type;
            std::vector<String> appliesTo;
            reader.ReadString("id", affix.key, true);
            reader.ReadString("name", affix.name, true);
            reader.ReadString("type", type, true);
            reader.ReadStringArray("appliesTo", appliesTo);
            reader.ReadInt("damage", affix.damage, false);
            reader.ReadInt("attackBonus", affix.attackBonus, false);
            reader.ReadInt("armorClass", affix.armorClass, false);
            reader.ReadInt("armor", affix.armor, false);
            reader.ReadInt("spellPower", affix.spellPower, false);
            reader.ReadFloat("manaRegen", affix.manaRegen, false);
            reader.ReadInt("strength", affix.strength, false);
            reader.ReadInt("dexterity", affix.dexterity, false);
            reader.ReadInt("intelligence", affix.intelligence, false);
            reader.ReadInt("wisdom", affix.wisdom, false);
            reader.ReadInt("hitPoints", affix.hitPoints, false);
            reader.ReadInt("mana", affix.mana, false);
            reader.ReadString("power", affix.power, type == "Power");
            reader.ReadFloat("chance", affix.chance, false);
            reader.ReadInt("powerDamage", affix.powerDamage, false);
            reader.ReadString("description", affix.description, false);
            if (type != "Prefix" && type != "Suffix" && type != "Power") reader.Error(std::format("'type' must be Prefix, Suffix, or Power, not '{}'", type));
            if (type == "Power" && affix.power != "ChainLightning" && affix.power != "LifeSteal" && affix.power != "Frostbite" && affix.power != "Ignite" && affix.power != "Thorns") {
                reader.Error(std::format("'power' '{}' is not one the game knows (ChainLightning, LifeSteal, Frostbite, Ignite, Thorns)", affix.power));
            }
            if (affix.chance < 0.0f || affix.chance > 1.0f) reader.Error("'chance' must be in [0, 1]");
            affix.prefix = type == "Prefix";
            for (const auto& target : appliesTo) {
                if (target == "Weapon") affix.onWeapons = true;
                else if (target == "Armor") affix.onArmour = true;
                else reader.Error(std::format("'appliesTo' lists '{}' (Weapon or Armor)", target));
            }
            if (!affix.onWeapons && !affix.onArmour) reader.Error("'appliesTo' must list Weapon, Armor, or both");
            return affix;
        }

        StatusDefinition ReadStatus(EntryReader& reader) {
            StatusDefinition status{};
            reader.ReadString("id", status.key, true);
            reader.ReadString("displayName", status.displayName, true);
            reader.ReadStringArray("categories", status.categories);
            reader.ReadFloat("durationSeconds", status.durationSeconds, true);
            reader.ReadFloat("tickIntervalSeconds", status.tickIntervalSeconds, false);
            reader.ReadFloat("movementMultiplier", status.movementMultiplier, false);
            reader.ReadBool("preventsActions", status.preventsActions);
            reader.ReadInt("damagePerTick", status.damagePerTick, false);
            reader.ReadEnum("damageType", status.damageType, DamageTypes, false);
            reader.ReadInt("attackBonus", status.attackBonus, false);
            reader.ReadFloat("damageTakenMultiplier", status.damageTakenMultiplier, false);

            if (status.durationSeconds <= 0.0f) reader.Error("'durationSeconds' must be positive");
            if (status.damageTakenMultiplier < 0.0f) reader.Error("'damageTakenMultiplier' must not be negative");
            if (status.tickIntervalSeconds <= 0.0f) reader.Error("'tickIntervalSeconds' must be positive");
            if (status.movementMultiplier < 0.0f || status.movementMultiplier > 1.0f) reader.Error("'movementMultiplier' must be between 0 and 1");
            if (status.damagePerTick < 0) reader.Error("'damagePerTick' must not be negative");
            if (status.damagePerTick > 0 && status.damageType == DamageType::None) reader.Error("'damagePerTick' needs a 'damageType'");
            return status;
        }

        ConsumableDefinition ReadConsumable(EntryReader& reader) {
            ConsumableDefinition consumable{};
            consumable.healing = LB::Dice{ 0, 0, 0 };
            reader.ReadString("id", consumable.key, true);
            reader.ReadString("name", consumable.name, true);
            reader.ReadDice("healing", consumable.healing, false);
            reader.ReadFloat("healingPerLevel", consumable.healingPerLevel, false);
            if (consumable.healingPerLevel < 0.0f) reader.Error("'healingPerLevel' must not be negative");
            reader.ReadInt("maxStack", consumable.maxStack, false);
            reader.ReadString("ammoType", consumable.ammoType, false);
            reader.ReadBool("isKey", consumable.isKey);
            reader.ReadInt("value", consumable.value, false);
            reader.ReadFloat("weight", consumable.weight, false);
            if (consumable.weight < 0.0f) reader.Error("'weight' must not be negative");
            reader.ReadString("model", consumable.model, true);

            if (consumable.maxStack < 1) reader.Error("'maxStack' must be at least 1");
            return consumable;
        }

        LootTableDefinition ReadLootTable(EntryReader& reader) {
            LootTableDefinition table{};
            reader.ReadString("id", table.key, true);
            reader.ReadFloat("goldChance", table.goldChance, false);
            reader.ReadDice("gold", table.gold, table.goldChance > 0.0f);
            reader.ReadFloat("itemChance", table.itemChance, false);

            if (const Json* items = reader.Nested("items", table.itemChance > 0.0f, true)) {
                std::size_t index = 0;
                for (const auto& item : *items) {
                    const std::string context = std::format("{}: items[{}]", reader.Context(), index++);
                    if (!item.is_object()) {
                        reader.Errors().push_back(std::format("{}: must be a JSON object", context));
                        continue;
                    }
                    EntryReader itemReader(item, context, reader.Errors());
                    LootEntry entry{};
                    itemReader.ReadString("item", entry.item, true);
                    itemReader.ReadInt("weight", entry.weight, false);
                    itemReader.ReadDice("quantity", entry.quantity, false);
                    if (entry.weight < 1) itemReader.Error("'weight' must be at least 1");
                    if (entry.quantity.count < 0 || entry.quantity.GetMaxRoll() < 1) itemReader.Error("'quantity' must be able to roll at least 1");
                    table.items.push_back(std::move(entry));
                }
            }

            if (table.goldChance < 0.0f || table.goldChance > 1.0f) reader.Error("'goldChance' must be in [0, 1]");
            if (table.itemChance < 0.0f || table.itemChance > 1.0f) reader.Error("'itemChance' must be in [0, 1]");
            if (table.itemChance > 0.0f && table.items.empty()) reader.Error("'items' must list at least one item when 'itemChance' is above 0");
            return table;
        }

        // Rules spanning files: loot tables name existing items, enemies name existing tables
        void CheckLootRules(const std::vector<LootTableDefinition>& tables, const std::vector<EnemyDefinition>& enemies,
                            const std::vector<WeaponDefinition>& weapons, const std::vector<ArmorDefinition>& armour,
                            const std::vector<ConsumableDefinition>& consumables, std::vector<String>& errors) {
            const auto isItem = [&](const String& key) {
                return std::ranges::find(weapons, key, &WeaponDefinition::key) != weapons.end()
                    || std::ranges::find(armour, key, &ArmorDefinition::key) != armour.end()
                    || std::ranges::find(consumables, key, &ConsumableDefinition::key) != consumables.end();
            };
            for (const auto& table : tables) {
                for (const auto& entry : table.items) {
                    if (!isItem(entry.item)) {
                        errors.push_back(std::format("loot.json: {}: item '{}' is not a weapon, armour piece, or consumable", table.key, entry.item));
                    }
                }
            }
            for (const auto& enemy : enemies) {
                if (!enemy.lootTable.empty() && std::ranges::find(tables, enemy.lootTable, &LootTableDefinition::key) == tables.end()) {
                    errors.push_back(std::format("enemies.json: {}: loot table '{}' is not in loot.json", enemy.key, enemy.lootTable));
                }
            }
        }

        // Rules spanning files: every class defined once, and class spells agree with class schools
        void CheckClassRules(const std::vector<ClassDefinition>& classes, const std::vector<SpellDefinition>& spells,
                             std::vector<String>& errors) {
            if (classes.empty()) return;  // no class data (tools, tests): nothing to check against

            for (const auto& entry : CharacterClasses) {
                const auto count = std::ranges::count(classes, entry.value, &ClassDefinition::characterClass);
                if (count == 0) errors.push_back(std::format("classes.json: class '{}' is not defined", entry.name));
                if (count > 1) errors.push_back(std::format("classes.json: class '{}' is defined {} times", entry.name, count));
            }

            const auto find = [&classes](CharacterClass c) {
                auto it = std::ranges::find(classes, c, &ClassDefinition::characterClass);
                return it != classes.end() ? &*it : nullptr;
            };

            for (const auto& spell : spells) {
                if (!spell.requiredClass) continue;
                const auto* definition = find(*spell.requiredClass);
                if (definition && !definition->HasSpellSchool(spell.school)) {
                    errors.push_back(std::format("spells.json: {}: requires {} but {} has no '{}' spell school",
                        spell.key, NameOf(CharacterClasses, *spell.requiredClass), definition->key, NameOf(SpellSchools, spell.school)));
                }
            }
        }

        // Spells, classes, and enemies name statuses, spells, and items: each must exist, and what a
        // class casts or starts with must suit it
        void CheckReferences(const std::vector<SpellDefinition>& spells, const std::vector<StatusDefinition>& statuses,
                             const std::vector<ClassDefinition>& classes, const std::vector<EnemyDefinition>& enemies,
                             const std::vector<WeaponDefinition>& weapons, const std::vector<ArmorDefinition>& armour,
                             const std::vector<ConsumableDefinition>& consumables, std::vector<String>& errors) {
            const auto findSpell = [&](std::string_view key) {
                const auto it = std::ranges::find(spells, key, &SpellDefinition::key);
                return it != spells.end() ? &*it : nullptr;
            };
            for (const auto& spell : spells) {
                for (const auto& status : spell.appliedStatusIds) {
                    if (std::ranges::find(statuses, status, &StatusDefinition::key) == statuses.end()) {
                        errors.push_back(std::format("spells.json: {}: unknown status '{}'", spell.key, status));
                    }
                }
            }
            for (const auto& definition : classes) {
                for (const auto& key : definition.spellBar) {
                    const auto* spell = findSpell(key);
                    if (!spell) errors.push_back(std::format("classes.json: {}: 'spellBar' names unknown spell '{}'", definition.key, key));
                    else if (!spell->IsCastable()) errors.push_back(std::format("classes.json: {}: '{}' is not a missile or burst the game can cast yet", definition.key, key));
                    else if (!spell->CanBeCastBy(definition.characterClass)) errors.push_back(std::format("classes.json: {}: '{}' is for another class", definition.key, key));
                }
                if (!definition.startingWeapon.empty()) {
                    const auto it = std::ranges::find(weapons, definition.startingWeapon, &WeaponDefinition::key);
                    if (it == weapons.end()) errors.push_back(std::format("classes.json: {}: unknown starting weapon '{}'", definition.key, definition.startingWeapon));
                    else if (!it->IsUsableBy(definition.characterClass)) errors.push_back(std::format("classes.json: {}: cannot wield its starting weapon '{}'", definition.key, definition.startingWeapon));
                }
                for (const auto& entry : definition.startingItems) {
                    const auto colon = entry.find(':');
                    const String key = entry.substr(0, colon);
                    std::int32_t quantity = 1;
                    if (colon != String::npos) {
                        const auto text = std::string_view(entry).substr(colon + 1);
                        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), quantity);
                        if (ec != std::errc{} || end != text.data() + text.size() || quantity < 1) {
                            errors.push_back(std::format("classes.json: {}: starting item '{}' has a bad quantity (\"key:count\")", definition.key, entry));
                        }
                    }
                    const bool known = std::ranges::find(weapons, key, &WeaponDefinition::key) != weapons.end()
                                    || std::ranges::find(armour, key, &ArmorDefinition::key) != armour.end()
                                    || std::ranges::find(consumables, key, &ConsumableDefinition::key) != consumables.end();
                    if (!known) errors.push_back(std::format("classes.json: {}: unknown starting item '{}'", definition.key, key));
                }
            }
            for (const auto& enemy : enemies) {
                auto raised = enemy.summons;
                if (!enemy.summon.empty()) raised.push_back(enemy.summon);
                for (const auto& key : raised) {
                    const auto summoned = std::ranges::find(enemies, key, &EnemyDefinition::key);
                    if (summoned == enemies.end()) errors.push_back(std::format("enemies.json: {}: unknown summon '{}'", enemy.key, key));
                    else if (summoned->IsSummoner()) errors.push_back(std::format("enemies.json: {}: its summon '{}' summons too", enemy.key, key));
                }
                if (!enemy.IsCaster()) continue;
                const auto* spell = findSpell(enemy.spell);
                if (!spell) errors.push_back(std::format("enemies.json: {}: unknown spell '{}'", enemy.key, enemy.spell));
                else if (!spell->IsMissile() || !spell->IsCastable()) errors.push_back(std::format("enemies.json: {}: '{}' is not a missile", enemy.key, enemy.spell));
            }
        }

        // Parse one content file: a JSON array of objects. Assigns ids and checks key uniqueness.
        template<typename T, typename ReadFn>
        std::vector<T> ReadArray(const String& text, const std::string& source, ReadFn read, std::vector<String>& errors) {
            std::vector<T> items;

            const Json json = Json::parse(text, nullptr, /*allow_exceptions*/ false);
            if (json.is_discarded()) {
                errors.push_back(std::format("{}: not valid JSON", source));
                return items;
            }
            if (!json.is_array()) {
                errors.push_back(std::format("{}: expected a JSON array of entries", source));
                return items;
            }

            std::set<String> keys;
            std::size_t index = 0;
            for (const auto& entry : json) {
                ++index;
                const String key = entry.is_object() && entry.contains("id") && entry["id"].is_string() ? entry["id"].get<String>() : String{};
                const std::string context = key.empty()
                    ? std::format("{}: entry {}", source, index)
                    : std::format("{}: entry {} ({})", source, index, key);

                if (!entry.is_object()) {
                    errors.push_back(std::format("{}: must be a JSON object", context));
                    continue;
                }

                EntryReader reader(entry, context, errors);
                T item = read(reader);
                if (!item.key.empty() && !keys.insert(item.key).second) {
                    reader.Error(std::format("duplicate id '{}'", item.key));
                }
                item.id = static_cast<ContentId>(items.size() + 1);
                items.push_back(std::move(item));
            }
            return items;
        }

        std::optional<String> ReadTextFile(const std::filesystem::path& path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) return std::nullopt;
            return String(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }

    ContentLoadResult ContentDatabase::LoadFromDirectory(const String& directory) {
        const std::filesystem::path root(std::u8string(directory.begin(), directory.end()));

        ContentLoadResult result{};
        String texts[14];
        constexpr const char* files[14] = { "spells.json", "weapons.json", "armour.json", "classes.json", "enemies.json", "consumables.json", "loot.json",
                                            "statuses.json", "rarities.json", "affixes.json", "allies.json", "objectives.json", "lore.json", "mutators.json" };
        for (int i = 0; i < 14; ++i) {
            if (auto text = ReadTextFile(root / files[i])) texts[i] = std::move(*text);
            else result.errors.push_back(std::format("{}: file not found in content directory", files[i]));
        }

        if (!result.Succeeded()) {
            Clear();
            return result;
        }
        return LoadFromJson(texts[0], texts[1], texts[2], texts[3], texts[4], texts[5], texts[6], texts[7], texts[8], texts[9], texts[10], texts[11], texts[12], texts[13]);
    }

    ContentLoadResult ContentDatabase::LoadFromJson(const String& spellsJson, const String& weaponsJson, const String& armourJson,
                                                    const String& classesJson, const String& enemiesJson,
                                                    const String& consumablesJson, const String& lootJson, const String& statusesJson,
                                                    const String& raritiesJson, const String& affixesJson, const String& alliesJson,
                                                    const String& objectivesJson, const String& loreJson, const String& mutatorsJson) {
        ContentLoadResult result{};

        auto spells = ReadArray<SpellDefinition>(spellsJson, "spells.json", ReadSpell, result.errors);
        auto weapons = ReadArray<WeaponDefinition>(weaponsJson, "weapons.json", ReadWeapon, result.errors);
        auto armour = ReadArray<ArmorDefinition>(armourJson, "armour.json", ReadArmor, result.errors);
        auto classes = ReadArray<ClassDefinition>(classesJson, "classes.json", ReadClass, result.errors);
        auto enemies = ReadArray<EnemyDefinition>(enemiesJson, "enemies.json", ReadEnemy, result.errors);
        auto consumables = ReadArray<ConsumableDefinition>(consumablesJson, "consumables.json", ReadConsumable, result.errors);
        auto lootTables = ReadArray<LootTableDefinition>(lootJson, "loot.json", ReadLootTable, result.errors);
        auto statuses = ReadArray<StatusDefinition>(statusesJson, "statuses.json", ReadStatus, result.errors);
        auto rarities = ReadArray<RarityDefinition>(raritiesJson, "rarities.json", ReadRarity, result.errors);
        auto affixes = ReadArray<AffixDefinition>(affixesJson, "affixes.json", ReadAffix, result.errors);
        auto allies = ReadArray<AllyDefinition>(alliesJson, "allies.json", ReadAlly, result.errors);
        auto objectives = objectivesJson.empty() ? ObjectiveSettings{} : ReadObjectives(objectivesJson, result.errors);
        auto lore = ReadArray<LoreDefinition>(loreJson, "lore.json", ReadLore, result.errors);
        auto mutators = ReadArray<MutatorDefinition>(mutatorsJson, "mutators.json", ReadMutator, result.errors);
        if (!rarities.empty() && std::ranges::none_of(rarities, [](const RarityDefinition& r) { return r.weight > 0.0f; })) {
            result.errors.push_back("rarities.json: at least one rarity needs a positive 'weight'");
        }
        for (const auto& weapon : weapons) {
            if (weapon.ammoType.empty()) continue;
            if (!consumables.empty() && std::ranges::find(consumables, weapon.ammoType, &ConsumableDefinition::ammoType) == consumables.end()) {
                result.errors.push_back(std::format("weapons.json: {}: no consumable is ammunition of type '{}'", weapon.key, weapon.ammoType));
            }
        }

        CheckClassRules(classes, spells, result.errors);
        CheckReferences(spells, statuses, classes, enemies, weapons, armour, consumables, result.errors);
        CheckLootRules(lootTables, enemies, weapons, armour, consumables, result.errors);

        if (!result.Succeeded()) {
            Clear();
            return result;
        }

        spells_ = std::move(spells);
        weapons_ = std::move(weapons);
        armour_ = std::move(armour);
        classes_ = std::move(classes);
        enemies_ = std::move(enemies);
        consumables_ = std::move(consumables);
        lootTables_ = std::move(lootTables);
        statuses_ = std::move(statuses);
        rarities_ = std::move(rarities);
        affixes_ = std::move(affixes);
        allies_ = std::move(allies);
        objectives_ = std::move(objectives);
        lore_ = std::move(lore);
        mutators_ = std::move(mutators);
        return result;
    }

} // namespace LB::Content
