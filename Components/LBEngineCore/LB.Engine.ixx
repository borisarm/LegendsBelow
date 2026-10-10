// LB.Engine.ixx - Game engine: game loop, frame timing, and system orchestration
export module LB.Engine;

// Hosts forward input using the input vocabulary (Keys, MouseButton)
export import LB.InputSystem;

// The engine exposes its camera and render queues in rendering terms (IsometricCamera, MeshShape)
export import LB.RenderingSystem;

export import :StepTimer;
export import :GameEngine;
