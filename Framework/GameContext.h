//--------------------------------------------------------------------------------------
// File: GameContext.h
//
// Game context class passed to scenes
//
// Date: 2026.3.3
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#pragma once

#include "Common/StepTimer.h"
#include "Common/DeviceResources.h"
#include "DebugingTools/DebugRenderer.h"
#include "DebugingTools/DebugCollisionRenderer.h"
#include "Common/EventManager.h"

namespace HEIN { 
	class CameraController; 
	class InputManager;
	class SceneManager;
	class ShadowSystem;
	class ActorManager;
}

enum class EditorViewportLayout
{
	SceneViewOnly = 0, // Editing view: Debug/Editor Camera + Colliders + Frustums + ImGuizmo (NO Game HUD!)
	GameViewOnly  = 1, // Player view: Main Gameplay Camera + Game HUD + Scene Transitions (NO Colliders/Gizmos!)
	SplitSideBySide = 2 // Unity-style Split View: Left half = Scene View, Right half = Game View
};

/**
 * @brief Common resources passed to each scene.
 * Describe common resources passed to each scene here.
 */
struct GameContext
{
	/// @brief Step timer
	DX::StepTimer& timer;

	/// @brief Device resources
	DX::DeviceResources& deviceResources;

	/// @brief Keyboard state tracker
	DirectX::Keyboard::KeyboardStateTracker& keyboardTracker;

	/// @brief Mouse state tracker
	DirectX::Mouse::ButtonStateTracker& mouseButtonTracker;

	/// @brief Common states
	DirectX::CommonStates& commonStates;


	/// @brief Current mouse state
	DirectX::Mouse::State mouseState;

	/// @brief Current keyboard state
	DirectX::Keyboard::State keyboardState;
	
	/// @brief Debug renderer
	HEIN::DebugRenderer* debugRenderer = nullptr;

	/// @brief Input manager
	HEIN::InputManager* inputManager = nullptr;

	/// @brief Debug collision renderer
	HEIN::DebugCollisionRenderer* debugCollisionRenderer = nullptr;

	/// @brief Event manager
	HEIN::EventManager* eventManager = nullptr;

	/// @brief Main camera controller
	HEIN::CameraController* mainCamera = nullptr;

	/// @brief Scene manager
	HEIN::SceneManager* sceneManager = nullptr;

	HEIN::ActorManager* actorManager = nullptr;
	HEIN::ShadowSystem* shadowSystem = nullptr;

	bool isEditorMode = false;
	bool showColliders = false;
	bool showUI = true;
	bool isSceneViewPass = false;
	EditorViewportLayout viewportLayout = EditorViewportLayout::SceneViewOnly;
	bool enableFrustumCulling = true;
};

