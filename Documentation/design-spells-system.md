# LegendsBelow – Spells System Design

## 1. Purpose

Defines the spells system for LegendsBelow, including:
- Spell schools and categories
- Spell levels and progression
- Casting costs, times, and cooldowns
- Damage and healing formulas
- Status effects and utility spells
- Data model for spells

---

## 2. Spell Schools and Categories

### 2.1 Schools

Core spell schools:

- `Evocation` – direct damage (fire, lightning, force).
- `Frost/Control` – slows, roots, freezes, battlefield control.
- `Holy` – healing, protection, radiant damage.
- `Shadow/Necrotic` – curses, drains, necrotic damage.
- `Utility` – movement, vision, detection, teleportation.

### 2.2 Categories

Each spell is tagged with one or more categories:

- `Damage`
- `Heal`
- `Buff`
- `Debuff`
- `Control`
- `Summon`
- `Utility`

These categories help UI presentation and AI decision-making.

---

## 3. Spell Levels and Progression

### 3.1 Spell Levels

Spells are grouped into levels indicating power and access requirements:

- Level 1: basic spells, available early.
- Level 2–3: stronger core spells.
- Level 4–5: high-impact abilities, limited slots.

This is a **design-level** grouping, not necessarily a strict D&D slot system. In-game, access can be based on:
- Character level
- Quest unlocks
- Talent/skill tree investments

### 3.2 Access Rules (Example)

- Mage:
  - Gains access to new spell level every few character levels (e.g., L3 → Spell Level 2, L7 → Spell Level 3, etc.).
- Cleric:
  - Unlocks Holy spells by character level and via devotion quests.
- Scrolls and items:
  - May temporarily grant access to spells above current level, with extra costs or drawbacks.

---

## 4. Casting Model

### 4.1 Costs

Spells generally cost mana (`MP`), and may have secondary costs:

- `manaCost` – primary cost.
- `hpCost` – optional, for blood magic.
- `reagentCost` – optional item consumed.

### 4.2 Cast Time and Cooldown

Each spell defines:

- `castTimeSeconds` – time from begin-cast to effect.
- `recoveryTimeSeconds` – post-cast delay before next standard action.
- `cooldownSeconds` – global delay before that spell can be cast again.

Instant spells may still have small recovery times.

### 4.3 Interrupts and Movement

- Taking significant damage during cast can interrupt spells.
- Moving beyond a small threshold during cast cancels for most spells (unless flagged `CanCastWhileMoving`).

---

## 5. Damage and Healing Formulas

### 5.1 Damage Spells

Generic damage formula:

```
Base = SpellBaseDamage
Coeff = SpellPowerCoefficient
Power = Caster.SpellPower

RawDamage = Base + Coeff * Power

// Crit (if spell can crit)
if IsCrit:
    RawDamage *= CritMultiplier

FinalDamage = ApplyResistancesAndModifiers(RawDamage, DamageType, Target)
```

Damage type is one of the elemental or spiritual types defined in combat design.

### 5.2 Healing Spells

Generic heal formula:

```
Base = SpellBaseHeal
Coeff = HealingPowerCoefficient
Power = Caster.HealingPower (derived from WIS and gear)

RawHeal = Base + Coeff * Power

// Some buffs/perks can crit heals
if IsCrit and SpellCanCritHeal:
    RawHeal *= CritMultiplier

FinalHeal = ApplyHealingModifiers(RawHeal, Target)
```

### 5.3 Over-Time Effects

For damage-over-time (DoT) and heal-over-time (HoT):

- Total magnitude is distributed across `durationSeconds / tickIntervalSeconds` ticks.
- Scaling behaves similarly to direct damage/healing but applied per tick.

---

## 6. Example Spell Lists

### 6.1 Mage – Evocation (Damage)

Level 1:
- `Arcane Bolt` – single-target magic damage.
  - Base: 8
  - Coeff: 0.8
  - Cost: 10 MP
  - Cast Time: 0.5s
  - Cooldown: 0s (spammable basic attack replacement)

Level 2:
- `Fireball` – AoE fire damage with burn chance.
  - Base: 25
  - Coeff: 1.1
  - Cost: 25 MP
  - Cast Time: 1.2s
  - Cooldown: 6s
  - Effect: 20% chance to apply Burning DoT.

