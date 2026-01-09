# LegendsBelow - Game Design Document

## Game Overview

**Title:** LegendsBelow  
**Genre:** Dungeon Crawler, Action RPG  
**Perspective:** Isometric  
**Platform:** Xbox/Windows (DirectX 12)  
**Style:** D&D-inspired fantasy dungeon crawler

### High Concept

LegendsBelow is an isometric, D&D-inspired dungeon crawler focused on tactical combat, meaningful character progression, and high replayability. Players lead a customizable hero (or party, in future expansions) into procedurally generated dungeons, balancing risk and reward as they delve deeper in search of powerful artifacts and ancient secrets.

The core experience emphasizes:
- Strategic positioning and action economy rather than twitch reflexes.
- Build crafting through class, attributes, feats, and equipment.
- A sense of discovery via procedural dungeons enhanced with handcrafted set pieces.

### Setting & Tone

- **World:** A low-to-mid fantasy realm built around a vast subterranean complex known as the “Below,” an ancient labyrinth extending beneath ruined cities, cursed forests, and forgotten temples.
- **Tone:** Dark but not grimdark. Magical and mysterious, with occasional bits of dark humor via items, NPCs, and environmental storytelling.
- **Lore Pillars:**
  - The Below predates current civilizations; its construction is partially magical, partially natural.
  - Multiple factions vie for control of artifacts found within (cultists, mercenary companies, royal explorers).
  - Magic is dangerous but widely used; delving into the Below is a sanctioned (if risky) profession.

---

## Core Gameplay

### Primary Gameplay Loop

1. **Prepare in Town**
   - Buy/sell equipment, rest, and manage inventory.
   - Accept quests/bounties and adjust build (feats, spells, hotbar).
2. **Descend into the Dungeon**
   - Choose a depth/floor range and a dungeon “contract” (specific modifiers, e.g., “+20% undead, -10% healing”).
3. **Explore & Engage**
   - Navigate procedurally generated floors, reveal fog-of-war.
   - Encounter enemies, traps, puzzles, and events.
4. **Fight & Survive**
   - Use tactical combat, resource management (HP/MP/consumables) to clear rooms and reach objectives.
5. **Loot & Progress**
   - Collect items, gold, materials, and lore; earn XP towards level-ups and unlocks.
6. **Return or Press On**
   - Decide whether to push deeper (increasing risk and reward) or return to town to bank progress.

### Core Gameplay Goals

- **Tactical depth:** Each decision in combat and exploration should have clear trade-offs.
- **Meaningful progression:** Level-ups, loot, and unlocks visibly expand what the player can do.
- **Replayability:** Different builds, dungeon mods, and procedural layouts encourage multiple runs.

### Perspective & Camera

- **Camera Type:** Isometric, ~35–45° downward angle, locked perspective with slight tilt.
- **Rotation:**
  - Optional 90° rotational steps (`RB+LB` on controller, `Q/E` on keyboard) if technical constraints allow.
  - Snap rotation with brief easing for readability.
- **Zoom:**
  - 2–3 zoom levels:
    - Close (character-focused, good for tactical details).
    - Medium (default).
    - Wide (situational awareness).
- **Camera Behavior:**
  - Soft follow with dead-zone:
    - The player can move freely within a central “dead-zone” without moving the camera.
    - Camera pans when player approaches edges of dead-zone.
  - Instant refocus on teleportation, major scene transitions, and cutscenes.
- **Occlusion Handling:**
  - Transparent fade-out for walls/ceilings that block the line of sight to the player.
  - Priority for showing units and interactive objects over static background.

---

## Character System

### Character Creation

- **Initial Choices:**
  - Class choice (Warrior, Rogue, Mage, Cleric; expandable).
  - One of several origin backgrounds (e.g., “Disgraced Knight”, “Street Urchin”, “Apprentice”, “Acolyte”) that:
    - Modify starting stats slightly.
    - Offer a minor passive perk.
    - Provide starting equipment variations.
- **Name & Appearance:**
  - Name input (free text).
  - Sprite/portrait variations:
    - Body type (2–3 patterns).
    - Primary color palette (armor/clothing).
    - Simple emblem or banner displayed in UI.

### Character Classes

