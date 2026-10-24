// EngineSetup.cpp - LB.Engine implementation unit: GameEngine startup and shutdown (content,
// models, fonts, the player, input bindings, preferences) and saving and loading games
module LB.Engine;

import std;
import LB.Core;
import LB.Character;
import LB.CharacterService;
import LB.Content;
import LB.AudioSystem;
import LB.InputSystem;
import LB.PersistenceSystem;
import LB.RenderingSystem;

namespace LB::Engine {

    bool GameEngine::Initialize(void* nativeWindow, std::int32_t width, std::int32_t height, const EngineOptions& options) {
        Shutdown();   // a second Initialize starts a fresh run
        initializationError_.clear();

        options_ = options;

        // Content first: invalid data is a fatal authoring error (fail fast, report everything)
        if (!LoadContent(options.contentDirectory)) {
            return false;
        }
        LoadCredits(options.contentDirectory);
        session_->world = WorldMap::Arena(ArenaHalfWidth, ArenaHalfHeight);

        if (!renderingSystem_.Initialize(nativeWindow, width, height)) {
            initializationError_ = "Failed to initialize Direct3D 12. A GPU supporting Shader Model 6.0 is required.";
            return false;
        }

        if (!LoadModels(options.modelDirectory) || !LoadFonts(options.fontDirectory) || !LoadSounds(options.soundDirectory) || !LoadMusic(options.musicDirectory) || !LoadMenuArt(options.uiDirectory)) {
            renderingSystem_.Shutdown();
            return false;
        }

        // An unusable save directory should not stop the game; fall back to memory only
        if (!persistenceSystem_.Initialize(options.saveDirectory)) {
            persistenceSystem_.Initialize({});
        }

        audioSystem_.Initialize();
        if (nativeWindow != nullptr) {
            audioSystem_.EnableDeviceOutput();
        }
        ApplyPreferences(persistenceSystem_.LoadPreferences());
        legacy_ = Legacy::FromProfile(persistenceSystem_.LoadProfile());

        inputSystem_.Initialize();
        RegisterDefaultBindings();
        ApplyBindingProfile();

        // Gamepads are read from the platform only with a real window; headless runs
        // (tests, tools) stay deterministic and receive input through the On* methods
        if (nativeWindow != nullptr) {
            inputSystem_.EnableDeviceInput();
        }
        active_ = true;

        renderingSystem_.SetDeviceCallbacks(
            [this] { OnDeviceLost(); },
            [this] { OnDeviceRestored(); });

        if (!StartRun()) {
            return false;
        }
        if (options.titleScreen) {
            OpenMenu(MenuPage::Title);
        }
        timer_.ResetElapsedTime();
        initialized_ = true;

        return true;
    }

    // A new run in a fresh session: the player with the starting kit, the first floor (or the test
    // arena) with its walls, the enemies, and the music. What the last run played stops.
    bool GameEngine::StartRun() {
        audioSystem_.StopAll();
        session_ = std::make_unique<Session>();
        // With the title screen a run is the player's only from New Game, Continue, or a load; one
        // started from a menu is behind it until play begins
        session_->playing = !menu_.IsOpen() && !options_.titleScreen;
        SubscribeToEvents();
        {
            const auto& clips = assets_.playerClips;
            session_->allies.SetClips(AllyClips{ clips.idle, clips.run, clips.slashTwoHanded, clips.bowDraw, clips.hit, clips.death, clips.castRelease });
        }
        session_->dungeonMode = options_.dungeon;
        session_->baseSeed = options_.randomSeed ? *options_.randomSeed : std::random_device{}();
        session_->world = WorldMap::Arena(ArenaHalfWidth, ArenaHalfHeight);
        if (assets_.models.loaded) {
            session_->walls.Build(session_->world, renderingSystem_, assets_.models.wall, assets_.models.corner, ModelScale);
        }

        if (!SpawnPlayer()) {
            return false;
        }
        session_->encounter = options_.randomSeed ? Encounter(*options_.randomSeed) : Encounter();
        session_->lootRoller = options_.randomSeed ? LootRoller(*options_.randomSeed + 1) : LootRoller();
        session_->spawnEnemies = options_.spawnEnemies;
        GiveStartingKit();
        if (options_.startLevel > 1 && session_->playerId) {
            GrantExperience(session_->characterService.GetExperienceForLevel(options_.startLevel));
        }
        if (session_->dungeonMode && session_->playerId) {
            session_->cycle = std::max(0, options_.startCycle);
            ChooseMutators();
            StartFloor(std::max(1, options_.startDepth));
        }
        StartEncounter();
        StartMusic();
        return true;
    }

