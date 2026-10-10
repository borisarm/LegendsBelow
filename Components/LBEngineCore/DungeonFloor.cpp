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
        BuildTorches();
        explored_.assign(layout.cells.size(), 0);
        RevealAround();
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


    // ------------------------------------------------------------------------------------------
    // Torches
    // ------------------------------------------------------------------------------------------

    // One torch in the middle of each room side, and more on long sides (one every four cells),
    // hung on the wall's inner face and facing into the room
    void GameEngine::BuildTorches() {
        torches_.clear();
        const auto& layout = world_.Layout();
        constexpr float cell = WorldMap::CellTiles;
        constexpr float pi = std::numbers::pi_v<float>;

        // The wall piece hanging over an edge, if the walls are built
        const auto wallAt = [this](WorldPoint at) -> std::optional<std::size_t> {
            std::optional<std::size_t> best;
            float bestDistance = 1.0f;
            for (std::size_t i = 0; i < walls_.size(); ++i) {
                if (walls_[i].model != models_.wall) continue;
                const float d = std::hypot(walls_[i].position.x - at.x, walls_[i].position.z - at.y);
                if (d < bestDistance) {
                    bestDistance = d;
                    best = i;
                }
            }
            return best;
        };

        for (const auto& room : layout.rooms) {
            const auto& area = room.area;
            // Each side: its cells along the wall, the step outward, and the facing into the room
            struct Side { std::int32_t length; std::int32_t x0, y0, dx, dy, outX, outY; float yaw; };
            const Side sides[4] = {
                { area.width, area.x, area.y, 1, 0, 0, -1, 0.0f },                                   // north wall, faces south (+Z)
                { area.width, area.x, area.y + area.height - 1, 1, 0, 0, 1, pi },                    // south wall
                { area.height, area.x, area.y, 0, 1, -1, 0, pi * 0.5f },                              // west wall, faces east
                { area.height, area.x + area.width - 1, area.y, 0, 1, 1, 0, -pi * 0.5f },            // east wall
            };
            for (const auto& side : sides) {
                std::vector<std::int32_t> spots{ side.length / 2 };
                for (std::int32_t k = side.length / 2 + 4; k < side.length - 1; k += 4) spots.push_back(k);
                for (std::int32_t k = side.length / 2 - 4; k > 0; k -= 4) spots.push_back(k);

                for (const auto k : spots) {
                    const std::int32_t x = side.x0 + side.dx * k;
                    const std::int32_t y = side.y0 + side.dy * k;
                    if (layout.IsFloor(x + side.outX, y + side.outY)) continue;   // a doorway, not a wall

                    // The middle of the cell's edge on that side, just off the wall
                    const auto center = world_.CellCenter(x, y);
                    const WorldPoint edge{ center.x + static_cast<float>(side.outX) * cell * 0.5f, center.y + static_cast<float>(side.outY) * cell * 0.5f };
                    Torch torch;
                    torch.position = { edge.x - static_cast<float>(side.outX) * 0.05f, edge.y - static_cast<float>(side.outY) * 0.05f };
                    torch.yaw = side.yaw;
                    torch.wall = wallAt({ edge.x + static_cast<float>(side.outX) * 0.4f, edge.y + static_cast<float>(side.outY) * 0.4f });
                    torch.phase = static_cast<float>(torches_.size()) * 1.7f;
                    torches_.push_back(torch);
                }
            }
        }
    }

    // A torch's flicker: a slow sway and a quicker shiver, between about 80% and 100%
    namespace {
        float Flicker(double time, float phase) {
            const auto t = static_cast<float>(time);
            return 0.9f + 0.06f * std::sin(t * 5.3f + phase) + 0.04f * std::sin(t * 13.7f + phase * 2.3f);
        }

        // Where a torch's flame burns: above and in front of its mount
        Systems::Rendering::Vector3 FlameOf(const GameEngine::Torch& torch) {
            constexpr float out = 0.285f;   // the torch head, in front of the wall plate
            return { torch.position.x + std::sin(torch.yaw) * out, 1.42f, torch.position.y + std::cos(torch.yaw) * out };
        }
    }

    // The torch props, a flickering flame on each, and their lights
    void GameEngine::DrawTorches(const Systems::Rendering::Color& tint) {
        const auto model = [this](std::string_view file) {
            const auto it = characterModels_.find(std::string(file));
            return it != characterModels_.end() ? it->second : Systems::Rendering::InvalidModelId;
        };
        for (const auto& torch : torches_) {
            const float opacity = torch.wall ? walls_[*torch.wall].opacity : 1.0f;
            const float flicker = Flicker(timer_.GetTotalSeconds(), torch.phase);
            const auto flame = FlameOf(torch);

            if (models_.loaded) {
                auto color = tint;
                color.a = opacity;
                renderingSystem_.DrawModel(Systems::Rendering::ModelRenderData{ model(TorchModel), { torch.position.x, 1.0f, torch.position.y }, torch.yaw, ModelScale, color });
            }
            if (opacity > 0.5f) {
                // An outer glow and a hot core, swaying with the flicker
                Systems::Rendering::MeshRenderData outer{};
                outer.shape = Systems::Rendering::MeshShape::Sphere;
                outer.position = { flame.x, flame.y + 0.02f * flicker, flame.z };
                outer.size = { 0.17f * flicker, 0.24f * flicker, 0.17f * flicker };
                outer.color = { 0.0f, 0.0f, 0.0f, 1.0f };
                outer.emissive = { 1.0f * flicker * tint.r, 0.42f * flicker * tint.g, 0.08f * flicker * tint.b, 1.0f };
                renderingSystem_.DrawMesh(outer);

                Systems::Rendering::MeshRenderData core = outer;
                core.position.y = flame.y - 0.02f;
                core.size = { 0.09f, 0.13f * flicker, 0.09f };
                core.emissive = { 1.0f * tint.r, 0.85f * tint.g, 0.45f * tint.b, 1.0f };
                renderingSystem_.DrawMesh(core);
            }

            Systems::Rendering::PointLight light{};
            light.position = { flame.x + std::sin(torch.yaw) * 0.25f, flame.y, flame.z + std::cos(torch.yaw) * 0.25f };
            light.color = { 1.0f, 0.55f, 0.22f, 1.0f };
            light.radius = TorchLightRadius;
            light.intensity = TorchLightIntensity * flicker;
            renderingSystem_.DrawLight(light);
        }
    }

    // ------------------------------------------------------------------------------------------
    // Minimap
    // ------------------------------------------------------------------------------------------

    // Floor cells near the player and in sight become explored; so does the whole room they are in
    void GameEngine::RevealAround() {
        const auto& layout = world_.Layout();
        if (explored_.size() != layout.cells.size()) return;
        const auto mark = [&](std::int32_t x, std::int32_t y) {
            if (layout.IsFloor(x, y)) explored_[static_cast<std::size_t>(y * layout.width + x)] = 1;
        };

        const auto [px, py] = world_.CellOf(playerPosition_);
        for (std::int32_t y = py - MapRevealCells; y <= py + MapRevealCells; ++y) {
            for (std::int32_t x = px - MapRevealCells; x <= px + MapRevealCells; ++x) {
                if (!layout.IsFloor(x, y) || (x - px) * (x - px) + (y - py) * (y - py) > MapRevealCells * MapRevealCells) continue;
                if (world_.LineOfSight(playerPosition_, world_.CellCenter(x, y))) mark(x, y);
            }
        }

        if (const auto room = world_.RoomAt(playerPosition_); room >= 0) {
            const auto& area = layout.rooms[static_cast<std::size_t>(room)].area;
            for (std::int32_t y = area.y; y < area.y + area.height; ++y) {
                for (std::int32_t x = area.x; x < area.x + area.width; ++x) mark(x, y);
            }
        }
    }

    // North-up map of the explored cells: in the top right corner centered on the player, or large
    // in the middle showing the whole floor. Marks the player, the trapdoor, unopened chests, and
    // enemies in sight.
    void GameEngine::DrawMinimap() {
        if (!dungeonMode_ || !playerId_ || characterScreen_.IsOpen()) return;
        const auto& layout = world_.Layout();
        if (explored_.size() != layout.cells.size() || layout.width == 0) return;

        std::int32_t screenWidth = 0;
        std::int32_t screenHeight = 0;
        renderingSystem_.GetScreenSize(screenWidth, screenHeight);
        // Panel (the visible window) and where cell (0, 0) lands
        std::int32_t cellPixels = 0, panelLeft = 0, panelTop = 0, panelWidth = 0, panelHeight = 0, left = 0, top = 0;
        if (mapExpanded_) {
            const std::int32_t side = std::min(screenWidth, screenHeight) * 3 / 4;
            cellPixels = std::max(2, side / std::max(layout.width, layout.height));
            panelWidth = cellPixels * layout.width;
            panelHeight = cellPixels * layout.height;
            panelLeft = left = (screenWidth - panelWidth) / 2;
            panelTop = top = (screenHeight - panelHeight) / 2;
        }
        else {
            cellPixels = 7;
            panelWidth = panelHeight = std::min(180, std::min(screenWidth, screenHeight) / 3);
            panelLeft = screenWidth - panelWidth - 14;
            panelTop = 14;
            const float u = (playerPosition_.x - world_.Origin().x) / WorldMap::CellTiles;
            const float v = (playerPosition_.y - world_.Origin().y) / WorldMap::CellTiles;
            left = panelLeft + panelWidth / 2 - static_cast<std::int32_t>(u * static_cast<float>(cellPixels));
            top = panelTop + panelHeight / 2 - static_cast<std::int32_t>(v * static_cast<float>(cellPixels));
        }
        const std::int32_t mapWidth = panelWidth;
        const std::int32_t mapHeight = panelHeight;

        const auto color = [](float r, float g, float b, float a) {
            return Systems::Rendering::Color{ std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), a };
        };
        // Rectangles are cut to the panel
        const auto rect = [&](std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, Systems::Rendering::Color c, std::int32_t layer) {
            const std::int32_t x0 = std::max(x, panelLeft);
            const std::int32_t y0 = std::max(y, panelTop);
            const std::int32_t x1 = std::min(x + w, panelLeft + panelWidth);
            const std::int32_t y1 = std::min(y + h, panelTop + panelHeight);
            if (x1 > x0 && y1 > y0) renderingSystem_.DrawRectangle({ x0, y0, x1 - x0, y1 - y0 }, c, layer);
        };
        // A marker centered on a world point
        const auto marker = [&](WorldPoint at, std::int32_t size, Systems::Rendering::Color c, std::int32_t layer) {
            const float u = (at.x - world_.Origin().x) / WorldMap::CellTiles;
            const float v = (at.y - world_.Origin().y) / WorldMap::CellTiles;
            rect(left + static_cast<std::int32_t>(u * static_cast<float>(cellPixels)) - size / 2,
                 top + static_cast<std::int32_t>(v * static_cast<float>(cellPixels)) - size / 2, size, size, c, layer);
        };

        renderingSystem_.DrawRectangle({ panelLeft - 4, panelTop - 4, mapWidth + 8, mapHeight + 8 }, color(0.02f, 0.02f, 0.03f, mapExpanded_ ? 0.85f : 0.6f), 14);
        const auto here = world_.RoomAt(playerPosition_);
        for (std::int32_t y = 0; y < layout.height; ++y) {
            for (std::int32_t x = 0; x < layout.width; ++x) {
                if (!IsCellExplored(x, y)) continue;
                const bool current = here >= 0 && layout.rooms[static_cast<std::size_t>(here)].area.Contains(x, y);
                rect(left + x * cellPixels, top + y * cellPixels, cellPixels, cellPixels,
                     current ? color(0.62f, 0.56f, 0.46f, 0.95f) : color(0.40f, 0.38f, 0.35f, 0.9f), 15);
            }
        }

        const std::int32_t dot = std::max(3, cellPixels);
        const auto explored = [&](WorldPoint at) {
            const auto [x, y] = world_.CellOf(at);
            return IsCellExplored(x, y);
        };
        const auto exit = world_.RoomCenter(layout.exitRoom);
        if (explored(exit)) marker(exit, dot + 2, color(0.35f, 0.65f, 1.0f, 1.0f), 16);
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            if (layout.rooms[i].type != Dungeon::RoomType::Treasure || rooms_[i].chestOpened) continue;
            const auto chest = world_.RoomCenter(static_cast<std::int32_t>(i));
            if (explored(chest)) marker(chest, dot, color(0.95f, 0.80f, 0.30f, 1.0f), 16);
        }
        for (const auto& enemy : encounter_.GetEnemies()) {
            if (enemy.IsDead() || !world_.LineOfSight(playerPosition_, enemy.position)) continue;
            marker(enemy.position, std::max(3, dot - 1), color(0.90f, 0.20f, 0.15f, 1.0f), 17);
        }
        marker(playerPosition_, dot + 2, color(1.0f, 1.0f, 1.0f, 1.0f), 18);
    }

} // namespace LB::Engine