1. **Warrior** – Frontline melee bruiser.
   - **Role:** Tank / melee damage dealer.
   - **Starting Stats:** STR 16, DEX 12, CON 14, INT 8, WIS 10, CHA 10.
   - **Armor Proficiency:** Heavy armor, shields.
   - **Weapon Proficiency:** Swords, axes, maces, polearms.
   - **Resource:** Stamina (subset of MP or separate, depending on final design).
   - **Signature Abilities:**
     - Battle Rage – Temporarily increase damage and damage reduction; may reduce accuracy or AC.
     - Shield Bash – Short stun and damage; requires a shield.
     - Taunting Shout – Increases chance that enemies target Warrior.
   - **Subclasses (later):**
     - Defender – Focus on mitigation, taunts, and party auras.
     - Berserker – High damage, self-risk mechanics (e.g., HP-for-damage trades).

2. **Rogue** – Agile striker with stealth and crits.
   - **Role:** High single-target DPS, trap/puzzle specialist.
   - **Starting Stats:** STR 10, DEX 16, CON 12, INT 12, WIS 10, CHA 10.
   - **Armor Proficiency:** Light armor.
   - **Weapon Proficiency:** Daggers, short swords, bows, crossbows.
   - **Signature Abilities:**
     - Sneak Attack – Bonus damage when attacking from stealth, from behind, or against CC’d foes.
     - Evasion – Strong saving throw bonus vs. area damage (e.g., fireballs).
     - Shadowstep – Short-range teleport or dash behind enemy.
   - **Utility:**
     - Trap disarm, lockpicking, higher detection for secrets.
   - **Subclasses (later):**
     - Assassin – Maximize burst damage from stealth.
     - Trickster – Focus on debuffs, illusions, and mobility.

3. **Mage** – Ranged magical glass cannon.
   - **Role:** AoE damage, battlefield control.
   - **Starting Stats:** STR 8, DEX 10, CON 10, INT 16, WIS 12, CHA 12.
   - **Armor Proficiency:** Cloth only.
   - **Weapon Proficiency:** Staves, wands, daggers.
   - **Resource:** Mana (MP).
   - **Signature Abilities:**
     - Arcane Bolt – Basic ranged attack scaling with INT.
     - Fireball – AoE damage with splash radius and chance to ignite.
     - Frost Nova – Short-range AoE slow/freeze.
     - Spell Mastery – Reduced mana cost and/or cooldown for certain spell schools.
   - **Spell Schools:**
     - Evocation (raw damage).
     - Control (slow, stun, root).
     - Utility (teleport, vision, detection).
   - **Subclasses (later):**
     - Elementalist – Focus on elemental synergy and status effects.
     - Arcanist – Focus on support magic, buffs, and debuffs.

4. **Cleric** – Support caster with healing and buffs.
   - **Role:** Healer, buffer, secondary tank/damage.
   - **Starting Stats:** STR 12, DEX 10, CON 12, INT 10, WIS 16, CHA 10.
   - **Armor Proficiency:** Medium / heavy (depending on lore).
   - **Weapon Proficiency:** Maces, hammers, flails, simple ranged.
   - **Resource:** Mana (MP) or “Faith” charges (design TBD).
   - **Signature Abilities:**
     - Divine Light – Single-target or small AoE heal.
     - Smite – Holy damage, bonus vs. undead/demons.
     - Bless – Party-wide buff to attack and/or saving throws.
     - Divine Intervention – Large AoE heal / shield with long cooldown.
   - **Subclasses (later):**
     - Guardian – More defensive, shields and damage reduction.
     - Inquisitor – More offensive, anti-undead/demon focus.

### Core Stats (D&D Style)

- **Strength (STR)** – Melee damage, carrying capacity, physical saves.
- **Dexterity (DEX)** – Ranged accuracy, Armor Class (AC), initiative, movement.
- **Constitution (CON)** – HP, resist physical damage, stamina.
- **Intelligence (INT)** – Spell power, mana pool, some skill checks (arcane knowledge).
- **Wisdom (WIS)** – Healing power, perception, resistance to mental effects.
- **Charisma (CHA)** – Social interactions, possible party buffs, morale.

### Derived Stats

- **Hit Points (HP):**  
  `HP = base_by_class + CON_modifier * level`.
- **Mana Points (MP):**  
  `MP = base_by_class + (INT or WIS)_modifier * level`.
- **Armor Class (AC):**  
  `AC = base (10) + DEX_mod + armor_bonus + shield_bonus + misc`.
- **Attack Bonus:**
  - Melee: Based on class progression + STR_mod.
  - Ranged: Based on class progression + DEX_mod.
  - Spells: Based on INT_mod or WIS_mod.
- **Movement Speed:**
  - Base tiles per turn + DEX_mod – armor/weight penalties.
- **Initiative:**
  - Determines turn order or action priority in pseudo-turn-based system.
  - `Initiative = d20 + DEX_mod + class_bonus`.
