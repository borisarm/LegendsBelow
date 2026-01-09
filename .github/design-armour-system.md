# LegendsBelow – Armour System Design

## 1. Purpose

Defines the armour system for LegendsBelow, including:
- Armour categories and slots
- Armour ratings and damage mitigation
- Movement and stamina penalties
- Class interactions and progression
- Data model for armour items

This document should guide both gameplay tuning and C++/data implementation.

---

## 2. Armour Categories and Slots

### 2.1 Slots

Armour occupies these core equipment slots:

- `Head` – helmets, hoods, circlets
- `Chest` – cuirasses, robes, leathers
- `Hands` – gloves, gauntlets
- `Feet` – boots, greaves
- `OffHand` – shields, tomes, foci (also considered in armour system)

### 2.2 Armour Categories

Armour pieces are assigned to broad categories:

- `Cloth` – minimal physical protection, no movement penalty
- `Light` – leathers and padded armour, low penalties
- `Medium` – chain and scale, noticeable penalties
- `Heavy` – plate, highest protection and penalties
- `Shield` – separate category that stacks with body armour

Each category defines:

- Base physical mitigation
- Typical weight range
- Stealth and movement impacts
- Class proficiency requirements (ref: character design)

---

## 3. Armour Rating and Damage Mitigation

### 3.1 Armour Rating (AR)

Each armour piece provides:

- `PhysicalAR` – mitigates physical damage (slashing, piercing, bludgeoning)
- `ElementalAR` – may mitigate elemental damage (fire, cold, lightning, acid)
- `SpiritualAR` – rare; mitigates radiant/necrotic/psychic damage

The character’s total AR is the sum of piece contributions plus any buffs:

```
TotalPhysicalAR = sum(piece.PhysicalAR) + BuffsPhysicalAR
TotalElementalAR[type] = sum(piece.ElementalAR[type]) + BuffsElementalAR[type]
TotalSpiritualAR[type] = sum(piece.SpiritualAR[type]) + BuffsSpiritualAR[type]
```

### 3.2 Mitigation Formula

For physical hits:

```
DamageAfterArmor = max(0, RawDamage - TotalPhysicalAR)
```

Optionally, for higher levels we can introduce diminishing returns:

```
EffectiveReduction = TotalPhysicalAR / (TotalPhysicalAR + K)
FinalDamage = RawDamage * (1.0f - EffectiveReduction)
```

Where `K` is a tuning constant. Initially we use the simpler subtractive model to keep behavior predictable.

For elemental/spiritual damage, armour typically contributes to resistances rather than flat subtraction (see combat design):

- Armour may add to `Resist[DamageType]` percentage modifiers.

---

## 4. Weight, Movement, and Stamina

### 4.1 Weight

Each piece has:

- `Weight` – numeric value representing encumbrance
- `Category` – Cloth/Light/Medium/Heavy/Shield

Character has a `CarryCapacity` derived from STR and background.

### 4.2 Movement Penalty

Movement speed penalty based on armour category and total weight:

- Cloth: 0% base penalty
- Light: -5% base movement speed
- Medium: -10% base movement speed
- Heavy: -15% base movement speed
- Shields: additional -5% when using heavy shields

Movement formula extension:

```
BaseSpeed = ClassBaseSpeed + DEX_mod * 0.1
WeightRatio = TotalWeight / CarryCapacity

WeightPenalty = clamp(WeightRatio - 0.5, 0.0, 0.5) * 0.5    // up to -25%
CategoryPenalty = sum(piece.CategoryPenalty)

TilesPerSecond = BaseSpeed * (1.0 - CategoryPenalty) * (1.0 - WeightPenalty)
```

### 4.3 Stamina and Casting Impacts

- Heavy/Medium armour increases stamina cost of dodge/block actions.
- Casting in Heavy armour may incur:
  - Longer cast times for spells
  - Or a small chance of spell fizzle (Cleric/Mage specifics in spells design)

These penalties should be data-driven per armour category.

---

## 5. Class Proficiencies and Restrictions

Baseline:

