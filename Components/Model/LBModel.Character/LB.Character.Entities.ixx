// LB.Character.Entities.ixx - Character domain entities
export module LB.Character.Entities;

import LB.Core;
import LB.Character.Enums;
import std;

export namespace LB::Character {
    // Type aliases for IDs
    using CharacterId = LB::Id;
    using SpellId = LB::Id;
    using ItemId = LB::Id;

    /// <summary>
    /// Character entity representing player characters and NPCs
    /// </summary>
    export class Character : public LB::Entity {
    private:
        CharacterClass class_;
        std::int32_t level_;
        std::int32_t experience_;
        LB::Attributes attributes_;
        std::int32_t currentHitPoints_;
        std::int32_t maxHitPoints_;
        std::int32_t currentManaPoints_;
        std::int32_t maxManaPoints_;
        std::int32_t armorClass_;
        LB::Position position_;
        std::int32_t unspentAttributePoints_{0};

    public:
        Character(CharacterId id, const LB::String& name, CharacterClass characterClass, LB::Attributes attributes)
            : Entity(id, name)
            , class_(characterClass)
            , level_(1)
            , experience_(0)
            , attributes_(attributes)
            , currentHitPoints_(20)
            , maxHitPoints_(20)
            , currentManaPoints_(0)
            , maxManaPoints_(0)
            , armorClass_(10)
            , position_(0, 0) {
        }

        // Accessors
        [[nodiscard]] CharacterClass GetClass() const noexcept { return class_; }
        [[nodiscard]] std::int32_t GetLevel() const noexcept { return level_; }
        [[nodiscard]] std::int32_t GetExperience() const noexcept { return experience_; }
        [[nodiscard]] const LB::Attributes& GetAttributes() const noexcept { return attributes_; }
        [[nodiscard]] std::int32_t GetCurrentHitPoints() const noexcept { return currentHitPoints_; }
        [[nodiscard]] std::int32_t GetMaxHitPoints() const noexcept { return maxHitPoints_; }
        [[nodiscard]] std::int32_t GetCurrentManaPoints() const noexcept { return currentManaPoints_; }
        [[nodiscard]] std::int32_t GetMaxManaPoints() const noexcept { return maxManaPoints_; }
        [[nodiscard]] std::int32_t GetArmorClass() const noexcept { return armorClass_; }
        [[nodiscard]] const LB::Position& GetPosition() const noexcept { return position_; }

        // Mutators
        void SetLevel(std::int32_t level) { level_ = level; }
        void SetExperience(std::int32_t experience) { experience_ = experience; }
        void SetAttributes(const LB::Attributes& attributes) { attributes_ = attributes; }
        void SetHitPoints(std::int32_t current, std::int32_t max) { currentHitPoints_ = current; maxHitPoints_ = max; }
        void SetManaPoints(std::int32_t current, std::int32_t max) { currentManaPoints_ = current; maxManaPoints_ = max; }
        void SetArmorClass(std::int32_t ac) { armorClass_ = ac; }
        void SetPosition(const LB::Position& pos) { position_ = pos; }

        [[nodiscard]] bool IsAlive() const noexcept { return currentHitPoints_ > 0; }

        // Attribute points earned on level-up that the player has not assigned yet
        [[nodiscard]] std::int32_t GetUnspentAttributePoints() const noexcept { return unspentAttributePoints_; }
        void SetUnspentAttributePoints(std::int32_t points) { unspentAttributePoints_ = points; }
    };

    /// <summary>
    /// Spell entity (placeholder for future spell system)
    /// </summary>
    export struct Spell {
        SpellId id;
        LB::String name;
        std::int32_t manaCost;
        std::int32_t baseDamage;
    };

} // namespace LB::Character