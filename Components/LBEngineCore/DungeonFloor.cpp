// DungeonFloor.cpp - LB.Engine implementation unit: generated dungeon floors (GameEngine members
// declared in GameEngine.ixx). A floor is a DungeonService layout placed in the world; walking into
// a room raises its enemies, treasure rooms hold a chest, and the exit room's trapdoor leads down
// once the room is cleared.
module LB.Engine;

import std;
import LB.Core;
import LB.Content;
import LB.Dungeon;
import LB.DungeonService;
import LB.CharacterService;
import LB.PersistenceSystem;
import LB.RenderingSystem;

namespace LB::Engine {

    namespace {
        using LB::Dungeon::RoomType;

        // Floors grow with depth: a longer critical path, more side rooms
        Dungeon::FloorGenerationParams ParamsFor(std::int32_t depth) {
            Dungeon::FloorGenerationParams params{};
            params.criticalPathRooms = std::min(4 + depth, 9);
            params.branchChance = 0.5f;
            params.maxBranchLength = 2;
            return params;
        }

        // Each floor of a run has its own layout, the same for the same run seed
        std::uint32_t FloorSeed(std::uint32_t runSeed, std::int32_t depth) {
            std::uint64_t mixed = (static_cast<std::uint64_t>(runSeed) << 32) ^ static_cast<std::uint64_t>(depth) * 0x9E3779B97F4A7C15ull;
            mixed ^= mixed >> 29;
            mixed *= 0xBF58476D1CE4E5B9ull;
            mixed ^= mixed >> 32;
            return static_cast<std::uint32_t>(mixed);
        }

        // Rooms without a fight are cleared as soon as the player walks in
        bool HasFight(RoomType type) {
            return type == RoomType::Combat || type == RoomType::Elite || type == RoomType::Exit;
        }

        std::string JoinIndices(const std::vector<std::int32_t>& indices) {
            std::string text;
            for (const auto index : indices) text += std::format("{};", index);
            return text;
        }