    void GameEngine::Shutdown() {
        if (initialized_) {
            AutoSave();   // closing the game keeps the run for Continue
            SaveOptions();

            inputSystem_.Shutdown();
            audioSystem_.Shutdown();
            persistenceSystem_.Shutdown();
            renderingSystem_.Shutdown();
        }

        // Everything this run owned goes at once (nothing to forget when state is added),
        // also what a failed Initialize had set up
        session_ = std::make_unique<Session>();
        assets_ = {};
        menu_ = {};
        quitRequested_ = false;
        initialized_ = false;
    }

    bool GameEngine::SaveGame(std::int32_t slotIndex) {
        if (!initialized_) return false;

        Systems::Persistence::SaveSlotInfo metadata{};
        if (const auto* player = PlayerCharacter()) {
            const auto* classData = PlayerClassDefinition();
            metadata.characterName = classData ? classData->name : std::string{};
            metadata.characterLevel = player->GetLevel();
            metadata.location = !session_->dungeonMode ? std::string("the arena")
                              : session_->cycle > 0 ? std::format("depth {}, New Game+{}{}", session_->depth, session_->cycle,
                                                                  GetMutators().empty() ? std::string{} : ": " + MutatorNames(GetMutators(), ", "))
                              : std::format("depth {}", session_->depth);
        }
        metadata.saveName = slotIndex == QuickSaveSlot ? "Quick Save" : slotIndex == AutoSaveSlot ? "Autosave" : std::format("Slot {}", slotIndex - SaveSlots.front() + 1);
        metadata.playtimeSeconds = static_cast<std::int32_t>(session_->gameTimeSeconds);

        Systems::Persistence::SaveData data;
        data["gameTimeSeconds"] = std::format("{}", session_->gameTimeSeconds);
        if (session_->playerId) {
            data["player.x"] = std::format("{}", session_->playerPosition.x);
            data["player.y"] = std::format("{}", session_->playerPosition.y);
            WriteProgress(data);
            if (session_->dungeonMode) WriteFloor(data);
        }

        return persistenceSystem_.SaveGame(slotIndex, metadata, data).success;
    }

    bool GameEngine::AutoSave() {
        if (!options_.autoSave || !initialized_ || !session_->playing || !session_->playerId || !IsPlayerAlive()) return false;
        SaveLegacy();   // the records go with every autosave
        if (!SaveGame(AutoSaveSlot)) return false;
        session_->savedNoticeSeconds = SavedNoticeSeconds;
        return true;
    }

    std::optional<std::int32_t> GameEngine::LatestSaveSlot() const {
        std::optional<std::int32_t> latest;
        std::string newest;
        for (const auto slot : { QuickSaveSlot, AutoSaveSlot, SaveSlots[0], SaveSlots[1], SaveSlots[2] }) {
            const auto info = persistenceSystem_.GetSaveSlotInfo(slot);
            if (!info || !info->exists) continue;
            if (!latest || info->timestamp >= newest) {   // ISO 8601: newer sorts later
                latest = slot;
                newest = info->timestamp;
            }
        }
        return latest;
    }

