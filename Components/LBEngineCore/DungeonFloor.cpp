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
        depth_ = depth;
        world_ = WorldMap(dungeonService_.GenerateFloor(ParamsFor(depth), FloorSeed(baseSeed_, depth)), {});
        const auto& layout = world_.Layout();

        rooms_.assign(layout.rooms.size(), {});
        auto& entrance = rooms_[static_cast<std::size_t>(layout.entranceRoom)];
        entrance.triggered = true;
        entrance.cleared = true;

        encounter_.Reset();
        groundItems_.clear();
        if (playerId_) {
            playerPosition_ = world_.RoomCenter(layout.entranceRoom);
            characterService_.PlaceCharacter(*playerId_, ToTile(playerPosition_));
            camera_.SnapTo({ playerPosition_.x, playerPosition_.y });
        }
        if (models_.loaded) {
            BuildWalls();
        }
    }

    void GameEngine::Descend() {
        if (!dungeonMode_ || !playerId_) return;
        StartFloor(depth_ + 1);
        characterService_.RestCharacter(*playerId_);   // a breather on the stairs down
        floatingTexts_.clear();
    }

    // Walking into a room for the first time raises its enemies (scaled by depth); a room is
    // cleared once all of them are dead
    void GameEngine::UpdateRooms() {
        const auto& layout = world_.Layout();
        const auto here = world_.RoomAt(playerPosition_);
        if (here >= 0 && IsPlayerAlive() && !rooms_[static_cast<std::size_t>(here)].triggered) {
            auto& room = rooms_[static_cast<std::size_t>(here)];
            room.triggered = true;
            const auto type = layout.rooms[static_cast<std::size_t>(here)].type;
            const auto* light = content_->FindEnemy(LightEnemy);
            const auto* heavy = content_->FindEnemy(HeavyEnemy);
            const auto raise = [&](const Content::EnemyDefinition* kind, std::int32_t count) {
                if (!kind || count <= 0) return;
                for (const auto id : encounter_.SpawnInRoom(*kind, count, here, playerPosition_, depth_)) room.enemies.push_back(id);
            };
            switch (type) {
            case RoomType::Combat:
                raise(light, std::min(2 + depth_ / 2, 4));
                raise(heavy, depth_ >= 2 ? std::min(1 + depth_ / 4, 2) : 0);
                break;
            case RoomType::Elite:
                raise(heavy, 2);
                raise(light, 1 + depth_ / 3);
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

        for (auto& room : rooms_) {
            if (!room.triggered || room.cleared) continue;
            room.cleared = std::ranges::all_of(room.enemies, [this](std::uint32_t id) {
                const auto* enemy = encounter_.FindEnemy(id);
                return !enemy || enemy->IsDead();
            });
        }
    }

    // Chests open when the player steps up to them; the trapdoor of a cleared exit room leads down
    void GameEngine::UpdateFloorInteractions() {
        if (!IsPlayerAlive()) return;
        const auto& layout = world_.Layout();
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            if (layout.rooms[i].type != RoomType::Treasure || rooms_[i].chestOpened) continue;
            const auto chest = world_.RoomCenter(static_cast<std::int32_t>(i));
            if (Encounter::Distance(chest, playerPosition_) <= InteractRadius) {
                rooms_[i].chestOpened = true;
                DropLoot(ChestLootTable, chest);
            }
        }

        const auto exit = static_cast<std::size_t>(layout.exitRoom);
        if (rooms_[exit].cleared && Encounter::Distance(world_.RoomCenter(layout.exitRoom), playerPosition_) <= InteractRadius * 0.7f) {
            Descend();
        }
    }

    void GameEngine::DrawFloorProps(const Systems::Rendering::Color& tint) {
        if (!dungeonMode_) return;
        const auto& layout = world_.Layout();
        const auto model = [this](std::string_view file) {
            const auto it = characterModels_.find(std::string(file));
            return it != characterModels_.end() ? it->second : Systems::Rendering::InvalidModelId;
        };
        const auto draw = [&](std::string_view file, WorldPoint at, Systems::Rendering::Color color) {
            if (models_.loaded) {
                renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ model(file), { at.x, 0.0f, at.y }, 0.0f, ModelScale, tint });
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
            draw(rooms_[i].chestOpened ? OpenChestModel : ChestModel, world_.RoomCenter(static_cast<std::int32_t>(i)), { 0.35f, 0.20f, 0.08f, 1.0f });
        }
        draw(TrapdoorModel, world_.RoomCenter(layout.exitRoom), { 0.10f, 0.08f, 0.06f, 1.0f });
    }

    void GameEngine::WriteFloor(Systems::Persistence::SaveData& data) const {
        std::vector<std::int32_t> cleared;
        std::vector<std::int32_t> chests;
        for (std::size_t i = 0; i < rooms_.size(); ++i) {
            if (rooms_[i].cleared) cleared.push_back(static_cast<std::int32_t>(i));
            if (rooms_[i].chestOpened) chests.push_back(static_cast<std::int32_t>(i));
        }
        data["floor.seed"] = std::format("{}", baseSeed_);
        data["floor.depth"] = std::format("{}", depth_);
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

        baseSeed_ = seed;
        StartFloor(depth);
        for (const auto index : ParseIndices(field("floor.cleared").value_or(""))) {
            if (index < 0 || static_cast<std::size_t>(index) >= rooms_.size()) continue;
            rooms_[static_cast<std::size_t>(index)].triggered = true;
            rooms_[static_cast<std::size_t>(index)].cleared = true;
        }
        for (const auto index : ParseIndices(field("floor.chests").value_or(""))) {
            if (index >= 0 && static_cast<std::size_t>(index) < rooms_.size()) rooms_[static_cast<std::size_t>(index)].chestOpened = true;
        }
    }

} // namespace LB::Engine
