# LegendsBelow

An isometric, D&D-inspired dungeon crawler for Xbox and Windows, built on a custom C++20 engine using DirectX 12 and the Microsoft GDK.

| | |
|---|---|
| **Genre** | Isometric dungeon crawler / action RPG (real-time with tactical pause) |
| **Platform** | Xbox Series X\|S and Windows via Microsoft GDK |
| **Language** | C++20 with modules |
| **Graphics** | DirectX 12 |
| **Architecture** | Layered / clean architecture with domain-driven design |

## Repository Layout

```
LegendsBelow.slnx          Solution
LegendsBelow/              Win32/GDK shell: window, message loop, forwards events to LB.Engine
Components/
├── Model/                 Domain modules: LB.Core, LB.Character, LB.Combat, LB.Inventory, LB.Dungeon
├── Services/              Gameplay rules: Character, Combat, Inventory, Progression, Dungeon, Repositories
├── Systems/               Technical systems: Rendering, Input, Audio, Asset, Persistence
├── LBEngineCore/          LB.Engine: fixed 60 Hz game loop and system orchestration
└── Tests/                 GoogleTest project per component (Model/, Services/, System/)
Tools/
├── LBAssetProcessor/      JSON validation and binary packing (empty project)
└── LBGameEditor/          WinUI content editor (template)
Assets/Data/               spells.json, weapons.json, armour.json, classes.json, enemies.json, consumables.json, loot.json
Assets/Models/             KayKit glTF models (CC0); Animations/ holds the characters' animation sets
Assets/Fonts/              MedievalSharp (SIL Open Font License)
Assets/Sounds/             Kenney Impact Sounds and RPG Audio (CC0)
Assets/UI/                 Menu frames: Kenney Fantasy UI Borders (CC0)
Assets/Music/              Music tracks; see Assets/Music/CREDITS.txt (one is CC-BY: credit required)
Documentation/             Game design and architecture documents
```

## Status

