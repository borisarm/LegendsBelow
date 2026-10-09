# LegendsBelow – Dungeon and World Design

## 1. Purpose

Defines how dungeons and the surrounding world are structured, generated, and presented, including:
- Procedural generation pipeline
- Room types and templates
- Layout rules and gating
- Themes and environmental hazards
- Objectives and special events

---

## 2. Dungeon Structure

### 2.1 Floor Model

Each dungeon floor is a graph of rooms connected by corridors:

- Nodes: rooms
- Edges: corridors/doors

Per-floor parameters:
- Min/Max rooms
- Theme (Crypts, Caves, etc.)
- Depth index (used for scaling difficulty and rewards)

### 2.2 Room Types

Core room categories:

- Combat Room – main enemy encounters.
- Treasure Room – high loot density, often guarded or trapped.
- Boss Room – large arena with bespoke layout.
- Rest Room – safe room with no enemy spawns.
- Puzzle Room – contains environment-based puzzles.
- Trap Room – emphasizes hazard navigation.

Rooms are built from templates with metadata:
- Shape (rectangle, L-shape, cross, multi-level).
- Entry points (doors, staircases, portals).
- Valid enemy groups and difficulty tiers.
- Valid props and interactive elements.

---

## 3. Procedural Generation Pipeline

### 3.1 High-Level Steps

1. Select theme and difficulty based on requested depth.
2. Generate abstract room graph (nodes and edges).
3. Assign room types according to weights and constraints.
4. Instantiate room templates and fit them into a grid.
5. Place enemies, traps, and loot per room.
6. Connect rooms with corridors and generate navigation mesh.

### 3.2 Room Graph Generation

The room graph must:
- Always have a path from Entrance to Exit (critical path).
- Optionally include branches for bonus content.

Algorithm (example):
- Generate a linear critical path of N rooms.
- For each critical room (except entrance/exit), randomly add 0–2 side branches.
- Enforce maximum total rooms.

### 3.3 Room Type Assignment

Use weighted random selection subject to constraints:

- One Entrance (start) room.
- One Exit (goal) room.
- At most one Boss room per floor (or every X floors).
- 0–2 Rest rooms per floor.

For each non-critical room:
- Roll for type using theme-specific weights.
- Ensure required number of combat rooms to meet XP/loot budget.

### 3.4 Placement of Content

Each room template has spawn markers for:
- Enemies (groups by tier).
- Chests and loose loot.
- Traps and hazards.
- Interactive objects (levers, altars, doors).

Content placement is data-driven.

---

## 4. Layout Rules and Gating

### 4.1 Critical Path

Rules:
- Entrance to Exit is always reachable without soft-locks.
- Required keys and switches for critical doors must be on or before the critical path.

### 4.2 Branches and Dead-Ends

- Branches often lead to:
  - Treasure rooms
  - Optional elites
  - Lore rooms
- Dead-ends should be short and usually rewarded.

### 4.3 Keys, Doors, and Gates

Types of gates:

- Keyed Doors – require a key item dropped by an elite or found in a side room.
- Switch Gates – require one or more levers to be activated.
- Puzzle Gates – require solving a puzzle in a nearby room.

Data example for a keyed door:

```
{
  "id": "door_crypt_iron_locked",
  "requiredItemId": "key_iron_crypt",
  "opensOnUse": true
}
```

---

## 5. Themes

### 5.1 Crypts

- Visuals: stone coffins, skulls, flickering torches, damp floors.
- Enemies: skeletons, zombies, wights, ghosts.
- Hazards: necrotic clouds, curse runes, collapsing coffins.

### 5.2 Caves

- Visuals: irregular rock formations, underground lakes, glowing fungi.
- Enemies: beasts, bats, spiders, trolls.
- Hazards: cave-ins, unstable bridges, slippery ground.

### 5.3 Ancient Ruins

- Visuals: broken pillars, arcane sigils, dormant constructs.
- Enemies: golems, arcane guardians, cultists.
- Hazards: arcane traps, animated statues, unstable portals.

### 5.4 Underground Cities

- Visuals: multi-level walkways, ruined markets, mechanical remnants.
- Enemies: humanoid factions, dark elves, assassins.
- Hazards: turret traps, mechanical crushers, crossfire zones.

### 5.5 Elemental Planes (Late-Game)

- Visuals: hyper-stylized elemental areas (lava seas, floating ice platforms).
- Enemies: elementals, empowered cultists, late-game bosses.
- Hazards: persistent fire/ice zones, periodic elemental storms.

Each theme provides:
- Tile set and prop library.
- Enemy pool and spawn weights.
- Trap/hazard pool.

---

## 6. Environmental Hazards

### 6.1 Trap Types

- Pressure Plate Traps – trigger darts, spikes, or flames.
- Tripwire Traps – trigger swinging blades or alarms.
- Rune Traps – magical effects (curse, fear, damage zones).

Each trap type has:
- Detection difficulty (perception threshold).
- Disarm difficulty (skill/roll threshold).
- Failure consequences (extra damage, status effects).

### 6.2 Hazards

- Static Hazards – lava pools, poison pools, bottomless pits.
- Dynamic Hazards – moving saws, timed fire jets, falling rocks.

Implementation:
- Hazards are entities with trigger volumes and periodic logic.

### 6.3 Interactive Elements

- Levers and Switches – open doors, disable traps, rearrange layouts.
- Cranks – move platforms or bridges.
- Teleporters – connect distant rooms.

All interactive elements expose generic events (e.g., OnActivated, OnDeactivated).

---

## 7. Dungeon Progression

### 7.1 Depth Tiers

Group floors into tiers, each with its own:
- Visual theme
- Enemy scaling baseline
- Boss encounter at the end of the tier

Example:

- Tier 1 (Floors 1–3): Crypts
- Tier 2 (Floors 4–6): Caves
- Tier 3 (Floors 7–9): Ancient Ruins

### 7.2 Boss Floors

- Use hand-authored arenas layered on procedural context.
- Boss floors may:
  - Lock exits until boss is defeated.
  - Feature multiple phases and add waves.

### 7.3 Rest Floors

- Occasionally, a full floor is designated a rest floor:
  - No random enemy spawns.
  - Includes a merchant, shrine, or storage access.
  - Provides a safe save point and loadout management.

---

## 8. Objectives and Events

### 8.1 Primary Objectives

Per floor, at least one primary objective:
- Reach the exit.
- Defeat the floor boss.

### 8.2 Secondary Objectives

Optional goals:
- Rescue an NPC.
- Retrieve a specific artifact.
- Slay a named elite.
- Clear all monsters on the floor.

Secondary objectives grant extra XP, loot, or meta progression.

### 8.3 Hidden Content

- Secret rooms behind breakable or illusionary walls.
- Hidden levers that open treasure chambers.
- Lore tablets that unlock codex entries and hints.

Implementation hint:
- Use a generic "secret" flag with discovery conditions (perception check, item usage, etc.).

---

## 9. World Outside the Dungeon (High-Level)

Initial release focuses mostly on dungeons and a single town hub:

- Town Hub:
  - Vendors, crafting, training, quest givers.
  - Entrance to multiple dungeon entrances/tiers.

Future expansion:
- Overworld regions linking multiple dungeon complexes.
- Faction reputation, open-world events, and world map.
