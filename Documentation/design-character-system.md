# LegendsBelow – Character System Design

## 1. Purpose

Defines all gameplay-relevant aspects of characters:
- Creation flow
- Classes and subclasses
- Attributes and derived stats (with formulas)
- Leveling, perks, and skill trees
- Status effects

This spec is meant to be directly mappable to data structures and C++ code.

---

## 2. Character Creation

### 2.1 Creation Flow

High-level steps:
1. Enter name.
2. Select class.
3. Select background.
4. Confirm difficulty (for meta systems only).
5. Generate starting stats, perks, and gear.
6. Spawn character in town hub.

Creation should be handled by a simple UI state machine:
- `State_NameEntry`
- `State_ClassSelection`
- `State_BackgroundSelection`
- `State_ReviewAndConfirm`

Each step populates a `CharacterCreationContext` struct. On confirm, this struct is consumed to build the runtime `Character` object.

### 2.2 Backgrounds

Backgrounds tweak starting attributes, perks, and loadout.

Example backgrounds:

- Disgraced Knight  
  - Attribute delta: STR +1, CON +1, CHA -1  
  - Passive: +5% damage reduction when below 30% HP.  
  - Starting gear: rusted longsword, battered shield, worn mail armor.

- Street Urchin  
  - Attribute delta: DEX +1, INT +1, CON -1  
  - Passive: +10% gold dropped by enemies.  
  - Starting gear: dagger, light leather armor, lockpick kit (3 charges).

- Apprentice  
  - Attribute delta: INT +2, STR -1  
  - Passive: -10% mana cost for Tier-1 spells.  
  - Starting gear: wand, robe, basic spellbook.

- Acolyte  
  - Attribute delta: WIS +2, CHA -1  
  - Passive: +10% healing done.  
  - Starting gear: mace, cloth armor, simple holy symbol.

Example data shape (JSON or equivalent):

```
{
  "id": "background_disgraced_knight",
  "name": "Disgraced Knight",
  "description": "A former noble warrior seeking redemption in the depths.",
  "attributeDelta": { "STR": 1, "DEX": 0, "CON": 1, "INT": 0, "WIS": 0, "CHA": -1 },
  "passiveId": "passive_low_hp_damage_reduction",
  "startingItems": [
    "weapon_longsword_rusted",
    "shield_wooden_worn",
    "armor_mail_rusted"
  ]
}
```

---

## 3. Classes

Each class has:
- Base stat template
- Growth tables for HP/MP, attack, and saves (level 1 tuned in balance passes: the Warrior starts with 6 stamina for its abilities, the Rogue with 11 HP and the Mage with 10)
- Proficiencies
- Signature abilities
- Skill trees with passive/active nodes

All numbers here are first-pass targets and will be tuned later. Tables below are **design reference values**, not final balance.

### 3.1 Warrior

**Role:** Frontline tank / melee DPS  
**Primary attributes:** STR, CON

#### 3.1.1 Progression Table (Levels 1–20)

| Lvl | Max HP Gain | Max MP/Stam Gain | Melee Atk Bonus | Ranged Atk Bonus | Phys Save Bonus | Mental Save Bonus |
|-----|-------------|-------------------|-----------------|------------------|-----------------|-------------------|
| 1   | 12          | 6                 | +2              | +0               | +2              | +0                |
| 2   | +8          | +2                | +0              | +0               | +0              | +0                |
| 3   | +8          | +2                | +1              | +0               | +0              | +0                |
| 4   | +8          | +2                | +0              | +1               | +1              | +0                |
| 5   | +8          | +2                | +1              | +0               | +0              | +1                |
| 6   | +8          | +2                | +0              | +0               | +0              | +0                |
| 7   | +8          | +2                | +1              | +1               | +1              | +0                |
| 8   | +8          | +2                | +0              | +0               | +0              | +1                |
| 9   | +8          | +2                | +1              | +0               | +0              | +0                |
| 10  | +8          | +2                | +0              | +1               | +1              | +0                |
| 11  | +10         | +3                | +1              | +0               | +0              | +1                |
| 12  | +10         | +3                | +0              | +0               | +1              | +0                |
| 13  | +10         | +3                | +1              | +1               | +0              | +0                |
| 14  | +10         | +3                | +0              | +0               | +1              | +1                |
| 15  | +10         | +3                | +1              | +0               | +0              | +0                |
| 16  | +10         | +3                | +0              | +1               | +1              | +0                |
| 17  | +10         | +3                | +1              | +0               | +0              | +1                |
| 18  | +10         | +3                | +0              | +0               | +1              | +0                |
| 19  | +10         | +3                | +1              | +1               | +0              | +0                |
| 20  | +10         | +3                | +0              | +0               | +1              | +1                |

