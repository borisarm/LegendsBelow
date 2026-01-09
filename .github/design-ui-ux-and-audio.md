# LegendsBelow – UI/UX and Audio Design

## 1. Purpose

Defines:
- HUD and in-game UI
- Menus and flows
- Control schemes
- Accessibility goals
- Audio pillars and implementation hooks

---

## 2. HUD and In-Game UI

### 2.1 HUD Layout

Baseline layout (subject to iteration):

- Top-left:
  - Player portrait.
  - HP and MP bars.
  - Status effect icons.
- Top-right:
  - Mini-map with fog-of-war.
  - Compass markers for key objectives.
- Bottom-center:
  - Action bar (abilities and items, 1–8 slots).
  - Cooldown overlays and keybind hints.
- Bottom:
  - XP bar spanning the lower edge of the screen.

### 2.2 Status Indicators

- Buffs and debuffs shown as small icons around the portrait.
- Hovering (mouse) or cycling (controller) over icons shows tooltip.

### 2.3 Damage and Feedback

- Floating combat text for damage, healing, and crits.
- Subtle screen-edge flash when HP is critically low.
- Clear telegraphs for enemy big attacks (ground markers, windup animations).

---

## 3. Menus and Flows

### 3.1 Main Menu

Options:
- New Game
- Continue
- Settings
- Credits
- Exit

### 3.2 Pause Menu

Options:
- Resume
- Character Screen
- Inventory
- Options (Graphics/Audio/Controls/Game)
- Quit to Main Menu

### 3.3 Character Screen

Tabs:
- Attributes & Derived Stats
- Skills/Perks
- Reputation (future)

### 3.4 Inventory Screen

- Grid-based layout with drag-and-drop (mouse) and selection-based (controller).
- Equipment panel showing current gear.
- Context actions (equip, drop, compare, inspect).

---

## 4. Control Schemes

### 4.1 Keyboard & Mouse

- Movement: WASD (or click-to-move if implemented later).
- Primary action: Left Click.
- Secondary action: Right Click.
- Ability shortcuts: 1–8.
- Inventory: I.
- Character: C.
- Map: M.
- Pause: ESC.

### 4.2 Gamepad (Xbox Controller)

- Left Stick: movement.
- Right Stick: camera rotate (if supported) or target focus.
- A: interact/confirm/primary action.
- B: cancel/dodge.
- X: use item (context or from quick slot).
- Y: class-specific action (e.g., special ability).
- LB/RB: cycle action bar slots.
- View/Back: map.
- Menu/Start: pause menu.

Control bindings should be configurable where practical.

---

## 5. Accessibility

Goals:
- Text legibility at typical viewing distances.
- Color-blind friendly palettes for HP/MP and key status effects.
- Toggle for damage numbers and screen shake.
- Subtitles and configurable font sizes for narrative text.

---

## 6. Audio Design

### 6.1 Pillars

- Support atmosphere and tension without overwhelming the player.
- Clearly distinguish combat vs. exploration through music.
- Use sound to telegraph threats and reward player actions.

### 6.2 Music

States:
- Town/Hub: calm, melodic themes.
- Exploration: subtle, ambient tracks.
- Combat: higher intensity variations of exploration themes.
- Boss Fights: unique, memorable tracks.

Implementation:
- Use layered music or cross-fades between states.

### 6.3 Sound Effects

Categories:
- Player actions: attacks, spells, footsteps, inventory.
- Enemies: attacks, vocalizations, deaths.
- Environment: doors, traps, ambient loops (drips, wind, etc.).
- UI: button clicks, menu transitions, notifications.

### 6.4 Audio Implementation

- Use DirectX audio stack or a middleware abstraction (if added).
- 2D vs. 3D sounds:
  - 3D for in-world events (attacks, traps).
  - 2D for UI and music.

Volume controls (separate sliders):
- Master
- Music
- SFX
- Voice (if added)

---

## 7. Feedback and Iteration

- Regular playtests to validate clarity of telegraphs, readability of UI, and audio mix.
- Gather metrics where possible (time-to-understand attacks, deaths due to unclear feedback, etc.).
