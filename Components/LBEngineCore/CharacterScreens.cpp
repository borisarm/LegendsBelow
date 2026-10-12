// CharacterScreens.cpp - LB.Engine implementation unit: the character screen (its rows, input, and
// drawing), dropping items, the path cards, and the banner (GameEngine members declared in
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

} // namespace LB::Engine