| Area | State |
|------|-------|
| Domain model | Implemented as C++20 modules, unit tested |
| Services (Character, Combat, Inventory, Progression, Dungeon) | Implemented, unit tested; DungeonService generates the floor layouts; CharacterService uses the class data (HP/MP progression, movement, weapon adjustments); CombatService resolves the arena's weapon attacks (d20 vs AC, dice, crits, armour, resistances) |
| Systems | Unit tested. Rendering: Direct3D 12 with DirectXTK12: a lit 3D world pass of glTF models (fastgltf; textured, skinned, and animated with cross-fades; instanced per model part, see-through walls) and primitives through an isometric camera, plus a 2D overlay of rectangles and text (TrueType fonts as distance fields, MedievalSharp for the HUD and damage numbers). Input: keyboard/mouse from the window, gamepads through GameInput. Audio: sound effects and crossfading music (Ogg, MP3, WAV) through XAudio2, with channels, volume, and stereo pan. Persistence: JSON save slots and preferences in `%LOCALAPPDATA%\LegendsBelow\Saves`. The Asset system has no file backend yet |
| Repositories | Spell facade via Microsoft Proxy 4 with a content-backed implementation |
| Engine / game loop | `LB.Engine` runs the loop and all wired systems. In the game: a Warrior, Mage, Rogue, or Cleric (the animated KayKit Adventurers heroes; a spell or ability bar on 1-4 or LB, RB, Y, B: the Mage's bolts, Fireball and Frost Nova, the Rogue's Evasion and Shadowstep with sneak attacks, the Cleric's Smite, heals, and Turn Undead; moving cancels a cast) explores generated, torch-lit dungeon floors of KayKit rooms and corridors (M: map) with WASD/arrows or the left stick (screen-relative), fights the animated KayKit skeletons that rise when a room is entered (from the second floor, crossbowmen too: they shoot bolts that can be dodged and back away while reloading; from the third, skeleton mages whose frost bolts slow; golems in elite rooms; a room's enemies notice the hero together), in the caves werewolves that lunge, orc raiders and brutes, and witches, in the ruins vampires that drink life, tieflings that throw fire, and black knights, and at the end of every third floor a boss (the Necromancer, who raises the dead and erupts the ground under the hero, the Golem Warlord, who slams around itself, then the Frost Golem: their red rings warn before the blow), with a resting place below (a merchant who buys and sells for gold and hires out companions, two at most, a Barbarian with a great axe, a Ranger with a longbow, or an Acolyte who heals, who fight beside the hero, carry the gear they are given, and follow, hold, or attack on G and T; F or d-pad up to trade; and a shrine that restores once); each tier of floors has its own look (crypts, caves, ruins) with props in the rooms and spike traps in the corridors; enemies keep ranks (fighters behind their tank, shooters behind the front line); the Warrior's stamina abilities (Cleave, Shield Bash, Whirlwind) and the Rogue's open at levels 3 and 5, the Cleric's heals go to whoever is worst off and Healing Circle (level 3) mends everyone around, and at level 5 each class chooses a path from cards on the character screen (a banner greets every level and path); items can be dropped from the bag (X or Delete on the character screen); every floor has an objective (slay a named champion, find a relic, free a captive, or clear the floor) and the merchant posts bounties on the next tier's creatures; each tier drops its own grade of gear; treasure rooms hide behind locked gates (the key falls in a room on the way) and cracked walls (strike them), puzzle rooms hold candles to light, and greater chests reward them; items have levels, attribute and hit point affixes, and legendaries carry powers (lightning, life steal, frost, fire, thorns), with icons on the character screen; three save slots besides the autosave and quick save, and a legacy of records that unlock boons for new heroes; with sword or bow (the starting bow is in the bag: equip it on the character screen; arrows fly at the nearest enemy in sight and stop at walls), opens treasure chests (gear drops with a rarity and affixes; bows need arrows), and takes the trapdoor down to deeper floors with Left Click, Space, or gamepad A, gaining experience and picking up gold and gear they drop (I/C or View: character screen; R or X: drink a potion); Tab or the left stick click pauses the fight to give orders (right click a spot to walk there or an enemy to attack it; arrows or d-pad + Space to pick and attack, 1-4 to cast at it); Q/E or right stick left/right rotate the camera 90 degrees, the mouse wheel or right stick up/down zoom; the game starts at a title menu (new game with a choice of class, continue, options with volumes, controls (every key and button, rebindable), credits); a fallen hero sees the run so far and may rise again at the entrance; walls and characters cast real shadows from the key light and soft ones that stretch away from torches, heavy blows shake the camera and throw bodies back, each tier has its own ambience, footsteps, and air (dust, mist, embers), gates sink, cracked walls crumble, and greater chests' lids swing open; Esc/Start opens the pause menu (resume, save, load, options, controls, credits, quit to title); F5/F9 quick save and load; the game saves itself (a new game, each floor down, quitting) and Continue loads the newest save |
| Asset pipeline | Not started (see `Documentation/asset-preprocesing-strategy-instructions.md`) |
| Content | 4 classes, 47 spells and abilities, 12 weapons, 15 armour pieces, 6 enemies (a crossbowman, a caster, a golem, and a boss), 3 consumables (a potion, arrows, bolts), 7 loot tables, 8 status effects, 4 rarities, 11 affixes authored in JSON, validated (including cross-file class rules) and loaded at startup. 3D models: a starter set of the KayKit Dungeon Pack, KayKit Skeletons, and the KayKit Adventurers heroes and weapons (CC0, with the Rig_Medium General, MovementBasic, MovementAdvanced, CombatMelee, and CombatRanged animation sets, and the golem's Rig_Large sets) in `Assets/Models` |

## Getting Started

### Prerequisites
- Visual Studio 2022 17.14+ (or later) with the C++ desktop and game development workloads; `.slnx` solution support is required
- Microsoft Game Development Kit (GDK)
- vcpkg with Visual Studio integration enabled

### Build
1. Open `LegendsBelow.slnx` in Visual Studio.
2. vcpkg packages restore automatically from the root `vcpkg.json` on first build.
3. Restore the test projects' NuGet packages (GoogleTest) via Visual Studio's package restore, or `msbuild LegendsBelow.slnx -t:restore -p:RestorePackagesConfig=true`.
4. Build the `x64` platform. Profile configurations map to Release for the component libraries.

### Tests
Run the projects under `Components/Tests/` from Test Explorer. Each Model, Service, and System component has its own GoogleTest project.

## Documentation

- [Game Design Document](Documentation/game-design.md): vision, core loop, and an overview of every system
- System designs: [character](Documentation/design-character-system.md), [combat](Documentation/design-combat-system.md), [spells](Documentation/design-spells-system.md), [weapons](Documentation/design-weapons-system.md), [armour](Documentation/design-armour-system.md), [loot & progression](Documentation/design-loot-and-progression.md), [dungeon & world](Documentation/design-dungeon-and-world.md), [UI/UX & audio](Documentation/design-ui-ux-and-audio.md)
- [Architecture](Documentation/architecture.md): layers, module map, dependency rules, and known issues
- [Engine Modularization Plan](Documentation/modularized-game-architecture.md)
- [DirectX 12 Technical Design](Documentation/design-technical-directx12.md)
- [Asset Processing Strategy](Documentation/asset-preprocesing-strategy-instructions.md)
- [Microsoft Proxy Repositories](Documentation/microsoft-proxy-implementation.md)
- [Coding Guidelines](.github/copilot-instructions.md): module conventions, namespaces, and common build errors

## Credits

- 3D models: KayKit Dungeon Pack, Skeletons, Adventurers, and Character Animations by Kay Lousberg (CC0)
- Sound effects and menu frames: Impact Sounds, RPG Audio, and Fantasy UI Borders by Kenney, kenney.nl (CC0)
- Font: MedievalSharp (SIL Open Font License)
- Music:
  - "Mystical Theme" by Alexandr Zhelanov, https://soundcloud.com/alexandr-zhelanov (CC-BY 3.0, https://creativecommons.org/licenses/by/3.0/)
  - "Battle Theme A" by cynicmusic, cynicmusic.com pixelsphere.org (CC0)
  - "Menu Music" by mrpoly (CC0)

Details and sources: `Assets/Music/CREDITS.txt` and the license files next to each asset set.

## Origin

The project started from the Microsoft GDK DirectX 12 desktop template. `LegendsBelow/README.md` contains the original template notes.
