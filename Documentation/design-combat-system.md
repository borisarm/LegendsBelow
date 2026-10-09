# LegendsBelow – Combat System Design

## 1. Purpose

Defines the combat model, timing, action economy, attack/damage calculations, AI hooks, and difficulty scaling for LegendsBelow.

This is implementation-oriented and should map cleanly to C++ systems and data.

---

## 2. Combat Model

### 2.1 Real-Time with Tactical Pause

- Combat runs continuously at a fixed timestep (e.g., 60 Hz).
- Player can toggle a tactical pause:
  - While paused, simulation is halted (no timers, AI, movement).
  - UI still accepts commands that are queued and applied when unpaused.

Core rule: no hidden simulation while paused.

### 2.2 Update Order

Each frame (when not paused):

1. Process player input and queued commands.
2. Advance timers and cooldowns.
3. Update AI and decision logic.
4. Resolve movement.
5. Resolve attacks and ability execution.
6. Update status effects.
7. Update animations and FX.

The actual order may be tuned, but should remain consistent across runs.

---

## 3. Action Economy

### 3.1 Action Types

Each ability defines an `actionType`:

- Standard Action – primary combat action (weapon attack, major spell, heavy item use).
- Move Action – movement up to move allowance for the current time slice.
- Bonus Action – minor actions (small buffs, quick strikes, slides).
- Reaction – triggered actions based on events (e.g., enemy leaving melee range).

Even in real-time, internal logic should ensure characters do not exceed allowed actions per time window.

### 3.2 Opportunity Attacks

Triggered when:

- A hostile moves out of melee range without using a dedicated disengage/teleport ability.

Rules:

- One opportunity attack per attacker per short cooldown (e.g., 1s).
- Uses basic attack profile.
- Does not consume the attacker's standard action.

---

## 4. Attack and Damage Resolution

### 4.1 Attack Rolls (D20-Inspired)

For attacks that can hit/miss:

```
Roll = d20 + AttackBonus
if Roll >= Target.AC:
    Hit
else:
    Miss
```

- Natural 1: always miss.
- Natural 20: always hit and may be a critical.

### 4.2 Critical Hits

- Crit range: baseline 20, expanded by gear/perks.
- Crit effect: multiplier on final damage (e.g., 1.5× or 2×).

### 4.3 Damage Calculation

Weapon-based example:

```
BaseDamageRoll = roll(WeaponDamageDice) // e.g. 1d8
AttributeBonus = STR_mod or DEX_mod
FlatBonuses   = from gear, buffs, abilities

RawDamage = BaseDamageRoll + AttributeBonus + FlatBonuses

// Apply crit
if IsCrit:
    RawDamage *= CritMultiplier

// Apply resistances/weaknesses
FinalDamage = ApplyResistancesAndModifiers(RawDamage, DamageType, Target)
```

Spell-based example:

```
SpellBase   = SpellData.BaseDamage
Coeff       = SpellData.PowerCoeff
SpellPower  = Caster.SpellPower

RawDamage = SpellBase + Coeff * SpellPower

if IsCrit and SpellCanCrit:
    RawDamage *= CritMultiplier

FinalDamage = ApplyResistancesAndModifiers(RawDamage, SpellDamageType, Target)
```

### 4.4 Saving Throws

For abilities that allow saving throws instead of attack rolls:

```
SaveRoll = d20 + (PhysicalSave or MentalSave)
if SaveRoll >= DC:
    ApplyReducedEffect()
else:
    ApplyFullEffect()
```

`DC` is defined per ability and can scale with caster level or attribute.

---

## 5. Damage Types and Resistances

### 5.1 Damage Types

- Physical: Slashing, Piercing, Bludgeoning.
- Elemental: Fire, Cold, Lightning, Acid.
- Spiritual: Radiant, Necrotic, Psychic.

### 5.2 Resistance Model

Each character has:

- `ArmorPhysical` – flat physical mitigation.
- `Resist[DamageType]` – percentage modifier per type.

Example application:

```
DamageAfterArmor = max(0, RawDamage - ArmorPhysical)

float resist = Target.Resist[DamageType]; // e.g. 0.25 for 25% reduction
FinalDamage = DamageAfterArmor * (1.0f - resist)
```

Negative resist values indicate vulnerability (extra damage).

### 5.3 Status-On-Hit

Some damage types can apply status effects (e.g., burning for Fire). Abilities define:

