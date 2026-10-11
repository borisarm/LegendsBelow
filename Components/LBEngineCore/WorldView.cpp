// WorldView.cpp - LB.Engine implementation unit: drawing a GameEngine frame (the floor and walls,
// the player and enemies, props, effects, and the HUD)
module LB.Engine;

import std;
import LB.Core;
import LB.Character;
import LB.CharacterService;
import LB.Content;
import LB.RenderingSystem;

namespace LB::Engine {

    namespace {
        // Smooth 0 -> 1 between edge0 and edge1
        float SmoothStep(float edge0, float edge1, float x) noexcept {
            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
    }

    Systems::Rendering::ModelId GameEngine::ModelOf(std::string_view file) const {
        const auto it = assets_.characterModels.find(file);
        return it != assets_.characterModels.end() ? it->second : Systems::Rendering::InvalidModelId;
    }

    /// <summary>
    /// Draw the current frame.
    /// </summary>
    void GameEngine::Render() {
        // Don't try to render anything before the first Update.
        if (timer_.GetFrameCount() == 0) {
            return;
        }

        renderingSystem_.BeginFrame();

        // Placeholder pause feedback until there is a pause menu: dim the whole frame.
        // Colors are picked as display (sRGB) values and handed to the renderer as linear.
        const float dim = session_.paused || session_.characterScreen.IsOpen() ? 0.4f : 1.0f;
        const Shade shade{ dim };

        // Dim ambient background (design-technical-directx12.md 2.2), then the 3D world
        renderingSystem_.Clear(shade(0.05f, 0.05f, 0.07f));
        // Dungeon floors are dark: the torches light them
        Systems::Rendering::SceneLighting lighting{};
        if (session_.dungeonMode) {
            lighting.directional = DungeonDirectionalLight * dim;
            lighting.ambient = { 0.02f * dim, 0.018f * dim, 0.025f * dim, 1.0f };
        }
        else if (dim < 1.0f) {
            lighting.directional = dim;
        }
        renderingSystem_.SetLighting(lighting);
        renderingSystem_.SetCamera(session_.camera.GetView());
        DrawArena(shade);
        DrawPlayer(shade);
        DrawEnemies(shade);
        DrawGroundItems(shade(1.0f, 1.0f, 1.0f));
        DrawFloorProps(shade(1.0f, 1.0f, 1.0f));
        session_.torches.Draw(renderingSystem_, session_.walls, assets_.models.loaded ? ModelOf(TorchModel) : Systems::Rendering::InvalidModelId,
                              ModelScale, shade(1.0f, 1.0f, 1.0f), timer_.GetTotalSeconds());
        session_.effects.Draw(renderingSystem_, shade(1.0f, 1.0f, 1.0f));
        DrawHud();
        DrawMinimap();
        DrawCharacterScreen();

        renderingSystem_.EndFrame();
    }

    // World height just over a character's head
    float GameEngine::EnemyTop(const Enemy& enemy) const {
        if (const auto* model = assets_.models.loaded ? renderingSystem_.GetModel(ModelOf(enemy.definition->model)) : nullptr) {
            return model->boundsMax.y * enemy.definition->modelScale + 0.1f;
        }
        return 1.4f;
    }

    float GameEngine::PlayerTop() const {
        if (assets_.models.loaded) {
            const auto* knight = renderingSystem_.GetModel(assets_.models.player);
            return (knight->boundsMax.y - knight->boundsMin.y) * ModelScale + 0.1f;
        }
        return 1.6f;
    }

