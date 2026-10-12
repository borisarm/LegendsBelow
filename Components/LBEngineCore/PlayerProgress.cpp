// PlayerProgress.cpp - LB.Engine implementation unit: experience, loot, the bag, equipment, the
// character screen, and saving the player's progress (GameEngine members declared in
// GameEngine.ixx)
module LB.Engine;

import std;
import LB.Core;
import LB.Character;
import LB.CharacterService;
import LB.Combat;
import LB.Content;
import LB.Inventory;
import LB.InventoryService;
import LB.InputSystem;
import LB.PersistenceSystem;
import LB.RenderingSystem;

namespace LB::Engine {

    namespace {
        using LB::Combat::EquipmentSlot;
        using Systems::Rendering::Color;

        // Display (sRGB) color to the renderer's linear color
        Color Display(float r, float g, float b, float a = 1.0f) {
            return { std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), a };
        }

        constexpr std::array<std::pair<LB::AttributeType, std::string_view>, 6> AttributeNames = { {
            { LB::AttributeType::Strength, "STR" }, { LB::AttributeType::Dexterity, "DEX" },
            { LB::AttributeType::Constitution, "CON" }, { LB::AttributeType::Intelligence, "INT" },
            { LB::AttributeType::Wisdom, "WIS" }, { LB::AttributeType::Charisma, "CHA" } } };

        std::string_view SlotName(EquipmentSlot slot) {
            switch (slot) {
            case EquipmentSlot::Head: return "Head";
            case EquipmentSlot::Chest: return "Chest";
            case EquipmentSlot::Hands: return "Hands";
            case EquipmentSlot::Legs: return "Legs";
            case EquipmentSlot::Feet: return "Feet";
            case EquipmentSlot::MainHand: return "Main hand";
            case EquipmentSlot::OffHand: return "Off hand";
            default: return "Other";
            }
        }

        std::string DiceText(const LB::Dice& dice) {
            return dice.bonus > 0 ? std::format("{}d{}+{}", dice.count, dice.sides, dice.bonus) : std::format("{}d{}", dice.count, dice.sides);
        }

        // Split "a;b;c" (empty parts skipped)
        std::vector<std::string> Split(const std::string& text, char separator) {
            std::vector<std::string> parts;
            std::string part;
            for (const char c : text + separator) {
                if (c != separator) { part += c; continue; }
                if (!part.empty()) parts.push_back(part);
                part.clear();
            }
            return parts;
        }