- **Saving Throws:**
  - **Physical Save:** vs. poison, disease, knockdown (STR/CON).
  - **Mental Save:** vs. charm, fear, confusion (INT/WIS/CHA).
  - Each class has strong and weak saves.

### Leveling System

- **XP Curve:**
  - Low early levels for quick ramp-up.
  - Increasing XP required per level, with soft cap at 20.
- **On Level Up:**
  - Gain HP/MP according to class.
  - Gain attribute points (e.g., +2 points per level to distribute).
  - Gain skill points for skill trees (class-specific).
  - Unlock or upgrade abilities at specific levels.
- **Respec Options:**
  - Partial respec available in town via NPC (for cost).
  - Full respec limited and/or expensive to encourage meaningful choices.

### Character Customization

- **Attributes:**
  - Hard minimum/maximum per attribute to prevent extreme min-maxing.
  - Background modifies starting attributes slightly (±1 or ±2).
- **Feat-Like Perks (Milestone Levels):**
  - Examples:
    - Toughness – +10% max HP.
    - Spell Focus (Fire) – +15% fire damage, +5% chance to ignite.
    - Battle Hardened – -10% damage from crits against you.
    - Quick Hands – Faster item usage, shorter potion cooldown.
- **Skill Specializations (Per Class):**
  - Each class has 2–3 trees emphasizing different roles.
  - Example (Warrior):
    - Defender Tree – Shield skills, taunts, damage reduction.
    - Weapon Master Tree – Two-handed weapon damage, crits.
- **Cosmetics:**
  - Armor color schemes tied to item rarity and dye system (future).
  - Visuals to clearly communicate role and gear quality.

### Status Effects

- **Buffs:**
  - Temporary increases to stats (e.g., +2 STR), AC, movement speed.
  - Some buffs are concentration-based (only one at a time).
- **Debuffs:**
  - Poison – HP loss over time, stacking intensity.
  - Slow – Reduced movement/action speed.
  - Blind – Reduced hit chance, limited vision radius.
- **Crowd Control (CC):**
  - Stun – Cannot act or move; vulnerable to crits.
  - Root – Cannot move; can still attack/cast.
  - Silence – No spells or some abilities; can still melee.
  - Fear – Forced movement away from source; limited control.
- **Duration & Cleansing:**
  - Measured in seconds and/or “ticks” of the combat loop.
  - Saving throws each tick or after initial hit depending on effect.
  - Specific items/spells can remove certain conditions.

---

## Combat System

### Combat Model

- **Core Approach:** Real-time with tactical pause (player can pause to issue commands and queue actions).
- **Granularity:**
  - Internally ticked at a fixed timestep (e.g., 60 updates per second).
  - Actions have cast times, recovery times, and cooldowns.

### Combat Mechanics

- **Attack Rolls (D20-inspired):**
  - `d20 + AttackBonus vs. AC`.
  - Natural 1 always misses; natural 20 always hits and may crit.
- **Damage Rolls:**
  - Weapons define base dice (e.g., d8 longsword).
  - Damage = weapon_dice + attribute_mod + flat bonuses.
- **Critical Hits:**
  - Triggered on high rolls within crit range (e.g., 19–20).
  - Crit multiplier (e.g., 1.5× or 2× base damage).
- **Saving Throws:**
  - Spell/ability defines DC (difficulty).
  - `d20 + SaveBonus vs. DC`:
    - Full effect on failed save; half or reduced effect on success.

### Action Economy

- **Standard Action:**
  - Main combat action: weapon attack, cast standard spell, use heavy item.
- **Move Action:**
  - Move up to movement allowance; leaving melee range may trigger opportunity attacks.
- **Bonus Action:**
  - Smaller, quick abilities (minor buffs, dashes, secondary attacks).
- **Reaction:**
  - Triggered by conditions:
    - Enemy moving out of melee range.
    - Spell cast within range.
    - Ally HP dropping below threshold (for certain abilities).

### Damage Types & Resistances

- **Physical:**
  - Slashing, piercing, bludgeoning.
- **Elemental:**
  - Fire, cold, lightning, acid.
- **Spiritual:**
  - Radiant, necrotic, psychic.
- **Interactions:**
  - Some armors give high physical but low elemental defense.
  - Bosses and elites may require exploiting weaknesses (e.g., fire vs. undead, radiant vs. demons).
- **Status-on-Hit:**
  - Chance to apply burning, freezing, shocking, etc., based on damage type and gear.

### Combat Actions

