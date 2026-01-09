#pragma once
#include "pch.h"
#include "../../LBModel/Interfaces/IEntityRepository.h"
#include "../../Systems/LBAssetSystem/IAssetManager.h"
#include <unordered_map>
#include <memory>

namespace LB {
namespace Services {

    /// <summary>
    /// Base template implementation for entity repositories
    /// Provides common functionality for all asset-based repositories
    /// </summary>
    template<typename TEntity, typename TEntityId, typename TPrimaryCategory>
    class BaseEntityRepository : public Domain::IEntityRepository<TEntity, TEntityId, TPrimaryCategory> {
    public:
        explicit BaseEntityRepository(std::shared_ptr<Systems::IAssetManager> assetManager)
            : assetManager_(std::move(assetManager)) {}

        virtual ~BaseEntityRepository() = default;

        // Common IEntityRepository implementation
        std::optional<TEntity> GetEntity(TEntityId id) const override {
            auto it = entities_.find(id);
            if (it != entities_.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        std::vector<TEntityId> GetEntitiesByCategory(TPrimaryCategory category) const override {
            std::vector<TEntityId> result;
            for (const auto& [id, entity] : entities_) {
                if (GetEntityCategory(entity) == category) {
                    result.push_back(id);
                }
            }
            return result;
        }

        std::vector<TEntityId> GetEntitiesByClass(Domain::CharacterClass characterClass) const override {
            std::vector<TEntityId> result;
            for (const auto& [id, entity] : entities_) {
                if (CanEntityBeUsedByClass(entity, characterClass)) {
                    result.push_back(id);
                }
            }
            return result;
        }

        std::vector<TEntityId> GetEntitiesByLevelRange(int32_t minLevel, int32_t maxLevel) const override {
            std::vector<TEntityId> result;
            for (const auto& [id, entity] : entities_) {
                int32_t entityLevel = GetEntityLevel(entity);
                if (entityLevel >= minLevel && entityLevel <= maxLevel) {
                    result.push_back(id);
                }
            }
            return result;
        }

        std::vector<TEntityId> GetAllEntityIds() const override {
            std::vector<TEntityId> result;
            result.reserve(entities_.size());
            for (const auto& [id, entity] : entities_) {
                result.push_back(id);
            }
            return result;
        }

        bool EntityExists(TEntityId id) const override {
            return entities_.find(id) != entities_.end();
        }

        std::optional<String> GetCategoryDescription(TPrimaryCategory category) const override {
            // Default implementation using ToString functions
            return String(ToString(category));
        }

    protected:
        std::shared_ptr<Systems::IAssetManager> assetManager_;
        std::unordered_map<TEntityId, TEntity> entities_;
        std::unordered_map<String, TEntityId> entitiesByName_;

        /// <summary>
        /// Virtual methods that derived classes must implement for entity-specific logic
        /// </summary>
        virtual TPrimaryCategory GetEntityCategory(const TEntity& entity) const = 0;
        virtual bool CanEntityBeUsedByClass(const TEntity& entity, Domain::CharacterClass characterClass) const = 0;
        virtual int32_t GetEntityLevel(const TEntity& entity) const = 0;

        /// <summary>
        /// Helper to parse JSON content and populate entities
        /// </summary>
        virtual bool ParseEntitiesFromJSON(const String& jsonContent) = 0;

        /// <summary>
        /// Load entities from asset file
        /// </summary>
        bool LoadEntitiesFromFile(const String& fileName) {
            if (!assetManager_) {
                return false;
            }

            auto jsonContent = assetManager_->LoadTextAsset(fileName);
            if (!jsonContent) {
                return false;
            }

            return ParseEntitiesFromJSON(jsonContent.value());
        }

        /// <summary>
        /// Generate consistent ID from name using simple hash
        /// </summary>
        TEntityId GenerateEntityId(const String& name) const {
            // Simple hash function for consistent IDs
            std::hash<String> hasher;
            return static_cast<TEntityId>(hasher(name));
        }
    };

} // namespace Services
} // namespace LB