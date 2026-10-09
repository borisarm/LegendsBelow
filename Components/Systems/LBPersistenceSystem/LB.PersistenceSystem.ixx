// LB.PersistenceSystem.ixx - Save/load and data persistence system
export module LB.PersistenceSystem;

import LB.Core;
import std;

using namespace std;

/// <summary>
/// Persistence system for LegendsBelow
/// Handles save slots and player preferences. With a save directory, everything is stored as
/// JSON files there (written atomically); with an empty directory it is kept in memory only.
/// The system knows nothing about game types: callers store their state as SaveData.
/// </summary>
export namespace LB::Systems::Persistence {
    using LB::String;

    /// <summary>
    /// Game state payload of a save: key/value pairs owned by the caller
    /// (e.g. "gameTimeSeconds" -> "12.5")
    /// </summary>
    using SaveData = std::map<String, String>;

    /// <summary>
    /// Save slot information
    /// </summary>
    struct SaveSlotInfo {
        int32_t slotIndex{0};
        String saveName;
        String timestamp;           // UTC, ISO 8601 (set when saved)
        int32_t characterLevel{1};
        String characterName;
        String location;
        int32_t playtimeSeconds{0};
        bool exists{false};
    };

    /// <summary>
    /// Save operation result
    /// </summary>
    struct SaveResult {
        bool success{false};
        String errorMessage;
        String filePath;
    };

    /// <summary>
    /// Load operation result
    /// </summary>
    struct LoadResult {
        bool success{false};
        String errorMessage;
        SaveSlotInfo slotInfo;
        SaveData data;
    };

    /// <summary>
    /// Player preferences/settings
    /// </summary>
    struct PlayerPreferences {
        float masterVolume{1.0f};
        float musicVolume{0.7f};
        float sfxVolume{1.0f};
        bool fullscreen{true};
        int32_t resolutionWidth{1920};
        int32_t resolutionHeight{1080};
        String language{"en-US"};
    };

    /// <summary>
    /// Persistence system for save/load operations
    /// </summary>
    class PersistenceSystem {
    public:
        PersistenceSystem() = default;
        ~PersistenceSystem() = default;

        /// <summary>
        /// Initialize the persistence system. saveDirectory is a UTF-8 path, created if missing;
        /// existing saves in it are discovered. An empty directory keeps everything in memory.
        /// Returns false if the directory cannot be created.
        /// </summary>
        bool Initialize(const String& saveDirectory);

        /// <summary>
        /// Shutdown the persistence system (files on disk are kept)
        /// </summary>
        void Shutdown() {
            saveSlots_.clear();
            saveData_.clear();
            preferences_ = PlayerPreferences{};
            initialized_ = false;
        }

        /// <summary>
        /// Save to a slot with only a name (no game state payload)
        /// </summary>
        SaveResult SaveGame(int32_t slotIndex, const String& saveName) {
            SaveSlotInfo info{};
            info.saveName = saveName;
            return SaveGame(slotIndex, info, SaveData{});
        }

        /// <summary>
        /// Save game state to a slot. slotIndex, timestamp and exists are set by the system.
        /// </summary>
        SaveResult SaveGame(int32_t slotIndex, const SaveSlotInfo& metadata, const SaveData& data);

        /// <summary>
        /// Load game state from a slot
        /// </summary>
        LoadResult LoadGame(int32_t slotIndex);

        /// <summary>
        /// Delete a save slot (and its file)
        /// </summary>
        bool DeleteSave(int32_t slotIndex);

        /// <summary>
        /// Get information about a save slot
        /// </summary>
        std::optional<SaveSlotInfo> GetSaveSlotInfo(int32_t slotIndex) const {
            auto it = saveSlots_.find(slotIndex);
            if (it == saveSlots_.end()) return std::nullopt;
            return it->second;
        }

        /// <summary>
        /// Get all save slots, ordered by slot index
        /// </summary>
        std::vector<SaveSlotInfo> GetAllSaveSlots() const {
            std::vector<SaveSlotInfo> slots;
            slots.reserve(saveSlots_.size());
            for (const auto& [index, info] : saveSlots_) {
                slots.push_back(info);
            }
            return slots;
        }

        /// <summary>
        /// Check if a save slot exists
        /// </summary>
        bool SaveSlotExists(int32_t slotIndex) const {
            auto it = saveSlots_.find(slotIndex);
            return it != saveSlots_.end() && it->second.exists;
        }

        /// <summary>
        /// Save player preferences
        /// </summary>
        bool SavePreferences(const PlayerPreferences& prefs);

        /// <summary>
        /// Load player preferences (defaults if none were saved)
        /// </summary>
        PlayerPreferences LoadPreferences() const {
            if (!initialized_) return PlayerPreferences{};
            return preferences_;
        }

        /// <summary>
        /// Get maximum number of save slots
        /// </summary>
        int32_t GetMaxSaveSlots() const {
            return maxSaveSlots_;
        }

        /// <summary>
        /// True when saves are stored on disk (false when in memory only)
        /// </summary>
        [[nodiscard]] bool IsPersistentStorage() const noexcept {
            return !saveDirectory_.empty();
        }

        /// <summary>
        /// Check if system is initialized
        /// </summary>
        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized_;
        }

    private:
        bool IsValidSlot(int32_t slotIndex) const noexcept {
            return slotIndex >= 0 && slotIndex < maxSaveSlots_;
        }

        std::filesystem::path GetSaveFilePath(int32_t slotIndex) const;
        std::filesystem::path GetPreferencesFilePath() const;

        bool initialized_{false};
        std::filesystem::path saveDirectory_;
        int32_t maxSaveSlots_{10};
        std::map<int32_t, SaveSlotInfo> saveSlots_;
        std::map<int32_t, SaveData> saveData_;   // in-memory payloads (memory-only mode)
        PlayerPreferences preferences_{};
    };

} // namespace LB::Systems::Persistence