- `statusOnHitId`
- `statusChance` (e.g., 0.2 for 20%)

On hit:

- Roll random; if under `statusChance`, apply the status to the target.

---

## 6. Combat Actions

### 6.1 Basic Actions

- Attack – use current weapon profile.
- Defend – raise AC/DR for a short duration.
- Use Item – consume potion or activate usable.
- Cast Spell – execute an ability from spell list.
- Special Ability – class-specific action.
- Move – pathfind and step towards desired location.

### 6.2 Advanced/Contextual

- Interact – doors, levers, shrines, destructible props.
- Push/Pull – context-sensitive knockback/kick abilities.
- Swap Loadout – change between defined equipment sets.

Each action is represented by a command object with:

- Initiator entity
- Target entity/position
- Ability or action type
- Time remaining (cast or channel)

---

## 7. Positional and Tactical Elements

### 7.1 Flanking

- If two allies are positioned roughly opposite each other around an enemy, both get a bonus:
  - +X to hit and/or +Y% crit chance.

Implementation:

- Check relative angles between attacker, defender, and existing attackers.

### 7.2 Elevation

- Attacks from higher ground gain small bonuses:
  - +to-hit and +damage (tunable, e.g., +5% each).

Implementation:

- Use tile height or layer information; mark cells as higher or lower.

### 7.3 Cover

- Partial cover reduces hit chance of ranged attacks.
- Full cover blocks line of fire.

Implementation:

- Raycast from attacker to target.
- Count number/type of cover volumes intersected.

### 7.4 Line of Sight

- Required for targeted spells and ranged attacks.
- Blocked by walls and some large props.

Implementation:

- Use grid-based or geometry-based visibility checks.

---

## 8. Difficulty and Scaling

### 8.1 Depth-Based Scaling

Base enemy stats scale with dungeon depth (`Depth`):

```
EnemyHP   = BaseHP   * (1.0f + Depth * HPScalePerDepth)
EnemyDmg  = BaseDmg  * (1.0f + Depth * DmgScalePerDepth)
EnemyXP   = BaseXP   * (1.0f + Depth * XpScalePerDepth)
LootValue = BaseLoot * (1.0f + Depth * LootScalePerDepth)
```

Scale factors are tuned per difficulty mode.

### 8.2 Dungeon Contracts / Modifiers

Contracts add modifiers such as:

- +20% undead, -10% healing.
- +25% enemy damage, +25% loot quality.

Each modifier is a data entry applied to global combat calculations when active.

### 8.3 Elite and Boss Rules

Elites:
- Extra HP, damage, and one or more special abilities.
- Unique status resistances or auras.

Bosses:
- Multi-phase fights with scripted behavior thresholds (e.g., at 70%, 40%, 10% HP).
- Mechanics that encourage movement and positioning (area denial, adds, shields).

---

## 9. Enemy AI

### 9.1 Behavior Tiers

- Tier 1 (Basic): move to nearest target and attack.
- Tier 2 (Intermediate): choose targets based on threat, role (e.g., prioritize Mage/Cleric), and proximity.
- Tier 3 (Advanced):
  - Kite (ranged enemies maintain distance).
  - Protect (bodyguards stay near casters and intercept threats).
  - Retreat (flee at low HP and regroup).

### 9.2 Awareness

- Vision radius and cones; enemies only engage when player is seen.
- Sound radius; combat noise can draw nearby enemies.

Implementation:

- Maintain a simple perception system per AI group (vision + hearing checks).

### 9.3 Group Dynamics

- Link enemies into groups with shared aggro.
- Some AIs coordinate:
  - Tanks engage first.
  - Ranged units try to maintain LOS while staying behind tanks.
  - Healers/buffers stay at mid-range and avoid front line.

---

## 10. Systems Integration

### 10.1 Data-Driven Abilities

All abilities (attacks, spells, specials) are defined in data:

- Costs (mana, stamina, cooldown).
- Targeting rules (self, ally, enemy, AoE).
- Animations and FX IDs.
- Damage, scaling, status effects applied.

### 10.2 Hooks Into Engine

Core systems involved:

- Input/Command system – translates player input into action commands.
- Action/Ability system – validates and executes commands.
- Combat resolution system – hit/miss, damage, status.
- AI system – chooses abilities and targets.
- Status effect system – manages buffs/debuffs over time.

These should be modular so they can be unit-tested where possible.