    bool GameEngine::LoadGame(std::int32_t slotIndex) {
        if (!initialized_) return false;

        auto result = persistenceSystem_.LoadGame(slotIndex);
        if (!result.success) return false;

        double gameTime = 0.0;
        if (!ParseDouble(result.data, "gameTimeSeconds", gameTime)) {
            return false;
        }

        // The player's position is restored when both the game and the save have a player
        double playerX = 0.0;
        double playerY = 0.0;
        const bool hasPlayerPosition = ParseDouble(result.data, "player.x", playerX) && ParseDouble(result.data, "player.y", playerY);

        // A save of another class: a run as that class first (its floor and progress follow)
        if (const auto it = result.data.find("player.class"); it != result.data.end()) {
            if (const auto* saved = content_->FindClass(std::string_view(it->second)); saved && saved->characterClass != session_->playerClass) {
                options_.playerClass = saved->characterClass;
                if (!StartRun()) return false;
            }
        }

        session_->gameTimeSeconds = gameTime;
        if (session_->playerId && session_->dungeonMode) {
            ReadFloor(result.data);      // the floor first: the position is on it
        }
        const WorldPoint saved{ static_cast<float>(playerX), static_cast<float>(playerY) };
        if (session_->playerId && hasPlayerPosition && session_->world.CircleFits(saved, PlayerRadius)) {
            session_->playerPosition = saved;
            session_->characterService.PlaceCharacter(*session_->playerId, ToTile(session_->playerPosition));
            session_->camera.SnapTo({ session_->playerPosition.x, session_->playerPosition.y });  // instant refocus
        }
        if (session_->playerId) {
            ReadProgress(result.data);   // saves from before progression keep the current one
        }
        session_->playing = true;
        return true;
    }

    /// <summary>
    /// Load the arena and player models. Without a model directory the world is drawn with
    /// primitives; a missing or invalid model stops startup with every problem listed.
    /// </summary>
    bool GameEngine::LoadModels(const std::string& modelDirectory) {
        assets_.models = {};
        session_->walls.Clear();
        if (modelDirectory.empty()) return true;

        const auto directory = std::filesystem::path(std::u8string(modelDirectory.begin(), modelDirectory.end()));
        std::vector<std::string> errors;
        const auto load = [&](std::string_view file) {
            const auto result = renderingSystem_.LoadModel(directory / file);
            if (!result.Succeeded()) errors.push_back(result.error);
            return result.id;
        };

        assets_.models.floor = load(FloorModel);
        assets_.models.floorVariant = load(FloorVariantModel);
        assets_.models.wall = load(WallModel);
        assets_.models.corner = load(CornerModel);
        assets_.models.player = load(PlayerModel);
        assets_.models.mage = load(MageModel);
        assets_.models.rogue = load(RogueModel);
        assets_.models.cleric = load(ClericModel);
        assets_.models.playerWeapon = load(PlayerWeaponModel);
        assets_.characterModels.clear();

        // Loot on the floor, weapons in the knight's hand, and dungeon props (KayKit Dungeon Pack)
        for (const auto file : { ChestModel, OpenChestModel, TrapdoorModel, TorchModel, MerchantModel, ShrineModel, CandlesModel,
                                 GreaterChestModel, GateModel, CandleModel, LitCandleModel, LeverModel, WaystoneModel, std::string_view("key.gltf") }) {
            if (!assets_.characterModels.contains(std::string(file))) assets_.characterModels[std::string(file)] = load(file);
        }
        for (const auto file : PropModels) {
            assets_.characterModels[std::string(file)] = load(file);
        }
        for (const auto file : HeldModels) {
            assets_.characterModels[std::string(file)] = load(file);
        }
        // The companions to hire: their bodies, weapons, and arrows
        for (const auto& ally : content_->GetAllies()) {
            for (const auto* file : { &ally.model, &ally.mainHandModel, &ally.projectileModel }) {
                if (!file->empty() && !assets_.characterModels.contains(*file)) assets_.characterModels[*file] = load(*file);
            }
        }
        // The floors' objectives: the relics, the captive, and their cage; the books of lore
        const auto& objectives = content_->GetObjectives();
        std::vector<std::string> objectiveModels{ objectives.captiveModel, objectives.cageModel, std::string(LoreBookModel) };
        for (const auto& relic : objectives.relics) objectiveModels.push_back(relic.model);
        for (const auto& file : objectiveModels) {
            if (!file.empty() && !assets_.characterModels.contains(file)) assets_.characterModels[file] = load(file);
        }
        // The tiers' floors and walls, props, and traps (KayKit Dungeon Pack 1.1)
        for (const auto file : FloorDecor::Models) {
            if (!assets_.characterModels.contains(file)) assets_.characterModels[std::string(file)] = load(file);
        }
        for (const auto file : FloorDecor::MoreModels) {
            if (!assets_.characterModels.contains(file)) assets_.characterModels[std::string(file)] = load(file);
        }
        for (const auto& consumable : content_->GetConsumables()) {
            if (!assets_.characterModels.contains(consumable.model)) assets_.characterModels[consumable.model] = load(consumable.model);
        }

        // Enemy bodies, the weapons they hold, and what they shoot, by file name
        for (const auto& enemy : content_->GetEnemies()) {
            for (const auto& file : { enemy.model, enemy.mainHandModel, enemy.offHandModel, enemy.projectileModel }) {
                if (!file.empty() && !assets_.characterModels.contains(file)) assets_.characterModels[file] = load(file);
            }
        }

        for (const auto file : AnimationFiles) {
            const auto result = renderingSystem_.LoadAnimations(directory / file);
            if (!result.Succeeded()) errors.push_back(result.error);
        }
        for (const auto file : LargeAnimationFiles) {
            const auto result = renderingSystem_.LoadAnimations(directory / file, LargeClipPrefix);
            if (!result.Succeeded()) errors.push_back(result.error);
        }
        if (errors.empty()) {
            ResolveClips(errors);
        }

        if (!errors.empty()) {
            initializationError_ = std::format("3D models in '{}' could not be loaded ({} problems):", modelDirectory, errors.size());
            for (const auto& error : errors) {
                initializationError_ += "\n" + error;
            }
            assets_.models = {};
            assets_.characterModels.clear();
            assets_.enemyClips = {};
            assets_.playerClips = {};
            return false;
        }
        assets_.models.loaded = true;
        session_->walls.Build(session_->world, renderingSystem_, assets_.models.wall, assets_.models.corner, ModelScale);
        return true;
    }

