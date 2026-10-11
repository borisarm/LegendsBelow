// LB.AssetSystem.ixx - Asset loading and management system
export module LB.AssetSystem;

import LB.Core;
import std;

/// <summary>
/// Asset system for LegendsBelow
/// Handles loading, caching, and lifetime management of game assets
/// Supports textures, audio, fonts, and data files
/// </summary>
export namespace LB::Systems::Asset {
    using LB::String;

    /// <summary>
    /// Asset type enumeration
    /// </summary>
    enum class AssetType {
        Unknown = 0,
        Texture,
        Audio,
        Font,
        Data,
        Shader,
        Model
    };

    /// <summary>
    /// Asset load status
    /// </summary>
    enum class AssetStatus {
        Unloaded = 0,
        Loading,
        Loaded,
        Failed
    };

    /// <summary>
    /// Asset metadata
    /// </summary>
    struct AssetInfo {
        String id;
        String path;
        AssetType type{AssetType::Unknown};
        AssetStatus status{AssetStatus::Unloaded};
        std::size_t sizeBytes{0};

        [[nodiscard]] bool IsLoaded() const noexcept {
            return status == AssetStatus::Loaded;
        }
    };

    /// <summary>
    /// Asset system for managing game resources
    /// </summary>
    class AssetSystem {
    public:
        AssetSystem() = default;
        ~AssetSystem() = default;

        /// <summary>
        /// Initialize the asset system
        /// </summary>
        bool Initialize(const String& assetRootPath) {
            assetRootPath_ = assetRootPath;
            initialized_ = true;
            return true;
        }

        /// <summary>
        /// Shutdown the asset system and release all resources
        /// </summary>
        void Shutdown() {
            UnloadAllAssets();
            initialized_ = false;
        }

        /// <summary>
        /// Load an asset by ID
        /// </summary>
        bool LoadAsset(const String& assetId, AssetType type) {
            if (!initialized_) return false;

            AssetInfo info{};
            info.id = assetId;
            info.type = type;
            info.status = AssetStatus::Loading;

            // TODO: Implement actual asset loading
            info.status = AssetStatus::Loaded;

            assets_[assetId] = info;
            return true;
        }

        /// <summary>
        /// Unload an asset by ID
        /// </summary>
        bool UnloadAsset(const String& assetId) {
            auto it = assets_.find(assetId);
            if (it == assets_.end()) return false;

            // TODO: Implement actual resource cleanup
            assets_.erase(it);
            return true;
        }

        /// <summary>
        /// Unload all assets
        /// </summary>
        void UnloadAllAssets() {
            assets_.clear();
        }

        /// <summary>
        /// Check if an asset is loaded
        /// </summary>
        bool IsAssetLoaded(const String& assetId) const {
            auto it = assets_.find(assetId);
            if (it == assets_.end()) return false;
            return it->second.IsLoaded();
        }

        /// <summary>
        /// Get asset information
        /// </summary>
        std::optional<AssetInfo> GetAssetInfo(const String& assetId) const {
            auto it = assets_.find(assetId);
            if (it == assets_.end()) return std::nullopt;
            return it->second;
        }

        /// <summary>
        /// Get total memory usage of loaded assets
        /// </summary>
        std::size_t GetTotalMemoryUsage() const {
            std::size_t total = 0;
            for (const auto& [id, info] : assets_) {
                if (info.IsLoaded()) {
                    total += info.sizeBytes;
                }
            }
            return total;
        }

        /// <summary>
        /// Check if system is initialized
        /// </summary>
        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized_;
        }

    private:
        bool initialized_{false};
        String assetRootPath_;
        std::unordered_map<String, AssetInfo> assets_;
    };

} // namespace LB::Systems::Asset