Interpretation:
- Cumulative HP = 12 + sum of `Max HP Gain` up to current level.
- Cumulative MP/Stam likewise.
- Attack/save bonuses are additive modifiers on top of attribute-based values.

#### 3.1.2 Proficiencies

- Armor: light, medium, heavy, shields
- Weapons: all melee, basic thrown weapons

#### 3.1.3 Signature Abilities

- Battle Rage  
  - Type: Self buff  
  - Duration: 10s  
  - Cooldown: 30s  
  - Effects:
    - `DamageMultiplier += 0.20`
    - `DamageTakenMultiplier -= 0.10`
    - `HitChanceMultiplier -= 0.10`  
  - Notes: mark character as "Berserking" for AI/FX hooks.

- Shield Bash  
  - Type: Melee attack + crowd control  
  - Damage: `0.8 × weaponDamage + STR_mod`  
  - Additional: 1.0s stun on hit (subject to target CC resistance)  
  - Requirements: Shield equipped

- Taunting Shout  
  - Type: AoE threat manipulation  
  - Radius: 6 tiles  
  - Effects:
    - For each enemy in radius: apply `threat += ThreatValue` (configured).
    - Optional: -10% damage output for 5s.

#### 3.1.4 Skill Trees (Example)

Two trees; each node costs 1 skill point.

**Defender Tree**

- Iron Wall  
  - Requirements: Warrior level ≥ 3  
  - Effect: +10% shield block chance.

- Unyielding Guard  
  - Requires: Iron Wall  
  - Effect: when you successfully block, gain +5% damage reduction for 3s (stacks up to 3×).

- Last Stand  
  - Requires: Unyielding Guard, level ≥ 10  
  - Effect: once per encounter, on taking lethal damage, stay at 1 HP and gain +30% damage reduction for 5s.

**Weapon Master Tree**

- Heavy Swing  
  - Requirements: Warrior level ≥ 3  
  - Effect: charged attack that deals `1.5×` weapon damage and has +10% crit chance.

- Cleave  
  - Requires: Heavy Swing  
  - Effect: attacks hit up to 3 enemies in a frontal arc.

- Executioner  
  - Requires: Cleave, level ≥ 10  
  - Effect: deal +25% damage to enemies below 25% HP.

### 3.2 Rogue

**Role:** Single-target burst, traps and utility  
**Primary attributes:** DEX, INT

#### 3.2.1 Progression Table (Levels 1–20)

| Lvl | Max HP Gain | Max MP Gain | Melee Atk Bonus | Ranged Atk Bonus | Phys Save Bonus | Mental Save Bonus |
|-----|-------------|-------------|-----------------|------------------|-----------------|-------------------|
| 1   | 11          | 6           | +1              | +2               | +1              | +1                |
| 2   | +6          | +4          | +0              | +0               | +0              | +0                |
| 3   | +6          | +4          | +1              | +0               | +0              | +0                |
| 4   | +6          | +4          | +0              | +1               | +1              | +0                |
| 5   | +6          | +4          | +1              | +0               | +0              | +1                |
| 6   | +6          | +4          | +0              | +0               | +0              | +0                |
| 7   | +6          | +4          | +1              | +1               | +1              | +0                |
| 8   | +6          | +4          | +0              | +0               | +0              | +1                |
| 9   | +6          | +4          | +1              | +0               | +0              | +0                |
| 10  | +6          | +4          | +0              | +1               | +1              | +0                |
| 11  | +7          | +5          | +1              | +0               | +0              | +1                |
| 12  | +7          | +5          | +0              | +0               | +1              | +0                |
| 13  | +7          | +5          | +1              | +1               | +0              | +0                |
| 14  | +7          | +5          | +0              | +0               | +1              | +1                |
| 15  | +7          | +5          | +1              | +0               | +0              | +0                |
| 16  | +7          | +5          | +0              | +1               | +1              | +0                |
| 17  | +7          | +5          | +1              | +0               | +0              | +1                |
| 18  | +7          | +5          | +0              | +0               | +1              | +0                |
| 19  | +7          | +5          | +1              | +1               | +0              | +0                |
| 20  | +7          | +5          | +0              | +0               | +1              | +1                |

Notes:
- Rogues gain more MP than Warriors to support utility skills.
- Ranged attack bonuses advance slightly faster than melee.

#### 3.2.2 Key Hooks

- Sneak Attack bonus damage when target is unaware, flanked, or hard-CC'd.
- Higher detection for traps and secrets via perception-like stat/skill.

Rogue trees:
- Assassin – amplifies sneak attack, crit chance, damage from stealth.
- Trickster – emphasizes debuffs, decoys, mobility.