    /// <summary>
    /// Load the UI font. Without a font directory text uses the pixel font; a missing or
    /// invalid font stops startup.
    /// </summary>
    bool GameEngine::LoadFonts(const std::string& fontDirectory) {
        if (fontDirectory.empty()) return true;

        const auto directory = std::filesystem::path(std::u8string(fontDirectory.begin(), fontDirectory.end()));
        const auto result = renderingSystem_.LoadFont(directory / UiFont);
        if (!result.Succeeded()) {
            initializationError_ = std::format("The font in '{}' could not be loaded:\n{}", fontDirectory, result.error);
            return false;
        }
        return true;
    }

    // Animation clips by name (KayKit Rig_Medium): the player's, and the enemies'. Each enemy type
    // names its attack clip (CombatMelee or CombatRanged set); the overhand Throw is the fallback
    // for types that do not.
    void GameEngine::ResolveClips(std::vector<std::string>& errors) {
        const auto clip = [&](std::string_view name, float* seconds = nullptr) {
            const auto id = renderingSystem_.FindAnimation(name);
            if (!id) {
                errors.push_back(std::format("animation '{}' not found in the Rig_Medium animation sets", name));
                return Systems::Rendering::InvalidAnimationId;
            }
            if (seconds) *seconds = std::max(0.01f, renderingSystem_.GetAnimation(*id)->durationSeconds);
            return *id;
        };

        assets_.enemyClips.idle = clip("Idle_A");
        assets_.enemyClips.walk = clip("Walking_A", &assets_.enemyClips.walkSeconds);
        assets_.enemyClips.attack = clip("Throw", &assets_.enemyClips.attackSeconds);
        assets_.enemyClips.hit = clip("Hit_A", &assets_.enemyClips.hitSeconds);
        assets_.enemyClips.death = clip("Death_A");
        assets_.enemyClips.spawn = clip("Spawn_Ground", &assets_.enemyClips.spawnSeconds);

        const auto info = [&](std::string_view name) {
            ClipInfo result{};
            result.id = clip(name, &result.seconds);
            return result;
        };
        auto& player = assets_.playerClips;
        player.idle = info("Idle_A");
        player.bowIdle = info("Ranged_Bow_Idle");
        player.run = info("Running_A");
        player.slash = info("Melee_1H_Attack_Slice_Diagonal");
        player.slashTwoHanded = info("Melee_2H_Attack_Slice");
        player.stab = info("Melee_1H_Attack_Stab");
        player.bowDraw = info("Ranged_Bow_Draw");
        player.bowRelease = info("Ranged_Bow_Release");
        player.shoot = info("Ranged_1H_Shoot");
        player.hit = info("Hit_A");
        player.death = info("Death_A");
        player.castLoop = info("Ranged_Magic_Spellcasting");
        player.castRelease = info("Ranged_Magic_Shoot");
        player.dodge = info("Dodge_Forward");

        // The large rig's clips (the golem's): no rising clip, it stands up idle
        auto large = std::make_shared<EnemyClips>();
        const std::string prefix(LargeClipPrefix);
        large->idle = clip(prefix + "Idle_A");
        large->walk = clip(prefix + "Walking_A", &large->walkSeconds);
        large->attack = clip(prefix + "Melee_2H_Attack", &large->attackSeconds);
        large->hit = clip(prefix + "Hit_A", &large->hitSeconds);
        large->death = clip(prefix + "Death_A");
        large->spawn = large->idle;

        assets_.enemyClips.attacks.clear();
        for (const auto& enemy : content_->GetEnemies()) {
            if (enemy.attackAnimation.empty()) continue;
            const bool onLarge = enemy.rig == "Large";
            ClipInfo attack{};
            attack.id = clip((onLarge ? prefix : std::string{}) + enemy.attackAnimation, &attack.seconds);
            (onLarge ? large->attacks : assets_.enemyClips.attacks).emplace(enemy.key, attack);
        }
        assets_.enemyClips.large = std::move(large);
    }

