#pragma once
#include "pch.h"

namespace LB {
namespace Domain {

    /// <summary>
    /// Base class for all domain entities with identity
    /// Provides common ID functionality and comparison semantics
    /// </summary>
    class Entity {
    public:
        /// <summary>
        /// Create entity with specific ID (for loading from assets)
        /// </summary>
        explicit constexpr Entity(Id id) noexcept : id_(id) {}

        /// <summary>
        /// Create entity with generated ID (for runtime creation)
        /// </summary>
        Entity() noexcept : id_(GenerateId()) {}

        // Make entities non-copyable but movable
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;
        Entity(Entity&&) = default;
        Entity& operator=(Entity&&) = default;

        virtual ~Entity() = default;

        /// <summary>
        /// Get the unique identifier for this entity
        /// </summary>
        [[nodiscard]] constexpr Id GetId() const noexcept { return id_; }

        /// <summary>
        /// Entities are equal if they have the same ID
        /// </summary>
        [[nodiscard]] bool operator==(const Entity& other) const noexcept {
            return id_ == other.id_;
        }

        [[nodiscard]] bool operator!=(const Entity& other) const noexcept {
            return !(*this == other);
        }

    protected:
        /// <summary>
        /// Allow derived classes to change ID (for deserialization scenarios)
        /// </summary>
        void SetId(Id newId) noexcept { id_ = newId; }

    private:
        Id id_;

        /// <summary>
        /// Generate a new unique ID (simple incrementing counter for now)
        /// In a full implementation, this would be more sophisticated
        /// </summary>
        static Id GenerateId() noexcept {
            static std::atomic<Id> counter{1};
            return counter.fetch_add(1);
        }
    };

} // namespace Domain
} // namespace LB

// Hash function for Entity-based types to use in unordered containers
namespace std {
    template<>
    struct hash<LB::Domain::Entity> {
        std::size_t operator()(const LB::Domain::Entity& entity) const noexcept {
            return std::hash<LB::Id>{}(entity.GetId());
        }
    };
}