    // The world with primitives: one floor slab per tile (top at height 0) and wall blocks
    // around the floor. Tile (tx, ty) covers [tx, tx + 1) x [ty, ty + 1); ground y is world Z.
    void GameEngine::DrawArena(const Shade& shade) {
        if (assets_.models.loaded) {
            DrawArenaModels(shade);
            return;
        }

        using Systems::Rendering::MeshRenderData;
        using Systems::Rendering::MeshShape;

        // Every tile of a floor cell is a flagstone; every solid tile touching one is a wall block
        constexpr float FloorThickness = 0.2f;
        const auto& layout = session_.world.Layout();
        const auto origin = session_.world.Origin();
        const auto tiles = static_cast<std::int32_t>(WorldMap::CellTiles);
        const auto isFloorTile = [&](std::int32_t tx, std::int32_t ty) {
            return layout.IsFloor(static_cast<std::int32_t>(std::floor(static_cast<float>(tx) / WorldMap::CellTiles)),
                                  static_cast<std::int32_t>(std::floor(static_cast<float>(ty) / WorldMap::CellTiles)));
        };
        for (std::int32_t ty = 0; ty < layout.height * tiles; ++ty) {
            for (std::int32_t tx = 0; tx < layout.width * tiles; ++tx) {
                const bool floor = isFloorTile(tx, ty);
                bool wall = false;
                for (std::int32_t dy = -1; dy <= 1 && !floor && !wall; ++dy) {
                    for (std::int32_t dx = -1; dx <= 1 && !wall; ++dx) wall = isFloorTile(tx + dx, ty + dy);
                }
                if (!floor && !wall) continue;

                const float worldX = origin.x + static_cast<float>(tx);
                const float worldZ = origin.y + static_cast<float>(ty);
                MeshRenderData mesh{};
                mesh.shape = MeshShape::Box;
                if (wall) {
                    mesh.position = { worldX + 0.5f, WallHeight * 0.5f - FloorThickness * 0.5f, worldZ + 0.5f };
                    mesh.size = { 1.0f, WallHeight + FloorThickness, 1.0f };
                    mesh.color = shade(0.42f, 0.40f, 0.38f);
                }
                else {
                    const auto ix = static_cast<std::int32_t>(std::floor(worldX));
                    const auto iz = static_cast<std::int32_t>(std::floor(worldZ));
                    const bool light = ((ix + iz) & 1) == 0;
                    mesh.position = { worldX + 0.5f, -FloorThickness * 0.5f, worldZ + 0.5f };
                    mesh.size = { 0.96f, FloorThickness, 0.96f };  // thin seams between flagstones
                    mesh.color = light ? shade(0.30f, 0.29f, 0.27f) : shade(0.24f, 0.23f, 0.22f);
                }
                renderingSystem_.DrawMesh(mesh);
            }
        }
    }

    // The world from KayKit pieces: a floor tile per tile (top at height 0), wall segments two
    // tiles long whose inner face is on the floor's edge, and pillars filling the corners
    void GameEngine::DrawArenaModels(const Shade& shade) {
        using Systems::Rendering::ModelRenderData;

        const auto tint = shade(1.0f, 1.0f, 1.0f);
        const auto place = [&](Systems::Rendering::ModelId model, float x, float y, float z, float yaw) {
            renderingSystem_.DrawModel(ModelRenderData{ model, { x, y, z }, yaw, ModelScale, tint });
        };
        const auto topOf = [&](Systems::Rendering::ModelId model) {
            return renderingSystem_.GetModel(model)->boundsMax.y * ModelScale;
        };

        const auto& layout = session_.world.Layout();
        for (std::int32_t cy = 0; cy < layout.height; ++cy) {
            for (std::int32_t cx = 0; cx < layout.width; ++cx) {
                if (!layout.IsFloor(cx, cy)) continue;
                const auto corner = session_.world.CellCorner(cx, cy);
                for (std::int32_t dy = 0; dy < static_cast<std::int32_t>(WorldMap::CellTiles); ++dy) {
                    for (std::int32_t dx = 0; dx < static_cast<std::int32_t>(WorldMap::CellTiles); ++dx) {
                        const auto tx = static_cast<std::int32_t>(std::floor(corner.x)) + dx;
                        const auto ty = static_cast<std::int32_t>(std::floor(corner.y)) + dy;
                        // A fixed scatter of the decorated variant breaks up the grid
                        const bool variant = ((tx * 7 + ty * 13) % 11 + 11) % 11 == 0;
                        const auto model = variant ? assets_.models.floorVariant : assets_.models.floor;
                        place(model, static_cast<float>(tx) + 0.5f, -topOf(model), static_cast<float>(ty) + 0.5f, 0.0f);
                    }
                }
            }
        }

        session_.walls.Draw(renderingSystem_, tint, ModelScale);
    }

    // The sword arm's pose at the shoulder during a swing: a horizontal slash that reads from
    // above. The arm lifts forward (around X, negative raises it) while winding back to the
    // knight's right (around Y), sweeps across the front through the impact, then settles.
    Systems::Rendering::Matrix4 GameEngine::SwordArmPose() const noexcept {
        using Systems::Rendering::Matrix4;
        if (!session_.playerCombat.swinging) return {};

        constexpr float lifted = -1.0f;
        constexpr float windUp = -1.1f;     // toward the knight's right
        constexpr float followThrough = 1.0f;
        const float phase = session_.playerCombat.swingSeconds / PlayerAttackSeconds();
        const float impact = Encounter::AttackImpactFraction;
        const float windUpEnd = impact * 0.65f;

        float pitch = 0.0f;
        float sweep = 0.0f;
        if (phase < windUpEnd) {
            const float t = SmoothStep(0.0f, windUpEnd, phase);
            pitch = lifted * t;
            sweep = windUp * t;
        }
        else if (phase < impact) {
            pitch = lifted;
            sweep = windUp + (followThrough - windUp) * SmoothStep(windUpEnd, impact, phase);
        }
        else {
            const float t = 1.0f - SmoothStep(impact, 1.0f, phase);
            pitch = lifted * t;
            sweep = followThrough * t;
        }
        return Matrix4::RotationX(pitch) * Matrix4::RotationY(sweep);
    }