Level 3:
- `Chain Lightning` – jumps between multiple enemies.
  - Base: 30
  - Coeff: 1.0
  - Cost: 30 MP
  - Cast Time: 1.0s
  - Cooldown: 8s
  - Effect: arcs up to 3 additional targets with diminishing damage.

### 6.2 Mage – Frost/Control

Level 1:
- `Frost Bolt` – damage + slow.
  - Base: 6
  - Coeff: 0.7
  - Cost: 8 MP
  - Cast Time: 0.6s
  - Slow: -25% movement for 4s.

Level 2:
- `Frost Nova` – point-blank AoE slow/freeze.
  - Base: 12
  - Coeff: 0.6
  - Cost: 22 MP
  - Cast Time: 0.8s
  - Cooldown: 10s
  - Effect: briefly roots or heavily slows nearby enemies.

### 6.3 Cleric – Holy (Heal/Protect)

Level 1:
- `Divine Light` – single-target heal.
  - Base Heal: 15
  - Coeff: 0.9
  - Cost: 12 MP
  - Cast Time: 0.8s
  - Cooldown: 2s

Level 2:
- `Bless` – party-wide buff.
  - Effect: +10% hit chance and +5% saves for 15s.
  - Cost: 20 MP
  - Cast Time: 1.0s
  - Cooldown: 20s

Level 3:
- `Sanctuary` – AoE protective field.
  - Effect: reduces incoming damage by 20% for allies in area.
  - Duration: 10s
  - Cost: 35 MP
  - Cast Time: 1.5s
  - Cooldown: 30s

### 6.4 Shadow/Necrotic Spells

Level 1:
- `Weakening Curse` – debuff.
  - Effect: -10% damage dealt and -10% PhysicalSave.
  - Cost: 10 MP
  - Duration: 12s

Level 2:
- `Life Drain` – damage + self-heal.
  - Base Damage: 14
  - Coeff: 0.9
  - Cost: 20 MP
  - Cast Time: 1.0s
  - Cooldown: 6s
  - Effect: heal caster for 50% of damage dealt.

---

## 7. Status Effects and Utility Spells

### 7.1 Status Effect Spells

Many spells are primarily about applying statuses:

- `Fear` – forces enemies to flee for a short time.
- `Silence` – prevents spellcasting.
- `Stun` – completely disables actions.

These use the shared status effect model (see character system) with spell-specific DCs and durations.

### 7.2 Utility

Examples:

- `Blink` – short-range teleport.
  - Fixed distance or cursor-targeted within a radius.
  - Cost: moderate MP, medium cooldown.

- `Arcane Sight` – temporarily reveals traps and secret doors.

- `Portal` (late-game) – create temporary link between two dungeon points or dungeon ↔ town.

---

## 8. Data Model

Spell record fields:

- `id`
- `name`
- `school` – `Evocation`, `Frost`, `Holy`, `Shadow`, `Utility`, etc.
- `level` – design-level grouping (1–5)
- `categories` – `["Damage", "Control"]`, etc.
- `requiredClass` (optional)
- `requiredLevel` (character level)
- `manaCost`
- `hpCost`
- `reagentCostId` + `reagentCostAmount` (optional)
- `castTimeSeconds`
- `recoveryTimeSeconds`
- `cooldownSeconds`
- `range` and `radius` (where applicable)
- `canCastWhileMoving` (bool)
- `damageType` (if damage)
- `baseDamage` (or `baseHeal`)
- `powerCoefficient`
- `canCrit` and `critMultiplier`
- `appliedStatusIds` (list of status effect IDs)
- `scalingAttributes` (e.g., `INT`, `WIS`)

Example JSON:

```
{
  "id": "spell_fireball",
  "name": "Fireball",
  "school": "Evocation",
  "level": 2,
  "categories": ["Damage"],
  "requiredClass": "Mage",
  "requiredLevel": 5,
  "manaCost": 25,
  "hpCost": 0,
  "castTimeSeconds": 1.2,
  "recoveryTimeSeconds": 0.4,
  "cooldownSeconds": 6.0,
  "range": 10.0,
  "radius": 3.0,
  "canCastWhileMoving": false,
  "damageType": "Fire",
  "baseDamage": 25,
  "powerCoefficient": 1.1,
  "canCrit": true,
  "critMultiplier": 1.5,
  "appliedStatusIds": ["status_burning_basic"],
  "scalingAttributes": ["INT"]
}
```

C++20 implementation should represent schools, categories, and damage types as `enum class` values, and keep numerical fields in a plain `struct` for easy serialization.
