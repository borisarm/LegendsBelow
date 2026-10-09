# LegendsBelow – Weapons System Design

## 1. Purpose

Defines the weapons system for LegendsBelow, including:
- Weapon categories and slots
- Damage models and scaling
- Critical hits and special properties
- Class interactions and progression
- Data model for weapon items

---

## 2. Weapon Categories and Slots

### 2.1 Slot

Weapons occupy the `Weapon` slot; some may also use `OffHand` (dual wield, two-handed in a single slot, etc.).

- `MainHand` – primary weapon slot.
- `OffHand` – secondary weapon for dual wield or shields/focus.

### 2.2 Categories

Weapon categories define basic behavior:

- `Sword` – balanced melee, slashing.
- `Axe` – high damage, higher variance, slashing.
- `Mace/Hammer` – bludgeoning, better vs. armored targets.
- `Dagger` – low damage, high crit chance, piercing.
- `Spear/Polearm` – extended reach melee.
- `Bow` – ranged, requires arrows.
- `Crossbow` – ranged, slower but higher impact.
- `Staff` – melee + spell focus for casters.
- `Wand` – purely spell focus, low melee damage.

Each category has default:
- Damage dice
- Crit range and multiplier
- Damage type(s)
- Attack speed

---

## 3. Damage and Scaling

### 3.1 Base Damage

Weapons specify damage as a dice expression and flat bonuses:

- `damageDice` (e.g., `1d6`, `2d4`)
- `damageBonus` (flat number)

### 3.2 Attribute Scaling

Physical weapons:
- STR-scaling weapons (most swords, axes, maces):

```
PhysicalDamage = roll(damageDice) + damageBonus + STR_mod * StrScale
```

- DEX-scaling weapons (daggers, bows):

```
PhysicalDamage = roll(damageDice) + damageBonus + DEX_mod * DexScale
```

Caster weapons (staves, wands):

```
SpellChannelDamage = roll(damageDice) + damageBonus + SpellPower * SpellScale
```

`StrScale`, `DexScale`, `SpellScale` are tuning constants (e.g., 1.0 by default).

### 3.3 Attack Speed

Each weapon has an `attackTimeSeconds` value representing the base time between attacks.

- Daggers: fast (e.g., 0.7s)
- Swords: medium (e.g., 1.0s)
- Axes/Maces: slower (e.g., 1.2–1.4s)
- Bows: moderate (e.g., 1.3s)
- Crossbows: slower but heavier hits (e.g., 1.6s)

Attack speed can be modified by:
- Class passives
- Perks
- Temporary buffs/debuffs

### 3.4 Two-Handed vs One-Handed

- Two-handed weapons generally:
  - Deal ~30–40% more base damage.
  - Cannot be paired with a shield or off-hand.

Flags:
- `isTwoHanded` – if true, occupies both main and off-hand.

---

## 4. Critical Hits and Special Properties

### 4.1 Critical Hit Profiles

Each weapon type defines:
- `critRange` – natural roll range on d20 (e.g., 20; or 19–20 for daggers).
- `critMultiplier` – damage multiplier (e.g., 1.5× or 2×).

### 4.2 Special Properties

Examples:
- Daggers:
  - Higher crit range (19–20).
  - Bonus Sneak Attack damage when used by Rogue.
- Maces/Hammers:
  - Increased damage vs. heavily armored or construct enemies.
  - Small chance to stun on hit.
- Spears/Polearms:
  - Extended melee reach.
- Elemental weapons:
  - Add elemental damage component (fire, cold, lightning, acid).
  - Chance to apply a matching status effect.

Properties should be data-driven per weapon entry.

---

## 5. Class Interactions and Proficiencies

Defaults (can be extended via perks):

- Warrior:
  - Proficient with all melee weapons and basic ranged.
  - Gains extra damage with two-handed and heavy weapons.
- Rogue:
  - Proficient with daggers, short swords, bows, crossbows.
  - Bonus damage from behind and when in stealth.
- Mage:
  - Proficient with staves, wands.
  - Minimal physical damage; primarily uses spells.
- Cleric:
  - Proficient with maces, hammers, some staves.
  - Gains bonuses when using blunt weapons vs. undead.

Using weapons without proficiency:
- Apply to-hit penalties and/or stamina penalties.
- Optionally restrict access completely.

---

## 6. Weapon Progression and Tiers

### 6.1 Level Tiers

Weapons are grouped into tiers similar to armour:

- Tier 1: levels 1–5
- Tier 2: levels 6–10
- Tier 3: levels 11–15
- Tier 4: levels 16–20

Each tier increases base damage and may unlock new special properties.

Example (Swords):

| Tier | Example Name       | Required Level | Damage Dice | Damage Bonus | CritRange | CritMult |
|------|--------------------|----------------|------------:|------------: |---------: |--------:|
| 1    | Rusted Shortsword  | 1              | 1d6         | 0           | 20        | 1.5×    |
| 1    | Iron Longsword     | 4              | 1d8         | 1           | 20        | 1.5×    |
| 2    | Steel Longsword    | 7              | 1d8         | 3           | 20        | 1.5×    |
| 2    | Knight's Blade     | 10             | 1d10        | 2           | 19–20     | 1.5×    |

### 6.2 Rarity Integration

Rarity tiers from the loot system modify base weapons:

- Common – base stats only.
- Uncommon – +small flat damage, minor stat bonus.
- Rare – +moderate damage, 1–2 affixes.
- Epic – stronger base damage, 2–3 powerful affixes.
- Legendary – unique effects and named items.

Affixes can add:
- Elemental damage
- Lifesteal or mana on hit
- Crit chance and crit damage
- Class-specific bonuses

---

## 7. Ranged Weapons and Ammo

### 7.1 Ammo Types

- Arrows – used by bows.
- Bolts – used by crossbows.

Ammo properties:
- Base damage modifier (adds or multiplies base weapon damage).
- Damage type (physical, elemental).
- On-hit effects (bleed, poison, etc.).

If ammo is limited, attacks consume one unit per shot; if infinite, treat ammo as a bound property of the weapon only.

### 7.2 Minimum and Maximum Range

- Bows and crossbows may have reduced accuracy or cannot be used in melee range.
- Per-weapon or per-category settings define min and max ranges.

---

## 8. Data Model

Fields for a weapon item record:

- `id`
- `name`
- `category` – `Sword`, `Axe`, etc.
- `slot` – `MainHand`, optionally flags for `isTwoHanded`
- `requiredLevel`
- `allowedClasses`
- `damageDice`
- `damageBonus`
- `damageType` (or list)
- `attackTimeSeconds`
- `critRange`
- `critMultiplier`
- `attributeScale` (e.g., `STR`, `DEX`, `INT`)
- `scaleFactor` (float)
- `specialProperties` (list of tags/IDs)
- `affixSlots`

Example JSON:

```
{
  "id": "weapon_sword_iron_longsword",
  "name": "Iron Longsword",
  "category": "Sword",
  "slot": "MainHand",
  "requiredLevel": 4,
  "allowedClasses": ["Warrior", "Rogue", "Cleric"],
  "damageDice": "1d8",
  "damageBonus": 1,
  "damageType": ["Slashing"],
  "attackTimeSeconds": 1.0,
  "critRange": "20",
  "critMultiplier": 1.5,
  "attributeScale": "STR",
  "scaleFactor": 1.0,
  "specialProperties": [],
  "affixSlots": 2
}
```

C++ representation should use `enum class` for category/slot/damage type, and strongly-typed helpers for dice expressions.