### 3.3 Mage

**Role:** Ranged caster, AoE and control  
**Primary attributes:** INT, WIS

#### 3.3.1 Progression Table (Levels 1–20)

| Lvl | Max HP Gain | Max MP Gain | Melee Atk Bonus | Ranged/Spell Atk Bonus | Phys Save Bonus | Mental Save Bonus |
|-----|-------------|-------------|-----------------|------------------------|-----------------|-------------------|
| 1   | 10          | 10          | +0              | +2                     | +0              | +2                |
| 2   | +4          | +8          | +0              | +0                     | +0              | +0                |
| 3   | +4          | +8          | +0              | +1                     | +0              | +0                |
| 4   | +4          | +8          | +0              | +0                     | +0              | +1                |
| 5   | +4          | +8          | +0              | +1                     | +0              | +0                |
| 6   | +4          | +8          | +0              | +0                     | +0              | +1                |
| 7   | +4          | +8          | +0              | +1                     | +0              | +0                |
| 8   | +4          | +8          | +0              | +0                     | +0              | +1                |
| 9   | +4          | +8          | +0              | +1                     | +0              | +0                |
| 10  | +4          | +8          | +0              | +0                     | +0              | +1                |
| 11  | +5          | +9          | +0              | +1                     | +0              | +0                |
| 12  | +5          | +9          | +0              | +0                     | +0              | +1                |
| 13  | +5          | +9          | +0              | +1                     | +0              | +0                |
| 14  | +5          | +9          | +0              | +0                     | +0              | +1                |
| 15  | +5          | +9          | +0              | +1                     | +0              | +0                |
| 16  | +5          | +9          | +0              | +0                     | +0              | +1                |
| 17  | +5          | +9          | +0              | +1                     | +0              | +0                |
| 18  | +5          | +9          | +0              | +0                     | +0              | +1                |
| 19  | +5          | +9          | +0              | +1                     | +0              | +0                |
| 20  | +5          | +9          | +0              | +0                     | +0              | +1                |

Notes:
- Mages have the lowest HP progression but highest MP and Mental saves.
- "Ranged/Spell Atk Bonus" applies to spell hit chance or save DC where appropriate.

Spell schools:
- Evocation – direct damage (Fireball, Lightning Bolt).
- Frost/Control – slows, roots, freezes (Frost Nova, Ice Wall).
- Utility – teleport, detection, defensive wards.

Each spell defines base damage, coefficient on `SpellPower`, cast time, cooldown, mana cost, and damage tags.

### 3.4 Cleric

**Role:** Healer, buffer, hybrid frontliner  
**Primary attributes:** WIS, CON

#### 3.4.1 Progression Table (Levels 1–20)

| Lvl | Max HP Gain | Max MP Gain | Melee Atk Bonus | Spell Atk/Heal Bonus | Phys Save Bonus | Mental Save Bonus |
|-----|-------------|-------------|-----------------|----------------------|-----------------|-------------------|
| 1   | 10          | 8           | +1              | +1                   | +1              | +2                |
| 2   | +7          | +6          | +0              | +0                   | +0              | +0                |
| 3   | +7          | +6          | +1              | +0                   | +0              | +0                |
| 4   | +7          | +6          | +0              | +1                   | +1              | +0                |
| 5   | +7          | +6          | +1              | +0                   | +0              | +1                |
| 6   | +7          | +6          | +0              | +0                   | +0              | +0                |
| 7   | +7          | +6          | +1              | +1                   | +1              | +0                |
| 8   | +7          | +6          | +0              | +0                   | +0              | +1                |
| 9   | +7          | +6          | +1              | +0                   | +0              | +0                |
| 10  | +7          | +6          | +0              | +1                   | +1              | +0                |
| 11  | +8          | +7          | +1              | +0                   | +0              | +1                |
| 12  | +8          | +7          | +0              | +0                   | +1              | +0                |
| 13  | +8          | +7          | +1              | +1                   | +0              | +0                |
| 14  | +8          | +7          | +0              | +0                   | +1              | +1                |
| 15  | +8          | +7          | +1              | +0                   | +0              | +0                |
| 16  | +8          | +7          | +0              | +1                   | +1              | +0                |
| 17  | +8          | +7          | +1              | +0                   | +0              | +1                |
| 18  | +8          | +7          | +0              | +0                   | +1              | +0                |
| 19  | +8          | +7          | +1              | +1                   | +0              | +0                |
| 20  | +8          | +7          | +0              | +0                   | +1              | +1                |

Notes:
- Clerics sit between Warriors and Mages in HP/MP growth.
- Strong Mental saves and moderate Physical saves reflect their hybrid role.