    // Red flash on characters that were just hit (tints multiply, so green and blue drop)
    Systems::Rendering::Color GameEngine::HitTint(const Shade& shade, float flashSeconds) {
        const float flash = std::clamp(flashSeconds / Encounter::HitFlashSeconds, 0.0f, 1.0f);
        return shade(1.0f, 1.0f - 0.65f * flash, 1.0f - 0.65f * flash);
    }

    // Placeholder hero: a body, a head, and a visor showing where they face (or the knight model)
    void GameEngine::DrawPlayer(const Shade& shade) {
        if (!session_.playerId) return;

        if (assets_.models.loaded) {
            // Feet on the floor, facing the walk direction (glTF characters face +Z)
            using Systems::Rendering::Matrix4;
            const auto* knight = renderingSystem_.GetModel(assets_.models.player);
            const float feet = -knight->boundsMin.y * ModelScale;
            Systems::Rendering::ModelRenderData body{
                assets_.models.player, { session_.playerPosition.x, feet, session_.playerPosition.y }, session_.playerFacingYaw, ModelScale, HitTint(shade, session_.playerCombat.hitFlashSeconds) };

            // Down: tip over backwards around the feet, lifted so the back rests on the floor
            if (!IsPlayerAlive()) {
                const float fall = SmoothStep(0.0f, 0.6f, session_.playerCombat.deadSeconds);
                body.transform = Matrix4::RotationX(-1.5f * fall) * Matrix4::Translation({ 0.0f, 0.35f * fall, 0.0f });
            }

            // The sword arm swings at the shoulder; the sword sits in its fist, blade forward
            const auto arm = knight->FindPartIndex(PlayerSwordArmPart);
            const auto armPose = SwordArmPose();
            if (arm) {
                body.partPoses.push_back({ *arm, armPose });
                constexpr Systems::Rendering::Vector3 fist{ -0.195f, -0.43f, 0.0f };   // in the arm's space
                Systems::Rendering::ModelRenderData sword{ HeldWeaponModel(), body.position, body.yawRadians, ModelScale, body.tint };
                sword.transform = Matrix4::RotationX(std::numbers::pi_v<float> * 0.5f) * Matrix4::Translation(fist)
                                * armPose * knight->parts[*arm].transform;
                if (body.transform) *sword.transform = *sword.transform * *body.transform;
                renderingSystem_.DrawModel(sword);
            }
            renderingSystem_.DrawModel(body);
            return;
        }

        using Systems::Rendering::MeshRenderData;
        using Systems::Rendering::MeshShape;

        constexpr float BodyHeight = 1.1f;
        constexpr float HeadSize = 0.42f;
        const float x = session_.playerPosition.x;
        const float z = session_.playerPosition.y;
        const float forwardX = std::sin(session_.playerFacingYaw);
        const float forwardZ = std::cos(session_.playerFacingYaw);

        MeshRenderData body{};
        body.shape = MeshShape::Cylinder;
        body.position = { x, BodyHeight * 0.5f, z };
        body.size = { PlayerSizeTiles * 0.8f, BodyHeight, PlayerSizeTiles * 0.8f };
        body.color = shade(0.95f, 0.75f, 0.20f);
        renderingSystem_.DrawMesh(body);

        MeshRenderData head{};
        head.shape = MeshShape::Sphere;
        head.position = { x, BodyHeight + HeadSize * 0.45f, z };
        head.size = { HeadSize, HeadSize, HeadSize };
        head.color = shade(0.85f, 0.70f, 0.55f);
        renderingSystem_.DrawMesh(head);

        MeshRenderData visor{};
        visor.shape = MeshShape::Box;
        visor.position = { x + forwardX * HeadSize * 0.45f, BodyHeight + HeadSize * 0.5f, z + forwardZ * HeadSize * 0.45f };
        visor.size = { HeadSize * 0.7f, HeadSize * 0.2f, HeadSize * 0.2f };
        visor.yawRadians = session_.playerFacingYaw;
        visor.color = shade(0.15f, 0.15f, 0.20f);
        renderingSystem_.DrawMesh(visor);
    }