    bool GameEngine::LoadContent(const std::string& contentDirectory) {
        auto content = std::make_shared<Content::ContentDatabase>();
        if (!contentDirectory.empty()) {
            const auto result = content->LoadFromDirectory(contentDirectory);
            if (!result.Succeeded()) {
                initializationError_ = std::format("Game content in '{}' is invalid ({} problems):", contentDirectory, result.errors.size());
                for (const auto& error : result.errors) {
                    initializationError_ += "\n" + error;
                }
                return false;
            }
        }
        content_ = std::move(content);
        return true;
    }

    // Class rules (progression, speed, weapon adjustments) come from classes.json
    void GameEngine::ConfigureClassRules() {
        for (const auto& definition : content_->GetClasses()) {
            Services::Character::ClassRules rules{};
            rules.baseMovementSpeed = definition.baseMovementSpeed;
            rules.progression = definition.progression;
            rules.weaponAdjustments = definition.weaponAdjustments;
            rules.primaryAttributes = definition.primaryAttributes;
            rules.attributeGrowth = definition.attributeGrowth;
            rules.armorAdjustments = definition.armorAdjustments;
            session_->characterService.SetClassRules(definition.characterClass, std::move(rules));
        }
    }

    /// <summary>
    /// Create the player from class data. Without class data (no content) there is no player.
    /// </summary>
    bool GameEngine::SpawnPlayer() {
        ConfigureClassRules();

        const auto playerClass = options_.playerClass;
        session_->playerClass = playerClass;
        const auto* classData = content_->FindClass(playerClass);
        if (!classData) {
            return true;
        }

        Services::Character::CharacterCreationRequest request("Hero", playerClass, classData->startingAttributes);

        session_->playerId = session_->characterService.CreateCharacter(request);
        if (!session_->playerId) return false;

        session_->playerPosition = WorldPoint{};
        session_->characterService.PlaceCharacter(*session_->playerId, ToTile(session_->playerPosition));
        session_->camera.SnapTo({ session_->playerPosition.x, session_->playerPosition.y });
        return true;
    }

    void GameEngine::SetPaused(bool paused) {
        session_->paused = paused;

        // Gameplay sounds stop with the simulation; music keeps playing
        using Systems::Audio::AudioChannel;
        for (auto channel : { AudioChannel::SoundEffects, AudioChannel::Ambient, AudioChannel::Voice }) {
            if (session_->paused) audioSystem_.PauseChannel(channel);
            else audioSystem_.ResumeChannel(channel);
        }
    }

