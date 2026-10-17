// PlayerProgress.cpp - LB.Engine implementation unit: experience, loot, the bag, equipment, item
// tiers, allies' gear, and saving the player's progress (GameEngine members declared in
// GameEngine.ixx; the character screen is in CharacterScreens.cpp)
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
        return MenuView::Display(rolled->rarity->color[0], rolled->rarity->color[1], rolled->rarity->color[2]);
    }

    // A weapon or armour piece dropped: its rarity at this depth (the wave in the arena) and, above
    // common, its affixes (design-loot-and-progression.md 3-4)
    ItemRef GameEngine::RollDrop(ItemRef item, std::int32_t depthBonus) {
        if (item.kind != ItemKind::Weapon && item.kind != ItemKind::Armor) return item;
        const auto depth = (session_->depth > 0 ? ChallengeDepth() : std::max(1, session_->encounter.GetWave())) + depthBonus;
        item = ForTier(item, TierForDepth(depth));   // the deeper tiers drop their own gear
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
            const auto healed = session_->characterService.HealCharacter(*session_->playerId, session_->lootRoller.RollDice(consumable->healing)
                + static_cast<std::int32_t>(consumable->healingPerLevel * static_cast<float>(character.GetLevel())));
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
            if (const auto* slain = content_->FindEnemy(kill.enemyKey); slain && slain->boss) ++session_->stats.bosses;
            if (kill.enemyKey == FinalBoss && IsFinalFloor()) session_->events.Publish(RunWonEvent{ kill.at });
            if (session_->objective.kind == ObjectiveKind::SlayChampion && session_->objective.champion == kill.enemyId) CompleteObjective();
            CountBounty(kill.enemyKey);
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
        events.Subscribe<RunWonEvent>([this](const RunWonEvent& won) { WinRun(won); });
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
        // The merchant's bounty: "kind:killed:needed:gold" (empty: none)
        const auto& bounty = session_->bounty;
        data["party.bounty"] = bounty.Active() ? std::format("{}:{}:{}:{}", bounty.enemyKey, bounty.killed, bounty.needed, bounty.gold) : std::string{};
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
        // The merchant's bounty, if one was posted and not yet fulfilled
        session_->bounty = {};
        if (const auto* text = field("party.bounty"); text && !text->empty()) {
            const auto parts = Split(*text, ':');
            Bounty bounty;
            if (parts.size() == 4 && ParseInt(parts[1], bounty.killed) && ParseInt(parts[2], bounty.needed) && ParseInt(parts[3], bounty.gold)) {
                if (const auto* kind = content_->FindEnemy(parts[0])) {
                    bounty.enemyKey = kind->key;
                    bounty.plural = Plural(kind->name);
                    session_->bounty = bounty;
                }
            }
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

    // ------------------------------------------------------------------------------------------
    // Item tiers (design-armour-system.md 6.1, design-weapons-system.md): each dungeon tier its gear
    // ------------------------------------------------------------------------------------------

    // The same kind of item from a deeper tier: a weapon of the same category and hands, armour of
    // the same slot and weight; the deepest there is up to `tier` (itself when none is deeper)
    ItemRef GameEngine::ForTier(ItemRef item, std::int32_t tier) const {
        if (item.kind == ItemKind::Weapon) {
            const auto* weapon = content_->FindWeapon(item.id);
            if (!weapon) return item;
            const Content::WeaponDefinition* best = weapon;
            for (const auto& other : content_->GetWeapons()) {
                if (other.category == weapon->category && other.isTwoHanded == weapon->isTwoHanded && other.tier > best->tier && other.tier <= tier) best = &other;
            }
            return ItemRef{ ItemKind::Weapon, best->id };
        }
        if (item.kind == ItemKind::Armor) {
            const auto* armor = content_->FindArmor(item.id);
            if (!armor) return item;
            const Content::ArmorDefinition* best = armor;
            for (const auto& other : content_->GetArmour()) {
                if (other.slot == armor->slot && other.category == armor->category && other.tier > best->tier && other.tier <= tier) best = &other;
            }
            return ItemRef{ ItemKind::Armor, best->id };
        }
        return item;
    }

} // namespace LB::Engine
