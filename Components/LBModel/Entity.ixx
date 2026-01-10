export module LB.Entities:Entity;

import LB.Common;
import LB.Enums;
import LB.ValueObjects;


export namespace LB::Domain {
    // Type aliases
    using SpellId = Id;
    using WeaponId = Id;
    using ArmorId = Id;
    using CharacterId = Id;
    using ItemId = Id;
    /// <summary>
    /// Base class for all game entities
    /// Provides common identity and naming functionality
    /// </summary>
    export class Entity {
    protected:
        Id id_;
        String name_;

    public:
        /// <summary>
        /// Create entity with unique ID and name
        /// </summary>
        constexpr Entity(Id id, String name) noexcept
            : id_(id), name_(std::move(name)) {
        }

        // Virtual destructor for polymorphism
        virtual ~Entity() = default;

        // Copy and move constructors
        Entity(const Entity&) = default;
        Entity& operator=(const Entity&) = default;
        Entity(Entity&&) noexcept = default;
        Entity& operator=(Entity&&) noexcept = default;

        // Accessors
        [[nodiscard]] constexpr Id GetId() const noexcept { return id_; }
        [[nodiscard]] const String& GetName() const noexcept { return name_; }

        // Equality based on ID
        [[nodiscard]] constexpr bool operator==(const Entity& other) const noexcept {
            return id_ == other.id_;
        }

        [[nodiscard]] constexpr bool operator!=(const Entity& other) const noexcept {
            return id_ != other.id_;
        }
    };
}
