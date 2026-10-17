// GameMenus.cpp - LB.Engine implementation unit: the title screen, the pause menu, options, and
// credits (GameEngine members; the pages and the cursor are in :Menus). Any open menu pauses the
// game; the music keeps playing.
module LB.Engine;

import std;
import LB.AudioSystem;
import LB.PersistenceSystem;
import LB.RenderingSystem;

namespace LB::Engine {

    // The credits shown on their page: credits.txt in the content directory, one line each
    // (optional: without it the page is empty)
    void GameEngine::LoadCredits(const std::string& contentDirectory) {
        credits_.clear();
        if (contentDirectory.empty()) return;
        const auto path = std::filesystem::path(std::u8string(contentDirectory.begin(), contentDirectory.end())) / "credits.txt";
        std::ifstream in(path);
        for (std::string line; std::getline(in, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            credits_.push_back(std::move(line));
        }
        while (!credits_.empty() && credits_.back().empty()) credits_.pop_back();
    }

    // The images that frame the menus, from the UI directory; without a directory the menus are
    // plain panels, a missing or invalid image stops startup
    bool GameEngine::LoadMenuArt(const std::string& uiDirectory) {
        assets_.menuArt = {};
        if (uiDirectory.empty()) return true;

        const auto directory = std::filesystem::path(std::u8string(uiDirectory.begin(), uiDirectory.end()));
        std::vector<std::string> errors;
        const auto load = [&](std::string_view file) {
            const auto result = renderingSystem_.LoadImage(directory / file);
            if (!result.Succeeded()) errors.push_back(result.error);
            return result.id;
        };
        MenuArt art;
        art.frame = load(MenuArt::FrameFile);
        art.selection = load(MenuArt::SelectionFile);
        art.divider = load(MenuArt::DividerFile);
        std::unordered_map<std::string, Systems::Rendering::ImageId, StringHash, std::equal_to<>> icons;
        for (const auto name : ItemIcons) icons[std::string(name)] = load(std::format("icon_{}.png", name));
        if (!errors.empty()) {
            initializationError_ = std::format("UI images in '{}' could not be loaded ({} problems):", uiDirectory, errors.size());
            for (const auto& error : errors) initializationError_ += "\n" + error;
            return false;
        }
        assets_.menuArt = art;
        assets_.itemIcons = std::move(icons);
        return true;
    }

    void GameEngine::OpenMenu(MenuPage page) {
        if (page == MenuPage::Title) session_->playing = false;   // the run behind the title is nobody's
        menu_.Open(page);
        SetPaused(true);
        menu_.SetItems(BuildMenuItems());
    }

    void GameEngine::CloseMenu() {
        menu_.Close();
        SetPaused(false);
    }

    std::vector<MenuItem> GameEngine::BuildMenuItems() const {
        const bool saved = LatestSaveSlot().has_value();
        const auto percent = [this](Systems::Audio::AudioChannel channel) {
            return std::format("{}%", static_cast<int>(std::lround(audioSystem_.GetChannelVolume(channel) * 100.0f)));
        };
        using Systems::Audio::AudioChannel;
        switch (menu_.Page()) {
        case MenuPage::Title: {
            std::vector<MenuItem> items{
                { "New Game", MenuCommand::NewGame },
                { "Continue", MenuCommand::Continue, saved },
                { "Load Game", MenuCommand::OpenLoadSlots, saved },
                { "Legacy", MenuCommand::Legacy },
                { "Codex", MenuCommand::Codex },
                { "Options", MenuCommand::Options },
                { "Controls", MenuCommand::Controls },
                { "Credits", MenuCommand::Credits },
            };
            if (options_.canQuit) items.push_back({ "Quit", MenuCommand::Quit });   // a console's games never quit
            return items;
        }
        case MenuPage::Pause:
            return {
                { "Resume", MenuCommand::Resume },
                { "Save Game", MenuCommand::OpenSaveSlots },
                { "Load Game", MenuCommand::OpenLoadSlots, saved },
                { "Codex", MenuCommand::Codex },
                { "Options", MenuCommand::Options },
                { "Controls", MenuCommand::Controls },
                { "Credits", MenuCommand::Credits },
                { "Quit to Title", MenuCommand::QuitToTitle },
            };
        case MenuPage::Options:
            return {
                { "Master volume", MenuCommand::MasterVolume, true, percent(AudioChannel::Master) },
                { "Music volume", MenuCommand::MusicVolume, true, percent(AudioChannel::Music) },
                { "Effects volume", MenuCommand::EffectsVolume, true, percent(AudioChannel::SoundEffects) },
                { "Screen margin", MenuCommand::ScreenMargin, true, std::format("{}%", static_cast<int>(std::lround((1.0f - uiSafeArea_) * 50.0f))) },
                { "Back", MenuCommand::Back },
            };
        case MenuPage::Credits:
        case MenuPage::Controls:
        case MenuPage::Legacy:
        case MenuPage::Codex:
            return { { "Back", MenuCommand::Back } };
        case MenuPage::SaveSlots:
        case MenuPage::LoadSlots: {
            // Saving: the player's slots (over what is there); loading: every save there is
            const bool saving = menu_.Page() == MenuPage::SaveSlots;
            std::vector<MenuItem> items;
            const auto add = [&](std::string name, std::int32_t slot) {
                const auto summary = SlotSummary(slot);
                MenuItem item{ std::move(name), saving ? MenuCommand::SaveToSlot : MenuCommand::LoadFromSlot, saving || !summary.empty(), summary.empty() ? std::string("Empty") : summary };
                item.slot = slot;
                items.push_back(std::move(item));
            };
            if (!saving) {
                add("Autosave", AutoSaveSlot);
                add("Quick Save", QuickSaveSlot);
            }
            for (std::size_t i = 0; i < SaveSlots.size(); ++i) add(std::format("Slot {}", i + 1), SaveSlots[i]);
            items.push_back({ "Back", MenuCommand::Back });
            return items;
        }
        case MenuPage::Fallen:
            return {
                { "Rise Again", MenuCommand::RiseAgain, true, "at the entrance" },
                { "Load Last Save", MenuCommand::LoadGame, saved },
                { "Quit to Title", MenuCommand::QuitToTitle },
            };
        case MenuPage::Victory:
            return {
                { "Linger in the Hall", MenuCommand::Linger, true, "the way up stays open" },
                { "Quit to Title", MenuCommand::QuitToTitle },
            };
        case MenuPage::ChooseClass:
            return {
                { "Warrior", MenuCommand::PlayWarrior, true, "sword and bow" },
                { "Mage", MenuCommand::PlayMage, true, "wand and spells" },
                { "Rogue", MenuCommand::PlayRogue, true, "dagger, bow, and stealth" },
                { "Cleric", MenuCommand::PlayCleric, true, "mace, shield, and healing" },
                { "Back", MenuCommand::Back },
            };
        default:
            return {};
        }
    }

    // Input while a menu is open: Up/Down move (or scroll the credits), Left/Right change a value,
    // Confirm chooses, Back (Esc, B, or Start) returns to the page before or closes the pause menu
    void GameEngine::UpdateMenu() {
        const auto triggered = [this](std::string_view action) { return inputSystem_.IsActionTriggered(std::string(action)); };
        menu_.SetItems(BuildMenuItems());

        if (menu_.Page() == MenuPage::Controls && UpdateControlsPage()) return;
        if (triggered(MenuBackAction) || triggered(PauseAction)) {
            if (menu_.Page() == MenuPage::Title || menu_.Page() == MenuPage::Fallen) return;   // nowhere to go back to
            PlaySound(GameSound::ScreenClose);
            if (!menu_.Back()) {
                SetPaused(false);   // the pause menu closed: back to the game
            }
            else {
                menu_.SetItems(BuildMenuItems());
            }
            return;
        }

        const int vertical = (triggered(MenuDownAction) ? 1 : 0) - (triggered(MenuUpAction) ? 1 : 0);
        if (vertical != 0) {
            if (menu_.Page() == MenuPage::Credits) {
                menu_.Scroll(vertical * 3, credits_.size(), MenuView::CreditLinesShown);
            }
            else if (menu_.Page() == MenuPage::Codex) {
                menu_.Scroll(vertical * 3, CodexLines().size(), MenuView::CreditLinesShown);
            }
            else if (menu_.Step(vertical)) {
                PlaySound(GameSound::MenuMove);
            }
        }

        const auto* selected = menu_.Selected();
        if (!selected) return;
        const int horizontal = (triggered(MenuRightAction) ? 1 : 0) - (triggered(MenuLeftAction) ? 1 : 0);
        if (horizontal != 0 && selected->IsAdjustable()) {
            AdjustMenuItem(selected->command, horizontal);
            return;
        }
        if (triggered(MenuConfirmAction)) {
            const auto command = selected->command;   // the items are rebuilt by what it does
            ChooseMenuItem(command, selected->slot);
        }
    }

    // A volume a tenth up or down, kept in the preferences at once
    void GameEngine::AdjustMenuItem(MenuCommand command, int direction) {
        using Systems::Audio::AudioChannel;
        if (command == MenuCommand::ScreenMargin) {   // a margin of 0 to 10% of each side, by 1%
            SetUiSafeArea(uiSafeArea_ - 0.02f * static_cast<float>(direction));
            PlaySound(GameSound::MenuMove);
            menu_.SetItems(BuildMenuItems());
            return;
        }
        const auto channel = command == MenuCommand::MusicVolume ? AudioChannel::Music
                           : command == MenuCommand::EffectsVolume ? AudioChannel::SoundEffects : AudioChannel::Master;
        const float tenths = std::round(audioSystem_.GetChannelVolume(channel) * 10.0f) + static_cast<float>(direction);
        audioSystem_.SetChannelVolume(channel, std::clamp(tenths, 0.0f, 10.0f) / 10.0f);
        SaveOptions();
        PlaySound(GameSound::MenuMove);
        menu_.SetItems(BuildMenuItems());
    }

    void GameEngine::ChooseMenuItem(MenuCommand command, std::int32_t slot) {
        PlaySound(GameSound::MenuConfirm);
        switch (command) {
        case MenuCommand::NewGame:
            menu_.Push(MenuPage::ChooseClass);
            break;
        case MenuCommand::PlayWarrior:
        case MenuCommand::PlayMage:
        case MenuCommand::PlayRogue:
        case MenuCommand::PlayCleric:
            options_.playerClass = command == MenuCommand::PlayMage ? LB::Character::CharacterClass::Mage
                                 : command == MenuCommand::PlayRogue ? LB::Character::CharacterClass::Rogue
                                 : command == MenuCommand::PlayCleric ? LB::Character::CharacterClass::Cleric : LB::Character::CharacterClass::Warrior;
            StartRun();
            CloseMenu();
            session_->playing = true;
            GrantBoons();   // what past runs unlocked
            AutoSave();     // Continue works from the first floor on
            break;
        case MenuCommand::Continue:
            StartRun();
            if (const auto latest = LatestSaveSlot(); latest && LoadGame(*latest)) CloseMenu();
            else {
                OpenMenu(MenuPage::Title);
                menu_.SetMessage("The saved game could not be loaded");
            }
            break;
        case MenuCommand::Resume:
            CloseMenu();
            break;
        case MenuCommand::SaveGame:
            menu_.SetMessage(SaveGame(QuickSaveSlot) ? "Game saved" : "The game could not be saved");
            break;
        case MenuCommand::LoadGame:
            if (const auto latest = LatestSaveSlot(); latest && LoadGame(*latest)) CloseMenu();
            else menu_.SetMessage("The saved game could not be loaded");
            break;
        case MenuCommand::Options:
            menu_.Push(MenuPage::Options);
            break;
        case MenuCommand::Controls: {
            menu_.Push(MenuPage::Controls);
            const auto rows = GetControls();
            controlsCursor_ = static_cast<std::size_t>(std::ranges::find_if(rows, [](const auto& row) { return !row.binding.empty(); }) - rows.begin());
            listening_ = false;
            break;
        }
        case MenuCommand::OpenSaveSlots:
            menu_.Push(MenuPage::SaveSlots);
            break;
        case MenuCommand::OpenLoadSlots:
            menu_.Push(MenuPage::LoadSlots);
            break;
        case MenuCommand::Legacy:
            menu_.Push(MenuPage::Legacy);
            break;
        case MenuCommand::Codex:
            menu_.Push(MenuPage::Codex);
            break;
        case MenuCommand::SaveToSlot:
            menu_.SetMessage(slot >= 0 && SaveGame(slot) ? "Game saved" : "The game could not be saved");
            break;
        case MenuCommand::LoadFromSlot:
            // From the title, over a fresh run (the one behind it is not the player's)
            if (!session_->playing) StartRun();
            if (slot >= 0 && LoadGame(slot)) CloseMenu();
            else menu_.SetMessage("The saved game could not be loaded");
            break;
        case MenuCommand::RiseAgain:
            RespawnPlayer();
            CloseMenu();
            break;
        case MenuCommand::Linger:
            CloseMenu();
            break;
        case MenuCommand::Credits:
            menu_.Push(MenuPage::Credits);
            break;
        case MenuCommand::QuitToTitle:
            AutoSave();   // the run waits for Continue
            StartRun();   // a fresh floor behind the title
            OpenMenu(MenuPage::Title);
            break;
        case MenuCommand::Quit:
            AutoSave();
            quitRequested_ = true;
            break;
        case MenuCommand::Back:
            if (!menu_.Back()) SetPaused(false);
            break;
        case MenuCommand::MasterVolume:
        case MenuCommand::MusicVolume:
        case MenuCommand::EffectsVolume:
            break;   // changed with left and right
        }
        if (menu_.IsOpen()) menu_.SetItems(BuildMenuItems());
    }

    void GameEngine::DrawMenu() {
        if (!menu_.IsOpen()) return;
        MenuModel model;
        model.titlePage = menu_.Page() == MenuPage::Title;
        model.heading = menu_.Page() == MenuPage::Pause ? "PAUSED" : menu_.Page() == MenuPage::Options ? "OPTIONS"
                      : menu_.Page() == MenuPage::Credits ? "CREDITS" : menu_.Page() == MenuPage::ChooseClass ? "CHOOSE YOUR HERO"
                      : menu_.Page() == MenuPage::Controls ? "CONTROLS" : menu_.Page() == MenuPage::Fallen ? "YOU HAVE FALLEN"
                      : menu_.Page() == MenuPage::SaveSlots ? "SAVE GAME" : menu_.Page() == MenuPage::LoadSlots ? "LOAD GAME"
                      : menu_.Page() == MenuPage::Legacy ? "LEGACY" : menu_.Page() == MenuPage::Codex ? "CODEX"
                      : menu_.Page() == MenuPage::Victory ? "VICTORY" : "";
        model.items = menu_.Items();
        model.cursor = menu_.Cursor();
        model.message = menu_.Message();
        model.art = assets_.menuArt;
        if (menu_.Page() == MenuPage::Credits) {
            model.lines = credits_;
            model.firstLine = menu_.ScrollPosition();
        }
        if (menu_.Page() == MenuPage::Controls) {
            model.controls = GetControls();
            model.firstLine = menu_.ScrollPosition();
            model.controlsCursor = controlsCursor_;
            model.listening = listening_;
        }
        if (menu_.Page() == MenuPage::Fallen) model.lines = FallenSummary();
        if (menu_.Page() == MenuPage::Victory) model.lines = VictorySummary();
        if (menu_.Page() == MenuPage::Legacy) model.lines = LegacyLines();
        if (menu_.Page() == MenuPage::Codex) {
            model.lines = CodexLines();
            model.firstLine = menu_.ScrollPosition();
        }
        MenuView::Draw(renderingSystem_, model);
    }

    // The controls page: the bound actions read from the input system (so the page follows the
    // bindings), with the stick and wheel controls the game reads directly
    std::vector<MenuModel::ControlRow> GameEngine::GetControls() const {
        using Systems::Input::InputDevice;
        // An action's keys and mouse buttons, and its controller buttons, each joined with " / "
        const auto bound = [this](std::string_view action) {
            std::string keyboard, controller;
            for (const auto& binding : inputSystem_.GetBindings(std::string(action))) {
                std::string name;
                auto* into = &keyboard;
                switch (binding.deviceType) {
                case InputDevice::Keyboard: name = Systems::Input::Keys::Name(binding.keyCode); break;
                case InputDevice::Mouse:
                    name = binding.mouseButton == Systems::Input::MouseButton::Right ? "Right click"
                         : binding.mouseButton == Systems::Input::MouseButton::Middle ? "Middle click" : "Left click";
                    break;
                case InputDevice::Gamepad:
                    name = std::string(Systems::Input::GamepadButtonName(binding.gamepadButton));
                    into = &controller;
                    break;
                default: continue;
                }
                if (into->find(name) != std::string::npos) continue;
                *into += (into->empty() ? "" : " / ") + name;
            }
            return std::pair{ keyboard.empty() ? std::string("-") : keyboard, controller.empty() ? std::string("-") : controller };
        };
        std::vector<MenuModel::ControlRow> rows;
        const auto section = [&](std::string title) { rows.push_back({ std::move(title), {}, {} }); };
        const auto action = [&](std::string what, std::string_view name) {
            auto [keyboard, controller] = bound(name);
            const bool remappable = std::ranges::find(RemappableActions, name) != RemappableActions.end()
                && std::ranges::none_of(rows, [&](const auto& row) { return row.binding == name; });   // once
            rows.push_back({ std::move(what), std::move(keyboard), std::move(controller), remappable ? std::string(name) : std::string{} });
        };
        const auto fixed = [&](std::string what, std::string keyboard, std::string controller) {
            rows.push_back({ std::move(what), std::move(keyboard), std::move(controller) });
        };

        section("EXPLORING");
        fixed("Move", "W A S D / arrows", "Left stick");
        action("Talk, trade", InteractAction);
        action("Map", MapAction);
        action("Turn the camera left", RotateCameraLeftAction);
        if (rows.back().controller == "-") rows.back().controller = "Right stick left";
        action("Turn the camera right", RotateCameraRightAction);
        if (rows.back().controller == "-") rows.back().controller = "Right stick right";
        fixed("Zoom", "Mouse wheel", "Right stick up / down");

        section("FIGHTING");
        action("Attack", AttackAction);
        std::string spellKeys, spellButtons;
        for (std::size_t slot = 0; slot < SpellSlots; ++slot) {
            const auto [keyboard, controller] = bound(SpellActions[slot]);
            spellKeys += (slot ? " " : "") + keyboard;
            spellButtons += (slot ? " " : "") + controller;
            action(std::format("Spell, ability {}", slot + 1), SpellActions[slot]);
        }
        action("Drink a potion", UsePotionAction);
        action("Allies: follow / hold", AllyStanceAction);
        action("Allies: attack", AllyFocusAction);

        section("TACTICAL PAUSE");
        action("Pause, resume", TacticalPauseAction);
        action("Walk to, attack it", OrderAction);
        const auto [leftKeys, leftButtons] = bound(MenuLeftAction);
        const auto [rightKeys, rightButtons] = bound(MenuRightAction);
        fixed("Pick an enemy", leftKeys + " / " + rightKeys, leftButtons + " / " + rightButtons);
        action("Attack it", AttackAction);
        fixed("Cast at it", spellKeys, spellButtons);

        section("SCREENS AND MENUS");
        action("Character", CharacterScreenAction);
        action("Drop an item (character)", DropAction);
        action("Pause menu", PauseAction);
        action("Quick save", QuickSaveAction);
        action("Quick load", QuickLoadAction);
        const auto [upKeys, upButtons] = bound(MenuUpAction);
        const auto [downKeys, downButtons] = bound(MenuDownAction);
        fixed("Move in menus", upKeys + " / " + downKeys, upButtons + " / " + downButtons);
        action("Choose", MenuConfirmAction);
        action("Back", MenuBackAction);
        rows.push_back({ "Reset to the defaults", {}, {}, std::string(ResetControlsRow) });
        return rows;
    }

    // The death screen's summary of the run so far
    std::vector<std::string> GameEngine::FallenSummary() const {
        const auto* player = PlayerCharacter();
        const auto* classData = content_->FindClass(session_->playerClass);
        const auto& stats = session_->stats;
        const auto seconds = static_cast<std::int64_t>(session_->gameTimeSeconds);
        return {
            std::format("{}, level {}, on depth {}", classData ? classData->name : std::string("The hero"), player ? player->GetLevel() : 1, session_->depth),
            "",
            std::format("Deepest floor reached: {}", stats.deepest),
            std::format("Enemies slain: {}", stats.kills),
            std::format("Gold found: {}", stats.goldFound),
            std::format("Time below: {}:{:02}", seconds / 60, seconds % 60),
            std::format("Falls: {}", stats.falls),
        };
    }

    // The victory page's summary of the run (design-dungeon-and-world.md 7.4)
    std::vector<std::string> GameEngine::VictorySummary() const {
        const auto* player = PlayerCharacter();
        const auto* classData = content_->FindClass(session_->playerClass);
        const auto& stats = session_->stats;
        const auto seconds = static_cast<std::int64_t>(session_->gameTimeSeconds);
        const auto& lore = content_->GetLore();
        const auto pages = std::ranges::count_if(lore, [&](const Content::LoreDefinition& entry) { return legacy_.lore.contains(entry.key); });
        return {
            std::format("{}, level {}, has silenced the Below", classData ? classData->name : std::string("The hero"), player ? player->GetLevel() : 1),
            "",
            std::format("Time below: {}:{:02}", seconds / 60, seconds % 60),
            std::format("Enemies slain: {}", stats.kills),
            std::format("Bosses defeated: {}", stats.bosses),
            std::format("Gold found: {}", stats.goldFound),
            std::format("Falls: {}", stats.falls),
            std::format("Pages of the codex: {} of {}", pages, lore.size()),
        };
    }

    // The final boss fell: a banner, the codex's last page, and the legacy's records (a run played
    // from the title; not the one behind it)
    void GameEngine::WinRun(const RunWonEvent& won) {
        const auto* page = content_->FindLore(FinalLore);
        session_->fanfare.ShowFirst(FanfareCard{ "THE BELOW IS SILENT", "The First Archivist has fallen",
                                                 page ? WrapText(page->text, 52) : std::vector<std::string>{ "A way up opens in the hall" }, { 1.0f, 0.85f, 0.45f } });
        if (session_->playing) {
            auto next = legacy_;
            ++next.victories;
            const auto seconds = std::max(1, static_cast<std::int32_t>(session_->gameTimeSeconds));
            next.fastestVictory = next.fastestVictory == 0 ? seconds : std::min(next.fastestVictory, seconds);
            next.lore.insert(std::string(FinalLore));
            UpdateLegacy(next);
        }
        PlaySound(GameSound::LevelUp, won.at);
        Jolt(0.0f, 0.6f, 0.3f);
        AutoSave();   // the win is kept
    }

    std::string GameEngine::SlotSummary(std::int32_t slot) const {
        const auto info = persistenceSystem_.GetSaveSlotInfo(slot);
        if (!info || !info->exists) return {};
        const auto minutes = info->playtimeSeconds / 60;
        const auto who = info->characterName.empty() ? std::string("A hero") : info->characterName;
        return info->location.empty() ? std::format("{}, level {} - {}:{:02}", who, info->characterLevel, minutes / 60, minutes % 60)
                                      : std::format("{}, level {}, {} - {}:{:02}", who, info->characterLevel, info->location, minutes / 60, minutes % 60);
    }

    // ------------------------------------------------------------------------------------------
    // Legacy (design-loot-and-progression.md 7.1): records of every run, boons for new heroes
    // ------------------------------------------------------------------------------------------

    // New records: kept in the profile; a boon they unlock is announced across the screen
    void GameEngine::UpdateLegacy(const Legacy& next) {
        const auto unlocked = next.NewSince(legacy_);
        legacy_ = next;
        SaveLegacy();
        for (const auto* boon : unlocked) {
            std::string name(boon->name);
            std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            session_->fanfare.Show(FanfareCard{ std::format("LEGACY: {}", name), "for every new hero", { std::format("New heroes {}", boon->gives) }, { 0.70f, 0.85f, 1.0f } });
        }
    }

    void GameEngine::SaveLegacy() {
        auto profile = persistenceSystem_.LoadProfile();
        for (const auto& [key, value] : legacy_.ToProfile()) profile[key] = value;
        persistenceSystem_.SaveProfile(profile);
    }

    // A new hero's boons: a potion, gold, an heirloom weapon (their class's, rolled uncommon)
    void GameEngine::GrantBoons() {
        auto next = legacy_;
        ++next.runs;
        next.deepest = std::max(next.deepest, session_->depth);
        UpdateLegacy(next);
        if (!session_->playerId) return;
        if (legacy_.Has(Boon::Provisions)) GiveItem(HealingPotion);
        if (legacy_.Has(Boon::OldCoin)) session_->gold += Legacy::OldCoinGold;
        if (legacy_.Has(Boon::Heirloom)) {
            // Laurels: a winner's heirs start with it rare
            const auto weapon = GetPlayerWeaponItem();
            const auto* rarity = content_->FindRarity(legacy_.Has(Boon::Laurels) ? "rarity_rare" : "rarity_uncommon");
            auto affixes = session_->lootRoller.RollAffixes(*content_, true, rarity ? std::max(1, rarity->affixes) : 1);
            if (weapon.IsValid() && rarity && !affixes.empty()) {
                const auto heirloom = session_->rolls.Add(RolledItem{ session_->rolls.BaseOf(weapon), rarity, std::move(affixes), {}, 1 }, *content_);
                GiveItemRef(heirloom);
                EquipItemRef(heirloom);   // the plain one goes to the bag
            }
        }
    }

    std::vector<std::string> GameEngine::LegacyLines() const {
        std::vector<std::string> lines{
            std::format("Deepest floor reached: {}", legacy_.deepest),
            std::format("Bosses defeated: {}", legacy_.bosses),
            std::format("Enemies slain: {}", legacy_.kills),
            std::format("Heroes sent below: {}", legacy_.runs),
            legacy_.victories > 0 ? std::format("Victories: {} (the quickest {}:{:02})", legacy_.victories, legacy_.fastestVictory / 60, legacy_.fastestVictory % 60)
                                  : std::string("Victories: none yet"),
            "",
        };
        for (const auto& boon : Boons) {
            lines.push_back(legacy_.Has(boon.boon) ? std::format("{}: {}", boon.name, boon.gives) : std::format("{} (locked): {}", boon.name, boon.requirement));
        }
        return lines;
    }

    // ------------------------------------------------------------------------------------------
    // Remapping (the Controls page)
    // ------------------------------------------------------------------------------------------

    namespace {
        bool SameInput(const Systems::Input::InputAction& a, const Systems::Input::InputAction& b) noexcept {
            using Systems::Input::InputDevice;
            if (a.deviceType != b.deviceType) return false;
            switch (a.deviceType) {
            case InputDevice::Keyboard: return a.keyCode == b.keyCode;
            case InputDevice::Mouse: return a.mouseButton == b.mouseButton;
            case InputDevice::Gamepad: return a.gamepadButton == b.gamepadButton;
            default: return false;
            }
        }

        // "k32,m1,g4": an action's bindings in the profile
        std::string BindingsText(const std::vector<Systems::Input::InputAction>& bindings) {
            using Systems::Input::InputDevice;
            std::string text;
            for (const auto& binding : bindings) {
                if (!text.empty()) text += ',';
                switch (binding.deviceType) {
                case InputDevice::Keyboard: text += std::format("k{}", binding.keyCode); break;
                case InputDevice::Mouse: text += std::format("m{}", static_cast<std::int32_t>(binding.mouseButton)); break;
                case InputDevice::Gamepad: text += std::format("g{}", static_cast<std::int32_t>(binding.gamepadButton)); break;
                default: break;
                }
            }
            return text;
        }

        std::optional<std::vector<Systems::Input::InputAction>> ParseBindings(std::string_view text) {
            using Systems::Input::InputDevice;
            std::vector<Systems::Input::InputAction> bindings;
            while (!text.empty()) {
                const auto comma = text.find(',');
                const auto part = text.substr(0, comma);
                text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
                std::int32_t value = 0;
                if (part.size() < 2) return std::nullopt;
                const auto [end, ec] = std::from_chars(part.data() + 1, part.data() + part.size(), value);
                if (ec != std::errc{} || end != part.data() + part.size() || value < 0) return std::nullopt;
                Systems::Input::InputAction binding{};
                switch (part[0]) {
                case 'k': binding.deviceType = InputDevice::Keyboard; binding.keyCode = value; break;
                case 'm':
                    if (value > static_cast<std::int32_t>(Systems::Input::MouseButton::Middle)) return std::nullopt;
                    binding.deviceType = InputDevice::Mouse;
                    binding.mouseButton = static_cast<Systems::Input::MouseButton>(value);
                    break;
                case 'g':
                    if (value > static_cast<std::int32_t>(Systems::Input::GamepadButton::DPadRight)) return std::nullopt;
                    binding.deviceType = InputDevice::Gamepad;
                    binding.gamepadButton = static_cast<Systems::Input::GamepadButton>(value);
                    break;
                default: return std::nullopt;
                }
                bindings.push_back(binding);
            }
            return bindings;
        }

        std::string ProfileKey(std::string_view action) { return std::format("controls.{}", action); }
    }

    bool GameEngine::Rebind(std::string_view action, Systems::Input::InputAction binding) {
        using Systems::Input::InputDevice;
        namespace Keys = Systems::Input::Keys;
        if (std::ranges::find(RemappableActions, action) == RemappableActions.end()) return false;
        if (binding.deviceType == InputDevice::Keyboard) {
            for (const auto reserved : { Keys::FromChar('W'), Keys::FromChar('A'), Keys::FromChar('S'), Keys::FromChar('D'),
                                         Keys::Up, Keys::Down, Keys::Left, Keys::Right, Keys::Escape }) {
                if (binding.keyCode == reserved) return false;
            }
        }
        if (binding.deviceType == InputDevice::Gamepad && binding.gamepadButton == Systems::Input::GamepadButton::Start) return false;
        const std::string name(action);
        binding.name = name;

        auto mine = inputSystem_.GetBindings(name);
        const auto primary = std::ranges::find_if(mine, [&](const auto& b) { return b.deviceType == binding.deviceType; });
        const std::optional<Systems::Input::InputAction> old = primary != mine.end() ? std::optional{ *primary } : std::nullopt;
        if (old && SameInput(*old, binding)) return true;

        // An action that had it takes the old one instead (or loses it when there was none)
        for (const auto other : RemappableActions) {
            if (other == action) continue;
            auto theirs = inputSystem_.GetBindings(std::string(other));
            bool changed = false;
            for (auto it = theirs.begin(); it != theirs.end();) {
                if (!SameInput(*it, binding)) { ++it; continue; }
                changed = true;
                if (old) { *it = *old; ++it; }
                else { it = theirs.erase(it); }
            }
            if (changed) inputSystem_.SetBindings(std::string(other), std::move(theirs));
        }
        if (primary != mine.end()) *primary = binding;
        else mine.push_back(binding);
        // No input twice (it had C and I: I to C leaves C once)
        std::vector<Systems::Input::InputAction> once;
        for (const auto& b : mine) {
            if (std::ranges::none_of(once, [&](const auto& kept) { return SameInput(kept, b); })) once.push_back(b);
        }
        inputSystem_.SetBindings(name, std::move(once));
        SaveBindings();
        return true;
    }

    void GameEngine::ResetBindings() {
        inputSystem_.ClearActions();
        RegisterDefaultBindings();
        auto profile = persistenceSystem_.LoadProfile();
        for (const auto action : RemappableActions) profile.erase(ProfileKey(action));
        persistenceSystem_.SaveProfile(profile);
    }

    // The remappable actions' bindings, every one (a swap or a second key kept as they are)
    void GameEngine::SaveBindings() {
        auto profile = persistenceSystem_.LoadProfile();
        for (const auto action : RemappableActions) profile[ProfileKey(action)] = BindingsText(inputSystem_.GetBindings(std::string(action)));
        persistenceSystem_.SaveProfile(profile);
    }

    void GameEngine::ApplyBindingProfile() {
        const auto profile = persistenceSystem_.LoadProfile();
        for (const auto action : RemappableActions) {
            const auto it = profile.find(ProfileKey(action));
            if (it == profile.end()) continue;
            if (auto bindings = ParseBindings(it->second)) inputSystem_.SetBindings(std::string(action), std::move(*bindings));
        }
    }

    // Up and down choose a row (skipping the sections' titles), Enter or A waits for the row's new
    // key or button (Esc or Start keep the old one); true when it took the step's input
    bool GameEngine::UpdateControlsPage() {
        using Systems::Input::GamepadButton;
        const auto rows = GetControls();
        if (rows.empty()) return false;
        controlsCursor_ = std::min(controlsCursor_, rows.size() - 1);
        const auto triggered = [this](std::string_view action) { return inputSystem_.IsActionTriggered(std::string(action)); };

        if (listening_) {
            const auto& row = rows[controlsCursor_];
            if (inputSystem_.IsKeyPressed(Systems::Input::Keys::Escape) || inputSystem_.IsGamepadButtonPressed(GamepadButton::Start)) {
                listening_ = false;
                menu_.SetMessage({});
                return true;
            }
            Systems::Input::InputAction binding{};
            if (const auto key = inputSystem_.FirstKeyPressed()) {
                binding.deviceType = Systems::Input::InputDevice::Keyboard;
                binding.keyCode = *key;
            }
            else if (const auto button = inputSystem_.FirstGamepadButtonPressed()) {
                binding.deviceType = Systems::Input::InputDevice::Gamepad;
                binding.gamepadButton = *button;
            }
            else {
                return true;   // still waiting
            }
            listening_ = false;
            const bool bound = Rebind(row.binding, binding);
            menu_.SetMessage(bound ? std::string{} : std::string("That one moves the hero or opens the menu"));
            PlaySound(bound ? GameSound::MenuConfirm : GameSound::ScreenClose);
            return true;
        }

        const int vertical = (triggered(MenuDownAction) ? 1 : 0) - (triggered(MenuUpAction) ? 1 : 0);
        if (vertical != 0) {
            auto next = static_cast<std::ptrdiff_t>(controlsCursor_);
            do next += vertical;
            while (next >= 0 && next < static_cast<std::ptrdiff_t>(rows.size()) && rows[static_cast<std::size_t>(next)].binding.empty());
            if (next >= 0 && next < static_cast<std::ptrdiff_t>(rows.size())) {
                controlsCursor_ = static_cast<std::size_t>(next);
                PlaySound(GameSound::MenuMove);
            }
            // Keep it in view (and the section's title above the first)
            const auto first = menu_.ScrollPosition();
            const auto wanted = controlsCursor_ < first + 1 ? (controlsCursor_ > 0 ? controlsCursor_ - 1 : 0)
                              : controlsCursor_ >= first + MenuView::ControlRowsShown ? controlsCursor_ + 1 - MenuView::ControlRowsShown : first;
            menu_.Scroll(static_cast<int>(wanted) - static_cast<int>(first), rows.size(), MenuView::ControlRowsShown);
            return true;
        }
        if (triggered(MenuConfirmAction) && !rows[controlsCursor_].binding.empty()) {
            if (rows[controlsCursor_].binding == ResetControlsRow) {
                ResetBindings();
                menu_.SetMessage("The controls are as they were");
            }
            else {
                listening_ = true;
                menu_.SetMessage({});
            }
            PlaySound(GameSound::MenuConfirm);
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------------------------------------
    // Away from the game, and the TV's edges
    // ------------------------------------------------------------------------------------------

    // The window left, the console suspending, the controller gone: a run in play waits in the
    // pause menu (not behind the title, not a hero already fallen)
    void GameEngine::PauseForInterruption() {
        if (!initialized_ || !session_->playing || menu_.IsOpen() || !IsPlayerAlive()) return;
        OpenMenu(MenuPage::Pause);
    }

    void GameEngine::SetUiSafeArea(float fraction) {
        uiSafeArea_ = std::clamp(std::round(fraction * 50.0f) / 50.0f, Systems::Rendering::RenderingSystem::MinUiSafeArea, 1.0f);
        SaveOptions();
    }

} // namespace LB::Engine