    /// <summary>
    /// Enemies: skinned models posed by their animation, with their weapons in the hand slots
    /// (or placeholder primitives without models). Corpses fade out before they disappear.
    /// </summary>
    void GameEngine::DrawEnemies(const Shade& shade) {

        for (const auto& enemy : session_.encounter.GetEnemies()) {
            const auto& type = *enemy.definition;
            auto tint = HitTint(shade, enemy.hitFlashSeconds);
            tint.a = Encounter::Opacity(enemy);
            const Systems::Rendering::Vector3 position{ enemy.position.x, 0.0f, enemy.position.y };

            if (assets_.models.loaded) {
                Systems::Rendering::ModelRenderData body{ ModelOf(type.model), position, enemy.facingYaw, type.modelScale, tint };
                body.animation = enemy.animator.GetState();
                renderingSystem_.DrawModel(body);

                for (const auto& [file, slot] : { std::pair{ &type.mainHandModel, "handslot.r" }, std::pair{ &type.offHandModel, "handslot.l" } }) {
                    if (file->empty()) continue;
                    Systems::Rendering::ModelRenderData held{ ModelOf(*file), position, enemy.facingYaw, type.modelScale, tint };
                    held.transform = renderingSystem_.GetJointTransform(body.model, slot, body.animation);
                    if (held.transform) renderingSystem_.DrawModel(held);
                }
                continue;
            }

            // Placeholder: a bone-colored body and skull, lying flat once dead
            using Systems::Rendering::MeshRenderData;
            using Systems::Rendering::MeshShape;
            const bool dead = enemy.IsDead();
            const float height = dead ? 0.2f : 0.9f;
            const float flash = 1.0f - 0.65f * std::clamp(enemy.hitFlashSeconds / Encounter::HitFlashSeconds, 0.0f, 1.0f);
            auto bone = shade(0.82f, 0.78f * flash, 0.66f * flash);
            bone.a = tint.a;

            MeshRenderData body{};
            body.shape = MeshShape::Cylinder;
            body.position = { position.x, height * 0.5f, position.z };
            body.size = { Encounter::CharacterRadius * 1.6f, height, Encounter::CharacterRadius * 1.6f };
            body.color = bone;
            renderingSystem_.DrawMesh(body);
            if (dead) continue;

            MeshRenderData skull{};
            skull.shape = MeshShape::Sphere;
            skull.position = { position.x, height + 0.18f, position.z };
            skull.size = { 0.4f, 0.4f, 0.4f };
            skull.color = bone;
            renderingSystem_.DrawMesh(skull);
        }
    }

    // The HUD from a snapshot of the player, hurt enemies, and the wave or depth; then the
    // floating combat texts
    void GameEngine::DrawHud() {
        HudModel hud;
        if (session_.playerId) {
            if (const auto status = session_.characterService.GetCharacterStatus(*session_.playerId)) {
                HudModel::Player player;
                player.hitPoints = status->currentHitPoints;
                player.maxHitPoints = status->maxHitPoints;
                player.healthFraction = static_cast<float>(status->healthPercentage());
                player.level = status->level;
                const float previous = static_cast<float>(session_.characterService.GetExperienceForLevel(status->level));
                const float next = static_cast<float>(status->experienceToNextLevel);
                player.experienceFraction = next > previous ? (static_cast<float>(status->currentExperience) - previous) / (next - previous) : 1.0f;
                player.gold = session_.gold;
                player.potions = CountItem(HealingPotion);
                const auto character = session_.characterService.GetCharacter(*session_.playerId);
                player.pointsToSpend = character && character->get().GetUnspentAttributePoints() > 0;
                hud.player = player;
            }
        }
        for (const auto& enemy : session_.encounter.GetEnemies()) {
            if (!enemy.IsTargetable() || enemy.hitPoints >= enemy.maxHitPoints) continue;
            hud.enemyBars.push_back({ { enemy.position.x, EnemyTop(enemy), enemy.position.y },
                                      static_cast<float>(enemy.hitPoints) / static_cast<float>(enemy.maxHitPoints) });
        }
        if (session_.depth > 0) hud.banner = std::format("DEPTH {}", session_.depth);
        else if (session_.encounter.GetWave() > 0) hud.banner = std::format("WAVE {}", session_.encounter.GetWave());

        Hud::Draw(renderingSystem_, hud);
        session_.combatText.Draw(renderingSystem_);
    }

} // namespace LB::Engine
