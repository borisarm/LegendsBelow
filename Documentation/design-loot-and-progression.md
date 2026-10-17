# LegendsBelow – Loot and Progression Design

## 1. Purpose

Defines:
- Item system and categories
- Affixes and rarity
- Drop logic and scaling
- Inventory and encumbrance
- Character and account-level progression

---

## 2. Item System

### 2.1 Categories

- Weapons – melee and ranged.
- Armor – helmets, chest pieces, gloves, boots, shields.
- Accessories – rings, amulets, trinkets.
- Consumables – potions, scrolls, temporary buffs.
- Magical Items – unique effects, artifacts.
- Quest Items – non-droppable, used for objectives.
- Materials (future) – crafting components, upgrade reagents.

### 2.2 Base Item Definition

Each base item defines:
- Slot (weapon, chest, etc.)
- Base damage/armor (where applicable)
- Allowed affix tiers
- Level range
- Tags (e.g., "Sword", "Fire", "UndeadSlayer")

Example:

```
{
  "id": "weapon_sword_iron",
  "slot": "Weapon",
  "tags": ["Sword", "Melee"],
  "baseDamage": "1d8",
  "minLevel": 1,
  "maxLevel": 10,
  "allowedAffixTiers": ["low", "mid"]
}
```

---

## 3. Rarity and Affixes

### 3.1 Rarity Tiers

- Common (White) – base stats only.
- Uncommon (Green) – 1 affix.
- Rare (Blue) – 2–3 affixes.
- Epic (Purple) – 3–4 strong affixes.
- Legendary (Orange) – custom effects and hand-authored behavior.

### 3.2 Affix Types

Affix examples:
- Prefix:
  - Fiery – adds fire damage and chance to burn.
  - Stalwart – adds armor and CON.
- Suffix:
  - of the Bear – adds STR and max HP.
  - of Focus – adds INT and spell power.

Data example:

```
{
  "id": "affix_prefix_fiery",
  "type": "Prefix",
  "description": "+X-Y Fire damage, +Z% burn chance.",
  "modifiers": [
    { "stat": "FireDamageMin", "op": "add", "value": 1 },
    { "stat": "FireDamageMax", "op": "add", "value": 3 },
    { "stat": "BurnChance", "op": "add", "value": 0.05 }
  ]
}
```

### 3.3 Legendary Items

- Hand-authored stat blocks and unique effects.
- Named items with lore and custom visuals.
- Often modify gameplay rules (e.g., "gain a shield when you crit").

---

## 4. Loot Generation

### 4.1 Drop Sources

- Enemies (normal, elite, boss).
- Chests and containers.
- Quest rewards.
- Vendors (purchase only, not random drop).

### 4.2 Drop Tables and Weights

Each source uses a table:

- Base item list with weights.
- Rarity distribution with depth scaling.
- Currency (gold) range.

Example rarity distribution (early depth):
- 70% Common
- 20% Uncommon
- 8% Rare
- 1.5% Epic
- 0.5% Legendary

Late depth:
- 40% Common
- 30% Uncommon
- 20% Rare
- 8% Epic
- 2% Legendary

### 4.3 Depth and Difficulty Scaling

Higher depths and contract difficulties:
- Increase chance of Rare/Epic/Legendary items.
- Increase average item level and affix strength.

---

## 5. Inventory and Encumbrance

### 5.1 Grid Inventory

- Player inventory is a grid (e.g., 8×10 cells).
- Items have sizes (1×1 up to 2×4 or similar).
- Player must arrange items to fit available space.

### 5.2 Weight System

- Each item has a weight value.
- Total carried weight vs. carry capacity (based on STR).

Effects of over-encumbrance:
- Reduced movement speed.
- Possible stamina penalties.

### 5.3 Equipment Slots

- Head, Chest, Hands, Legs/Feet.
- Weapon main hand.
- Off-hand (shield, weapon, focus).
- 2× Ring, 1× Amulet.

Equipped items contribute directly to stats and derived stats.

---

## 6. Character Progression

### 6.1 Leveling (Overview)

- XP from enemies, quests, and objectives.
- Level cap around 20 (tunable).
- On level-up: attribute points, skill points, and occasional perks.

### 6.2 Unlocking Subclasses

Conditions to unlock advanced paths (example):
- Reach specific level threshold (e.g., 10).
- Defeat a thematic boss.
- Complete a class-specific quest.

Once unlocked, player chooses a subclass that unlocks additional skill tree nodes and passives.

---

## 7. Meta Progression

### 7.1 Account-Level Unlocks

Small, global upgrades that persist across characters (keep them modest):

- +1% gold find per unlocked milestone (max 5–10%).
- +1 shared stash tab.
- Unlocking new dungeon contracts or challenge modes.

### 7.2 Achievements and Challenges

- In-game achievements for feats (e.g., "Defeat Tier 2 boss without using potions").
- Some achievements unlock cosmetics or minor account perks.

### 7.3 New Game+

Optional mode once main progression is completed:
- Enemies scaled up in HP, damage, and AI behavior.
- Better baseline loot quality.
- Additional modifiers or mutators.

As built: after a victory (design-dungeon-and-world.md 7.4) the victory page offers **Descend Again**. The same hero (level, path, gear, bag, gold, companions) goes down from the first floor of a new Below (other layouts), one cycle stronger. Each cycle a floor counts FinalDepth (14) deeper for its enemies' strength (the wave scaling: hit points, damage, experience), its traps, and its finds (rarity weights, item levels, the deepest tier's gear, gold, objective and bounty rewards); its look, its inhabitants, and how many fill a room go by its depth. Mutators are not built.
