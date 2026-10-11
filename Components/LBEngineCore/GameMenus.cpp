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
        if (!errors.empty()) {
            initializationError_ = std::format("UI images in '{}' could not be loaded ({} problems):", uiDirectory, errors.size());
            for (const auto& error : errors) initializationError_ += "\n" + error;
            return false;
        }
        assets_.menuArt = art;
        return true;
    }

    void GameEngine::OpenMenu(MenuPage page) {
        menu_.Open(page);
        SetPaused(true);
        menu_.SetItems(BuildMenuItems());
    }

    void GameEngine::CloseMenu() {
        menu_.Close();
        SetPaused(false);
    }

    std::vector<MenuItem> GameEngine::BuildMenuItems() const {
        const bool saved = persistenceSystem_.SaveSlotExists(QuickSaveSlot);
        const auto percent = [this](Systems::Audio::AudioChannel channel) {
            return std::format("{}%", static_cast<int>(std::lround(audioSystem_.GetChannelVolume(channel) * 100.0f)));
        };
        using Systems::Audio::AudioChannel;
        switch (menu_.Page()) {
        case MenuPage::Title:
            return {
                { "New Game", MenuCommand::NewGame },
                { "Continue", MenuCommand::Continue, saved },
                { "Options", MenuCommand::Options },
                { "Credits", MenuCommand::Credits },
                { "Quit", MenuCommand::Quit },
            };
        case MenuPage::Pause:
            return {
                { "Resume", MenuCommand::Resume },
                { "Save Game", MenuCommand::SaveGame },
                { "Load Game", MenuCommand::LoadGame, saved },
                { "Options", MenuCommand::Options },
                { "Credits", MenuCommand::Credits },
                { "Quit to Title", MenuCommand::QuitToTitle },
            };
        case MenuPage::Options:
            return {
                { "Master volume", MenuCommand::MasterVolume, true, percent(AudioChannel::Master) },
                { "Music volume", MenuCommand::MusicVolume, true, percent(AudioChannel::Music) },
                { "Effects volume", MenuCommand::EffectsVolume, true, percent(AudioChannel::SoundEffects) },
                { "Back", MenuCommand::Back },
            };
        case MenuPage::Credits:
            return { { "Back", MenuCommand::Back } };
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

        if (triggered(MenuBackAction) || triggered(PauseAction)) {
            if (menu_.Page() == MenuPage::Title) return;   // nowhere to go back to
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
            ChooseMenuItem(command);
        }
    }

    // A volume a tenth up or down, kept in the preferences at once
    void GameEngine::AdjustMenuItem(MenuCommand command, int direction) {
        using Systems::Audio::AudioChannel;
        const auto channel = command == MenuCommand::MusicVolume ? AudioChannel::Music
                           : command == MenuCommand::EffectsVolume ? AudioChannel::SoundEffects : AudioChannel::Master;
        const float tenths = std::round(audioSystem_.GetChannelVolume(channel) * 10.0f) + static_cast<float>(direction);
        audioSystem_.SetChannelVolume(channel, std::clamp(tenths, 0.0f, 10.0f) / 10.0f);
        SaveVolumePreferences();
        PlaySound(GameSound::MenuMove);
        menu_.SetItems(BuildMenuItems());
    }

    void GameEngine::ChooseMenuItem(MenuCommand command) {
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
            break;
        case MenuCommand::Continue:
            StartRun();
            if (LoadGame(QuickSaveSlot)) CloseMenu();
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
            if (LoadGame(QuickSaveSlot)) CloseMenu();
            else menu_.SetMessage("The saved game could not be loaded");
            break;
        case MenuCommand::Options:
            menu_.Push(MenuPage::Options);
            break;
        case MenuCommand::Credits:
            menu_.Push(MenuPage::Credits);
            break;
        case MenuCommand::QuitToTitle:
            StartRun();   // a fresh floor behind the title
            OpenMenu(MenuPage::Title);
            break;
        case MenuCommand::Quit:
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
                      : menu_.Page() == MenuPage::Credits ? "CREDITS" : menu_.Page() == MenuPage::ChooseClass ? "CHOOSE YOUR HERO" : "";
        model.items = menu_.Items();
        model.cursor = menu_.Cursor();
        model.message = menu_.Message();
        model.art = assets_.menuArt;
        if (menu_.Page() == MenuPage::Credits) {
            model.lines = credits_;
            model.firstLine = menu_.ScrollPosition();
        }
        MenuView::Draw(renderingSystem_, model);
    }

} // namespace LB::Engine
