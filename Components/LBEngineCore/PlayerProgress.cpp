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
        if (!playerId_ || !item.IsValid() || quantity <= 0) return 0;

        Inventory::InventoryItem stack{};
        stack.itemId = item.Pack();
        stack.name = DescribeItem(*content_, item).first;
        stack.quantity = quantity;
        stack.isEquippable = item.kind != ItemKind::Consumable;
        stack.isConsumable = item.kind == ItemKind::Consumable;
        if (item.kind == ItemKind::Consumable) stack.maxStack = content_->FindConsumable(item.id)->maxStack;
        if (item.kind == ItemKind::Weapon) stack.equipmentSlot = content_->FindWeapon(item.id)->slot;
        if (item.kind == ItemKind::Armor) stack.equipmentSlot = content_->FindArmor(item.id)->slot;
        return inventoryService_.AddItem(*playerId_, stack).itemsAffected;
    }

    void GameEngine::GiveStartingKit() {
        gold_ = 0;
        groundItems_.clear();
        playerWeapon_ = content_->FindWeapon(StartingWeapon);
        if (!playerId_) return;
        inventoryService_.CreateInventory(*playerId_, BagCapacity);
        GiveItem(HealingPotion, StartingPotions);
    }

    // Why the player cannot equip an item, or empty when they can
    std::string GameEngine::CanEquip(ItemRef item) const {
        const auto& character = characterService_.GetCharacter(*playerId_)->get();
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
        if (!playerId_ || !item.IsValid() || CountItem(key) == 0 || !CanEquip(item).empty()) return false;

        inventoryService_.RemoveItem(*playerId_, item.Pack(), 1);
        if (item.kind == ItemKind::Weapon) {
            if (playerWeapon_) GiveItem(playerWeapon_->key);
            playerWeapon_ = content_->FindWeapon(item.id);
            return true;
        }

        // The piece in the same slot goes back to the bag (there is room: one item just left it)
        const auto* armor = content_->FindArmor(item.id);
        for (const auto& worn : characterService_.GetEquippedArmor(*playerId_)) {
            if (worn.slot == armor->slot) {
                if (const auto* previous = content_->FindArmor(worn.armorId)) GiveItem(previous->key);
            }
        }
        return EquipPlayerArmor(armor->key);
    }

    bool GameEngine::UnequipArmorSlot(EquipmentSlot slot) {
        if (!playerId_ || !inventoryService_.HasInventorySpace(*playerId_)) return false;
        for (const auto& worn : characterService_.GetEquippedArmor(*playerId_)) {
            if (worn.slot != slot) continue;
            const auto* armor = content_->FindArmor(worn.armorId);
            if (!armor || !characterService_.UnequipArmor(*playerId_, slot)) return false;
            GiveItem(armor->key);
            return true;
        }
        return false;
    }

    bool GameEngine::UseItem(std::string_view key) {
        const auto item = ItemRef::FromKey(*content_, key);
        if (!playerId_ || item.kind != ItemKind::Consumable || CountItem(key) == 0 || !IsPlayerAlive()) return false;

        const auto* consumable = content_->FindConsumable(item.id);
        const auto& character = characterService_.GetCharacter(*playerId_)->get();
        if (consumable->healing.count > 0 && character.GetCurrentHitPoints() >= character.GetMaxHitPoints()) {
            return false;   // no waste at full health
        }

        inventoryService_.RemoveItem(*playerId_, item.Pack(), 1);
        if (consumable->healing.count > 0) {
            const auto healed = characterService_.HealCharacter(*playerId_, lootRoller_.RollDice(consumable->healing));
            AddFloatingText(std::format("+{}", healed), { playerPosition_.x, PlayerTop(), playerPosition_.y }, { 0.45f, 0.95f, 0.45f, 1.0f }, 3.0f);
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
        if (!playerId_) return;

        const Systems::Rendering::Vector3 overPlayer{ playerPosition_.x, PlayerTop(), playerPosition_.y };
        if (enemy.experience > 0) {
            if (const auto progress = characterService_.AwardExperience(*playerId_, enemy.experience); progress && progress->levelIncreased) {
                AddFloatingText("LEVEL UP!", overPlayer, { 1.0f, 0.82f, 0.25f, 1.0f }, 4.0f);
            }
        }
        if (!enemy.definition->lootTable.empty()) {
            DropLoot(enemy.definition->lootTable, enemy.position);
        }
    }

    void GameEngine::DropLoot(std::string_view lootTable, WorldPoint at) {
        const auto* table = content_->FindLootTable(lootTable);
        if (!table) return;
        for (auto drop : lootRoller_.Roll(*table, *content_)) {
            const auto offset = lootRoller_.Scatter(0.45f);
            drop.id = nextGroundItemId_++;
            drop.position = ClampToArena({ at.x + offset.x, at.y + offset.y });
            groundItems_.push_back(drop);
        }
    }

    // Walking over gold or an item picks it up; an item that does not fit stays (warned once)
    void GameEngine::UpdateGroundItems(float elapsedSeconds) {
        if (!playerId_) return;
        const bool alive = IsPlayerAlive();
        std::erase_if(groundItems_, [&](GroundItem& ground) {
            ground.ageSeconds += elapsedSeconds;
            if (!alive || Encounter::Distance(ground.position, playerPosition_) > PickupRadius) return false;

            const Systems::Rendering::Vector3 at{ ground.position.x, 0.6f, ground.position.y };
            if (ground.IsGold()) {
                gold_ += ground.quantity;
                AddFloatingText(std::format("+{} GOLD", ground.quantity), at, { 0.95f, 0.80f, 0.35f, 1.0f }, 2.0f);
                return true;
            }

            const auto [name, key] = DescribeItem(*content_, ground.item);
            if (GiveItem(key, ground.quantity) == ground.quantity) {
                AddFloatingText(name, at, { 1.0f, 1.0f, 1.0f, 1.0f }, 2.0f);
                return true;
            }
            if (!ground.fullWarningShown) {
                ground.fullWarningShown = true;
                AddFloatingText("BAG FULL", at, { 0.95f, 0.45f, 0.40f, 1.0f }, 2.0f);
            }
            return false;
        });
    }

    void GameEngine::AddFloatingText(std::string text, Systems::Rendering::Vector3 at, Color color, float scale) {
        FloatingText floating{};
        floating.text = std::move(text);
        floating.position = at;
        floating.color = color;
        floating.scale = scale;
        floatingTexts_.push_back(std::move(floating));
    }

    // ------------------------------------------------------------------------------------------
    // Character screen
    // ------------------------------------------------------------------------------------------

    std::vector<ScreenRow> GameEngine::BuildScreenRows() const {
        std::vector<ScreenRow> rows;
        if (!playerId_) return rows;
        const auto status = *characterService_.GetCharacterStatus(*playerId_);
        const auto& character = characterService_.GetCharacter(*playerId_)->get();
        const auto heading = [&](std::string text) { rows.push_back(ScreenRow{ std::move(text), {}, ScreenAction::None, true, true }); };
        const auto info = [&](std::string text, std::string detail = {}) { rows.push_back(ScreenRow{ std::move(text), std::move(detail) }); };

        const auto* classDefinition = content_->FindClass(status.characterClass);
        heading(std::format("{} - level {} {}", status.name, status.level, classDefinition ? classDefinition->name : std::string{}));
        info(std::format("Experience {} / {}", status.currentExperience, status.experienceToNextLevel),
             std::format("HP {}/{}   Gold {}", status.currentHitPoints, status.maxHitPoints, gold_));

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
        if (playerWeapon_) {
            info(std::format("Main hand: {}", playerWeapon_->name), std::format("{} +{}", DiceText(playerWeapon_->damageDice), GetPlayerAttackProfile().damageBonus));
        }
        for (const auto& worn : characterService_.GetEquippedArmor(*playerId_)) {
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
        if (!playerId_) return;

        if (!characterScreen_.IsOpen()) {
            if (triggered(CharacterScreenAction)) {
                characterScreen_.Open();
                characterScreen_.SetRows(BuildScreenRows());
            }
            return;
        }

        if (triggered(CharacterScreenAction) || triggered(MenuBackAction)) {
            characterScreen_.Close();
            return;
        }

        characterScreen_.SetRows(BuildScreenRows());
        if (triggered(MenuUpAction)) characterScreen_.Step(-1);
        if (triggered(MenuDownAction)) characterScreen_.Step(+1);
        if (!triggered(MenuConfirmAction)) return;

        const auto* row = characterScreen_.Selected();
        if (!row || !row->enabled) return;
        const auto key = DescribeItem(*content_, row->item).second;
        switch (row->action) {
        case ScreenAction::SpendPoint: characterService_.SpendAttributePoint(*playerId_, row->attribute); break;
        case ScreenAction::Equip: EquipItem(key); break;
        case ScreenAction::Use: UseItem(key); break;
        case ScreenAction::Unequip: UnequipArmorSlot(row->slot); break;
        case ScreenAction::None: break;
        }
        characterScreen_.SetRows(BuildScreenRows());
    }

    void GameEngine::DrawCharacterScreen() {
        if (!characterScreen_.IsOpen()) return;

        std::int32_t width = 0;
        std::int32_t height = 0;
        renderingSystem_.GetScreenSize(width, height);
        const std::int32_t panelWidth = std::min(width - 40, 620);
        const std::int32_t left = (width - panelWidth) / 2;
        const std::int32_t top = 30;
        const auto& rows = characterScreen_.GetRows();
        constexpr std::int32_t lineHeight = 22;
        const std::int32_t panelHeight = std::min(height - 60, static_cast<std::int32_t>(rows.size()) * lineHeight + 60);
        renderingSystem_.DrawRectangle({ left, top, panelWidth, panelHeight }, Display(0.06f, 0.05f, 0.06f, 0.92f), 30);

        const auto text = [&](std::string line, std::int32_t x, std::int32_t y, float scale, Color color) {
            renderingSystem_.DrawText(Systems::Rendering::TextRenderData{ std::move(line), LB::Position{ x, y }, {}, color, scale, 32, false });
        };
        text("CHARACTER", left + 16, top + 12, 2.6f, Display(0.95f, 0.75f, 0.30f));
        text("Enter/A: choose   Esc/B: close", left + panelWidth - 250, top + 16, 1.6f, Display(0.6f, 0.6f, 0.6f));

        // Keep the cursor's row in view when the list is taller than the panel
        const auto visible = static_cast<std::size_t>(std::max(1, (panelHeight - 60) / lineHeight));
        const auto first = characterScreen_.GetCursor() >= visible ? characterScreen_.GetCursor() - visible + 1 : 0;
        for (std::size_t i = first; i < rows.size() && i < first + visible; ++i) {
            const auto& row = rows[i];
            const std::int32_t y = top + 50 + static_cast<std::int32_t>(i - first) * lineHeight;
            const bool selected = i == characterScreen_.GetCursor() && row.IsSelectable();
            if (selected) {
                renderingSystem_.DrawRectangle({ left + 8, y - 4, panelWidth - 16, lineHeight }, Display(0.30f, 0.22f, 0.10f, 0.9f), 31);
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
        const auto model = [this](std::string_view file) {
            const auto it = characterModels_.find(std::string(file));
            return it != characterModels_.end() ? it->second : Systems::Rendering::InvalidModelId;
        };
        if (item.IsGold()) return model(item.quantity < 5 ? "coinsSmall.glb" : item.quantity < 12 ? "coinsMedium.glb" : "coinsLarge.glb");
        switch (item.item.kind) {
        case ItemKind::Consumable: return model(content_->FindConsumable(item.item.id)->model);
        case ItemKind::Armor:
            return model(content_->FindArmor(item.item.id)->category == LB::Combat::ArmorCategory::Shield ? "shield_common.glb" : "lootSackA.glb");
        case ItemKind::Weapon: {
            switch (content_->FindWeapon(item.item.id)->category) {
            case LB::Combat::WeaponType::Sword: return models_.playerWeapon;
            case LB::Combat::WeaponType::Axe: return model("axe_common.glb");
            case LB::Combat::WeaponType::Dagger: return model("dagger_common.glb");
            case LB::Combat::WeaponType::Mace: return model("hammer_common.glb");
            case LB::Combat::WeaponType::Staff: return model("staff_common.glb");
            case LB::Combat::WeaponType::Crossbow: return model("crossbow_common.glb");
            default: return model("lootSackA.glb");
            }
        }
        case ItemKind::None: break;
        }
        return model("lootSackA.glb");
    }

    // The model of the equipped weapon's category, held in the knight's hand
    Systems::Rendering::ModelId GameEngine::HeldWeaponModel() const {
        if (!playerWeapon_) return models_.playerWeapon;
        const auto model = ItemModel(GroundItem{ 0, { ItemKind::Weapon, playerWeapon_->id } });
        return model == characterModels_.at("lootSackA.glb") ? models_.playerWeapon : model;
    }

    // Loot lies on the floor turning slowly, weapons laid flat, so it catches the eye
    void GameEngine::DrawGroundItems(const Color& tint) {
        for (const auto& item : groundItems_) {
            const float spin = item.ageSeconds * 1.2f;
            if (!models_.loaded) {
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
            if (item.item.kind == ItemKind::Weapon) {
                // Blades point up in the props: lay them on their side, a little above the floor
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
        const auto& character = characterService_.GetCharacter(*playerId_)->get();
        const auto& a = character.GetAttributes();
        data["player.level"] = std::format("{}", character.GetLevel());
        data["player.experience"] = std::format("{}", character.GetExperience());
        data["player.attributes"] = std::format("{};{};{};{};{};{}", a.strength, a.dexterity, a.constitution, a.intelligence, a.wisdom, a.charisma);
        data["player.points"] = std::format("{}", character.GetUnspentAttributePoints());
        data["player.gold"] = std::format("{}", gold_);
        data["player.weapon"] = playerWeapon_ ? playerWeapon_->key : std::string{};

        std::string armour;
        for (const auto& worn : characterService_.GetEquippedArmor(*playerId_)) {
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
        if (!characterService_.RestoreProgress(*playerId_, level, experience,
                LB::Attributes{ attributes[0], attributes[1], attributes[2], attributes[3], attributes[4], attributes[5] }, points)) {
            return;
        }

        if (const auto* text = field("player.gold"); text && ParseInt(*text, gold)) gold_ = std::max(0, gold);
        if (const auto* text = field("player.weapon"); text && content_->FindWeapon(*text)) playerWeapon_ = content_->FindWeapon(*text);

        for (const auto& worn : characterService_.GetEquippedArmor(*playerId_)) characterService_.UnequipArmor(*playerId_, worn.slot);
        if (const auto* text = field("player.armour")) {
            for (const auto& key : Split(*text, ';')) EquipPlayerArmor(key);
        }

        inventoryService_.ClearInventory(*playerId_);
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
