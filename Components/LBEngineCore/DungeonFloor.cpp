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
            params.puzzleWeight = depth >= 2 ? 2 : 0;   // candle puzzles from the second floor
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
        session_->allies.PlaceAround(session_->world, session_->playerPosition, true);   // the companions come along (on their feet)
        session_->stats.deepest = std::max(session_->stats.deepest, depth);
        if (session_->playing && depth > legacy_.deepest) {
            auto next = legacy_;
            next.deepest = depth;
            UpdateLegacy(next);
        }
        // Gates, secret walls, and puzzles first (traps keep off their cells); then the tier's look,
        // props in the rooms, traps in the corridors (the same for the same floor)
        session_->features.Build(session_->world, depth, FloorSeed(session_->baseSeed, depth) ^ 0xFEA7u, session_->restFloor);
        // A treasure room's gold lies about its chest
        if (!session_->restFloor) {
            for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
                if (layout.rooms[i].type != RoomType::Treasure) continue;
                const auto center = session_->world.RoomCenter(static_cast<std::int32_t>(i));
                const auto piles = session_->features.IsGuarded(static_cast<std::int32_t>(i)) ? 4 : 2;
                for (std::int32_t pile = 0; pile < piles; ++pile) {
                    const float angle = 0.7f + static_cast<float>(pile) * 1.9f;
                    GroundItem gold{};
                    gold.id = session_->nextGroundItemId++;
                    gold.quantity = session_->lootRoller.RollDice(LB::Dice{ 2, 6, 2 + depth });
                    gold.position = { center.x + std::cos(angle) * 1.4f, center.y + std::sin(angle) * 1.4f };
                    if (session_->world.CircleFits(gold.position, 0.2f)) session_->groundItems.push_back(gold);
                }
            }
        }
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
        AutoSave();   // each floor reached is kept
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
            const auto& roster = RosterForDepth(session_->depth);
            const auto* light = content_->FindEnemy(roster.light);
            const auto* heavy = content_->FindEnemy(roster.heavy);
            const auto* ranged = content_->FindEnemy(roster.ranged);
            const auto* caster = content_->FindEnemy(roster.caster);
            const auto* brute = content_->FindEnemy(roster.brute);
            const auto* boss = content_->FindEnemy(BossForDepth(session_->depth));
            const auto raise = [&](const Content::EnemyDefinition* kind, std::int32_t count) {
                if (!kind || count <= 0) return;
                for (const auto id : session_->encounter.SpawnInRoom(*kind, count, here, session_->playerPosition, session_->depth)) room.enemies.push_back(id);
            };
            switch (type) {
            case RoomType::Combat:
                // A room grows by tiers (each tier's inhabitants are stronger already): two light
                // ones in the first, three in the second, four after; a second heavy one and a
                // second shooter from the sixth floor
                raise(light, std::min(2 + (session_->depth - 1) / BossFloorInterval, 4));
                raise(heavy, session_->depth >= 2 ? std::min(1 + session_->depth / 6, 2) : 0);
                raise(ranged, session_->depth >= 2 ? std::min(1 + session_->depth / 6, 2) : 0);   // shooters from the second floor
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
        std::vector<std::pair<std::uint32_t, WorldPoint>> allies;
        for (const auto& ally : session_->allies.Allies()) {
            if (!ally.IsDown()) allies.emplace_back(ally.id, ally.position);
        }
        const auto step = session_->decor.UpdateTraps(session_->gameTimeSeconds, session_->playerPosition, PlayerRadius, IsPlayerAlive(), allies);
        for (const auto at : step.rose) PlaySound(GameSound::Spikes, at);
        for (const auto id : step.othersStruck) {
            if (const auto* ally = session_->allies.Find(id)) {
                const auto raw = session_->lootRoller.RollDice(LB::Dice{ 1, 6, session_->depth / 2 });
                session_->allies.Damage(id, Services::Combat::CombatService::ResolveSpellHit(raw, LB::Combat::DamageType::Piercing, ally->Defense()).finalDamage);
            }
        }
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
            session_->allies.RestoreAll();
            session_->characterService.RestCharacter(*session_->playerId);
            session_->playerStatuses.Clear();
            session_->combatText.Add("BLESSED", { session_->playerPosition.x, PlayerTop(), session_->playerPosition.y }, { 0.95f, 0.85f, 0.45f, 1.0f }, 3.0f);
            EmitLevelUpEffect();
            PlaySound(GameSound::LevelUp, *session_->shrine);
        }
        const auto& layout = session_->world.Layout();
        UpdateFeatures();
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            const auto type = layout.rooms[i].type;
            const auto room = static_cast<std::int32_t>(i);
            if ((type != RoomType::Treasure && type != RoomType::Puzzle) || session_->rooms[i].chestOpened || !session_->features.ChestReady(room)) continue;
            const auto chest = session_->world.RoomCenter(room);
            if (Encounter::Distance(chest, session_->playerPosition) <= InteractRadius) {
                session_->rooms[i].chestOpened = true;
                session_->rooms[i].chestOpenedAt = session_->gameTimeSeconds;
                EmitChestEffect(chest);
                PlaySound(GameSound::ChestOpen, chest);
                // A greater chest behind a gate or a puzzle: more finds, and rarer
                if (session_->features.IsGuarded(room)) DropLoot(ChestLootTable, chest, GreaterChestRolls, GreaterChestDepthBonus);
                else DropLoot(ChestLootTable, chest);
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
            const auto type = layout.rooms[i].type;
            if (type != RoomType::Treasure && type != RoomType::Puzzle) continue;
            const bool greater = session_->features.IsGuarded(static_cast<std::int32_t>(i));
            const auto at = session_->world.RoomCenter(static_cast<std::int32_t>(i));
            // A greater chest keeps its shape: its lid (a part of its own, hinged at the back)
            // swings open, easing out
            if (greater && session_->rooms[i].chestOpened && assets_.models.loaded) {
                const auto& room = session_->rooms[i];
                const float t = room.chestOpenedAt <= 0.0 ? 1.0f
                    : std::clamp(static_cast<float>(session_->gameTimeSeconds - room.chestOpenedAt) / LidSwingSeconds, 0.0f, 1.0f);
                const float eased = 1.0f - (1.0f - t) * (1.0f - t);
                Systems::Rendering::ModelRenderData chest{ ModelOf(GreaterChestModel), { at.x, 0.0f, at.y }, 0.0f, ModelScale, tint };
                if (const auto* model = renderingSystem_.GetModel(chest.model)) {
                    const auto lid = std::ranges::find(model->parts, GreaterChestLid, &Systems::Rendering::ModelPart::name);
                    if (lid != model->parts.end()) {
                        chest.partPoses.push_back({ static_cast<std::uint32_t>(lid - model->parts.begin()), Systems::Rendering::Matrix4::RotationX(-LidOpenRadians * eased) });
                    }
                }
                renderingSystem_.DrawModel(chest);
                continue;
            }
            draw(session_->rooms[i].chestOpened ? OpenChestModel : greater ? GreaterChestModel : ChestModel, at, { 0.35f, 0.20f, 0.08f, 1.0f });
        }
        DrawFeatures(tint);
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
        // Gates opened, the key's fall, the candles' states ("lit bits" per puzzle)
        std::vector<std::int32_t> gates;
        for (std::size_t i = 0; i < session_->features.Gates().size(); ++i) {
            if (session_->features.Gates()[i].open) gates.push_back(static_cast<std::int32_t>(i));
        }
        data["floor.gates"] = JoinIndices(gates);
        data["floor.key"] = session_->features.KeyDropped() ? "1" : "0";
        std::vector<std::int32_t> candles;
        for (const auto& puzzle : session_->features.Puzzles()) {
            std::int32_t bits = 0;
            for (std::size_t c = 0; c < puzzle.candles.size(); ++c) bits |= puzzle.candles[c].lit ? (1 << c) : 0;
            candles.push_back(bits);
        }
        data["floor.candles"] = JoinIndices(candles);
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
        for (const auto index : ParseIndices(field("floor.gates").value_or(""))) {
            if (index >= 0) session_->features.Open(static_cast<std::size_t>(index), session_->world);
        }
        session_->features.SetKeyDropped(field("floor.key").value_or("0") == "1");
        const auto candles = ParseIndices(field("floor.candles").value_or(""));
        for (std::size_t p = 0; p < candles.size() && p < session_->features.Puzzles().size(); ++p) {
            auto& puzzle = session_->features.Puzzles()[p];
            for (std::size_t c = 0; c < puzzle.candles.size(); ++c) puzzle.candles[c].lit = (candles[p] >> c) & 1;
            puzzle.solved = std::ranges::all_of(puzzle.candles, &PuzzleCandle::lit);
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
            if ((layout.rooms[i].type == RoomType::Treasure || layout.rooms[i].type == RoomType::Puzzle) && !session_->rooms[i].chestOpened) {
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
        // Companions to hire (one at a time: AllyParty::MaxAllies)
        for (const auto& ally : content_->GetAllies()) {
            session_->stock.push_back(StockEntry{ ItemRef{}, 1, ally.price, false, &ally });
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
        // Rarer and deeper is dearer: the rarity's multiplier, and a tenth more for each item level
        const float multiplier = (rolled && rolled->rarity ? rolled->rarity->priceMultiplier : 1.0f)
                               * (rolled ? 1.0f + 0.1f * static_cast<float>(rolled->level - 1) : 1.0f);
        return std::max(1, static_cast<std::int32_t>(std::lround(static_cast<float>(value) * multiplier)));
    }

    bool GameEngine::BuyFromMerchant(std::size_t index) {
        if (!NearMerchant() || index >= session_->stock.size()) return false;
        const auto entry = session_->stock[index];
        if (session_->gold < entry.price) return false;
        if (entry.ally) {
            if (!HireAlly(entry.ally->key)) return false;   // the party is full
            session_->gold -= entry.price;
            session_->stock.erase(session_->stock.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }
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
            if (entry.ally) {
                const bool full = session_->allies.Allies().size() >= AllyParty::MaxAllies;
                ScreenRow row{ std::format("Hire: {}", entry.ally->name), full ? std::string("party full") : std::format("{} gold", entry.price) };
                row.action = ScreenAction::Buy;
                row.index = i;
                row.color = Systems::Rendering::Color{ 0.55f, 0.95f, 0.55f, 1.0f };
                row.enabled = !full && session_->gold >= entry.price;
                rows.push_back(std::move(row));
                continue;
            }
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
        const auto view = GetViewArea();
        for (const auto& prop : session_->decor.Props()) {
            if (!view.Contains(prop.position, 2.0f)) continue;
            const auto model = ModelOf(prop.model);
            if (model == Systems::Rendering::InvalidModelId) continue;
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ model, { prop.position.x, 0.0f, prop.position.y }, prop.yaw, ModelScale, tint });
        }
        for (const auto& trap : session_->decor.Traps()) {
            if (!view.Contains(trap.center, WorldMap::CellTiles)) continue;
            const float raise = FloorDecor::Raise(trap, session_->gameTimeSeconds);
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(FloorDecor::GrateModel), { trap.center.x, -0.03f, trap.center.y }, 0.0f, ModelScale, tint });
            // The spikes stand a tile tall: sunk below the floor, they come up through the grate
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(FloorDecor::SpikesModel), { trap.center.x, -1.0f + 0.65f * raise, trap.center.y }, 0.0f, ModelScale, tint });
        }
    }

    // ------------------------------------------------------------------------------------------
    // Gates, secret walls, candle puzzles (design-dungeon-and-world.md 4.3, 6.3, 8.3)
    // ------------------------------------------------------------------------------------------

    // The key falls in its room once that room is clear; a locked gate opens for a key (one is
    // spent); a secret wall breaks under a blow; Interact by a candle touches it
    void GameEngine::UpdateFeatures() {
        auto& features = session_->features;
        const auto& layout = session_->world.Layout();
        features.Update(1.0f / 60.0f);
        for (const auto& gate : features.Gates()) {   // a sinking gate grinds up dust
            if (gate.kind == GateKind::Locked && gate.open && gate.openSeconds < GateSinkSeconds) {
                session_->effects.Stream(ParticleEffect::RiseDust, { gate.center.x, 0.03f, gate.center.y }, 40.0f, 1.0f / 60.0f);
            }
        }
        const auto keyRoom = features.KeyRoom();
        if (keyRoom >= 0 && !features.KeyDropped() && session_->rooms[static_cast<std::size_t>(keyRoom)].cleared) {
            features.SetKeyDropped(true);
            GroundItem key{};
            key.id = session_->nextGroundItemId++;
            key.item = ItemRef::FromKey(*content_, KeyItem);
            key.position = session_->world.RoomCenter(keyRoom);
            if (key.item.IsValid()) {
                session_->groundItems.push_back(key);
                session_->combatText.Add("A KEY!", { key.position.x, 1.0f, key.position.y }, { 0.95f, 0.85f, 0.45f, 1.0f }, 2.5f);
            }
        }

        const auto hero = session_->playerPosition;
        for (std::size_t i = 0; i < features.Gates().size(); ++i) {
            auto& gate = features.Gates()[i];
            if (gate.open) continue;
            const float distance = Encounter::Distance(gate.center, hero);
            if (distance > GateReach * 2.0f) gate.warned = false;
            if (gate.kind == GateKind::Locked && distance <= GateReach) {
                if (CountItem(KeyItem) > 0) {
                    session_->inventoryService.RemoveItem(*session_->playerId, ItemRef::FromKey(*content_, KeyItem).Pack(), 1);
                    features.Open(i, session_->world);
                    session_->combatText.Add("UNLOCKED", { gate.center.x, 1.4f, gate.center.y }, { 0.95f, 0.85f, 0.45f, 1.0f }, 2.5f);
                    PlaySound(GameSound::GateOpens, gate.center);
                }
                else if (!gate.warned) {
                    gate.warned = true;
                    session_->combatText.Add("LOCKED - FIND THE KEY", { gate.center.x, 1.4f, gate.center.y }, { 0.85f, 0.85f, 0.85f, 1.0f }, 2.5f);
                    PlaySound(GameSound::NoMana, gate.center);
                }
            }
            // A blow at a cracked wall (a swing, facing it, close enough) breaks it
            if (gate.kind == GateKind::Secret && session_->playerCombat.swinging && !session_->playerCombat.shooting && distance <= PlayerReach + 1.0f) {
                const auto forward = Forward(session_->playerFacingYaw);
                const float facing = ((gate.center.x - hero.x) * forward.x + (gate.center.y - hero.y) * forward.y) / std::max(distance, 0.01f);
                if (facing > 0.5f) {
                    features.Open(i, session_->world);
                    session_->effects.Emit(ParticleEffect::BoneDust, { gate.center.x, 0.6f, gate.center.y }, 60, 1.6f);
                    session_->combatText.Add("A SECRET PASSAGE!", { gate.center.x, 1.4f, gate.center.y }, { 0.95f, 0.85f, 0.45f, 1.0f }, 3.0f);
                    PlaySound(GameSound::WallBreaks, gate.center);
                }
            }
        }

        // Candles: Interact touches the nearest within reach
        if (!inputSystem_.IsActionTriggered(std::string(InteractAction))) return;
        for (std::size_t p = 0; p < features.Puzzles().size(); ++p) {
            const auto& puzzle = features.Puzzles()[p];
            for (std::size_t c = 0; c < puzzle.candles.size(); ++c) {
                if (Encounter::Distance(puzzle.candles[c].position, hero) <= CandleReach) {
                    TouchCandle(p, c);
                    return;
                }
            }
        }
        (void)layout;
    }

    void GameEngine::TouchCandle(std::size_t puzzleIndex, std::size_t candle) {
        auto& puzzles = session_->features.Puzzles();
        if (puzzleIndex >= puzzles.size() || candle >= puzzles[puzzleIndex].candles.size()) return;
        auto& puzzle = puzzles[puzzleIndex];
        if (puzzle.solved) return;
        puzzle.Touch(candle);
        const auto at = puzzle.candles[candle].position;
        PlaySound(GameSound::CandleTouched, at);
        if (!puzzle.solved) return;
        const auto chest = session_->world.RoomCenter(puzzle.room);
        session_->combatText.Add("THE CANDLES BURN AS ONE", { chest.x, 1.2f, chest.y }, { 0.95f, 0.85f, 0.45f, 1.0f }, 3.0f);
        EmitChestEffect(chest);
        PlaySound(GameSound::LevelUp, chest);
    }

    // Closed gates across their corridors (an opened one sinks into the floor); the candles, the
    // lit ones with their flames and light
    void GameEngine::DrawFeatures(const Systems::Rendering::Color& tint) {
        if (!assets_.models.loaded) return;
        const auto view = GetViewArea();
        for (const auto& gate : session_->features.Gates()) {
            if (!view.Contains(gate.center, WorldMap::CellTiles)) continue;
            const auto model = ModelOf(gate.kind == GateKind::Locked ? GateModel : SecretWallModel);
            if (gate.kind == GateKind::Secret) {
                // Broken: it shudders and falls in on itself, faster and faster, in a cloud of dust
                const float t = gate.open ? gate.openSeconds / WallCrumbleSeconds : 0.0f;
                if (t >= 1.0f) continue;
                const float shudder = gate.open ? 0.06f * (1.0f - t) * std::sin(gate.openSeconds * 70.0f) : 0.0f;
                renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ model,
                    { gate.center.x + shudder, -2.2f * t * t, gate.center.y + shudder }, gate.yaw, ModelScale, tint });
                continue;
            }
            // A gate sinks into the floor, slowing as it goes, grinding up dust
            const float t = gate.open ? std::min(1.0f, gate.openSeconds / GateSinkSeconds) : 0.0f;
            if (t >= 1.0f) continue;
            const float sink = 1.0f - (1.0f - t) * (1.0f - t);
            renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ model, { gate.center.x, -2.0f * sink, gate.center.y }, gate.yaw, ModelScale, tint });
        }
        for (const auto& puzzle : session_->features.Puzzles()) {
            for (const auto& candle : puzzle.candles) {
                if (!view.Contains(candle.position, 1.0f)) continue;
                renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ ModelOf(candle.lit ? LitCandleModel : CandleModel),
                    { candle.position.x, 0.0f, candle.position.y }, 0.0f, ModelScale * 1.4f, tint });
                if (!candle.lit) continue;
                const Systems::Rendering::Vector3 flame{ candle.position.x, 0.78f, candle.position.y };
                renderingSystem_.DrawParticle(Systems::Rendering::ParticleRenderData{ flame, 0.18f, { 1.0f, 0.65f, 0.25f, 2.2f }, true });
                Systems::Rendering::PointLight light{};
                light.position = flame;
                light.color = { 1.0f, 0.6f, 0.25f, 1.0f };
                light.radius = 3.0f;
                light.intensity = 0.8f;
                renderingSystem_.DrawLight(light);
            }
        }
    }

} // namespace LB::Engine
