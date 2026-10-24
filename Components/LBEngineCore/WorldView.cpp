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

    // A held model in a hand slot. Crossbows are built with the barrel along +Z, but the KayKit
    // shooting clips hold the fist like a pistol grip (the slot's +X forward, +Y up): a quarter
    // turn around the grip aims them (and lets them hang toward the floor at rest).
    std::optional<Systems::Rendering::Matrix4> GameEngine::HeldTransform(Systems::Rendering::ModelId body, std::string_view file, const char* slot,
                                                                        const Systems::Rendering::AnimationState& animation) const {
        auto transform = renderingSystem_.GetJointTransform(body, slot, animation);
        if (transform && std::ranges::find(PistolGripModels, file) != PistolGripModels.end()) {
            transform = Systems::Rendering::Matrix4::RotationY(std::numbers::pi_v<float> * 0.5f) * *transform;
        }
        return transform;
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

        // The HUD and menus grow with the window
        std::int32_t screenWidth = 0;
        std::int32_t screenHeight = 0;
        renderingSystem_.GetScreenSize(screenWidth, screenHeight);
        // ...within the safe area a TV leaves
        renderingSystem_.SetUiSafeArea(uiSafeArea_);
        renderingSystem_.SetUiScale(UiScaleFor(static_cast<std::int32_t>(static_cast<float>(screenWidth) * uiSafeArea_),
                                               static_cast<std::int32_t>(static_cast<float>(screenHeight) * uiSafeArea_)));

        // Placeholder pause feedback until there is a pause menu: dim the whole frame.
        // Colors are picked as display (sRGB) values and handed to the renderer as linear.
        const float dim = session_->paused || ScreenOpen() ? 0.4f : 1.0f;
        const Shade shade{ dim };

        // Dim ambient background (design-technical-directx12.md 2.2), then the 3D world
        renderingSystem_.Clear(shade(0.05f, 0.05f, 0.07f));
        // Dungeon floors are dark: the torches light them
        Systems::Rendering::SceneLighting lighting{};
        if (session_->dungeonMode) {
            lighting.directional = DungeonDirectionalLight * dim;
            lighting.ambient = { 0.02f * dim, 0.018f * dim, 0.025f * dim, 1.0f };
            lighting.shadows = 1.0f;   // walls and bodies hide the key light entirely
        }
        else if (dim < 1.0f) {
            lighting.directional = dim;
        }
        renderingSystem_.SetLighting(lighting);
        // The camera, shaken by heavy blows (a quick wobble dying away)
        auto view = session_->camera.GetView();
        if (const float shake = GetShake(); shake > 0.0f) {
            const auto t = static_cast<float>(timer_.GetTotalSeconds());
            const Systems::Rendering::Vector3 offset{ shake * std::sin(t * 61.0f), shake * 0.6f * std::sin(t * 47.0f + 1.3f), shake * std::sin(t * 53.0f + 2.1f) };
            view.eye = { view.eye.x + offset.x, view.eye.y + offset.y, view.eye.z + offset.z };
            view.target = { view.target.x + offset.x, view.target.y + offset.y, view.target.z + offset.z };
        }
        renderingSystem_.SetCamera(view);
        DrawArena(shade);
        DrawShadows();
        DrawRestPlace(shade(1.0f, 1.0f, 1.0f));
        DrawDecor(shade(1.0f, 1.0f, 1.0f));
        DrawPlayer(shade);
        DrawAllies(shade);
        DrawEnemies(shade);
        DrawTelegraphs();
        DrawProjectiles(shade);
        DrawOrderMarks();
        DrawGroundItems(shade(1.0f, 1.0f, 1.0f));
        DrawFloorProps(shade(1.0f, 1.0f, 1.0f));
        session_->torches.Draw(renderingSystem_, session_->walls, assets_.models.loaded ? ModelOf(TorchModel) : Systems::Rendering::InvalidModelId,
                              ModelScale, shade(1.0f, 1.0f, 1.0f), timer_.GetTotalSeconds(), GetViewArea());
        session_->effects.Draw(renderingSystem_, shade(1.0f, 1.0f, 1.0f));
        if (!menu_.IsOpen() && !ScreenOpen()) {   // full-screen panels show only the dungeon behind them
            DrawHud();
            DrawFanfare();
            DrawMinimap();
        }
        DrawCharacterScreen();
        DrawMenu();

        renderingSystem_.EndFrame();
    }

    // World height just over a character's head
    float GameEngine::EnemyTop(const Enemy& enemy) const {
        if (const auto* model = assets_.models.loaded ? renderingSystem_.GetModel(ModelOf(enemy.definition->model)) : nullptr) {
            return model->boundsMax.y * enemy.definition->modelScale * (enemy.champion ? ChampionScale : 1.0f) + 0.1f;
        }
        return 1.4f;
    }

    float GameEngine::PlayerTop() const {
        if (assets_.models.loaded) {
            const auto* body = renderingSystem_.GetModel(PlayerBodyModel());
            return (body->boundsMax.y - body->boundsMin.y) * PlayerModelScale + 0.1f;
        }
        return 1.6f;
    }

    // A ring of sparks under the picked enemy (gold), the order's target (red), and where a move
    // order goes (green)
    void GameEngine::DrawOrderMarks() {
        const auto ring = [this](WorldPoint at, float radius, Systems::Rendering::Color color) {
            constexpr int Sparks = 14;
            for (int i = 0; i < Sparks; ++i) {
                const float angle = static_cast<float>(i) / Sparks * 2.0f * std::numbers::pi_v<float>;
                renderingSystem_.DrawParticle(Systems::Rendering::ParticleRenderData{
                    { at.x + std::sin(angle) * radius, 0.06f, at.y + std::cos(angle) * radius }, 0.11f, color, true });
            }
        };
        const auto& order = session_->order;
        const auto* target = order.target ? session_->encounter.FindEnemy(*order.target) : nullptr;
        if (target) ring(target->position, 0.45f, { 1.0f, 0.25f, 0.15f, 1.8f });
        if (session_->picked && (!target || *session_->picked != target->id)) {
            if (const auto* picked = session_->encounter.FindEnemy(*session_->picked)) ring(picked->position, 0.45f, { 1.0f, 0.75f, 0.25f, 1.6f });
        }
        if (order.kind == OrderKind::Move) ring(order.point, 0.3f, { 0.35f, 1.0f, 0.45f, 1.6f });
    }

    // The hero's body: the knight, or the mage
    Systems::Rendering::ModelId GameEngine::PlayerBodyModel() const {
        switch (session_->playerClass) {
        case LB::Character::CharacterClass::Mage: return assets_.models.mage;
        case LB::Character::CharacterClass::Rogue: return assets_.models.rogue;
        case LB::Character::CharacterClass::Cleric: return assets_.models.cleric;
        default: return assets_.models.player;
        }
    }

    /// <summary>
    /// Arrows and bolts at chest height along their heading (models lie along +Z with the tip
    /// forward; one built upright, like the skeletons' arrow, has its tip down and is tipped over
    /// onto it), fading out of the walls they stuck in; thin boxes without models
    /// </summary>
    void GameEngine::DrawProjectiles(const Shade& shade) {
        using Systems::Rendering::Matrix4;
        for (const auto& projectile : session_->encounter.GetProjectiles()) {
            const Systems::Rendering::Vector3 at{ projectile.position.x, ProjectileSet::FlightHeight, projectile.position.y };
            auto tint = shade(1.0f, 1.0f, 1.0f);
            tint.a = ProjectileSet::Opacity(projectile);

            // A spell or a wand's shot: a glowing orb of its element that lights the room
            if (projectile.model.empty()) {
                const auto palette = ElementPalette(projectile.Element());
                // The palette is linear already: only dimmed with the scene
                const auto color = [&](const std::array<float, 3>& rgb, float a) { return Systems::Rendering::Color{ rgb[0] * shade.dim, rgb[1] * shade.dim, rgb[2] * shade.dim, a }; };
                renderingSystem_.DrawParticle(Systems::Rendering::ParticleRenderData{ at, 0.42f, color(palette.middle, 2.2f), true });
                renderingSystem_.DrawParticle(Systems::Rendering::ParticleRenderData{ at, 0.2f, color(palette.start, 3.0f), true });
                renderingSystem_.DrawLight(Systems::Rendering::PointLight{ at, color(palette.middle, 1.0f), 3.5f, 1.4f });
                continue;
            }

            const auto model = assets_.models.loaded ? ModelOf(projectile.model) : Systems::Rendering::InvalidModelId;
            if (const auto* shape = model != Systems::Rendering::InvalidModelId ? renderingSystem_.GetModel(model) : nullptr) {
                Systems::Rendering::ModelRenderData draw{ model, at, projectile.Yaw(), PlayerModelScale, tint };
                if (shape->boundsMax.y - shape->boundsMin.y > shape->boundsMax.z - shape->boundsMin.z) {
                    draw.transform = Matrix4::RotationX(-std::numbers::pi_v<float> * 0.5f);   // -Y (the tip) to +Z
                }
                renderingSystem_.DrawModel(draw);
                continue;
            }

            Systems::Rendering::MeshRenderData shaft{};
            shaft.shape = Systems::Rendering::MeshShape::Box;
            shaft.position = at;
            shaft.size = { 0.04f, 0.04f, 0.45f };
            shaft.yawRadians = projectile.Yaw();
            shaft.color = shade(0.62f, 0.50f, 0.34f);
            shaft.color.a = tint.a;
            renderingSystem_.DrawMesh(shaft);
        }
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
        const auto& layout = session_->world.Layout();
        const auto origin = session_->world.Origin();
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

        const auto& theme = GetTheme();
        const auto themed = [&](std::string_view file, Systems::Rendering::ModelId fallback) {
            const auto model = ModelOf(file);
            return model != Systems::Rendering::InvalidModelId ? model : fallback;
        };
        const auto floor = themed(theme.floor, assets_.models.floor);
        const auto floorVariant = themed(theme.floorVariant, assets_.models.floorVariant);
        const auto& layout = session_->world.Layout();
        const auto view = GetViewArea();
        for (std::int32_t cy = 0; cy < layout.height; ++cy) {
            for (std::int32_t cx = 0; cx < layout.width; ++cx) {
                if (!layout.IsFloor(cx, cy) || session_->decor.IsTrapCell(cx, cy)) continue;
                if (!view.Contains(session_->world.CellCenter(cx, cy), WorldMap::CellTiles)) continue;
                const auto corner = session_->world.CellCorner(cx, cy);
                for (std::int32_t dy = 0; dy < static_cast<std::int32_t>(WorldMap::CellTiles); ++dy) {
                    for (std::int32_t dx = 0; dx < static_cast<std::int32_t>(WorldMap::CellTiles); ++dx) {
                        const auto tx = static_cast<std::int32_t>(std::floor(corner.x)) + dx;
                        const auto ty = static_cast<std::int32_t>(std::floor(corner.y)) + dy;
                        // A fixed scatter of the decorated variant breaks up the grid
                        const bool variant = ((tx * 7 + ty * 13) % 11 + 11) % 11 == 0;
                        const auto model = variant ? floorVariant : floor;
                        place(model, static_cast<float>(tx) + 0.5f, -topOf(model), static_cast<float>(ty) + 0.5f, 0.0f);
                    }
                }
            }
        }

        session_->walls.Draw(renderingSystem_, tint, ModelScale, view);
    }

    // Red flash on characters that were just hit (tints multiply, so green and blue drop)
    Systems::Rendering::Color GameEngine::HitTint(const Shade& shade, float flashSeconds) {
        const float flash = std::clamp(flashSeconds / Encounter::HitFlashSeconds, 0.0f, 1.0f);
        return shade(1.0f, 1.0f - 0.65f * flash, 1.0f - 0.65f * flash);
    }

    // The knight posed by its animation, with the weapon in its right hand (a bow in its left) and
    // a worn shield on its left arm; or a placeholder hero: a body, a head, and a visor showing
    // where they face
    void GameEngine::DrawPlayer(const Shade& shade) {
        if (!session_->playerId) return;

        if (assets_.models.loaded) {
            // Feet on the floor, facing the walk direction (glTF characters face +Z)
            const Systems::Rendering::Vector3 position{ session_->playerPosition.x, 0.0f, session_->playerPosition.y };
            const auto& statuses = session_->playerStatuses;
            auto tint = HitTint(shade, session_->playerCombat.hitFlashSeconds);
            if (statuses.MovementMultiplier() < 1.0f || statuses.PreventsActions()) tint = { tint.r * 0.65f, tint.g * 0.82f, tint.b, tint.a };   // icy
            Systems::Rendering::ModelRenderData body{ PlayerBodyModel(), position, session_->playerFacingYaw, PlayerModelScale, tint };
            body.animation = session_->playerAnimator.GetState();
            renderingSystem_.DrawModel(body);

            const auto weapon = HeldWeaponFile();
            const bool bow = session_->playerWeapon && session_->playerWeapon->category == LB::Combat::WeaponType::Bow;
            std::vector<std::pair<std::string_view, const char*>> held{ { weapon, bow ? "handslot.l" : "handslot.r" } };
            if (!bow && PlayerHoldsShield()) held.emplace_back(HeldShield, "handslot.l");
            for (const auto& [file, slot] : held) {
                Systems::Rendering::ModelRenderData item{ ModelOf(file), position, session_->playerFacingYaw, PlayerModelScale, tint };
                item.transform = HeldTransform(body.model, file, slot, body.animation);
                if (item.transform) renderingSystem_.DrawModel(item);
            }
            return;
        }

        using Systems::Rendering::MeshRenderData;
        using Systems::Rendering::MeshShape;

        constexpr float BodyHeight = 1.1f;
        constexpr float HeadSize = 0.42f;
        const float x = session_->playerPosition.x;
        const float z = session_->playerPosition.y;
        const float forwardX = std::sin(session_->playerFacingYaw);
        const float forwardZ = std::cos(session_->playerFacingYaw);

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
        visor.yawRadians = session_->playerFacingYaw;
        visor.color = shade(0.15f, 0.15f, 0.20f);
        renderingSystem_.DrawMesh(visor);
    }

    /// <summary>
    /// Enemies: skinned models posed by their animation, with their weapons in the hand slots
    /// (or placeholder primitives without models). Corpses fade out before they disappear.
    /// </summary>
    void GameEngine::DrawEnemies(const Shade& shade) {

        for (const auto& enemy : session_->encounter.GetEnemies()) {
            const auto& type = *enemy.definition;
            auto tint = HitTint(shade, enemy.hitFlashSeconds);
            if (enemy.statuses.MovementMultiplier() < 1.0f || enemy.statuses.PreventsActions()) tint = { tint.r * 0.65f, tint.g * 0.82f, tint.b, tint.a };   // icy
            tint = { tint.r * type.tint[0], tint.g * type.tint[1], tint.b * type.tint[2], Encounter::Opacity(enemy) };
            const Systems::Rendering::Vector3 position{ enemy.position.x, 0.0f, enemy.position.y };
            // A champion stands taller, warmer, in a golden light
            const float scale = type.modelScale * (enemy.champion ? ChampionScale : 1.0f);
            if (enemy.champion && !enemy.IsDead()) {
                tint = { tint.r * 1.15f, tint.g * 1.0f, tint.b * 0.75f, tint.a };
                renderingSystem_.DrawLight(Systems::Rendering::PointLight{ { position.x, 1.6f, position.z }, { 1.0f, 0.75f, 0.35f, 1.0f }, 2.5f, 0.8f });
            }

            if (assets_.models.loaded) {
                Systems::Rendering::ModelRenderData body{ ModelOf(type.model), position, enemy.facingYaw, scale, tint };
                body.animation = enemy.animator.GetState();
                renderingSystem_.DrawModel(body);

                for (const auto& [file, slot] : { std::pair{ &type.mainHandModel, "handslot.r" }, std::pair{ &type.offHandModel, "handslot.l" } }) {
                    if (file->empty()) continue;
                    Systems::Rendering::ModelRenderData held{ ModelOf(*file), position, enemy.facingYaw, scale, tint };
                    held.transform = HeldTransform(body.model, *file, slot, body.animation);
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
        if (session_->playerId) {
            if (const auto status = session_->characterService.GetCharacterStatus(*session_->playerId)) {
                HudModel::Player player;
                player.hitPoints = status->currentHitPoints;
                player.maxHitPoints = status->maxHitPoints;
                player.healthFraction = static_cast<float>(status->healthPercentage());
                player.level = status->level;
                const float previous = static_cast<float>(session_->characterService.GetExperienceForLevel(status->level));
                const float next = static_cast<float>(status->experienceToNextLevel);
                player.experienceFraction = next > previous ? (static_cast<float>(status->currentExperience) - previous) / (next - previous) : 1.0f;
                player.gold = session_->gold;
                player.potions = CountItem(HealingPotion);
                const auto character = session_->characterService.GetCharacter(*session_->playerId);
                player.pointsToSpend = character && character->get().GetUnspentAttributePoints() > 0;
                const auto* classData = PlayerClassDefinition();
                player.usesMana = classData && (classData->resource == "Mana" || !classData->spellBar.empty());
                player.resource = classData && classData->resource == "Stamina" ? "ST" : "MP";
                player.pathToChoose = classData && !classData->specializations.empty() && session_->path.empty()
                                   && status->level >= Content::ClassDefinition::SpecializationLevel;
                if (const auto* weapon = session_->playerWeapon; weapon && !weapon->ammoType.empty()) {
                    const auto ammo = std::ranges::find(content_->GetConsumables(), weapon->ammoType, &Content::ConsumableDefinition::ammoType);
                    std::string name = ammo != content_->GetConsumables().end() ? ammo->name : weapon->ammoType;
                    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                    player.ammo = std::format("{} {}", name, AmmoCount(weapon->ammoType));
                }
                player.mana = status->currentManaPoints;
                player.maxMana = status->maxManaPoints;
                for (const auto& active : session_->playerStatuses.Active()) {
                    if (std::ranges::find(player.statuses, active.definition->displayName) == player.statuses.end()) player.statuses.push_back(active.definition->displayName);
                }
                hud.player = player;

                // The spell bar, and the cast in progress
                const auto bar = GetSpellBar();
                for (std::size_t slot = 0; slot < bar.size(); ++slot) {
                    const auto& spell = *bar[slot];
                    HudModel::SpellSlot entry;
                    entry.key = std::format("{}", slot + 1);
                    entry.name = spell.name;
                    entry.manaCost = spell.manaCost;
                    entry.cooldownFraction = spell.cooldownSeconds > 0.0f ? std::clamp(session_->spellCooldowns[slot] / spell.cooldownSeconds, 0.0f, 1.0f) : 0.0f;
                    entry.blocker = SpellBlocker(slot);
                    const auto palette = ElementPalette(spell.damageType);
                    entry.accent = Systems::Rendering::Color{ palette.middle[0], palette.middle[1], palette.middle[2], 1.0f };
                    hud.spellBar.push_back(std::move(entry));
                }
                if (session_->casting.active) {
                    hud.castName = session_->casting.spell->name;
                    hud.castFraction = session_->casting.castSeconds > 0.0f
                        ? std::clamp(session_->casting.seconds / session_->casting.castSeconds, 0.0f, 1.0f) : 1.0f;
                }
            }
        }
        for (const auto& enemy : session_->encounter.GetEnemies()) {
            const bool engaged = enemy.state == EnemyState::Chasing || enemy.state == EnemyState::Attacking || enemy.state == EnemyState::Staggered;
            if ((enemy.definition->boss || enemy.champion) && !enemy.IsDead() && engaged) {
                hud.boss = HudModel::Boss{ enemy.champion ? enemy.title : enemy.definition->name, static_cast<float>(enemy.hitPoints) / static_cast<float>(enemy.maxHitPoints), enemy.Phase() };
                continue;   // its bar is across the top
            }
            if (!enemy.IsTargetable() || enemy.hitPoints >= enemy.maxHitPoints) continue;
            hud.enemyBars.push_back({ { enemy.position.x, EnemyTop(enemy), enemy.position.y },
                                      static_cast<float>(enemy.hitPoints) / static_cast<float>(enemy.maxHitPoints) });
        }
        // The tactical pause and the order in hand; by the merchant, how to trade
        if (IsTacticallyPaused()) hud.hint = "PAUSED - right click or arrows + Space: target   1-4: cast at it   Tab: resume";
        else if (NearMerchant() && !session_->trade.IsOpen()) hud.hint = "F or d-pad up: trade with the merchant";
        else if (session_->book && Distance(session_->book->at, session_->playerPosition) <= LoreReach) {
            hud.hint = session_->book->read ? "F or d-pad up: read the book again" : "F or d-pad up: read the book";
        }
        else {
            // A cracked wall, or a puzzle's candles, nearby
            for (const auto& lever : session_->features.Levers()) {
                if (!lever.pulled && Distance(lever.position, session_->playerPosition) <= LeverReach) hud.hint = "F: pull the lever";
            }
            for (const auto& stone : session_->features.Waystones()) {
                if (Distance(stone.position, session_->playerPosition) <= WaystoneReach && session_->features.WaystonesAwake()) hud.hint = "F: travel by the waystone";
            }
            for (const auto& gate : session_->features.Gates()) {
                if (!gate.open && gate.kind == GateKind::Secret && Distance(gate.center, session_->playerPosition) <= GateReach + 1.0f) hud.hint = "A cracked wall: strike it";
            }
            for (const auto& puzzle : session_->features.Puzzles()) {
                if (puzzle.solved) continue;
                for (const auto& candle : puzzle.candles) {
                    if (Distance(candle.position, session_->playerPosition) <= CandleReach) hud.hint = "F or d-pad up: touch the candle (it flips its neighbours too) - light them all";
                }
            }
        }
        const auto& order = session_->order;
        const auto* target = order.target ? session_->encounter.FindEnemy(*order.target) : nullptr;
        if (order.kind == OrderKind::Move) hud.order = "Order: move";
        else if (order.kind == OrderKind::Attack && target) hud.order = std::format("Order: attack {}", target->definition->name);
        else if (order.kind == OrderKind::Cast) {
            const auto bar = GetSpellBar();
            const auto spell = order.slot < bar.size() ? bar[order.slot]->name : std::string("spell");
            hud.order = target ? std::format("Order: {} at {}", spell, target->definition->name) : std::format("Order: {}", spell);
        }

        hud.savedNotice = session_->savedNoticeSeconds / SavedNoticeSeconds;
        if (session_->objective.kind != ObjectiveKind::None) {
            hud.goals.push_back(session_->objective.Title());
            hud.goalsDone.push_back(session_->objective.done);
        }
        if (session_->bounty.Active()) {
            hud.goals.push_back(session_->bounty.Title());
            hud.goalsDone.push_back(false);
        }
        for (const auto& ally : session_->allies.Allies()) {
            hud.allies.push_back({ ally.definition->name, static_cast<float>(ally.hitPoints) / static_cast<float>(std::max(1, ally.maxHitPoints)), ally.IsDown() });
        }
        if (session_->depth > 0) {
            hud.banner = session_->restFloor ? std::format("DEPTH {} - RESTING PLACE", session_->depth)
                       : IsFinalFloor() ? std::format("DEPTH {} - {}", session_->depth, FinalFloorName) : std::format("DEPTH {}", session_->depth);
            if (session_->cycle > 0) {
                auto names = MutatorNames(GetMutators(), ", ");
                std::ranges::transform(names, names.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                hud.banner += std::format(" - NEW GAME+{}{}", session_->cycle, names.empty() ? std::string{} : ": " + names);
            }
        }
        else if (session_->encounter.GetWave() > 0) hud.banner = std::format("WAVE {}", session_->encounter.GetWave());

        Hud::Draw(renderingSystem_, hud);
        session_->combatText.Draw(renderingSystem_);
    }

    // Telegraphed attacks: a ring of red sparks where the blow will fall, and a second ring growing
    // from the middle that meets it as the blow lands
    void GameEngine::DrawTelegraphs() {
        constexpr std::int32_t points = 40;
        for (const auto& enemy : session_->encounter.GetEnemies()) {
            if (enemy.slamSeconds <= 0.0f || enemy.IsDead()) continue;
            const auto slam = enemy.ActiveSlam();
            const float progress = std::clamp(1.0f - enemy.slamSeconds / slam.windupSeconds, 0.0f, 1.0f);
            const float pulse = 0.75f + 0.25f * std::sin(static_cast<float>(session_->gameTimeSeconds) * 18.0f);
            for (std::int32_t i = 0; i < points; ++i) {
                const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / points;
                const float c = std::cos(angle), s = std::sin(angle);
                renderingSystem_.DrawParticle(Systems::Rendering::ParticleRenderData{
                    { enemy.slamCenter.x + c * slam.radius, 0.08f, enemy.slamCenter.y + s * slam.radius }, 0.32f, { 1.0f, 0.15f, 0.06f, 3.2f * pulse }, true });
                if (i % 2 == 0) {
                    const float r = slam.radius * progress;
                    renderingSystem_.DrawParticle(Systems::Rendering::ParticleRenderData{
                        { enemy.slamCenter.x + c * r, 0.06f, enemy.slamCenter.y + s * r }, 0.22f, { 1.0f, 0.4f, 0.1f, 2.4f }, true });
                }
            }
        }
    }

    // The companions, posed by their animations, with their weapons (a bow in the left hand)
    void GameEngine::DrawAllies(const Shade& shade) {
        if (!assets_.models.loaded) return;
        for (const auto& ally : session_->allies.Allies()) {
            const auto body = ModelOf(ally.definition->model);
            if (body == Systems::Rendering::InvalidModelId) continue;
            const auto tint = HitTint(shade, ally.hitFlashSeconds);
            const Systems::Rendering::Vector3 position{ ally.position.x, 0.0f, ally.position.y };
            Systems::Rendering::ModelRenderData data{ body, position, ally.facingYaw, ally.definition->modelScale, tint };
            data.animation = ally.animator.GetState();
            renderingSystem_.DrawModel(data);
            const auto& file = ally.definition->mainHandModel;
            if (file.empty()) continue;
            Systems::Rendering::ModelRenderData held{ ModelOf(file), position, ally.facingYaw, ally.definition->modelScale, tint };
            held.transform = HeldTransform(body, file, ally.definition->IsRanged() ? "handslot.l" : "handslot.r", data.animation);
            if (held.transform) renderingSystem_.DrawModel(held);
        }
    }

    // A soft shadow under every character standing (and the fallen, fading with their bodies):
    // darkest at the feet, and, within a torch's light, stretched away from the flame by two
    // fainter discs (the nearer the torch, the stronger)
    void GameEngine::DrawShadows() {
        const auto view = GetViewArea();
        const auto cast = [&](WorldPoint at, float size, float strength) {
            if (strength <= 0.0f || !view.Contains(at, 2.0f)) return;
            renderingSystem_.DrawShadow({ { at.x, ShadowHeight, at.y }, size, ShadowOpacity * strength });
            const Torch* nearest = nullptr;
            float nearestDistance = TorchLightRadius;
            for (const auto& torch : session_->torches.Torches()) {
                const auto flame = TorchSet::FlameOf(torch);
                const float distance = Distance(at, { flame.x, flame.z });
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    nearest = &torch;
                }
            }
            if (!nearest || nearestDistance < 1e-3f) return;
            const auto flame = TorchSet::FlameOf(*nearest);
            const float dx = (at.x - flame.x) / nearestDistance;
            const float dy = (at.y - flame.z) / nearestDistance;
            const float reach = 1.0f - nearestDistance / TorchLightRadius;
            for (int k = 1; k <= 2; ++k) {
                const float along = size * 0.3f * static_cast<float>(k) * (0.6f + reach);
                renderingSystem_.DrawShadow({ { at.x + dx * along, ShadowHeight, at.y + dy * along }, size * (1.0f - 0.12f * static_cast<float>(k)),
                                              ShadowOpacity * strength * reach * 0.55f / static_cast<float>(k) });
            }
        };

        if (session_->playerId) cast(session_->playerPosition, 1.5f, 1.0f);
        for (const auto& ally : session_->allies.Allies()) cast(ally.position, 1.5f, 1.0f);
        for (const auto& enemy : session_->encounter.GetEnemies()) {
            if (enemy.state == EnemyState::Spawning) continue;
            const float size = enemy.definition->rig == "Large" ? 2.4f : 1.5f;
            cast(enemy.position, size, Encounter::Opacity(enemy));
        }
        if (session_->merchant) cast(*session_->merchant, 1.5f, 1.0f);
    }

} // namespace LB::Engine
