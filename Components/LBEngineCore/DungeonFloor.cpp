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
        if (session_->playerId) {
            session_->playerPosition = session_->world.RoomCenter(layout.entranceRoom);
            session_->characterService.PlaceCharacter(*session_->playerId, ToTile(session_->playerPosition));
            session_->camera.SnapTo({ session_->playerPosition.x, session_->playerPosition.y });
        }
        if (assets_.models.loaded) {
            session_->walls.Build(session_->world, renderingSystem_, assets_.models.wall, assets_.models.corner, ModelScale);
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
            const auto raise = [&](const Content::EnemyDefinition* kind, std::int32_t count) {
                if (!kind || count <= 0) return;
                for (const auto id : session_->encounter.SpawnInRoom(*kind, count, here, session_->playerPosition, session_->depth)) room.enemies.push_back(id);
            };
            switch (type) {
            case RoomType::Combat:
                raise(light, std::min(2 + session_->depth / 2, 4));
                raise(heavy, session_->depth >= 2 ? std::min(1 + session_->depth / 4, 2) : 0);
                raise(ranged, session_->depth >= 2 ? std::min(1 + session_->depth / 4, 2) : 0);   // shooters from the second floor
                raise(caster, session_->depth >= 3 ? 1 : 0);                                       // casters from the third
                break;
            case RoomType::Elite:
                raise(heavy, 2);
                raise(light, 1 + session_->depth / 3);
                raise(ranged, 1);
                raise(caster, session_->depth >= 2 ? 1 : 0);
                break;
            case RoomType::Exit:
                raise(light, 2);
                raise(heavy, 1);
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
    void GameEngine::UpdateFloorInteractions() {
        if (!IsPlayerAlive()) return;
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

} // namespace LB::Engine
