// Entity.ixx - LB.Core partition: the base class of domain entities
export module LB.Core:Entity;

import :Common;
import std;

export namespace LB {
	/// <summary>
	/// Base class for all game entities: identity and name. A plain value type, not a polymorphic
	/// base (no virtual functions, never deleted through an Entity pointer), so it has no vtable
	/// and copying a derived entity into an Entity only keeps its identity and name.
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
	};

} // namespace LB
