// LB.Entity.ixx - Base entity class for all domain entities
export module LB.Entity;

import LB.Common;
import std;

export namespace LB {
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
        Entity(Id id, String name) noexcept
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
        [[nodiscard]] Id GetId() const noexcept { return id_; }
        [[nodiscard]] const String& GetName() const noexcept { return name_; }

        // Mutators
        void SetName(const String& name) { name_ = name; }

        // Equality based on ID
        [[nodiscard]] bool operator==(const Entity& other) const noexcept {
            return id_ == other.id_;
        }

        [[nodiscard]] bool operator!=(const Entity& other) const noexcept {
            return !(*this == other);
        }
    };

} // namespace LB
            return id_ != other.id_;
        }
    };
}
