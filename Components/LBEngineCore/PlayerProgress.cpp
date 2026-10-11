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
        const auto item = ItemRef::FromKey(*content_, key);
        if (!session_->playerId || !item.IsValid() || quantity <= 0) return 0;

        Inventory::InventoryItem stack{};
        stack.itemId = item.Pack();
        stack.name = DescribeItem(*content_, item).first;
        stack.quantity = quantity;
        stack.isEquippable = item.kind != ItemKind::Consumable;
        stack.isConsumable = item.kind == ItemKind::Consumable;
        if (item.kind == ItemKind::Consumable) stack.maxStack = content_->FindConsumable(item.id)->maxStack;
        if (item.kind == ItemKind::Weapon) stack.equipmentSlot = content_->FindWeapon(item.id)->slot;
        if (item.kind == ItemKind::Armor) stack.equipmentSlot = content_->FindArmor(item.id)->slot;
        return session_->inventoryService.AddItem(*session_->playerId, stack).itemsAffected;
    }

    void GameEngine::GiveStartingKit() {
        session_->gold = 0;
        session_->groundItems.clear();
        session_->playerWeapon = content_->FindWeapon(StartingWeapon);
        if (!session_->playerId) return;
        session_->inventoryService.CreateInventory(*session_->playerId, BagCapacity);
        GiveItem(HealingPotion, StartingPotions);
        GiveItem(StartingBow);
    }

    // Why the player cannot equip an item, or empty when they can
    std::string GameEngine::CanEquip(ItemRef item) const {
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
        const auto item = ItemRef::FromKey(*content_, key);
        if (!session_->playerId || !item.IsValid() || CountItem(key) == 0 || !CanEquip(item).empty()) return false;

        session_->inventoryService.RemoveItem(*session_->playerId, item.Pack(), 1);
        if (item.kind == ItemKind::Weapon) {
            if (session_->playerWeapon) GiveItem(session_->playerWeapon->key);
            session_->playerWeapon = content_->FindWeapon(item.id);
            return true;
        }

        // The piece in the same slot goes back to the bag (there is room: one item just left it)
        const auto* armor = content_->FindArmor(item.id);
        for (const auto& worn : session_->characterService.GetEquippedArmor(*session_->playerId)) {
            if (worn.slot == armor->slot) {
                if (const auto* previous = content_->FindArmor(worn.armorId)) GiveItem(previous->key);
            }
        }
        return EquipPlayerArmor(armor->key);
    }

    bool GameEngine::UnequipArmorSlot(EquipmentSlot slot) {
        if (!session_->playerId || !session_->inventoryService.HasInventorySpace(*session_->playerId)) return false;
        for (const auto& worn : session_->characterService.GetEquippedArmor(*session_->playerId)) {
            if (worn.slot != slot) continue;
            const auto* armor = content_->FindArmor(worn.armorId);
            if (!armor || !session_->characterService.UnequipArmor(*session_->playerId, slot)) return false;
            GiveItem(armor->key);
            return true;
        }
        return false;
    }

    bool GameEngine::UseItem(std::string_view key) {
        const auto item = ItemRef::FromKey(*content_, key);
        if (!session_->playerId || item.kind != ItemKind::Consumable || CountItem(key) == 0 || !IsPlayerAlive()) return false;

        const auto* consumable = content_->FindConsumable(item.id);
        const auto* player = PlayerCharacter();
        if (!consumable || !player) return false;
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

    void GameEngine::OnEnemyKilled(const Enemy& enemy) {
        if (!session_->playerId) return;

        const Systems::Rendering::Vector3 overPlayer{ session_->playerPosition.x, PlayerTop(), session_->playerPosition.y };
        if (enemy.experience > 0) {
            if (const auto progress = session_->characterService.AwardExperience(*session_->playerId, enemy.experience); progress && progress->levelIncreased) {
                session_->combatText.Add("LEVEL UP!", overPlayer, { 1.0f, 0.82f, 0.25f, 1.0f }, 4.0f);
                EmitLevelUpEffect();
                PlaySound(GameSound::LevelUp, session_->playerPosition);
            }
        }
        if (!enemy.definition->lootTable.empty()) {
            DropLoot(enemy.definition->lootTable, enemy.position);
        }
    }

    void GameEngine::DropLoot(std::string_view lootTable, WorldPoint at) {
        const auto* table = content_->FindLootTable(lootTable);
        if (!table) return;
        for (auto drop : session_->lootRoller.Roll(*table, *content_)) {
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
            if (!alive || Encounter::Distance(ground.position, session_->playerPosition) > PickupRadius) return false;

            const Systems::Rendering::Vector3 at{ ground.position.x, 0.6f, ground.position.y };
            if (ground.IsGold()) {
                session_->gold += ground.quantity;
                session_->combatText.Add(std::format("+{} GOLD", ground.quantity), at, { 0.95f, 0.80f, 0.35f, 1.0f }, 2.0f);
                PlaySound(GameSound::Gold, ground.position);
                return true;
            }

            const auto [name, key] = DescribeItem(*content_, ground.item);
            if (GiveItem(key, ground.quantity) == ground.quantity) {
                session_->combatText.Add(name, at, { 1.0f, 1.0f, 1.0f, 1.0f }, 2.0f);
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
        info(std::format("Experience {} / {}", status.currentExperience, status.experienceToNextLevel),
             std::format("HP {}/{}   Gold {}", status.currentHitPoints, status.maxHitPoints, session_->gold));

        // Attributes: with points to spend, each one below its cap can be raised
        const auto points = character.GetUnspentAttributePoints();
        heading(points > 0 ? std::format("Attributes - {} point{} to spend", points, points == 1 ? "" : "s") : std::string("Attributes"));
        for (const auto& [attribute, name] : AttributeNames) {
            const auto value = status.currentAttributes.Get(attribute);
            const auto modifier = static_cast<std::int32_t>(std::floor((value - 10) / 2.0));
            ScreenRow row{ std::format("{} {}", name, value), std::format("{}{}", modifier >= 0 ? "+" : "", modifier) };
            if (points > 0) {
                row.action = ScreenAction::SpendPoint;
                row.attribute = attribute;
            }
            rows.push_back(std::move(row));
        }

        // Equipment: the weapon, then each armour slot
        heading("Equipped");
        if (session_->playerWeapon) {
            info(std::format("Main hand: {}", session_->playerWeapon->name), std::format("{} +{}", DiceText(session_->playerWeapon->damageDice), GetPlayerAttackProfile().damageBonus));
        }
        for (const auto& worn : session_->characterService.GetEquippedArmor(*session_->playerId)) {
            const auto* armor = content_->FindArmor(worn.armorId);
            if (!armor) continue;
            ScreenRow row{ std::format("{}: {}", SlotName(worn.slot), armor->name), std::format("AR {}", armor->physicalAR + armor->armorBonus) };
            row.action = ScreenAction::Unequip;
            row.slot = worn.slot;
            rows.push_back(std::move(row));
        }
        info("Defense", std::format("AC {}   AR {}", GetPlayerDefense().armorClass, GetPlayerDefense().physicalArmor));

        // The bag
        const auto bag = GetInventory();
        heading(std::format("Bag ({}/{})", bag.size(), BagCapacity));
        if (bag.empty()) info("Empty");
        for (const auto& stack : bag) {
            const auto item = ItemRef::Unpack(stack.itemId);
            ScreenRow row{ stack.quantity > 1 ? std::format("{} x{}", stack.name, stack.quantity) : stack.name };
            row.item = item;
            if (item.kind == ItemKind::Consumable) {
                const auto* consumable = content_->FindConsumable(item.id);
                row.action = ScreenAction::Use;
                row.detail = consumable->healing.count > 0 ? std::format("heals {}", DiceText(consumable->healing)) : std::string("use");
            }
            else {
                row.action = ScreenAction::Equip;
                const auto reason = CanEquip(item);
                row.enabled = reason.empty();
                if (item.kind == ItemKind::Weapon) {
                    const auto* weapon = content_->FindWeapon(item.id);
                    row.detail = reason.empty() ? std::format("{} {}", DiceText(weapon->damageDice), weapon->attackTimeSeconds) : reason;
                }
                else {
                    const auto* armor = content_->FindArmor(item.id);
                    row.detail = reason.empty() ? std::format("{} AR {}", SlotName(armor->slot), armor->physicalAR + armor->armorBonus) : reason;
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

        if (!session_->characterScreen.IsOpen()) {
            if (triggered(CharacterScreenAction)) {
                session_->characterScreen.Open();
                session_->characterScreen.SetRows(BuildScreenRows());
                PlaySound(GameSound::ScreenOpen);
            }
            return;
        }

        if (triggered(CharacterScreenAction) || triggered(MenuBackAction)) {
            session_->characterScreen.Close();
            PlaySound(GameSound::ScreenClose);
            return;
        }

        session_->characterScreen.SetRows(BuildScreenRows());
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
        const auto key = DescribeItem(*content_, row->item).second;
        if (row->action == ScreenAction::Equip || row->action == ScreenAction::Unequip) PlaySound(GameSound::Equip);
        else if (row->action != ScreenAction::None && row->action != ScreenAction::Use) PlaySound(GameSound::MenuConfirm);
        switch (row->action) {
        case ScreenAction::SpendPoint: session_->characterService.SpendAttributePoint(*session_->playerId, row->attribute); break;
        case ScreenAction::Equip: EquipItem(key); break;
        case ScreenAction::Use: UseItem(key); break;
        case ScreenAction::Unequip: UnequipArmorSlot(row->slot); break;
        case ScreenAction::None: break;
        }
        session_->characterScreen.SetRows(BuildScreenRows());
    }

    void GameEngine::DrawCharacterScreen() {
        if (!session_->characterScreen.IsOpen()) return;

        std::int32_t width = 0;
        std::int32_t height = 0;
        renderingSystem_.GetUiSize(width, height);
        const std::int32_t panelWidth = std::min(width - 40, 620);
        const std::int32_t left = (width - panelWidth) / 2;
        const std::int32_t top = 30;
        const auto& rows = session_->characterScreen.GetRows();
        constexpr std::int32_t lineHeight = 22;
        const std::int32_t panelHeight = std::min(height - 60, static_cast<std::int32_t>(rows.size()) * lineHeight + 60);
        MenuView::Panel(renderingSystem_, assets_.menuArt, { left, top, panelWidth, panelHeight }, 30);

        const auto text = [&](std::string line, std::int32_t x, std::int32_t y, float scale, Color color) {
            renderingSystem_.DrawText(Systems::Rendering::TextRenderData{ std::move(line), LB::Position{ x, y }, {}, color, scale, 32, false });
        };
        text("CHARACTER", left + 16, top + 12, 2.6f, Display(0.95f, 0.75f, 0.30f));
        text("Enter/A: choose   Esc/B: close", left + panelWidth - 250, top + 16, 1.6f, Display(0.6f, 0.6f, 0.6f));

        // Keep the cursor's row in view when the list is taller than the panel
        const auto visible = static_cast<std::size_t>(std::max(1, (panelHeight - 60) / lineHeight));
        const auto first = session_->characterScreen.GetCursor() >= visible ? session_->characterScreen.GetCursor() - visible + 1 : 0;
        for (std::size_t i = first; i < rows.size() && i < first + visible; ++i) {
            const auto& row = rows[i];
            const std::int32_t y = top + 50 + static_cast<std::int32_t>(i - first) * lineHeight;
            const bool selected = i == session_->characterScreen.GetCursor() && row.IsSelectable();
            if (selected) {
                MenuView::Selection(renderingSystem_, assets_.menuArt, { left + 8, y - 4, panelWidth - 16, lineHeight }, 31);
            }
            const auto color = row.heading ? Display(0.95f, 0.75f, 0.30f)
                             : !row.enabled ? Display(0.55f, 0.40f, 0.40f)
                             : row.IsSelectable() ? Display(1.0f, 1.0f, 1.0f) : Display(0.75f, 0.75f, 0.75f);
            text(row.heading ? row.text : "  " + row.text, left + 16, y, row.heading ? 2.0f : 1.8f, color);
            if (!row.detail.empty()) text(row.detail, left + panelWidth / 2 + 60, y, 1.8f, row.enabled ? Display(0.70f, 0.70f, 0.70f) : color);
        }
    }

    // ------------------------------------------------------------------------------------------
    // Drawing items
    // ------------------------------------------------------------------------------------------

    Systems::Rendering::ModelId GameEngine::ItemModel(const GroundItem& item) const {
        if (item.IsGold()) return ModelOf(item.quantity < 5 ? "coinsSmall.glb" : item.quantity < 12 ? "coinsMedium.glb" : "coinsLarge.glb");
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
        case LB::Combat::WeaponType::Staff:
        case LB::Combat::WeaponType::Wand: return "staff.gltf";
        case LB::Combat::WeaponType::Bow: return "bow_withString.gltf";
        case LB::Combat::WeaponType::Crossbow: return "crossbow_1handed.gltf";
        default: return HeldSword;
        }
    }

    // A shield is worn (drawn on the knight's left arm)
    bool GameEngine::PlayerHoldsShield() const {
        if (!session_->playerId) return false;
        for (const auto& worn : session_->characterService.GetEquippedArmor(*session_->playerId)) {
            const auto* armor = content_->FindArmor(worn.armorId);
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
            if (item.item.kind == ItemKind::Weapon && content_->FindWeapon(item.item.id)->category != LB::Combat::WeaponType::Bow) {
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
        data["player.level"] = std::format("{}", character.GetLevel());
        data["player.experience"] = std::format("{}", character.GetExperience());
        data["player.attributes"] = std::format("{};{};{};{};{};{}", a.strength, a.dexterity, a.constitution, a.intelligence, a.wisdom, a.charisma);
        data["player.points"] = std::format("{}", character.GetUnspentAttributePoints());
        data["player.gold"] = std::format("{}", session_->gold);
        data["player.weapon"] = session_->playerWeapon ? session_->playerWeapon->key : std::string{};

        std::string armour;
        for (const auto& worn : session_->characterService.GetEquippedArmor(*session_->playerId)) {
            if (const auto* piece = content_->FindArmor(worn.armorId)) armour += piece->key + ";";
        }
        data["player.armour"] = armour;

        std::string bag;
        for (const auto& stack : GetInventory()) {
            bag += std::format("{}:{};", DescribeItem(*content_, ItemRef::Unpack(stack.itemId)).second, stack.quantity);
        }
        data["player.bag"] = bag;
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
        if (const auto* text = field("player.weapon"); text && content_->FindWeapon(*text)) session_->playerWeapon = content_->FindWeapon(*text);

        for (const auto& worn : session_->characterService.GetEquippedArmor(*session_->playerId)) session_->characterService.UnequipArmor(*session_->playerId, worn.slot);
        if (const auto* text = field("player.armour")) {
            for (const auto& key : Split(*text, ';')) EquipPlayerArmor(key);
        }

        session_->inventoryService.ClearInventory(*session_->playerId);
        if (const auto* text = field("player.bag")) {
            for (const auto& entry : Split(*text, ';')) {
                const auto colon = entry.rfind(':');
                std::int32_t quantity = 0;
                if (colon == std::string::npos || !ParseInt(std::string_view(entry).substr(colon + 1), quantity)) continue;
                GiveItem(entry.substr(0, colon), quantity);
            }
        }
    }

} // namespace LB::Engine