        bool ParseInt(std::string_view text, std::int32_t& value) {
            const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
            return ec == std::errc{} && end == text.data() + text.size();
        }
    }

    // ------------------------------------------------------------------------------------------
    // The bag
    // ------------------------------------------------------------------------------------------

    std::int32_t GameEngine::CountItem(std::string_view key) const {
        const auto item = ItemRef::FromKey(*content_, key);
        if (!item.IsValid()) return 0;
        std::int32_t count = 0;
        for (const auto& stack : GetInventory()) {
            if (stack.itemId == item.Pack()) count += stack.quantity;
        }
        return count;
    }

    std::int32_t GameEngine::GiveItem(std::string_view key, std::int32_t quantity) {
        return GiveItemRef(ItemRef::FromKey(*content_, key), quantity);
    }

    std::int32_t GameEngine::GiveItemRef(ItemRef item, std::int32_t quantity) {
        if (!session_->playerId || !item.IsValid() || quantity <= 0) return 0;
        const auto base = session_->rolls.BaseOf(item);
        if (!base.IsValid() || base.kind == ItemKind::Rolled) return 0;

        Inventory::InventoryItem stack{};
        stack.itemId = item.Pack();
        stack.name = ItemName(item);
        stack.quantity = quantity;
        stack.isEquippable = base.kind != ItemKind::Consumable;
        stack.isConsumable = base.kind == ItemKind::Consumable;
        if (base.kind == ItemKind::Consumable) stack.maxStack = content_->FindConsumable(base.id)->maxStack;
        if (base.kind == ItemKind::Weapon) stack.equipmentSlot = content_->FindWeapon(base.id)->slot;
        if (base.kind == ItemKind::Armor) stack.equipmentSlot = content_->FindArmor(base.id)->slot;
        if (item.kind == ItemKind::Rolled) stack.maxStack = 1;   // each rolled item is its own
        return session_->inventoryService.AddItem(*session_->playerId, stack).itemsAffected;
    }

    std::string GameEngine::ItemName(ItemRef item) const {
        if (const auto* rolled = session_->rolls.Find(item)) return rolled->name;
        return DescribeItem(*content_, item).first;
    }

    // What the equipped items and the chosen path add
    ItemBonuses GameEngine::EquippedBonuses() const {
        auto bonuses = session_->rolls.BonusesOf(GetPlayerWeaponItem());
        for (const auto& [slot, item] : WornItems()) bonuses += session_->rolls.BonusesOf(item);
        if (const auto* path = GetPlayerPath()) {
            bonuses.damage += path->damage;
            bonuses.attackBonus += path->attackBonus;
            bonuses.armorClass += path->armorClass;
            bonuses.armor += path->armor;
            bonuses.spellPower += path->spellPower;
            bonuses.manaRegen += path->manaRegen;
        }
        return bonuses;
    }

    const Content::SpecializationDefinition* GameEngine::GetPlayerPath() const {
        const auto* classData = content_->FindClass(session_->playerClass);
        return classData && !session_->path.empty() ? classData->FindSpecialization(session_->path) : nullptr;
    }

    bool GameEngine::ChoosePath(std::string_view key) {
        const auto* classData = content_->FindClass(session_->playerClass);
        const auto* player = PlayerCharacter();
        if (!classData || !player || !session_->path.empty() || player->GetLevel() < Content::ClassDefinition::SpecializationLevel) return false;
        const auto* path = classData->FindSpecialization(key);
        if (!path) return false;
        session_->path = path->key;
        session_->events.Publish(PathChosenEvent{ path->key, path->name });
        return true;
    }

    std::optional<Systems::Rendering::Color> GameEngine::RarityColorOf(ItemRef item) const {
        const auto* rolled = session_->rolls.Find(item);
        if (!rolled || !rolled->rarity) return std::nullopt;
        return Display(rolled->rarity->color[0], rolled->rarity->color[1], rolled->rarity->color[2]);
    }

    // A weapon or armour piece dropped: its rarity at this depth (the wave in the arena) and, above
    // common, its affixes (design-loot-and-progression.md 3-4)
    ItemRef GameEngine::RollDrop(ItemRef item, std::int32_t depthBonus) {
        if (item.kind != ItemKind::Weapon && item.kind != ItemKind::Armor) return item;
        const auto depth = (session_->depth > 0 ? session_->depth : std::max(1, session_->encounter.GetWave())) + depthBonus;
        const auto* rarity = session_->lootRoller.RollRarity(*content_, depth);
        if (!rarity || rarity->affixes <= 0) return item;
        const bool weapon = item.kind == ItemKind::Weapon;
        auto affixes = session_->lootRoller.RollAffixes(*content_, weapon, rarity->affixes);
        if (rarity->legendary) {
            if (const auto* power = session_->lootRoller.RollPower(*content_, weapon)) affixes.push_back(power);
        }
        if (affixes.empty()) return item;
        // Its level: the depth it dropped at (a greater chest's deeper still)
        return session_->rolls.Add(RolledItem{ item, rarity, std::move(affixes), {}, depth }, *content_);
    }

    // ------------------------------------------------------------------------------------------
    // Ammunition: bows shoot arrows, crossbows bolts (weapons.json ammoType)
    // ------------------------------------------------------------------------------------------

    std::int32_t GameEngine::AmmoCount(std::string_view ammoType) const {
        std::int32_t count = 0;
        for (const auto& stack : GetInventory()) {
            const auto item = ItemRef::Unpack(stack.itemId);
            if (item.kind != ItemKind::Consumable) continue;
            const auto* consumable = content_->FindConsumable(item.id);
            if (consumable && consumable->ammoType == ammoType) count += stack.quantity;
        }
        return count;
    }

    bool GameEngine::SpendAmmo(std::string_view ammoType) {
        for (const auto& stack : GetInventory()) {
            const auto item = ItemRef::Unpack(stack.itemId);
            if (item.kind != ItemKind::Consumable) continue;
            const auto* consumable = content_->FindConsumable(item.id);
            if (consumable && consumable->ammoType == ammoType) return session_->inventoryService.RemoveItem(*session_->playerId, stack.itemId, 1).success;
        }
        return false;
    }

    void GameEngine::GiveStartingKit() {
        session_->gold = 0;
        session_->groundItems.clear();
        // The class's kit (classes.json startingWeapon, startingItems); a potion for everyone
        const auto* classData = content_->FindClass(session_->playerClass);
        session_->playerWeapon = nullptr;
        if (!session_->playerId) return;
        session_->inventoryService.CreateInventory(*session_->playerId, BagCapacity);   // empty slots too
        if (const auto* weapon = classData && !classData->startingWeapon.empty() ? content_->FindWeapon(classData->startingWeapon) : nullptr) {
            PutOn(ItemRef{ ItemKind::Weapon, weapon->id }, LB::Combat::EquipmentSlot::MainHand);
        }
        GiveItem(HealingPotion, StartingPotions);
        if (classData) {
            // "key" or "key:count"
            for (const auto& entry : classData->startingItems) {
                const auto colon = entry.find(':');
                std::int32_t quantity = 1;
                if (colon != std::string::npos && !ParseInt(std::string_view(entry).substr(colon + 1), quantity)) quantity = 1;
                GiveItem(entry.substr(0, colon), quantity);
            }
        }
    }

    // Why the player cannot equip an item, or empty when they can
    std::string GameEngine::CanEquip(ItemRef rolledOrBase) const {
        const auto item = session_->rolls.BaseOf(rolledOrBase);
        const auto* player = PlayerCharacter();
        if (!player) return "No character";
        const auto& character = *player;
        std::int32_t requiredLevel = 1;
        bool allowed = false;
        if (item.kind == ItemKind::Weapon) {
            const auto* weapon = content_->FindWeapon(item.id);
            requiredLevel = weapon->requiredLevel;
            allowed = weapon->IsUsableBy(character.GetClass());
        }
        else if (item.kind == ItemKind::Armor) {
            const auto* armor = content_->FindArmor(item.id);
            requiredLevel = armor->requiredLevel;
            allowed = armor->IsUsableBy(character.GetClass());
        }
        else {
            return "cannot be worn";
        }
        if (!allowed) return "not for your class";
        if (character.GetLevel() < requiredLevel) return std::format("needs level {}", requiredLevel);
        return {};
    }

    bool GameEngine::EquipItem(std::string_view key) {
        return EquipItemRef(ItemRef::FromKey(*content_, key));
    }

    bool GameEngine::EquipItemRef(ItemRef item) {
        if (!session_->playerId || !item.IsValid() || !CanEquip(item).empty()) return false;
        const auto inBag = std::ranges::any_of(GetInventory(), [&](const Inventory::InventoryItem& stack) { return stack.itemId == item.Pack(); });
        if (!inBag) return false;
        const auto base = session_->rolls.BaseOf(item);

        // The item in the same slot goes back to the bag (there is room: one item just left it)
        session_->inventoryService.RemoveItem(*session_->playerId, item.Pack(), 1);
        const auto slot = base.kind == ItemKind::Weapon ? LB::Combat::EquipmentSlot::MainHand : content_->FindArmor(base.id)->slot;
        if (const auto previous = EquippedItem(slot); previous.IsValid()) GiveItemRef(previous);
        return PutOn(item, slot);
    }

    ItemRef GameEngine::EquippedItem(LB::Combat::EquipmentSlot slot) const {
        if (!session_->playerId) return {};
        const auto equipped = session_->inventoryService.GetEquippedItems(*session_->playerId);
        const auto piece = equipped ? equipped->GetEquipment(slot) : std::nullopt;
        return piece ? ItemRef::Unpack(piece->itemId) : ItemRef{};
    }

    std::vector<std::pair<LB::Combat::EquipmentSlot, ItemRef>> GameEngine::WornItems() const {
        std::vector<std::pair<LB::Combat::EquipmentSlot, ItemRef>> worn;
        if (!session_->playerId) return worn;
        if (const auto equipped = session_->inventoryService.GetEquippedItems(*session_->playerId)) {
            for (const auto& [slot, piece] : equipped->equipment) {
                if (slot != LB::Combat::EquipmentSlot::MainHand) worn.emplace_back(slot, ItemRef::Unpack(piece.itemId));
            }
        }
        std::ranges::sort(worn, {}, [](const auto& entry) { return static_cast<std::uint32_t>(entry.first); });
        return worn;
    }

    bool GameEngine::PutOn(ItemRef item, LB::Combat::EquipmentSlot slot) {
        if (!session_->playerId || !item.IsValid()) return false;
        Inventory::Equipment piece{};
        piece.itemId = item.Pack();
        piece.name = ItemName(item);
        piece.slot = slot;
        if (!session_->inventoryService.EquipItem(*session_->playerId, piece)) return false;
        SyncEquipment();
        return true;
    }

    bool GameEngine::TakeOff(LB::Combat::EquipmentSlot slot) {
        if (!session_->playerId || !session_->inventoryService.UnequipItem(*session_->playerId, slot)) return false;
        SyncEquipment();
        return true;
    }

    // The weapon in hand, the armour pieces CharacterService works out speed and casting from, and
    // what the gear adds to the character (attributes, hit points, mana)
    void GameEngine::SyncEquipment() {
        const auto weapon = session_->rolls.BaseOf(GetPlayerWeaponItem());
        session_->playerWeapon = weapon.kind == ItemKind::Weapon ? content_->FindWeapon(weapon.id) : nullptr;
        if (!session_->playerId) return;
        const auto id = *session_->playerId;
        for (const auto& worn : session_->characterService.GetEquippedArmor(id)) session_->characterService.UnequipArmor(id, worn.slot);
        for (const auto& [slot, item] : WornItems()) {
            const auto base = session_->rolls.BaseOf(item);
            const auto* armor = base.kind == ItemKind::Armor ? content_->FindArmor(base.id) : nullptr;
            if (!armor) continue;
            Services::Character::ArmorPiece piece{};
            piece.armorId = armor->id;
            piece.slot = slot;
            piece.category = armor->category;
            piece.movementPenalty = armor->movementPenalty;
            piece.castPenalty = armor->castPenalty;
            session_->characterService.EquipArmor(id, piece);
        }
        auto bonuses = session_->rolls.BonusesOf(GetPlayerWeaponItem());
        for (const auto& [slot, item] : WornItems()) bonuses += session_->rolls.BonusesOf(item);
        Services::Character::GearBonus gear{};
        gear.attributes = LB::Attributes{ bonuses.strength, bonuses.dexterity, 0, bonuses.intelligence, bonuses.wisdom, 0 };
        gear.hitPoints = bonuses.hitPoints;
        gear.mana = bonuses.mana;
        session_->characterService.SetGear(id, gear);
    }

    bool GameEngine::UnequipArmorSlot(EquipmentSlot slot) {
        if (!session_->playerId || slot == LB::Combat::EquipmentSlot::MainHand || !session_->inventoryService.HasInventorySpace(*session_->playerId)) return false;
        const auto item = EquippedItem(slot);
        if (!item.IsValid() || !TakeOff(slot)) return false;
        GiveItemRef(item);
        return true;
    }

    bool GameEngine::UseItem(std::string_view key) {
        const auto item = ItemRef::FromKey(*content_, key);
        if (!session_->playerId || item.kind != ItemKind::Consumable || CountItem(key) == 0 || !IsPlayerAlive()) return false;

        const auto* consumable = content_->FindConsumable(item.id);
        const auto* player = PlayerCharacter();
        if (!consumable || !player || consumable->isKey) return false;
        const auto& character = *player;
        if (consumable->healing.count > 0 && character.GetCurrentHitPoints() >= character.GetMaxHitPoints()) {
            return false;   // no waste at full health
        }

        session_->inventoryService.RemoveItem(*session_->playerId, item.Pack(), 1);
        if (consumable->healing.count > 0) {
            const auto healed = session_->characterService.HealCharacter(*session_->playerId, session_->lootRoller.RollDice(consumable->healing));
            session_->combatText.Add(std::format("+{}", healed), { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.45f, 0.95f, 0.45f, 1.0f }, 3.0f);
            EmitHealingEffect();
            PlaySound(GameSound::Potion, session_->playerPosition);
        }
        return true;
    }

    void GameEngine::UpdateQuickUse() {
        if (inputSystem_.IsActionTriggered(std::string(UsePotionAction))) {
            UseItem(HealingPotion);
        }
    }

    // ------------------------------------------------------------------------------------------
    // Experience and loot
    // ------------------------------------------------------------------------------------------

    // A kill is published; progression (experience, loot) handles it when the step's events go out
    void GameEngine::OnEnemyKilled(const Enemy& enemy) {
        if (!session_->playerId) return;
        session_->events.Publish(EnemyKilledEvent{ enemy.id, enemy.definition->key, enemy.position, enemy.experience, enemy.definition->lootTable });
    }

    void GameEngine::GrantExperience(std::int32_t experience) {
        if (!session_->playerId || experience <= 0) return;
        if (legacy_.Has(Boon::Veteran)) experience = static_cast<std::int32_t>(std::lround(static_cast<float>(experience) * Legacy::VeteranExperience));
        const auto previous = PlayerCharacter()->GetLevel();
        if (const auto progress = session_->characterService.AwardExperience(*session_->playerId, experience); progress && progress->levelIncreased) {
            session_->events.Publish(LevelGainedEvent{ previous, progress->newLevel, progress->hitPointIncrease, progress->manaPointIncrease,
                                                       progress->attributePointsGained });
        }
    }

    // Who reacts to what happens in a run: progression to kills; the fanfare (texts, effects,
    // sounds) to levels and paths. Subscribed once per run, on the run's own bus.
    void GameEngine::SubscribeToEvents() {
        auto& events = session_->events;
        events.Subscribe<EnemyKilledEvent>([this](const EnemyKilledEvent& kill) {
            ++session_->stats.kills;
            if (!session_->playing) return;
            ++legacy_.kills;   // kept with the next autosave or record
            const auto* enemy = content_->FindEnemy(kill.enemyKey);
            if (enemy && enemy->boss) {
                auto next = legacy_;
                ++next.bosses;
                UpdateLegacy(next);
            }
        });
        events.Subscribe<EnemyKilledEvent>([this](const EnemyKilledEvent& kill) {
            GrantExperience(kill.experience);
            if (!kill.lootTable.empty()) DropLoot(kill.lootTable, kill.at);
        });
        events.Subscribe<LevelGainedEvent>([this](const LevelGainedEvent& gained) { session_->allies.SetLevel(gained.level); });
        // A level: a banner with what was gained (health, mana or stamina, points, abilities, a path)
        events.Subscribe<LevelGainedEvent>([this](const LevelGainedEvent& gained) {
            const auto* classData = PlayerClassDefinition();
            const bool stamina = classData && classData->resource == "Stamina";
            FanfareCard card{ std::format("LEVEL {}", gained.level), classData ? classData->name : std::string{}, {}, { 0.98f, 0.78f, 0.30f } };
            if (gained.hitPoints > 0) card.lines.push_back(std::format("+{} hit points", gained.hitPoints));
            if (gained.mana > 0) card.lines.push_back(std::format("+{} {}", gained.mana, stamina ? "stamina" : "mana"));
            if (gained.attributePoints > 0) card.lines.push_back(std::format("+{} attribute point{} to spend (I)", gained.attributePoints, gained.attributePoints == 1 ? "" : "s"));
            for (const auto* spell : GetSpellBar()) {
                if (spell->requiredLevel > gained.previous && spell->requiredLevel <= gained.level) card.lines.push_back(std::format("New: {}", spell->name));
            }
            const auto paths = Content::ClassDefinition::SpecializationLevel;
            if (gained.previous < paths && gained.level >= paths && session_->path.empty()) card.lines.push_back("A path to choose (I)");
            session_->fanfare.Show(std::move(card));
            EmitLevelUpEffect();
            PlaySound(GameSound::LevelUp, session_->playerPosition);
        });
        // A path: its banner in its color, and a second burst of light
        events.Subscribe<PathChosenEvent>([this](const PathChosenEvent& chosen) {
            const auto* classData = PlayerClassDefinition();
            const auto* path = classData ? classData->FindSpecialization(chosen.key) : nullptr;
            FanfareCard card{ std::format("PATH OF THE {}", [&] { auto upper = chosen.name; std::ranges::transform(upper, upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); }); return upper; }()), classData ? classData->name : std::string{}, {}, { 0.98f, 0.78f, 0.30f } };
            if (path) {
                card.lines = PathBonuses(*path);
                card.color = path->color;
                if (!path->flavor.empty()) card.lines.insert(card.lines.begin(), path->flavor);
            }
            session_->fanfare.Show(std::move(card));
            EmitLevelUpEffect();
            EmitLevelUpEffect();
            PlaySound(GameSound::LevelUp, session_->playerPosition);
        });
    }

    void GameEngine::DropLoot(std::string_view lootTable, WorldPoint at, std::int32_t rolls, std::int32_t depthBonus) {
        const auto* table = content_->FindLootTable(lootTable);
        if (!table) return;
        std::vector<GroundItem> drops;
        for (std::int32_t roll = 0; roll < std::max(1, rolls); ++roll) {
            for (auto drop : session_->lootRoller.Roll(*table, *content_)) drops.push_back(drop);
        }
        for (auto drop : drops) {
            drop.item = RollDrop(drop.item, depthBonus);
            const auto offset = session_->lootRoller.Scatter(0.45f);
            drop.id = session_->nextGroundItemId++;
            const WorldPoint spot{ at.x + offset.x, at.y + offset.y };
            drop.position = session_->world.CircleFits(spot, 0.2f) ? spot : at;
            session_->groundItems.push_back(drop);
        }
    }

    // Walking over gold or an item picks it up; an item that does not fit stays (warned once)
    void GameEngine::UpdateGroundItems(float elapsedSeconds) {
        if (!session_->playerId) return;
        const bool alive = IsPlayerAlive();
        std::erase_if(session_->groundItems, [&](GroundItem& ground) {
            ground.ageSeconds += elapsedSeconds;
            const bool near = Encounter::Distance(ground.position, session_->playerPosition) <= PickupRadius;
            if (ground.dropped) {
                if (!near) ground.dropped = false;   // walked away: it can be picked up again
                return false;
            }
            if (!alive || !near) return false;

            const Systems::Rendering::Vector3 at{ ground.position.x, 0.6f, ground.position.y };
            if (ground.IsGold()) {
                session_->gold += ground.quantity;
                session_->stats.goldFound += ground.quantity;
                session_->combatText.Add(std::format("+{} GOLD", ground.quantity), at, { 0.95f, 0.80f, 0.35f, 1.0f }, 2.0f);
                PlaySound(GameSound::Gold, ground.position);
                return true;
            }

            const auto name = ground.quantity > 1 ? std::format("{} x{}", ItemName(ground.item), ground.quantity) : ItemName(ground.item);
            if (GiveItemRef(ground.item, ground.quantity) == ground.quantity) {
                session_->combatText.Add(name, at, RarityColorOf(ground.item).value_or(Color{ 1.0f, 1.0f, 1.0f, 1.0f }), 2.0f);
                PlaySound(GameSound::ItemPickup, ground.position);
                return true;
            }
            if (!ground.fullWarningShown) {
                ground.fullWarningShown = true;
                session_->combatText.Add("BAG FULL", at, { 0.95f, 0.45f, 0.40f, 1.0f }, 2.0f);
                PlaySound(GameSound::BagFull, ground.position);
            }
            return false;
        });
    }

    // ------------------------------------------------------------------------------------------
    // Character screen
    // ------------------------------------------------------------------------------------------

    std::vector<ScreenRow> GameEngine::BuildScreenRows() const {
        std::vector<ScreenRow> rows;
        const auto playerStatus = session_->playerId ? session_->characterService.GetCharacterStatus(*session_->playerId) : std::nullopt;
        const auto* player = PlayerCharacter();
        if (!playerStatus || !player) return rows;
        const auto& status = *playerStatus;
        const auto& character = *player;
        const auto heading = [&](std::string text) { rows.push_back(ScreenRow{ std::move(text), {}, ScreenAction::None, true, true }); };
        const auto info = [&](std::string text, std::string detail = {}) { rows.push_back(ScreenRow{ std::move(text), std::move(detail) }); };

        const auto* classDefinition = content_->FindClass(status.characterClass);
        heading(std::format("{} - level {} {}", status.name, status.level, classDefinition ? classDefinition->name : std::string{}));
        const bool usesMana = classDefinition && classDefinition->resource == "Mana";
        info(std::format("Experience {} / {}", status.currentExperience, status.experienceToNextLevel),
             usesMana ? std::format("HP {}/{}   MP {}/{}   Gold {}", status.currentHitPoints, status.maxHitPoints, status.currentManaPoints, status.maxManaPoints, session_->gold)
                      : std::format("HP {}/{}   Gold {}", status.currentHitPoints, status.maxHitPoints, session_->gold));

        // Attributes: with points to spend, each one below its cap can be raised
        const auto points = character.GetUnspentAttributePoints();
        heading(points > 0 ? std::format("Attributes - {} point{} to spend", points, points == 1 ? "" : "s") : std::string("Attributes"));
        const auto effective = session_->characterService.GetEffectiveAttributes(*session_->playerId).value_or(status.currentAttributes);
        for (const auto& [attribute, name] : AttributeNames) {
            // The value with what the gear adds (its modifier is what counts), the gear's share shown
            const auto base = status.currentAttributes.Get(attribute);
            const auto value = effective.Get(attribute);
            const auto modifier = static_cast<std::int32_t>(std::floor((value - 10) / 2.0));
            ScreenRow row{ value != base ? std::format("{} {} ({:+} gear)", name, value, value - base) : std::format("{} {}", name, value),
                           std::format("{}{}", modifier >= 0 ? "+" : "", modifier) };
            if (points > 0) {
                row.action = ScreenAction::SpendPoint;
                row.attribute = attribute;
            }
            rows.push_back(std::move(row));
        }

        // The path: chosen, or to choose from SpecializationLevel
        if (classDefinition && !classDefinition->specializations.empty()) {
            if (const auto* path = GetPlayerPath()) {
                heading(std::format("Path - {}", path->name));
                info(path->description);
            }
            else if (status.level >= Content::ClassDefinition::SpecializationLevel) {
                heading("Path");
                std::string names;
                for (const auto& option : classDefinition->specializations) names += (names.empty() ? "" : " or ") + option.name;
                ScreenRow row{ "Choose your path...", names, ScreenAction::ChoosePath };
                row.color = Systems::Rendering::Color{ 0.55f, 0.95f, 0.55f, 1.0f };
                rows.push_back(std::move(row));
            }
        }

        // Spells on the bar: their keys and costs, or the level they open at
        const auto bar = GetSpellBar();
        if (!bar.empty()) {
            heading(std::format("Spells - spell power {}", GetPlayerSpellPower()));
            for (std::size_t slot = 0; slot < bar.size(); ++slot) {
                const auto& spell = *bar[slot];
                const auto power = GetPlayerSpellPower();
                const auto effect = spell.IsHeal() ? std::format("heals {}", static_cast<std::int32_t>(std::lround(static_cast<float>(spell.baseHeal) + spell.powerCoefficient * static_cast<float>(power))))
                                  : spell.IsDash() ? std::string("dodge")
                                  : spell.IsShadowstep() ? std::string("step behind")
                                  : spell.baseDamage > 0 ? std::format("{} dmg", Encounter::SpellDamage(spell, power)) : std::string("hinders");
                info(std::format("{}: {}", slot + 1, spell.name),
                     status.level < spell.requiredLevel ? std::format("from level {}", spell.requiredLevel) : std::format("{} MP   {}", spell.manaCost, effect));
            }
        }

        // Equipment: the weapon, then each armour slot
        heading("Equipped");
        if (session_->playerWeapon) {
            ScreenRow row{ std::format("Main hand: {}", ItemName(GetPlayerWeaponItem())), std::format("{} +{}", DiceText(session_->playerWeapon->damageDice), GetPlayerAttackProfile().damageBonus) };
            row.color = RarityColorOf(GetPlayerWeaponItem());
            row.item = GetPlayerWeaponItem();
            rows.push_back(std::move(row));
            if (const auto* rolled = session_->rolls.Find(GetPlayerWeaponItem()); rolled && rolled->Power()) info("  " + rolled->Power()->description);
        }
        for (const auto& [slot, item] : WornItems()) {
            const auto* armor = content_->FindArmor(session_->rolls.BaseOf(item).id);
            if (!armor) continue;
            ScreenRow row{ std::format("{}: {}", SlotName(slot), ItemName(item)), std::format("AR {}", armor->physicalAR + armor->armorBonus + session_->rolls.BonusesOf(item).armor) };
            row.color = RarityColorOf(item);
            row.action = ScreenAction::Unequip;
            row.slot = slot;
            row.item = item;
            rows.push_back(std::move(row));
            if (const auto* rolled = session_->rolls.Find(item); rolled && rolled->Power()) info("  " + rolled->Power()->description);
        }
        info("Defense", std::format("AC {}   AR {}", GetPlayerDefense().armorClass, GetPlayerDefense().physicalArmor));

        // The companions and their gear: choose one to give them an item from the bag
        if (!session_->allies.Allies().empty()) {
            heading(session_->givingTo ? std::string("Allies - choose a weapon or armour below for them (Esc: cancel)") : std::string("Allies"));
            for (const auto& ally : session_->allies.Allies()) {
                const auto [weapon, armour] = AllyItems(ally.id);
                const auto carried = std::format("{} / {}", weapon.IsValid() ? ItemName(weapon) : std::string("-"), armour.IsValid() ? ItemName(armour) : std::string("-"));   // weapon / armour
                ScreenRow row{ std::format("{} (HP {}/{})", ally.definition->name, ally.hitPoints, ally.maxHitPoints), carried, ScreenAction::GiveToAlly };
                row.index = ally.id;
                row.color = session_->givingTo == ally.id ? std::optional{ Systems::Rendering::Color{ 0.55f, 0.95f, 0.55f, 1.0f } } : std::nullopt;
                rows.push_back(std::move(row));
                if (weapon.IsValid() || armour.IsValid()) {
                    ScreenRow back{ std::format("  take back {}'s gear", ally.definition->name), {}, ScreenAction::TakeFromAlly };
                    back.index = ally.id;
                    rows.push_back(std::move(back));
                }
            }
        }

        // The bag
        const auto bag = GetInventory();
        heading(std::format("Bag ({}/{})", bag.size(), BagCapacity));
        if (bag.empty()) info("Empty");
        // A weapon's average blow and an armour piece's armour, against what is equipped there
        const auto averageBlow = [this](ItemRef item) {
            const auto* weapon = content_->FindWeapon(session_->rolls.BaseOf(item).id);
            if (!weapon) return 0.0f;
            const auto& dice = weapon->damageDice;
            return static_cast<float>(dice.count) * (static_cast<float>(dice.sides) + 1.0f) / 2.0f + static_cast<float>(dice.bonus + weapon->damageBonus + session_->rolls.BonusesOf(item).damage);
        };
        const auto armourOf = [this](ItemRef item) {
            const auto* armor = content_->FindArmor(session_->rolls.BaseOf(item).id);
            const auto bonuses = session_->rolls.BonusesOf(item);
            return armor ? armor->physicalAR + armor->armorBonus + bonuses.armor + bonuses.armorClass : 0;
        };
        for (const auto& stack : bag) {
            const auto item = ItemRef::Unpack(stack.itemId);
            const auto base = session_->rolls.BaseOf(item);
            ScreenRow row{ stack.quantity > 1 ? std::format("{} x{}", stack.name, stack.quantity) : stack.name };
            row.item = item;
            row.color = RarityColorOf(item);
            if (base.kind == ItemKind::Consumable) {
                const auto* consumable = content_->FindConsumable(base.id);
                if (consumable->IsAmmunition()) {
                    row.detail = "ammunition";
                }
                else if (consumable->isKey) {
                    row.detail = "opens a locked gate";
                }
                else {
                    row.action = ScreenAction::Use;
                    row.detail = consumable->healing.count > 0 ? std::format("heals {}", DiceText(consumable->healing)) : std::string("use");
                }
            }
            else {
                row.action = ScreenAction::Equip;
                const auto reason = CanEquip(item);
                row.enabled = reason.empty();
                const auto* rolled = session_->rolls.Find(item);
                const auto level = rolled ? std::format("L{} ", rolled->level) : std::string{};
                if (base.kind == ItemKind::Weapon) {
                    const auto* weapon = content_->FindWeapon(base.id);
                    const auto delta = averageBlow(item) - (GetPlayerWeaponItem().IsValid() ? averageBlow(GetPlayerWeaponItem()) : 0.0f);
                    const auto bonus = weapon->damageBonus + session_->rolls.BonusesOf(item).damage;
                    row.detail = reason.empty() ? std::format("{}{}{} {:+.1f}", level, DiceText(weapon->damageDice), bonus > 0 ? std::format("+{}", bonus) : std::string{}, delta) : reason;
                }
                else {
                    const auto* armor = content_->FindArmor(base.id);
                    const auto worn = EquippedItem(armor->slot);
                    const std::int32_t current = worn.IsValid() ? armourOf(worn) : 0;
                    const auto mine = armourOf(item);
                    row.detail = reason.empty() ? std::format("{}{} {} {:+d}", level, SlotName(armor->slot), mine, mine - current) : reason;
                }
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }

    // Open/close, move the cursor, and act on the chosen row
    void GameEngine::UpdateCharacterScreen() {
        const auto triggered = [this](std::string_view action) { return inputSystem_.IsActionTriggered(std::string(action)); };
        if (!session_->playerId) return;

        if (session_->trade.IsOpen()) return;
        if (session_->pathChoice.IsOpen()) {
            UpdatePathChoice();
            return;
        }
        if (!session_->characterScreen.IsOpen()) {
            if (triggered(CharacterScreenAction)) {
                session_->characterScreen.Open();
                session_->characterScreen.SetRows(BuildScreenRows());
                PlaySound(GameSound::ScreenOpen);
            }
            return;
        }

        if (session_->givingTo && triggered(MenuBackAction)) {   // Esc cancels choosing an ally's item first
            session_->givingTo.reset();
            session_->characterScreen.SetRows(BuildScreenRows());
            return;
        }
        if (triggered(CharacterScreenAction) || triggered(MenuBackAction)) {
            session_->characterScreen.Close();
            session_->givingTo.reset();
            PlaySound(GameSound::ScreenClose);
            return;
        }

        session_->characterScreen.SetRows(BuildScreenRows());
        // Drop: the bag item under the cursor goes to the floor
        if (triggered(DropAction)) {
            const auto* row = session_->characterScreen.Selected();
            if (row && row->item.IsValid() && row->action != ScreenAction::Unequip && DropItem(row->item)) {
                session_->characterScreen.SetRows(BuildScreenRows());
            }
            return;
        }
        if (triggered(MenuUpAction)) {
            session_->characterScreen.Step(-1);
            PlaySound(GameSound::MenuMove);
        }
        if (triggered(MenuDownAction)) {
            session_->characterScreen.Step(+1);
            PlaySound(GameSound::MenuMove);
        }
        if (!triggered(MenuConfirmAction)) return;

        const auto* row = session_->characterScreen.Selected();
        if (!row || !row->enabled) return;
        const auto key = DescribeItem(*content_, session_->rolls.BaseOf(row->item)).second;
        if (row->action == ScreenAction::Equip || row->action == ScreenAction::Unequip) PlaySound(GameSound::Equip);
        else if (row->action != ScreenAction::None && row->action != ScreenAction::Use) PlaySound(GameSound::MenuConfirm);
        switch (row->action) {
        case ScreenAction::SpendPoint: session_->characterService.SpendAttributePoint(*session_->playerId, row->attribute); break;
        case ScreenAction::Equip:
            if (session_->givingTo) {
                GiveToAlly(*session_->givingTo, row->item);
                session_->givingTo.reset();
            }
            else {
                EquipItemRef(row->item);
            }
            break;
        case ScreenAction::GiveToAlly: session_->givingTo = static_cast<std::uint32_t>(row->index); break;
        case ScreenAction::TakeFromAlly: TakeFromAlly(static_cast<std::uint32_t>(row->index)); break;
        case ScreenAction::Use: UseItem(key); break;
        case ScreenAction::Unequip: UnequipArmorSlot(row->slot); break;
        case ScreenAction::ChoosePath:
            session_->pathChoice.Open();
            break;
        case ScreenAction::Buy:
        case ScreenAction::Sell: break;
        case ScreenAction::None: break;
        }
        session_->characterScreen.SetRows(BuildScreenRows());
    }

    void GameEngine::DrawCharacterScreen() {
        if (session_->pathChoice.IsOpen()) {
            const auto cards = PathCards();
            PathChoice::Draw(renderingSystem_, assets_.menuArt, cards, session_->pathChoice.Cursor(), "CHOOSE YOUR PATH", static_cast<float>(timer_.GetTotalSeconds()));
            return;   // the cards take the screen
        }
        if (session_->trade.IsOpen()) {
            DrawRowScreen(session_->trade, "MERCHANT");
            return;
        }
        if (!session_->characterScreen.IsOpen()) return;
        DrawRowScreen(session_->characterScreen, "CHARACTER");
    }

    // A screen of rows with a cursor (the character screen, the merchant's): a framed panel
    void GameEngine::DrawRowScreen(const CharacterScreen& screen, std::string_view title) {

        std::int32_t width = 0;
        std::int32_t height = 0;
        renderingSystem_.GetUiSize(width, height);
        const std::int32_t panelWidth = std::min(width - 40, 620);
        const std::int32_t left = (width - panelWidth) / 2;
        const std::int32_t top = 30;
        const auto& rows = screen.GetRows();
        constexpr std::int32_t lineHeight = 22;
        const std::int32_t panelHeight = std::min(height - 60, static_cast<std::int32_t>(rows.size()) * lineHeight + 60);
        MenuView::Panel(renderingSystem_, assets_.menuArt, { left, top, panelWidth, panelHeight }, 30);

        const auto text = [&](std::string line, std::int32_t x, std::int32_t y, float scale, Color color) {
            renderingSystem_.DrawText(Systems::Rendering::TextRenderData{ std::move(line), LB::Position{ x, y }, {}, color, scale, 32, false });
        };
        // A name that would run into the detail column (a detail past the panel): smaller, then cut short with "..."
        const std::int32_t detailX = left + panelWidth / 2 + 60;
        const auto fitted = [&](std::string line, std::int32_t x, std::int32_t y, float scale, Color color, bool hasDetail) {
            const float room = static_cast<float>((hasDetail ? detailX - 10 : left + panelWidth - 16) - x);
            while (scale > 1.45f && renderingSystem_.MeasureText(line, scale).width > room) scale -= 0.1f;
            if (renderingSystem_.MeasureText(line, scale).width > room) {
                while (line.size() > 4 && renderingSystem_.MeasureText(line + "...", scale).width > room) line.pop_back();
                line += "...";
            }
            text(std::move(line), x, y, scale, color);
        };
        text(std::string(title), left + 16, top + 12, 2.6f, Display(0.95f, 0.75f, 0.30f));
        const std::string help = &screen == &session_->characterScreen ? "Enter/A: choose   X/Del: drop   Esc/B: close" : "Enter/A: choose   Esc/B: close";
        text(help, left + panelWidth - static_cast<std::int32_t>(renderingSystem_.MeasureText(help, 1.6f).width) - 16, top + 16, 1.6f, Display(0.6f, 0.6f, 0.6f));

        // Keep the cursor's row in view when the list is taller than the panel
        const auto visible = static_cast<std::size_t>(std::max(1, (panelHeight - 60) / lineHeight));
        const auto first = screen.GetCursor() >= visible ? screen.GetCursor() - visible + 1 : 0;
        for (std::size_t i = first; i < rows.size() && i < first + visible; ++i) {
            const auto& row = rows[i];
            const std::int32_t y = top + 50 + static_cast<std::int32_t>(i - first) * lineHeight;
            const bool selected = i == screen.GetCursor() && row.IsSelectable();
            if (selected) {
                MenuView::Selection(renderingSystem_, assets_.menuArt, { left + 8, y - 4, panelWidth - 16, lineHeight }, 31);
            }
            const auto color = row.heading ? Display(0.95f, 0.75f, 0.30f)
                             : !row.enabled ? Display(0.55f, 0.40f, 0.40f)
                             : row.color ? *row.color
                             : row.IsSelectable() ? Display(1.0f, 1.0f, 1.0f) : Display(0.75f, 0.75f, 0.75f);
            // Items get their icon before the name
            const auto icon = row.item.IsValid() ? IconFor(row.item) : Systems::Rendering::InvalidImageId;
            if (icon != Systems::Rendering::InvalidImageId) {
                Systems::Rendering::ImageRenderData image{};
                image.image = icon;
                image.rect = { left + 18, y - 3, lineHeight - 2, lineHeight - 2 };
                image.layerDepth = 33;
                image.color = row.enabled ? Display(1.0f, 1.0f, 1.0f) : Display(0.55f, 0.5f, 0.5f);
                renderingSystem_.DrawImage(image);
                fitted(row.text, left + 18 + lineHeight + 2, y, 1.8f, color, !row.detail.empty());
            }
            else {
                text(row.heading ? row.text : "  " + row.text, left + 16, y, row.heading ? 2.0f : 1.8f, color);
            }
            if (!row.detail.empty()) fitted(row.detail, detailX, y, 1.8f, row.enabled ? Display(0.70f, 0.70f, 0.70f) : color, false);
        }
    }

    // ------------------------------------------------------------------------------------------
    // Drawing items
    // ------------------------------------------------------------------------------------------

    Systems::Rendering::ModelId GameEngine::ItemModel(const GroundItem& ground) const {
        if (ground.IsGold()) return ModelOf(ground.quantity < 5 ? "coinsSmall.glb" : ground.quantity < 12 ? "coinsMedium.glb" : "coinsLarge.glb");
        GroundItem item = ground;
        item.item = session_->rolls.BaseOf(ground.item);
        switch (item.item.kind) {
        case ItemKind::Consumable: return ModelOf(content_->FindConsumable(item.item.id)->model);
        case ItemKind::Armor:
            return ModelOf(content_->FindArmor(item.item.id)->category == LB::Combat::ArmorCategory::Shield ? "shield_common.glb" : "lootSackA.glb");
        case ItemKind::Weapon: {
            switch (content_->FindWeapon(item.item.id)->category) {
            case LB::Combat::WeaponType::Sword: return assets_.models.playerWeapon;
            case LB::Combat::WeaponType::Axe: return ModelOf("axe_common.glb");
            case LB::Combat::WeaponType::Dagger: return ModelOf("dagger_common.glb");
            case LB::Combat::WeaponType::Mace: return ModelOf("hammer_common.glb");
            case LB::Combat::WeaponType::Staff: return ModelOf("staff_common.glb");
            case LB::Combat::WeaponType::Crossbow: return ModelOf("crossbow_common.glb");
            case LB::Combat::WeaponType::Bow: return ModelOf("bow_withString.gltf");
            default: return ModelOf("lootSackA.glb");
            }
        }
        case ItemKind::Rolled:
        case ItemKind::None: break;
        }
        return ModelOf("lootSackA.glb");
    }

    // The model of the equipped weapon's category, held in the knight's hand (a sword without one)
    std::string_view GameEngine::HeldWeaponFile() const {
        if (!session_->playerWeapon) return HeldSword;
        switch (session_->playerWeapon->category) {
        case LB::Combat::WeaponType::Axe: return "axe_1handed.gltf";
        case LB::Combat::WeaponType::Dagger: return "dagger.gltf";
        case LB::Combat::WeaponType::Mace: return "Skeleton_Mace.gltf";
        case LB::Combat::WeaponType::Staff: return "staff.gltf";
        case LB::Combat::WeaponType::Wand: return "wand.gltf";
        case LB::Combat::WeaponType::Bow: return "bow_withString.gltf";
        case LB::Combat::WeaponType::Crossbow: return "crossbow_1handed.gltf";
        default: return HeldSword;
        }
    }

    // A shield is worn (drawn on the knight's left arm)
    bool GameEngine::PlayerHoldsShield() const {
        if (!session_->playerId) return false;
        for (const auto& [slot, item] : WornItems()) {
            const auto* armor = content_->FindArmor(session_->rolls.BaseOf(item).id);
            if (armor && armor->category == LB::Combat::ArmorCategory::Shield) return true;
        }
        return false;
    }

    // Loot lies on the floor turning slowly, weapons laid flat, so it catches the eye
    void GameEngine::DrawGroundItems(const Color& tint) {
        for (const auto& item : session_->groundItems) {
            const float spin = item.ageSeconds * 1.2f;
            if (!assets_.models.loaded) {
                Systems::Rendering::MeshRenderData mesh{};
                mesh.shape = item.IsGold() ? Systems::Rendering::MeshShape::Cylinder : Systems::Rendering::MeshShape::Box;
                mesh.position = { item.position.x, 0.1f, item.position.y };
                mesh.size = { 0.3f, 0.2f, 0.3f };
                mesh.yawRadians = spin;
                mesh.color = item.IsGold() ? Display(0.9f, 0.7f, 0.2f) : Display(0.55f, 0.40f, 0.25f);
                renderingSystem_.DrawMesh(mesh);
                continue;
            }

            Systems::Rendering::ModelRenderData draw{ ItemModel(item), { item.position.x, 0.02f, item.position.y }, spin, ModelScale, tint };
            const auto base = session_->rolls.BaseOf(item.item);
            if (base.kind == ItemKind::Weapon && content_->FindWeapon(base.id)->category != LB::Combat::WeaponType::Bow) {
                // Blades point up in the props (the bow already lies flat): lay them on their side, a little above the floor
                draw.transform = Systems::Rendering::Matrix4::RotationX(std::numbers::pi_v<float> * 0.5f)
                               * Systems::Rendering::Matrix4::Translation({ 0.0f, 0.12f, 0.0f });
            }
            renderingSystem_.DrawModel(draw);
        }
    }

    // ------------------------------------------------------------------------------------------
    // Saving progress
    // ------------------------------------------------------------------------------------------

    void GameEngine::WriteProgress(Systems::Persistence::SaveData& data) const {
        const auto* player = PlayerCharacter();
        if (!player) return;
        const auto& character = *player;
        const auto& a = character.GetAttributes();
        if (const auto* classData = content_->FindClass(session_->playerClass)) data["player.class"] = classData->key;
        data["player.level"] = std::format("{}", character.GetLevel());
        data["player.experience"] = std::format("{}", character.GetExperience());
        data["player.attributes"] = std::format("{};{};{};{};{};{}", a.strength, a.dexterity, a.constitution, a.intelligence, a.wisdom, a.charisma);
        data["player.points"] = std::format("{}", character.GetUnspentAttributePoints());
        data["player.gold"] = std::format("{}", session_->gold);
        data["player.path"] = session_->path;
        std::string allies;
        for (const auto& ally : session_->allies.Allies()) {
            const auto [weapon, armour] = AllyItems(ally.id);
            allies += std::format("{}:{}:{}:{};", ally.definition->key, ally.hitPoints, weapon.IsValid() ? ItemSaveKey(weapon) : std::string("-"),
                                  armour.IsValid() ? ItemSaveKey(armour) : std::string("-"));
        }
        data["party.allies"] = allies;
        data["player.weapon"] = GetPlayerWeaponItem().IsValid() ? ItemSaveKey(GetPlayerWeaponItem()) : std::string{};

        std::string armour;
        for (const auto& [slot, item] : WornItems()) armour += ItemSaveKey(item) + ";";
        data["player.armour"] = armour;

        std::string bag;
        for (const auto& stack : GetInventory()) {
            bag += std::format("{}:{};", ItemSaveKey(ItemRef::Unpack(stack.itemId)), stack.quantity);
        }
        data["player.bag"] = bag;
    }

    // How a save names an item: its content key, or "~base|rarity|affix,affix" for a rolled one
    std::string GameEngine::ItemSaveKey(ItemRef item) const {
        const auto* rolled = session_->rolls.Find(item);
        if (!rolled) return DescribeItem(*content_, item).second;
        std::string affixes;
        for (const auto* affix : rolled->affixes) affixes += (affixes.empty() ? "" : ",") + affix->key;
        return std::format("~{}|{}|{}|{}", DescribeItem(*content_, rolled->base).second, rolled->rarity ? rolled->rarity->key : std::string{}, affixes, rolled->level);
    }

    // The item a save names (a rolled one is rolled again from its parts; unknown parts are left out)
    ItemRef GameEngine::ItemFromSaveKey(std::string_view text) {
        if (!text.starts_with('~')) return ItemRef::FromKey(*content_, text);
        const auto parts = Split(std::string(text.substr(1)), '|');
        if (parts.empty()) return {};
        const auto base = ItemRef::FromKey(*content_, parts[0]);
        if (!base.IsValid()) return {};
        RolledItem rolled{ base, parts.size() > 1 ? content_->FindRarity(parts[1]) : nullptr, {}, {} };
        if (parts.size() > 2) {
            for (const auto& key : Split(parts[2], ',')) {
                if (const auto* affix = content_->FindAffix(key)) rolled.affixes.push_back(affix);
            }
        }
        if (parts.size() > 3) ParseInt(parts[3], rolled.level);   // older saves: level 1
        rolled.level = std::max(1, rolled.level);
        return rolled.affixes.empty() ? base : session_->rolls.Add(std::move(rolled), *content_);
    }

    // Restores what the save holds; a save without progression (older) leaves it unchanged,
    // and unknown item keys (content changed since) are skipped
    void GameEngine::ReadProgress(const Systems::Persistence::SaveData& data) {
        const auto field = [&](const char* key) -> const std::string* {
            const auto it = data.find(key);
            return it != data.end() ? &it->second : nullptr;
        };
        std::int32_t level = 0, experience = 0, points = 0, gold = 0;
        const auto* attributesText = field("player.attributes");
        if (!field("player.level") || !ParseInt(*field("player.level"), level) || !field("player.experience") || !ParseInt(*field("player.experience"), experience)
            || !field("player.points") || !ParseInt(*field("player.points"), points) || !attributesText) {
            return;
        }

        const auto values = Split(*attributesText, ';');
        std::array<std::int32_t, 6> attributes{};
        if (values.size() != attributes.size()) return;
        for (std::size_t i = 0; i < attributes.size(); ++i) {
            if (!ParseInt(values[i], attributes[i])) return;
        }
        if (!session_->characterService.RestoreProgress(*session_->playerId, level, experience,
                LB::Attributes{ attributes[0], attributes[1], attributes[2], attributes[3], attributes[4], attributes[5] }, points)) {
            return;
        }

        if (const auto* text = field("player.gold"); text && ParseInt(*text, gold)) session_->gold = std::max(0, gold);
        if (const auto* text = field("player.path"); text && content_->FindClass(session_->playerClass) && content_->FindClass(session_->playerClass)->FindSpecialization(*text)) {
            session_->path = *text;
        }
        // The companions, with the health they had and what they carried ("key:hitPoints:weapon:armour;",
        // "-" for nothing)
        session_->allies.Clear();
        session_->allyItems.clear();
        if (const auto* text = field("party.allies")) {
            for (const auto& entry : Split(*text, ';')) {
                const auto parts = Split(entry, ':');
                std::int32_t hitPoints = 0;
                if (parts.size() < 2 || !ParseInt(parts[1], hitPoints)) continue;
                const auto* definition = content_->FindAlly(parts[0]);
                if (!definition) continue;
                if (const auto id = session_->allies.Add(*definition, session_->playerPosition, level)) {
                    auto& items = session_->allyItems[*id];
                    if (parts.size() > 2 && parts[2] != "-") items.first = ItemFromSaveKey(parts[2]);
                    if (parts.size() > 3 && parts[3] != "-") items.second = ItemFromSaveKey(parts[3]);
                    SyncAllyGear(*id);
                    const auto missing = session_->allies.Find(*id)->maxHitPoints - hitPoints;
                    if (missing > 0) session_->allies.Damage(*id, missing);
                }
            }
            session_->allies.PlaceAround(session_->world, session_->playerPosition, false);
        }
        if (const auto* text = field("player.weapon")) {
            const auto item = ItemFromSaveKey(*text);
            if (const auto* weapon = content_->FindWeapon(session_->rolls.BaseOf(item).id); weapon && session_->rolls.BaseOf(item).kind == ItemKind::Weapon) {
                PutOn(item, LB::Combat::EquipmentSlot::MainHand);
            }
        }

        for (const auto& [slot, item] : WornItems()) TakeOff(slot);
        if (const auto* text = field("player.armour")) {
            for (const auto& entry : Split(*text, ';')) {
                const auto item = ItemFromSaveKey(entry);
                const auto base = session_->rolls.BaseOf(item);
                if (const auto* armor = base.kind == ItemKind::Armor ? content_->FindArmor(base.id) : nullptr) PutOn(item, armor->slot);
            }
        }

        session_->inventoryService.ClearInventory(*session_->playerId);
        if (const auto* text = field("player.bag")) {
            for (const auto& entry : Split(*text, ';')) {
                const auto colon = entry.rfind(':');
                std::int32_t quantity = 0;
                if (colon == std::string::npos || !ParseInt(std::string_view(entry).substr(colon + 1), quantity)) continue;
                GiveItemRef(ItemFromSaveKey(entry.substr(0, colon)), quantity);
            }
        }
    }

    // ------------------------------------------------------------------------------------------
    // Dropping items; the path cards; the banner
    // ------------------------------------------------------------------------------------------

    bool GameEngine::DropItem(ItemRef item) {
        if (!session_->playerId || !item.IsValid()) return false;
        const auto bag = GetInventory();
        const auto stack = std::ranges::find(bag, item.Pack(), &Inventory::InventoryItem::itemId);
        if (stack == bag.end() || !session_->inventoryService.RemoveItem(*session_->playerId, item.Pack(), stack->quantity).success) return false;
        const auto offset = session_->lootRoller.Scatter(0.3f);
        GroundItem ground{};
        ground.id = session_->nextGroundItemId++;
        ground.item = item;
        ground.quantity = stack->quantity;
        ground.position = { session_->playerPosition.x + offset.x, session_->playerPosition.y + offset.y };
        ground.dropped = true;
        session_->groundItems.push_back(ground);
        session_->combatText.Add(std::format("DROPPED {}", stack->name), { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.8f, 0.8f, 0.8f, 1.0f }, 2.0f);
        PlaySound(GameSound::ItemPickup, session_->playerPosition);
        return true;
    }

    // What a path adds, one line each ("+2 damage")
    std::vector<std::string> GameEngine::PathBonuses(const Content::SpecializationDefinition& path) const {
        const auto* classData = PlayerClassDefinition();
        const bool stamina = classData && classData->resource == "Stamina";
        std::vector<std::string> lines;
        if (path.damage) lines.push_back(std::format("+{} damage", path.damage));
        if (path.attackBonus) lines.push_back(std::format("+{} to hit", path.attackBonus));
        if (path.armorClass) lines.push_back(std::format("+{} armour class", path.armorClass));
        if (path.armor) lines.push_back(std::format("+{} armour", path.armor));
        if (path.spellPower) lines.push_back(std::format("+{} spell power", path.spellPower));
        if (path.manaRegen > 0.0f) lines.push_back(std::format("+{:g} {} a second", path.manaRegen, stamina ? "stamina" : "mana"));
        return lines;
    }

    std::vector<PathCard> GameEngine::PathCards() const {
        std::vector<PathCard> cards;
        if (const auto* classData = PlayerClassDefinition()) {
            for (const auto& path : classData->specializations) {
                cards.push_back({ path.key, path.name, path.flavor.empty() ? path.description : path.flavor, PathBonuses(path), path.color });
            }
        }
        return cards;
    }

    // The cards: left and right (or up and down) compare, confirm takes the path, back returns
    void GameEngine::UpdatePathChoice() {
        const auto triggered = [this](std::string_view action) { return inputSystem_.IsActionTriggered(std::string(action)); };
        const auto cards = PathCards();
        if (triggered(MenuBackAction) || triggered(CharacterScreenAction)) {
            session_->pathChoice.Close();
            PlaySound(GameSound::ScreenClose);
            return;
        }
        const int step = (triggered(MenuRightAction) || triggered(MenuDownAction) ? 1 : 0) - (triggered(MenuLeftAction) || triggered(MenuUpAction) ? 1 : 0);
        if (step != 0) {
            session_->pathChoice.Step(step, cards.size());
            PlaySound(GameSound::MenuMove);
        }
        if (triggered(MenuConfirmAction) && session_->pathChoice.Cursor() < cards.size()) {
            if (ChoosePath(cards[session_->pathChoice.Cursor()].key)) {
                session_->pathChoice.Close();
                session_->characterScreen.Close();   // to the game, where the banner shows
                PlaySound(GameSound::MenuConfirm);
            }
        }
    }

    void GameEngine::DrawFanfare() {
        session_->fanfare.Draw(renderingSystem_, assets_.menuArt);
    }

    // An item's icon: by its weapon category, armour slot or kind, or what a consumable is
    Systems::Rendering::ImageId GameEngine::IconFor(ItemRef item) const {
        const auto base = session_->rolls.BaseOf(item);
        std::string_view name;
        switch (base.kind) {
        case ItemKind::Weapon:
            switch (content_->FindWeapon(base.id)->category) {
            case LB::Combat::WeaponType::Axe: name = "axe"; break;
            case LB::Combat::WeaponType::Dagger: name = "dagger"; break;
            case LB::Combat::WeaponType::Mace: name = "mace"; break;
            case LB::Combat::WeaponType::Staff: name = "staff"; break;
            case LB::Combat::WeaponType::Wand: name = "wand"; break;
            case LB::Combat::WeaponType::Bow: name = "bow"; break;
            case LB::Combat::WeaponType::Crossbow: name = "crossbow"; break;
            default: name = "sword"; break;
            }
            break;
        case ItemKind::Armor: {
            const auto* armor = content_->FindArmor(base.id);
            if (armor->category == LB::Combat::ArmorCategory::Shield) name = "shield";
            else switch (armor->slot) {
            case LB::Combat::EquipmentSlot::Head: name = "helmet"; break;
            case LB::Combat::EquipmentSlot::Hands: name = "gloves"; break;
            case LB::Combat::EquipmentSlot::Feet: name = "boots"; break;
            default: name = "chest"; break;
            }
            break;
        }
        case ItemKind::Consumable: {
            const auto* consumable = content_->FindConsumable(base.id);
            name = consumable->isKey ? "key" : consumable->ammoType == "Bolt" ? "bolts" : consumable->IsAmmunition() ? "arrows" : "potion";
            break;
        }
        default: return Systems::Rendering::InvalidImageId;
        }
        const auto it = assets_.itemIcons.find(name);
        return it != assets_.itemIcons.end() ? it->second : Systems::Rendering::InvalidImageId;
    }

    // ------------------------------------------------------------------------------------------
    // Allies' gear (design-character-system.md 7): a weapon and an armour piece each
    // ------------------------------------------------------------------------------------------

    bool GameEngine::GiveToAlly(std::uint32_t ally, ItemRef item) {
        if (!session_->playerId || !session_->allies.Find(ally) || !item.IsValid()) return false;
        const auto base = session_->rolls.BaseOf(item);
        if (base.kind != ItemKind::Weapon && base.kind != ItemKind::Armor) return false;
        const auto bag = GetInventory();
        if (std::ranges::none_of(bag, [&](const Inventory::InventoryItem& stack) { return stack.itemId == item.Pack(); })) return false;
        session_->inventoryService.RemoveItem(*session_->playerId, item.Pack(), 1);
        auto& [weapon, armour] = session_->allyItems[ally];
        auto& slot = base.kind == ItemKind::Weapon ? weapon : armour;
        if (slot.IsValid()) GiveItemRef(slot);   // what they had goes back to the bag
        slot = item;
        SyncAllyGear(ally);
        PlaySound(GameSound::Equip);
        return true;
    }

    bool GameEngine::TakeFromAlly(std::uint32_t ally) {
        auto it = session_->allyItems.find(ally);
        if (it == session_->allyItems.end() || !session_->playerId) return false;
        const auto needed = (it->second.first.IsValid() ? 1u : 0u) + (it->second.second.IsValid() ? 1u : 0u);
        if (needed == 0 || GetInventory().size() + needed > static_cast<std::size_t>(BagCapacity)) return false;
        if (it->second.first.IsValid()) GiveItemRef(it->second.first);
        if (it->second.second.IsValid()) GiveItemRef(it->second.second);
        session_->allyItems.erase(it);
        SyncAllyGear(ally);
        return true;
    }

    // A weapon adds its damage bonus and affixes; armour its armour, armour class, and hit points
    void GameEngine::SyncAllyGear(std::uint32_t ally) {
        AllyGear gear{};
        const auto [weapon, armour] = AllyItems(ally);
        if (weapon.IsValid()) {
            const auto bonuses = session_->rolls.BonusesOf(weapon);
            gear.damage = content_->FindWeapon(session_->rolls.BaseOf(weapon).id)->damageBonus + bonuses.damage;
            gear.attackBonus = bonuses.attackBonus;
        }
        if (armour.IsValid()) {
            const auto* piece = content_->FindArmor(session_->rolls.BaseOf(armour).id);
            const auto bonuses = session_->rolls.BonusesOf(armour);
            gear.armor = piece->physicalAR + piece->armorBonus + bonuses.armor;
            gear.armorClass = bonuses.armorClass;
            gear.hitPoints = bonuses.hitPoints;
        }
        session_->allies.SetGear(ally, gear);
    }

} // namespace LB::Engine