Specialization paths:
- Guardian – shields, barriers, group mitigation.
- Inquisitor – holy damage, anti-undead/demon bonuses.

Cleric healing spells scale mainly with WIS and may consume mana or a separate "Faith" resource (to be finalized).

---

## 4. Attributes and Derived Stats

### 4.1 Attribute Ranges

- Base creation range: 6–18.
- Absolute cap (including gear/perks): 25 (tunable).
- Attributes stored as integers; modifiers derived as:

```
modifier = floor((attribute - 10) / 2)
```

### 4.2 Derived Stat Formulas

Let `Lvl` be current level.

**Max HP**

```
MaxHP = HP_Base + HP_PerLevel * (Lvl - 1) + (CON_mod * Lvl)
```

**Max MP**

```
MaxMP = MP_Base + MP_PerLevel * (Lvl - 1) + (CasterPrimaryMod * Lvl)
CasterPrimaryMod = INT_mod (Mage), WIS_mod (Cleric), 0 or reduced for others.
```

**Armor Class (AC)**

```
AC = 10 + DEX_mod + ArmorBonus + ShieldBonus + MiscAC
```

**Melee Attack Bonus**

```
MeleeAB = AB_Base + AB_PerLevel * (Lvl - 1) + STR_mod + Buffs
```

**Ranged Attack Bonus**

```
RangedAB = AB_Base + AB_PerLevel * (Lvl - 1) + DEX_mod + Buffs
```

**Spell Power**

```
SpellPower = SP_Base + (INT_mod or WIS_mod) * Lvl + EquipmentSpellPower
```

**Physical Save**

```
PhysicalSave = SavePhys_Base + SavePhys_PerLevel * (Lvl - 1) + STR_mod + CON_mod
```

**Mental Save**

```
MentalSave = SaveMent_Base + SaveMent_PerLevel * (Lvl - 1) + WIS_mod + CHA_mod
```

**Movement Speed**

```
TilesPerSecond = ClassBaseSpeed + DEX_mod * 0.1 - ArmorPenalty
```

Values such as `ClassBaseSpeed`, `HP_Base`, `HP_PerLevel` are defined per class in data.

---

## 5. Leveling and Perks

### 5.1 XP Curve

XP required per level is stored in a table. Example (illustrative only):

| Level | Total XP Required |
|-------|-------------------|
| 1     | 0                 |
| 2     | 100               |
| 3     | 300               |
| 4     | 600               |
| 5     | 1000              |
| ...   | ...               |
| 20    | ~100000           |

`AddXp(int amount)` should handle level-up processing and UI notifications.

### 5.2 Level-Up Rewards

On each level:
- Recompute MaxHP/MaxMP.
- Grant attribute points (e.g., 2 per level).
- Grant skill points (e.g., 1 per level).
- At milestone levels (5, 10, 15, 20), present a perk choice.

### 5.3 Perk System

Perks are small, always-on modifiers.

Example data:

```
{
  "id": "perk_toughness",
  "name": "Toughness",
  "description": "Increase maximum health by 10%.",
  "requiredLevel": 5,
  "requiredClass": null,
  "modifiers": [
    { "stat": "MaxHPMultiplier", "op": "mul", "value": 1.10 }
  ]
}
```

Characters maintain a list of perk IDs. Final stat calculation applies all relevant modifiers.

---

## 6. Status Effects

### 6.1 Status Effect Model

Status effects share a generic definition:

- `id`
- `displayName`
- `durationSeconds`
- `tickIntervalSeconds`
- `maxStacks`
- `categories` (Buff, Debuff, CC, Poison, etc.)
- `onApply`, `onTick`, `onExpire` effect lists

Example (Poison):

```
{
  "id": "status_poison_basic",
  "displayName": "Poison",
  "durationSeconds": 10,
  "tickIntervalSeconds": 1,
  "maxStacks": 3,
  "categories": ["Debuff", "Poison"],
  "onApply": [],
  "onTick": [
    { "effectType": "ModifyHP", "value": -2, "scaling": "Flat" }
  ],
  "onExpire": []
}
```

### 6.2 Categories

- Buffs – positive modifiers (stats, speed, resistances).
- Debuffs – negative modifiers (DoTs, stat reductions).
- Control – Stun, Root, Silence, Fear.
- Elemental – tags like Fire, Frost, Lightning, used for resist logic.

### 6.3 Saving Throws and Resistance

For effects allowing saving throws:

```
roll = d20 + (PhysicalSave or MentalSave)
if roll >= DC:
    apply reduced or no effect
else:
    apply full effect
```

Equipment, perks, and buffs can modify DC or provide bonuses to specific categories (e.g., +3 vs. Poison).