- **Basic Actions:**
  - Attack, Defend (temporary AC buff), Use Item, Cast Spell, Special Ability, Move.
- **Advanced/Contextual Actions:**
  - Interact with environment (levers, explosive barrels).
  - Push or pull enemies (cliffs, traps).
  - Swap weapons/loadouts (e.g., sword+shield vs. two-handed).

### Positional & Tactical Elements

- **Flanking:**
  - Bonus to hit or damage when attacking from behind or both sides.
- **Elevation:**
  - Elevated positions grant accuracy/damage bonuses for ranged attackers.
- **Cover:**
  - Partial or full cover reduces chance to be hit by ranged attacks.
- **Line of Sight:**
  - Obstructions block projectiles and targeted spells.
  - Fog-of-war hides enemies until spotted.

### Difficulty & Scaling

- **Dungeon Depth-Based Scaling:**
  - Enemy HP, damage, and AI complexity scale with floor depth.
- **Modifiers:**
  - Dungeon contracts may add global modifiers (e.g., “Monsters deal +15% elemental damage”).
- **Elite & Boss Rules:**
  - Elites gain extra abilities/resistances.
  - Bosses have multiple phases, telegraphed attacks, and mechanics that force movement/positioning.

### Enemy AI

- **Behavior Layers:**
  - Basic: move toward player and attack.
  - Intermediate: choose targets based on threat (e.g., focus on Mage/Cleric).
  - Advanced:
    - Kite (ranged enemies keep distance).
    - Protect (bodyguards stay near casters).
    - Flee and regroup at low HP.
- **Awareness:**
  - Sound radius: noise from combat or spells draws attention.
  - Vision cones: enemies may patrol and only aggro when they see the player.
- **Group Dynamics:**
  - Coordinated attacks (e.g., tanks engage first, archers stay back).
  - Enemies may retreat to lure players into traps.

---

## Dungeon Design

### Procedural Generation

- **Structure:**
  - Floor = graph of rooms connected by corridors.
  - Minimum and maximum room count per floor.
- **Room Types:**
  - Combat Rooms – Standard or themed encounters.
  - Treasure Rooms – High loot density, often guarded or trapped.
  - Boss Rooms – Large arenas with special layout considerations.
  - Rest Rooms – Safe areas, occasional merchant or shrine.
  - Puzzle Rooms – Environmental riddles, switch sequences, pattern matching.
  - Trap Rooms – Focus on navigation and hazard avoidance.
- **Generation Steps:**
  1. Choose theme and difficulty parameters based on depth.
  2. Generate room graph (critical path + branches).
  3. Assign room types according to weights and constraints.
  4. Place enemies, traps, and loot respecting encounter budgets.
  5. Connect rooms with corridors and generate navigation mesh.

### Layout Rules

- **Critical Path:**
  - Start and exit always connected; no soft locks.
- **Branches:**
  - 20–40% of rooms are optional, offering extra risk/reward.
- **Backtracking Control:**
  - Limit dead-end length; provide occasional shortcuts after key events.
- **Key & Gate System:**
  - Locked doors requiring:
    - Physical keys (dropped by elites or found in side rooms).
    - Puzzle completion (lever combinations).
    - Boss defeat (door opens after combat).

### Dungeon Themes

1. **Crypts:**
   - Visuals: stone coffins, flickering torches, bone piles.
   - Enemies: skeletons, zombies, wights, ghostly apparitions.
   - Hazards: curse glyphs, necrotic clouds.
2. **Caves:**
   - Visuals: stalactites, underground rivers, bioluminescent fungi.
   - Enemies: beasts, spiders, trolls, insect swarms.
   - Hazards: cave-ins, slippery terrain, pits.
3. **Ancient Ruins:**
   - Visuals: crumbling masonry, arcane runes, statues.
   - Enemies: constructs, arcane guardians, cultists.
   - Hazards: magical traps, animated statues, unstable portals.
4. **Underground Cities:**
   - Visuals: multi-level walkways, markets in ruins, broken machinery.
   - Enemies: humanoid factions, thieves, dark elves (or equivalent).
   - Hazards: mechanical traps, turret defenses, crossfire lanes.
5. **Elemental Planes (late-game):**
   - Visuals: extreme elemental motifs (lava seas, ice caverns).
   - Enemies: elementals, empowered cultists, boss-tier entities.
   - Hazards: persistent elemental damage zones.

### Environmental Hazards

- **Traps:**
  - Pressure plates, tripwires, rune stones.
  - Some visible, some requiring perception checks.