- Warrior: Cloth, Light, Medium, Heavy, Shields
- Rogue: Cloth, Light, (optional Medium chest only), small Shields (bucklers)
- Mage: Cloth only, no Shields (uses off-hand foci instead)
- Cleric: Cloth, Light, Medium, Heavy (depending on lore), Shields

If a character equips armour they are not proficient with:

- Apply a hit chance penalty and/or spellcasting penalties.
- Optionally show a UI warning.

Proficiency is stored per-class and can be extended by perks/feats.

---

## 6. Armour Progression and Tiers

### 6.1 Level Tiers

Armour is grouped into level bands:

- Tier 1: levels 1–5
- Tier 2: levels 6–10
- Tier 3: levels 11–15
- Tier 4: levels 16–20

Each tier increases base AR and weight slightly.

Example (Chest, Warrior-focused):

| Tier | Category | Example Name        | Required Level | PhysicalAR | ElementalAR | Weight |
|------|----------|---------------------|----------------|-----------:|------------:|-------:|
| 1    | Light    | Leather Jerkin      | 1              |         3  |          0  |     5  |
| 1    | Medium   | Chain Shirt         | 3              |         5  |          1  |     8  |
| 1    | Heavy    | Rusted Breastplate  | 5              |         7  |          1  |    12  |
| 2    | Light    | Studded Leather     | 6              |         6  |          1  |     6  |
| 2    | Medium   | Reinforced Chain    | 8              |         9  |          2  |    10  |
| 2    | Heavy    | Polished Cuirass    | 10             |        12  |          3  |    14  |

### 6.2 Rarity Interaction

Rarity from the loot system (Common ? Legendary) adds affixes:

- Increased AR
- Resistance bonuses (fire, cold, etc.)
- Reduced weight
- Reduced movement penalties
- Class-specific bonuses (e.g., +% to Warrior skills)

Affixes must not break core class identity (e.g., Mages should not easily reach Warrior-level AR).

---

## 7. Shields

Shields are treated as part of the armour system but equipped in `OffHand`:

Key stats:
- `BlockChance` – % chance to block a physical hit when blocking.
- `BlockValue` – amount of damage absorbed on a successful block.
- `ArmorBonus` – additional AR even when not actively blocking.
- `Weight` – contributes to encumbrance and casting penalties.

Example tiers:

| Tier | Name            | Required Level | ArmorBonus | BlockChance | BlockValue | Weight |
|------|-----------------|----------------|-----------:|-----------: |----------: |-------:|
| 1    | Wooden Shield   | 1              |         1  |       15%   |        5   |     5  |
| 2    | Iron Shield     | 6              |         3  |       20%   |       10   |     8  |
| 3    | Tower Shield    | 12             |         5  |       25%   |       18   |    12  |

---

## 8. Data Model

Suggested data fields for an armour item record:

- `id` – unique string
- `name` – display name
- `slot` – `Head`, `Chest`, `Hands`, `Feet`, `OffHand`
- `category` – `Cloth`, `Light`, `Medium`, `Heavy`, `Shield`
- `requiredLevel`
- `allowedClasses` (optional, for class-specific sets)
- `physicalAR`
- `elementalAR` (per-type map or small fixed array)
- `spiritualAR` (per-type map)
- `weight`
- `movementPenalty` (category-level, but can be overridden)
- `castPenalty` (for spellcasting classes)
- `affixSlots` (to integrate with loot system)

Example JSON:

```
{
  "id": "armor_chest_chain_shirt",
  "name": "Chain Shirt",
  "slot": "Chest",
  "category": "Medium",
  "requiredLevel": 3,
  "allowedClasses": ["Warrior", "Cleric"],
  "physicalAR": 5,
  "elementalAR": { "Fire": 0, "Cold": 1, "Lightning": 0, "Acid": 0 },
  "spiritualAR": { "Radiant": 0, "Necrotic": 0, "Psychic": 0 },
  "weight": 8,
  "movementPenalty": 0.10,
  "castPenalty": 0.05,
  "affixSlots": 2
}
```

Implementation in C++ should mirror this structure, using C++20 features (`enum class` for slots/categories, `std::array`/`std::span` for damage types, etc.) as appropriate.