        std::vector<std::int32_t> ParseIndices(const std::string& text) {
            std::vector<std::int32_t> indices;
            std::int32_t value = 0;
            std::size_t start = 0;
            while (start < text.size()) {
                const auto end = text.find(';', start);
                const auto part = std::string_view(text).substr(start, end == std::string::npos ? std::string::npos : end - start);
                const auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), value);
                if (ec == std::errc{} && ptr == part.data() + part.size()) indices.push_back(value);
                if (end == std::string::npos) break;
                start = end + 1;
            }
            return indices;
        }
    }

    void GameEngine::StartFloor(std::int32_t depth) {
        session_->depth = depth;
        session_->world = WorldMap(dungeonService_.GenerateFloor(ParamsFor(depth), FloorSeed(session_->baseSeed, depth)), {});
        const auto& layout = session_->world.Layout();

        session_->rooms.assign(layout.rooms.size(), {});
        auto& entrance = session_->rooms[static_cast<std::size_t>(layout.entranceRoom)];
        entrance.triggered = true;
        entrance.cleared = true;

        session_->encounter.Reset();
        session_->groundItems.clear();

        // A resting place: every room is quiet; the merchant and the shrine stand in the entrance
        session_->restFloor = IsRestDepth(depth);
        session_->merchant.reset();
        session_->shrine.reset();
        session_->shrineUsed = false;
        session_->stock.clear();
        session_->trade.Close();
        if (session_->restFloor) {
            for (auto& room : session_->rooms) {
                room.triggered = true;
                room.cleared = true;
            }
            const auto center = session_->world.RoomCenter(layout.entranceRoom);
            const auto spot = [&](float dx) {
                const WorldPoint at{ center.x + dx, center.y - 1.2f };
                return session_->world.CircleFits(at, 0.5f) ? at : WorldPoint{ center.x + dx, center.y };
            };
            session_->merchant = spot(1.6f);
            session_->shrine = spot(-1.6f);
            StockMerchant();
        }
        if (session_->playerId) {
            session_->playerPosition = session_->world.RoomCenter(layout.entranceRoom);
            session_->characterService.PlaceCharacter(*session_->playerId, ToTile(session_->playerPosition));
            session_->camera.SnapTo({ session_->playerPosition.x, session_->playerPosition.y });
        }
        // The tier's look; props in the rooms, traps in the corridors (the same for the same floor)
        session_->decor.Build(session_->world, depth, FloorSeed(session_->baseSeed, depth) ^ 0x5EEDu, session_->restFloor);
        if (assets_.models.loaded) {
            const auto& theme = ThemeForDepth(depth);
            session_->walls.Build(session_->world, renderingSystem_, ModelOf(theme.wall) != Systems::Rendering::InvalidModelId ? ModelOf(theme.wall) : assets_.models.wall,
                                  assets_.models.corner, ModelScale);
        }
        session_->torches.Build(session_->world, session_->walls);
        session_->effects.Clear();
        session_->minimap.Reset(session_->world);
        session_->minimap.Reveal(session_->world, session_->playerPosition);
    }

    void GameEngine::Descend() {
        if (!session_->dungeonMode || !session_->playerId) return;
        PlaySound(GameSound::Descend);
        StartFloor(session_->depth + 1);
        session_->characterService.RestCharacter(*session_->playerId);   // a breather on the stairs down
        session_->combatText.Clear();
    }

    // Walking into a room for the first time raises its enemies (scaled by depth); a room is
    // cleared once all of them are dead
    void GameEngine::UpdateRooms() {
        const auto& layout = session_->world.Layout();
        const auto here = session_->world.RoomAt(session_->playerPosition);
        if (here >= 0 && IsPlayerAlive() && !session_->rooms[static_cast<std::size_t>(here)].triggered) {
            auto& room = session_->rooms[static_cast<std::size_t>(here)];
            room.triggered = true;
            const auto type = layout.rooms[static_cast<std::size_t>(here)].type;
            const auto* light = content_->FindEnemy(LightEnemy);
            const auto* heavy = content_->FindEnemy(HeavyEnemy);
            const auto* ranged = content_->FindEnemy(RangedEnemy);
            const auto* caster = content_->FindEnemy(CasterEnemy);
            const auto* brute = content_->FindEnemy(BruteEnemy);
            const auto* boss = content_->FindEnemy(BossForDepth(session_->depth));
            const auto raise = [&](const Content::EnemyDefinition* kind, std::int32_t count) {
                if (!kind || count <= 0) return;
                for (const auto id : session_->encounter.SpawnInRoom(*kind, count, here, session_->playerPosition, session_->depth)) room.enemies.push_back(id);
            };
            switch (type) {
            case RoomType::Combat:
                raise(light, std::min(2 + (session_->depth - 1) / 2, 4));
                raise(heavy, session_->depth >= 2 ? std::min(1 + session_->depth / 4, 2) : 0);
                raise(ranged, session_->depth >= 2 ? std::min(1 + session_->depth / 4, 2) : 0);   // shooters from the second floor
                raise(caster, session_->depth >= 3 ? 1 : 0);                                       // casters from the third
                raise(brute, session_->depth >= 5 ? 1 : 0);
                break;
            case RoomType::Elite:
                raise(heavy, session_->depth >= 2 ? 2 : 1);
                raise(light, 1 + session_->depth / 3);
                raise(ranged, 1);
                raise(caster, session_->depth >= 3 ? 1 : 0);
                raise(brute, session_->depth >= 3 ? 1 : 0);
                break;
            case RoomType::Exit:
                // The end of a tier: the boss guards the way down (it opens when the room is clear)
                if (session_->depth % BossFloorInterval == 0 && boss) {
                    raise(boss, 1);
                    raise(light, 2);
                }
                else {
                    raise(light, 2);
                    raise(heavy, 1);
                }
                break;
            default:
                break;
            }
            if (!HasFight(type) || room.enemies.empty()) room.cleared = true;
        }

        for (auto& room : session_->rooms) {
            if (!room.triggered || room.cleared) continue;
            room.cleared = std::ranges::all_of(room.enemies, [this](std::uint32_t id) {
                const auto* enemy = session_->encounter.FindEnemy(id);
                return !enemy || enemy->IsDead();
            });
        }
    }

    // Chests open when the player steps up to them; the trapdoor of a cleared exit room leads down
    // Spikes rising (heard nearby) and striking the player on their cell: 1d6 + half the depth,
    // piercing, through armour
    void GameEngine::UpdateTraps() {
        const auto step = session_->decor.UpdateTraps(session_->gameTimeSeconds, session_->playerPosition, PlayerRadius, IsPlayerAlive());
        for (const auto at : step.rose) PlaySound(GameSound::Spikes, at);
        if (!step.playerStruck) return;
        const auto damage = session_->lootRoller.RollDice(LB::Dice{ 1, 6, session_->depth / 2 });
        CombatEvent event{ CombatEventKind::EnemyAttack, 0, Services::Combat::CombatService::ResolveSpellHit(damage, LB::Combat::DamageType::Piercing, GetPlayerDefense()), false };
        session_->characterService.DamageCharacter(*session_->playerId, event.result.finalDamage);
        session_->playerCombat.hitFlashSeconds = Encounter::HitFlashSeconds;
        session_->combatEvents.push_back(event);
        if (!IsPlayerAlive()) {
            session_->playerCombat.swinging = false;
            session_->playerCombat.deadSeconds = 0.0f;
            CancelCast();
        }
    }

    void GameEngine::UpdateFloorInteractions() {
        if (!IsPlayerAlive()) return;
        UpdateTraps();

        // The shrine restores health and mana and lifts every status, once a floor
        if (session_->shrine && !session_->shrineUsed && Encounter::Distance(*session_->shrine, session_->playerPosition) <= InteractRadius) {
            session_->shrineUsed = true;
            session_->characterService.RestCharacter(*session_->playerId);
            session_->playerStatuses.Clear();
            session_->combatText.Add("BLESSED", { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.95f, 0.85f, 0.45f, 1.0f }, 3.0f);
            EmitLevelUpEffect();
            PlaySound(GameSound::LevelUp, *session_->shrine);
        }
        const auto& layout = session_->world.Layout();
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            if (layout.rooms[i].type != RoomType::Treasure || session_->rooms[i].chestOpened) continue;
            const auto chest = session_->world.RoomCenter(static_cast<std::int32_t>(i));
            if (Encounter::Distance(chest, session_->playerPosition) <= InteractRadius) {
                session_->rooms[i].chestOpened = true;
                EmitChestEffect(chest);
                PlaySound(GameSound::ChestOpen, chest);
                DropLoot(ChestLootTable, chest);
            }
        }

        const auto exit = static_cast<std::size_t>(layout.exitRoom);
        if (session_->rooms[exit].cleared && Encounter::Distance(session_->world.RoomCenter(layout.exitRoom), session_->playerPosition) <= InteractRadius * 0.7f) {
            Descend();
        }
    }

    void GameEngine::DrawFloorProps(const Systems::Rendering::Color& tint) {
        if (!session_->dungeonMode) return;
        const auto& layout = session_->world.Layout();
        const auto draw = [&](std::string_view file, WorldPoint at, Systems::Rendering::Color color) {
            if (assets_.models.loaded) {
                renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(file), { at.x, 0.0f, at.y }, 0.0f, ModelScale, tint });
                return;
            }
            Systems::Rendering::MeshRenderData mesh{};
            mesh.shape = Systems::Rendering::MeshShape::Box;
            mesh.position = { at.x, 0.2f, at.y };
            mesh.size = { 0.7f, 0.4f, 0.5f };
            mesh.color = color;
            renderingSystem_.DrawMesh(mesh);
        };

        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            if (layout.rooms[i].type != RoomType::Treasure) continue;
            draw(session_->rooms[i].chestOpened ? OpenChestModel : ChestModel, session_->world.RoomCenter(static_cast<std::int32_t>(i)), { 0.35f, 0.20f, 0.08f, 1.0f });
        }
        draw(TrapdoorModel, session_->world.RoomCenter(layout.exitRoom), { 0.10f, 0.08f, 0.06f, 1.0f });
    }

    void GameEngine::WriteFloor(Systems::Persistence::SaveData& data) const {
        std::vector<std::int32_t> cleared;
        std::vector<std::int32_t> chests;
        for (std::size_t i = 0; i < session_->rooms.size(); ++i) {
            if (session_->rooms[i].cleared) cleared.push_back(static_cast<std::int32_t>(i));
            if (session_->rooms[i].chestOpened) chests.push_back(static_cast<std::int32_t>(i));
        }
        data["floor.seed"] = std::format("{}", session_->baseSeed);
        data["floor.depth"] = std::format("{}", session_->depth);
        data["floor.cleared"] = JoinIndices(cleared);
        data["floor.chests"] = JoinIndices(chests);
    }

    // Rebuild the saved floor (same run seed and depth: same layout) and what was done on it;
    // rooms that were not cleared raise their enemies again
    void GameEngine::ReadFloor(const Systems::Persistence::SaveData& data) {
        const auto field = [&](const char* key) -> std::optional<std::string> {
            const auto it = data.find(key);
            if (it == data.end()) return std::nullopt;
            return it->second;
        };
        const auto seedText = field("floor.seed");
        const auto depthText = field("floor.depth");
        std::uint32_t seed = 0;
        std::int32_t depth = 0;
        if (!seedText || !depthText) return;
        if (std::from_chars(seedText->data(), seedText->data() + seedText->size(), seed).ec != std::errc{}) return;
        if (std::from_chars(depthText->data(), depthText->data() + depthText->size(), depth).ec != std::errc{} || depth < 1) return;

        session_->baseSeed = seed;
        StartFloor(depth);
        for (const auto index : ParseIndices(field("floor.cleared").value_or(""))) {
            if (index < 0 || static_cast<std::size_t>(index) >= session_->rooms.size()) continue;
            session_->rooms[static_cast<std::size_t>(index)].triggered = true;
            session_->rooms[static_cast<std::size_t>(index)].cleared = true;
        }
        for (const auto index : ParseIndices(field("floor.chests").value_or(""))) {
            if (index >= 0 && static_cast<std::size_t>(index) < session_->rooms.size()) session_->rooms[static_cast<std::size_t>(index)].chestOpened = true;
        }
    }


    // What the minimap marks: the trapdoor, unopened chests, and enemies in sight
    void GameEngine::DrawMinimap() {
        if (!session_->dungeonMode || !session_->playerId || session_->characterScreen.IsOpen()) return;
        const auto& layout = session_->world.Layout();

        MinimapMarkers markers;
        markers.player = session_->playerPosition;
        markers.exit = session_->world.RoomCenter(layout.exitRoom);
        for (std::size_t i = 0; i < layout.rooms.size() && i < session_->rooms.size(); ++i) {
            if (layout.rooms[i].type == RoomType::Treasure && !session_->rooms[i].chestOpened) {
                markers.chests.push_back(session_->world.RoomCenter(static_cast<std::int32_t>(i)));
            }
        }
        for (const auto& enemy : session_->encounter.GetEnemies()) {
            if (!enemy.IsDead() && session_->world.LineOfSight(session_->playerPosition, enemy.position)) markers.enemies.push_back(enemy.position);
        }
        session_->minimap.Draw(renderingSystem_, session_->world, markers);
    }

    // ------------------------------------------------------------------------------------------
    // The merchant (rest floors)
    // ------------------------------------------------------------------------------------------

    // Potions and ammunition without end, and a few pieces of gear rolled for this depth
    void GameEngine::StockMerchant() {
        session_->stock.clear();
        const auto add = [this](ItemRef item, std::int32_t quantity, bool unlimited) {
            if (!item.IsValid()) return;
            session_->stock.push_back(StockEntry{ item, quantity, PriceOf(item) * quantity, unlimited });
        };
        add(ItemRef::FromKey(*content_, HealingPotion), 1, true);
        for (const auto& consumable : content_->GetConsumables()) {
            if (consumable.IsAmmunition()) add(ItemRef{ ItemKind::Consumable, consumable.id }, MerchantArrowBundle, true);
        }
        if (const auto* table = content_->FindLootTable(MerchantStockTable)) {
            for (std::int32_t i = 0; i < MerchantGearCount; ++i) {
                for (const auto& drop : session_->lootRoller.Roll(*table, *content_)) {
                    if (drop.item.IsValid()) add(RollDrop(drop.item), 1, false);
                }
            }
        }
    }

    std::int32_t GameEngine::PriceOf(ItemRef item) const {
        const auto base = session_->rolls.BaseOf(item);
        std::int32_t value = 0;
        if (base.kind == ItemKind::Weapon) value = content_->FindWeapon(base.id)->value;
        else if (base.kind == ItemKind::Armor) value = content_->FindArmor(base.id)->value;
        else if (base.kind == ItemKind::Consumable) value = content_->FindConsumable(base.id)->value;
        const auto* rolled = session_->rolls.Find(item);
        const float multiplier = rolled && rolled->rarity ? rolled->rarity->priceMultiplier : 1.0f;
        return std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(value) * multiplier)));
    }

    bool GameEngine::BuyFromMerchant(std::size_t index) {
        if (!NearMerchant() || index >= session_->stock.size()) return false;
        const auto entry = session_->stock[index];
        if (session_->gold < entry.price) return false;
        if (GiveItemRef(entry.item, entry.quantity) < entry.quantity) return false;   // the bag is full
        session_->gold -= entry.price;
        if (!entry.unlimited) session_->stock.erase(session_->stock.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool GameEngine::SellToMerchant(ItemRef item) {
        if (!NearMerchant() || !item.IsValid()) return false;
        const auto inBag = std::ranges::any_of(GetInventory(), [&](const Inventory::InventoryItem& stack) { return stack.itemId == item.Pack(); });
        if (!inBag || !session_->inventoryService.RemoveItem(*session_->playerId, item.Pack(), 1).success) return false;
        session_->gold += SellPriceOf(item);
        return true;
    }

    // The merchant's wares to buy, then the bag to sell
    std::vector<ScreenRow> GameEngine::BuildTradeRows() const {
        std::vector<ScreenRow> rows;
        const auto heading = [&](std::string text) { rows.push_back(ScreenRow{ std::move(text), {}, ScreenAction::None, true, true }); };
        heading(std::format("Buy - you have {} gold", session_->gold));
        for (std::size_t i = 0; i < session_->stock.size(); ++i) {
            const auto& entry = session_->stock[i];
            ScreenRow row{ entry.quantity > 1 ? std::format("{} x{}", ItemName(entry.item), entry.quantity) : ItemName(entry.item), std::format("{} gold", entry.price) };
            row.action = ScreenAction::Buy;
            row.index = i;
            row.item = entry.item;
            row.color = RarityColorOf(entry.item);
            row.enabled = session_->gold >= entry.price;
            rows.push_back(std::move(row));
        }
        heading("Sell");
        const auto bag = GetInventory();
        if (bag.empty()) rows.push_back(ScreenRow{ "Nothing to sell" });
        for (const auto& stack : bag) {
            const auto item = ItemRef::Unpack(stack.itemId);
            ScreenRow row{ stack.quantity > 1 ? std::format("{} x{}", stack.name, stack.quantity) : stack.name, std::format("{} gold", SellPriceOf(item)) };
            row.action = ScreenAction::Sell;
            row.item = item;
            row.color = RarityColorOf(item);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    // Interact by the merchant opens the trade; up and down move, confirm buys or sells, back closes
    void GameEngine::UpdateTradeScreen() {
        const auto triggered = [this](std::string_view action) { return inputSystem_.IsActionTriggered(std::string(action)); };
        auto& trade = session_->trade;
        if (!trade.IsOpen()) {
            if (!session_->characterScreen.IsOpen() && NearMerchant() && triggered(InteractAction)) {
                trade.Open();
                trade.SetRows(BuildTradeRows());
                PlaySound(GameSound::ScreenOpen);
            }
            return;
        }
        if (triggered(InteractAction) || triggered(MenuBackAction) || triggered(CharacterScreenAction) || !NearMerchant()) {
            trade.Close();
            PlaySound(GameSound::ScreenClose);
            return;
        }
        trade.SetRows(BuildTradeRows());
        if (triggered(MenuUpAction)) trade.Step(-1);
        if (triggered(MenuDownAction)) trade.Step(+1);
        if (triggered(MenuConfirmAction)) {
            if (const auto* row = trade.Selected(); row && row->enabled) {
                const bool done = row->action == ScreenAction::Buy ? BuyFromMerchant(row->index) : row->action == ScreenAction::Sell && SellToMerchant(row->item);
                PlaySound(done ? GameSound::Gold : GameSound::NoMana);
            }
        }
        trade.SetRows(BuildTradeRows());
    }

    // The merchant (idling, facing the room), the shrine with its candles (lit until used), and
    // their light
    void GameEngine::DrawRestPlace(const Systems::Rendering::Color& tint) {
        if (!assets_.models.loaded) return;
        if (session_->merchant) {
            const auto model = ModelOf(MerchantModel);
            Systems::Rendering::ModelRenderData body{ model, { session_->merchant->x, 0.0f, session_->merchant->y }, 0.0f, PlayerModelScale, tint };
            if (const auto idle = renderingSystem_.FindAnimation("Idle_A")) {
                body.animation.current = { *idle, static_cast<float>(session_->gameTimeSeconds), true };
            }
            renderingSystem_.DrawModel(body);
        }
        if (session_->shrine) {
            const Systems::Rendering::Vector3 at{ session_->shrine->x, 0.0f, session_->shrine->y };
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(ShrineModel), at, 0.0f, ModelScale * 0.6f, tint });
            if (!session_->shrineUsed) {
                renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(CandlesModel), { at.x, 0.0f, at.z + 0.55f }, 0.0f, ModelScale, tint });
                renderingSystem_.DrawLight(Systems::Rendering::PointLight{ { at.x, 1.2f, at.z + 0.4f }, { 1.0f, 0.78f, 0.45f, 1.0f }, 4.5f, 1.3f });
            }
        }
    }

    // The floor's props, and its traps: a grate over the cell, the spikes rising through it
    void GameEngine::DrawDecor(const Systems::Rendering::Color& tint) {
        if (!assets_.models.loaded || !session_->dungeonMode) return;
        for (const auto& prop : session_->decor.Props()) {
            const auto model = ModelOf(prop.model);
            if (model == Systems::Rendering::InvalidModelId) continue;
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ model, { prop.position.x, 0.0f, prop.position.y }, prop.yaw, ModelScale, tint });
        }
        for (const auto& trap : session_->decor.Traps()) {
            const float raise = FloorDecor::Raise(trap, session_->gameTimeSeconds);
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(FloorDecor::GrateModel), { trap.center.x, -0.03f, trap.center.y }, 0.0f, ModelScale, tint });
            // The spikes stand a tile tall: sunk below the floor, they come up through the grate
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(FloorDecor::SpikesModel), { trap.center.x, -1.0f + 0.65f * raise, trap.center.y }, 0.0f, ModelScale, tint });
        }
    }

} // namespace LB::Engine