- **Hazards:**
  - Lava pits, poison pools, collapsing floors, swinging blades.
- **Interactive Elements:**
  - Levers, cranks, rotating platforms, teleport pads, timed doors.

### Dungeon Progression

- **Depth Tiers:**
  - Every 3–5 floors = one “chapter” with a specific theme and mini-boss.
- **Boss Floors:**
  - Hand-authored arenas layered atop procedural base.
  - Bosses with multiple phases and distinct mechanics.
- **Rest Floors:**
  - Safe floors with merchants or shrines.
  - Limited fast-travel between rest floors and town.
- **Scaling Loot Tables:**
  - Higher depths increase chances of rare+ items and unique drops.

### Dungeon Objectives

- **Primary:**
  - Reach the floor exit or defeat the floor boss to unlock deeper levels.
- **Secondary:**
  - Optional quests:
    - Rescue an NPC.
    - Recover a specific artifact.
    - Slay a named elite.
    - Clear the floor of all monsters.
- **Hidden Content:**
  - Secret rooms behind breakable walls or illusionary passages.
  - Lore tablets with backstory and hints for future runs.
  - Rare event shrines granting powerful boons with trade-offs.

---

## Loot & Inventory

### Item Categories

- Weapons, Armor, Consumables, Magical Items, Quest Items as already defined.
- Future expansion:
  - Crafting materials.
  - Socketable gems/runes.

### Item Generation

- Base item type → Affixes:
  - Prefix (e.g., “Fiery”, “Stalwart”).
  - Suffix (e.g., “of the Bear”, “of Focus”).
- Affixes draw from level-appropriate pool and theme context.

### Inventory System

- Grid-based with item sizes (1×1 potions, 2×3 greatsword).
- Weight limit to control hoarding and force trade-offs.
- Quickbar slots for potions and frequently-used items.

---

## Progression & Meta Systems

### Character Progression

- As above plus:
  - Unlocking new subclasses by meeting conditions (e.g., “Reach level 10 and defeat X boss”).
  - Achievements that unlock cosmetics or small bonuses.

### Town/Hub System

- Additional facilities:
  - Shrine / Temple – re-roll certain affixes, remove curses.
  - Library – unlock lore, hints for puzzles, and bestiary entries.
  - Training Hall – test skills and DPS in a dummy environment.

### Persistent Progression

- Account-level unlocks:
  - Small global bonuses (e.g., +1% gold find) to avoid pay-to-win feel.
  - Unlock new dungeon contracts, higher difficulty modes, and cosmetic rewards.

---

## Technical Design (DirectX 12)

### Rendering Architecture

- **Renderer:**
  - DirectX 12 forward or deferred renderer (TBD).
  - Isometric projection with orthographic or perspective projection matrix.
- **Batching:**
  - Tile-based batching for floor/walls to reduce draw calls.
  - Instancing for repeated props and units.
- **Lighting:**
  - Dynamic lights for torches, spells.
  - Shadow casting for key lights (boss arenas, player light source).
- **Post-Processing:**
  - Bloom, ambient occlusion, color grading, subtle vignette.

### Data-Driven Design

- **Definitions in JSON/XML:**
  - Items, enemies, spell definitions, dungeon templates.
- **Balance Workflow:**
  - Designers edit data files, no need to recompile.
  - Engine supports hot-reload of data in debug mode.

---

## Audio Design

- As defined plus:
  - Dynamic music layering (exploration vs. combat intensity).
  - Environmental reverb zones (crypts sound different from caves).

---

## UI/UX Design

- Emphasis on readability in isometric view:
  - Clear outlines and selection highlights.
  - Color-blind friendly palette for HP/MP/status icons.
- Tooltip system for:
  - Items, abilities, status effects (with duration, exact effects).

---

## Gameplay Features

- Core and stretch goals already listed.
- Additional possible features:
  - **Hardcore Mode:** One-life permadeath, with unique rewards.
  - **Weekly Dungeon:** Rotating mutators and leaderboards.

---

## Development Phases

- Already defined; for each phase outline:
  - Milestones.
  - Test plans (unit tests for systems, manual QA for gameplay loops).

---

## Design Pillars, Target Audience, Competitive Analysis, Success Metrics, Version History, Credits

- Keep as currently written but expand with:
  - Concrete metrics for performance (e.g., CPU/GPU budgets).
  - Explicit benchmark targets (e.g., X enemies on screen at 60 FPS on Series S).

---

*This document is a living design artifact. Any system changes should update both high-level intent and detailed rules here to stay synchronized with implementation.*