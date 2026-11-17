# HEIN Engine Architecture

This overview reflects the engine structure in the repository. The engine is a Windows C++20 static library built around Direct3D 11 and DirectXTK. `Game` owns the frame loop and shared services; registered `IScene` implementations supply game-specific update and rendering behavior.

```mermaid
flowchart TD
    Host[Windows application / window] --> Game[Game\nFramework/Game]
    Game --> Device[DeviceResources\nD3D11 device, swap chain, back/depth buffers]
    Game --> Context[GameContext\nshared frame services and state]
    Game --> Input[InputManager]
    Game --> Messenger[Messenger\ndelayed publish/subscribe messages]
    Game --> Scenes[SceneManager]
    Game --> Audio[AudioSystem]

    Device --> Render[Direct3D 11 rendering]
    Context -. exposes .-> Input
    Context -. exposes .-> Scenes
    Context -. exposes .-> Audio
    Context -. exposes .-> Actors[ActorManager]
    Context -. exposes .-> Events[EventManager]
    Context -. exposes .-> Camera[CameraController]
    Context -. exposes .-> Shadows[ShadowSystem]

    Scenes --> Registry[Scene registry and active scenes]
    Registry --> Scene[IScene implementations]
    Scene --> Actors
    Scene --> Camera
    Scene --> Physics[PhysicsSystem]
    Scene --> Shadows

    Actors --> Actor[Actor\nActorID and parent/child hierarchy]
    Actor --> Components[IComponent instances]
    Factory[ComponentFactory] -. creates from serialized type IDs .-> Components
    Actor <-->|JSON save/load| Factory

    Components --> Gameplay[Gameplay components\ntransform, rigid body, health, AI, UI]
    Components --> RenderComponents[Render components\nstatic/skinned models, terrain, particles, lights]
    Components --> Sound[SoundComponent]
    Sound --> Audio

    Physics --> Actors
    Physics --> Colliders[Collider components]
    Physics --> Collision[CollisionDispatcher / CollisionMath]
    Collision --> Events
    Events --> Gameplay

    RenderComponents --> Render
    Camera --> Render
    Shadows --> Render
    Shaders[HLSL shader programs] --> Render

    Editor[Optional editor and debug tools\nDear ImGui, ImGuizmo, DebugingTools] -. editor builds .-> Scene
    Editor -. inspect and edit .-> Actors
    Editor -. scene/game viewport targets .-> Render

    Audio --> Backend[DirectXTK AudioEngine]
```

## Runtime flow

`Game::Tick()` advances the `StepTimer`, calls `Game::Update()`, then renders. Update samples keyboard and mouse state, updates the input manager and messenger, and delegates scene updates to `SceneManager`; audio is advanced with the active camera position when available. Rendering clears the D3D11 target and asks `SceneManager` to render active scenes, followed by the editor UI when editor support is enabled.

Scenes implement `IScene::Update` and `IScene::Render`. They use `GameContext` to reach shared engine services and coordinate actor/component and system work. `ActorManager` owns actors by `ActorID`; each actor owns its components and drives their lifecycle and draw callbacks. The physics module queries actors and colliders, resolves solid overlaps, and dispatches trigger events through `EventManager`.

`ComponentFactory` maps component type names to constructors for actor JSON deserialization. Render components and HLSL programs feed the Direct3D 11 pipeline. Audio is a separate engine-wide service: `SoundComponent` submits cues while `AudioSystem` manages playback, buses, spatial positions, and BGM transitions.

## Main modules

- `Framework/` — `Game`, the frame loop, and the shared `GameContext`.
- `Scene/` — scene registration, lifecycle, transitions, and rendering delegation.
- `Entities/`, `Components/`, `Factory/` — actor/component model, component registry, and serialization support.
- `System/`, `Common/`, `Camera/` — physics/collision, audio and shadow services, device/input/event resources, and camera modes.
- `Shaders/`, `Effect/`, `DirectXTK_Utilities/` — HLSL programs and rendering/animation helpers.
- `DebugingTools/`, `ImGui/` — optional editor, gizmo, and debug UI support.
- `BehaviourTree/`, `Message/` — behavior-tree building blocks and message/observer utilities.

The repository contains the engine library and infrastructure; application entry points and concrete game scenes are provided by consuming applications rather than this project.
