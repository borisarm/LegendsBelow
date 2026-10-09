// PersistenceSystem.cpp - LB.PersistenceSystem implementation unit (JSON files on disk)
module;

#include <nlohmann/json.hpp>

module LB.PersistenceSystem;

import std;

namespace LB::Systems::Persistence {

    namespace {
        using Json = nlohmann::json;

        // Bump when the save layout changes; older files are still read field by field
        constexpr int32_t SaveFormatVersion = 1;

        std::filesystem::path FromUtf8(const String& path) {
            return std::filesystem::path(std::u8string(path.begin(), path.end()));
        }

        String ToUtf8(const std::filesystem::path& path) {
            const std::u8string text = path.u8string();
            return String(text.begin(), text.end());
        }

        String CurrentTimestampUtc() {
            const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
            return std::format("{:%Y-%m-%dT%H:%M:%SZ}", now);
        }

        // Write via a temporary file and rename, so an interrupted write never corrupts
        // the previous file
        bool WriteJsonFile(const std::filesystem::path& path, const Json& json) {
            const auto tempPath = std::filesystem::path(path).concat(".tmp");
            {
                std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
                if (!out) return false;
                out << json.dump(2);
                if (!out.good()) return false;
            }

            std::error_code ec;
            std::filesystem::rename(tempPath, path, ec);
            if (ec) {
                std::filesystem::remove(tempPath, ec);
                return false;
            }
            return true;
        }

        std::optional<Json> ReadJsonFile(const std::filesystem::path& path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) return std::nullopt;

            Json json = Json::parse(in, nullptr, /*allow_exceptions*/ false);
            if (json.is_discarded() || !json.is_object()) return std::nullopt;
            return json;
        }

        Json SlotToJson(const SaveSlotInfo& info, const SaveData& data) {
            return Json{
                {"version", SaveFormatVersion},
                {"slotIndex", info.slotIndex},
                {"saveName", info.saveName},
                {"timestamp", info.timestamp},
                {"characterName", info.characterName},
                {"characterLevel", info.characterLevel},
                {"location", info.location},
                {"playtimeSeconds", info.playtimeSeconds},
                {"data", data},
            };
        }

        SaveSlotInfo SlotInfoFromJson(const Json& json) {
            SaveSlotInfo info{};
            info.slotIndex = json.value("slotIndex", 0);
            info.saveName = json.value("saveName", String{});
            info.timestamp = json.value("timestamp", String{});
            info.characterName = json.value("characterName", String{});
            info.characterLevel = json.value("characterLevel", 1);
            info.location = json.value("location", String{});
            info.playtimeSeconds = json.value("playtimeSeconds", 0);
            info.exists = true;
            return info;
        }

        SaveData SaveDataFromJson(const Json& json) {
            SaveData data;
            auto it = json.find("data");
            if (it == json.end() || !it->is_object()) return data;

            for (const auto& [key, value] : it->items()) {
                if (value.is_string()) data[key] = value.get<String>();
            }
            return data;
        }

        Json PreferencesToJson(const PlayerPreferences& prefs) {
            return Json{
                {"masterVolume", prefs.masterVolume},
                {"musicVolume", prefs.musicVolume},
                {"sfxVolume", prefs.sfxVolume},
                {"fullscreen", prefs.fullscreen},
                {"resolutionWidth", prefs.resolutionWidth},
                {"resolutionHeight", prefs.resolutionHeight},
                {"language", prefs.language},
            };
        }