    void GameEngine::ApplyPreferences(const Systems::Persistence::PlayerPreferences& prefs) {
        using Systems::Audio::AudioChannel;
        audioSystem_.SetChannelVolume(AudioChannel::Master, prefs.masterVolume);
        audioSystem_.SetChannelVolume(AudioChannel::Music, prefs.musicVolume);
        audioSystem_.SetChannelVolume(AudioChannel::SoundEffects, prefs.sfxVolume);
        uiSafeArea_ = std::clamp(prefs.uiSafeArea, Systems::Rendering::RenderingSystem::MinUiSafeArea, 1.0f);
    }

    void GameEngine::SaveOptions() {
        using Systems::Audio::AudioChannel;
        auto prefs = persistenceSystem_.LoadPreferences();
        prefs.masterVolume = audioSystem_.GetChannelVolume(AudioChannel::Master);
        prefs.musicVolume = audioSystem_.GetChannelVolume(AudioChannel::Music);
        prefs.sfxVolume = audioSystem_.GetChannelVolume(AudioChannel::SoundEffects);
        prefs.uiSafeArea = uiSafeArea_;
        persistenceSystem_.SavePreferences(prefs);
    }

    bool GameEngine::ParseDouble(const Systems::Persistence::SaveData& data, const std::string& key, double& value) {
        auto it = data.find(key);
        if (it == data.end()) return false;

        const auto& text = it->second;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        return ec == std::errc{} && end == text.data() + text.size();
    }

    void GameEngine::RegisterGamepadBinding(std::string_view action, Systems::Input::GamepadButton button) {
        Systems::Input::InputAction binding{};
        binding.name = std::string(action);
        binding.deviceType = Systems::Input::InputDevice::Gamepad;
        binding.gamepadButton = button;
        inputSystem_.RegisterAction(binding);
    }

    void GameEngine::RegisterKeyBinding(std::string_view action, std::int32_t keyCode) {
        Systems::Input::InputAction binding{};
        binding.name = std::string(action);
        binding.deviceType = Systems::Input::InputDevice::Keyboard;
        binding.keyCode = keyCode;
        inputSystem_.RegisterAction(binding);
    }

