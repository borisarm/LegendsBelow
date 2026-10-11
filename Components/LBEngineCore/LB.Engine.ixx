// LB.Engine.ixx - Game engine: game loop, frame timing, and system orchestration
export module LB.Engine;

// Hosts forward input using the input vocabulary (Keys, MouseButton)
export import LB.InputSystem;

// The engine exposes its camera and render queues in rendering terms (IsometricCamera, MeshShape)
export import LB.RenderingSystem;

export import :StepTimer;
export import :World;
export import :Animator;
export import :Enemies;
export import :EnemyBehavior;
export import :Projectiles;
export import :Statuses;
export import :Waves;
export import :Encounter;
export import :Loot;
export import :Particles;
export import :Effects;
export import :Walls;
export import :Torches;
export import :Minimap;
export import :CombatText;
export import :Hud;
export import :Sounds;
export import :Music;
export import :Menus;
export import :CharacterScreen;
export import :GameEngine;