        PlayerPreferences PreferencesFromJson(const Json& json) {
            const PlayerPreferences defaults{};
            PlayerPreferences prefs{};
            prefs.masterVolume = json.value("masterVolume", defaults.masterVolume);
            prefs.musicVolume = json.value("musicVolume", defaults.musicVolume);
            prefs.sfxVolume = json.value("sfxVolume", defaults.sfxVolume);
            prefs.fullscreen = json.value("fullscreen", defaults.fullscreen);
            prefs.resolutionWidth = json.value("resolutionWidth", defaults.resolutionWidth);
            prefs.resolutionHeight = json.value("resolutionHeight", defaults.resolutionHeight);
            prefs.language = json.value("language", defaults.language);
            return prefs;
        }
    }

    bool PersistenceSystem::Initialize(const String& saveDirectory) {
        saveSlots_.clear();
        saveData_.clear();
        preferences_ = PlayerPreferences{};
        saveDirectory_ = saveDirectory.empty() ? std::filesystem::path{} : FromUtf8(saveDirectory);

        if (!saveDirectory_.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(saveDirectory_, ec);
            if (ec || !std::filesystem::is_directory(saveDirectory_)) {
                saveDirectory_.clear();
                return false;
            }

            // Discover existing saves; unreadable files are skipped rather than failing startup
            for (int32_t slot = 0; slot < maxSaveSlots_; ++slot) {
                if (auto json = ReadJsonFile(GetSaveFilePath(slot))) {
                    SaveSlotInfo info = SlotInfoFromJson(*json);
                    info.slotIndex = slot;
                    saveSlots_[slot] = info;
                }
            }

            if (auto json = ReadJsonFile(GetPreferencesFilePath())) {
                preferences_ = PreferencesFromJson(*json);
            }
        }

        initialized_ = true;
        return true;
    }

    SaveResult PersistenceSystem::SaveGame(int32_t slotIndex, const SaveSlotInfo& metadata, const SaveData& data) {
        SaveResult result{};

        if (!initialized_) {
            result.errorMessage = "Persistence system not initialized";
            return result;
        }

        if (!IsValidSlot(slotIndex)) {
            result.errorMessage = "Invalid save slot index";
            return result;
        }

        SaveSlotInfo info = metadata;
        info.slotIndex = slotIndex;
        info.timestamp = CurrentTimestampUtc();
        info.exists = true;

        if (IsPersistentStorage()) {
            const auto path = GetSaveFilePath(slotIndex);
            if (!WriteJsonFile(path, SlotToJson(info, data))) {
                result.errorMessage = "Failed to write save file";
                return result;
            }
            result.filePath = ToUtf8(path);
        }
        else {
            saveData_[slotIndex] = data;
        }

        saveSlots_[slotIndex] = info;
        result.success = true;
        return result;
    }

    LoadResult PersistenceSystem::LoadGame(int32_t slotIndex) {
        LoadResult result{};

        if (!initialized_) {
            result.errorMessage = "Persistence system not initialized";
            return result;
        }

        if (!IsValidSlot(slotIndex)) {
            result.errorMessage = "Invalid save slot index";
            return result;
        }

        if (!SaveSlotExists(slotIndex)) {
            result.errorMessage = "Save slot does not exist";
            return result;
        }

        if (IsPersistentStorage()) {
            auto json = ReadJsonFile(GetSaveFilePath(slotIndex));
            if (!json) {
                result.errorMessage = "Save file is missing or corrupt";
                return result;
            }
            result.slotInfo = SlotInfoFromJson(*json);
            result.slotInfo.slotIndex = slotIndex;
            result.data = SaveDataFromJson(*json);
        }
        else {
            result.slotInfo = saveSlots_[slotIndex];
            result.data = saveData_[slotIndex];
        }

        result.success = true;
        return result;
    }

    bool PersistenceSystem::DeleteSave(int32_t slotIndex) {
        if (!initialized_) return false;

        auto it = saveSlots_.find(slotIndex);
        if (it == saveSlots_.end()) return false;

        if (IsPersistentStorage()) {
            std::error_code ec;
            std::filesystem::remove(GetSaveFilePath(slotIndex), ec);
            if (ec) return false;
        }

        saveSlots_.erase(it);
        saveData_.erase(slotIndex);
        return true;
    }

    bool PersistenceSystem::SavePreferences(const PlayerPreferences& prefs) {
        if (!initialized_) return false;

        if (IsPersistentStorage() && !WriteJsonFile(GetPreferencesFilePath(), PreferencesToJson(prefs))) {
            return false;
        }

        preferences_ = prefs;
        return true;
    }

    std::filesystem::path PersistenceSystem::GetSaveFilePath(int32_t slotIndex) const {
        return saveDirectory_ / std::format("save_{}.json", slotIndex);
    }

    std::filesystem::path PersistenceSystem::GetPreferencesFilePath() const {
        return saveDirectory_ / "preferences.json";
    }

} // namespace LB::Systems::Persistence