    void GameEngine::RegisterDefaultBindings() {
        Systems::Input::InputAction pause{};
        pause.name = std::string(PauseAction);
        pause.deviceType = Systems::Input::InputDevice::Keyboard;
        pause.keyCode = Systems::Input::Keys::Escape;
        inputSystem_.RegisterAction(pause);

        Systems::Input::InputAction pauseGamepad{};
        pauseGamepad.name = std::string(PauseAction);
        pauseGamepad.deviceType = Systems::Input::InputDevice::Gamepad;
        pauseGamepad.gamepadButton = Systems::Input::GamepadButton::Start;
        inputSystem_.RegisterAction(pauseGamepad);

        RegisterKeyBinding(QuickSaveAction, Systems::Input::Keys::F5);
        RegisterKeyBinding(QuickLoadAction, Systems::Input::Keys::F9);
        RegisterKeyBinding(RotateCameraLeftAction, Systems::Input::Keys::FromChar('Q'));
        RegisterKeyBinding(RotateCameraRightAction, Systems::Input::Keys::FromChar('E'));

        // Character screen (I, C, gamepad View), quick potion (R, gamepad X: use item), and
        // the screen's navigation (WASD/arrows/D-pad, Enter/Space/A, Esc/B)
        RegisterKeyBinding(CharacterScreenAction, Systems::Input::Keys::FromChar('I'));
        RegisterKeyBinding(CharacterScreenAction, Systems::Input::Keys::FromChar('C'));
        RegisterGamepadBinding(CharacterScreenAction, Systems::Input::GamepadButton::Back);
        constexpr Systems::Input::GamepadButton spellButtons[] = {
            Systems::Input::GamepadButton::LeftShoulder, Systems::Input::GamepadButton::RightShoulder,
            Systems::Input::GamepadButton::Y, Systems::Input::GamepadButton::B };
        for (std::size_t slot = 0; slot < SpellSlots; ++slot) {
            RegisterKeyBinding(SpellActions[slot], Systems::Input::Keys::FromChar(static_cast<char>('1' + slot)));
            RegisterGamepadBinding(SpellActions[slot], spellButtons[slot]);
        }
        RegisterKeyBinding(UsePotionAction, Systems::Input::Keys::FromChar('R'));
        RegisterGamepadBinding(UsePotionAction, Systems::Input::GamepadButton::X);
        RegisterKeyBinding(MenuUpAction, Systems::Input::Keys::FromChar('W'));
        RegisterKeyBinding(MenuUpAction, Systems::Input::Keys::Up);
        RegisterGamepadBinding(MenuUpAction, Systems::Input::GamepadButton::DPadUp);
        RegisterKeyBinding(MenuDownAction, Systems::Input::Keys::FromChar('S'));
        RegisterKeyBinding(MenuDownAction, Systems::Input::Keys::Down);
        RegisterGamepadBinding(MenuDownAction, Systems::Input::GamepadButton::DPadDown);
        RegisterKeyBinding(MenuConfirmAction, Systems::Input::Keys::Enter);
        RegisterKeyBinding(MenuConfirmAction, Systems::Input::Keys::Space);
        RegisterGamepadBinding(MenuConfirmAction, Systems::Input::GamepadButton::A);
        RegisterKeyBinding(MenuBackAction, Systems::Input::Keys::Escape);
        RegisterKeyBinding(MenuLeftAction, Systems::Input::Keys::FromChar('A'));
        RegisterKeyBinding(MenuLeftAction, Systems::Input::Keys::Left);
        RegisterGamepadBinding(MenuLeftAction, Systems::Input::GamepadButton::DPadLeft);
        RegisterKeyBinding(MenuRightAction, Systems::Input::Keys::FromChar('D'));
        RegisterKeyBinding(MenuRightAction, Systems::Input::Keys::Right);
        RegisterGamepadBinding(MenuRightAction, Systems::Input::GamepadButton::DPadRight);
        RegisterKeyBinding(MapAction, Systems::Input::Keys::FromChar('M'));
        RegisterGamepadBinding(MapAction, Systems::Input::GamepadButton::RightThumb);
        RegisterGamepadBinding(MenuBackAction, Systems::Input::GamepadButton::B);

        // Primary action (design-ui-ux-and-audio.md 4): left click, gamepad A; Space as well
        RegisterKeyBinding(AttackAction, Systems::Input::Keys::Space);
        Systems::Input::InputAction attackMouse{};
        attackMouse.name = std::string(AttackAction);
        attackMouse.deviceType = Systems::Input::InputDevice::Mouse;
        attackMouse.mouseButton = Systems::Input::MouseButton::Left;
        inputSystem_.RegisterAction(attackMouse);
        RegisterKeyBinding(InteractAction, Systems::Input::Keys::FromChar('F'));
        RegisterKeyBinding(DropAction, Systems::Input::Keys::FromChar('X'));
        RegisterKeyBinding(DropAction, Systems::Input::Keys::Delete);
        RegisterGamepadBinding(DropAction, Systems::Input::GamepadButton::X);
        RegisterKeyBinding(AllyStanceAction, Systems::Input::Keys::FromChar('G'));
        RegisterGamepadBinding(AllyStanceAction, Systems::Input::GamepadButton::DPadDown);
        RegisterKeyBinding(AllyFocusAction, Systems::Input::Keys::FromChar('T'));
        RegisterGamepadBinding(AllyFocusAction, Systems::Input::GamepadButton::DPadRight);
        RegisterGamepadBinding(InteractAction, Systems::Input::GamepadButton::DPadUp);
        RegisterKeyBinding(TacticalPauseAction, Systems::Input::Keys::Tab);
        RegisterGamepadBinding(TacticalPauseAction, Systems::Input::GamepadButton::LeftThumb);
        Systems::Input::InputAction order{};
        order.name = std::string(OrderAction);
        order.deviceType = Systems::Input::InputDevice::Mouse;
        order.mouseButton = Systems::Input::MouseButton::Right;
        inputSystem_.RegisterAction(order);

        Systems::Input::InputAction attackGamepad{};
        attackGamepad.name = std::string(AttackAction);
        attackGamepad.deviceType = Systems::Input::InputDevice::Gamepad;
        attackGamepad.gamepadButton = Systems::Input::GamepadButton::A;
        inputSystem_.RegisterAction(attackGamepad);
    }

} // namespace LB::Engine
