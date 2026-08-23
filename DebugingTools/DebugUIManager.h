#pragma once
#include "Framework/GameContext.h"
#include "Entities/Actor.h"
#include <ImGui/imgui.h>
#include <ImGui/ImGuizmo.h>



namespace HEIN
{
	enum class EditorAction
	{
		None,
		PlayPressed,
		StopPressed,
		SavePressed,
		AutoSavePressed,
		LoadPressed,
		NewScenePressed,
		CreateStagePressed
	};

	class ActorManager;
	class IGizmoEditable;

	extern IGizmoEditable* g_ActiveGizmoTarget;

	/**
	 * @class DebugUIManager
	 * @brief Central coordinator for the Level Editor interface and debugging overlays.
	 * 
	 * DebugUIManager bridges the engine's internal actor/component data with the Dear ImGui 
	 * immediate-mode UI framework. It manages the full level editor pipeline state, including:
	 * - Scene Hierarchy: Tree-view traversal of the Actor parent-child scene graph.
	 * - Property Inspector: Dynamic reflection of selected actor and component properties.
	 * - 3D Viewport Controls: Integration with ImGuizmo to provide interactive translation, 
	 *   rotation, and scaling handles directly within the 3D scene view.
	 * 
	 * By decoupling the editor UI from the core engine runtime, this manager allows developers 
	 * to author, inspect, and modify the game world in real-time before saving the state 
	 * out to the JSON serialization pipeline.
	 */
	class DebugUIManager
	{
	private:
		bool m_isVisible = true;
		bool m_showViewportPreview = true;
		bool m_showAnimatorWindow = false;
		bool m_showColliders = true;

		double m_lastleftClickTime = -1;
		const double DOUBLE_CLICK_THRESHOLD = 0.3;

		HEIN::Actor* m_selectedActor = nullptr;
		ImGuizmo::OPERATION m_currentGinzmoOperation = ImGuizmo::TRANSLATE;
		ImGuizmo::MODE m_currentGinzmo = ImGuizmo::WORLD;

		DirectX::SimpleMath::Vector2 m_viewportPos = DirectX::SimpleMath::Vector2(0.0f, 0.0f);
		DirectX::SimpleMath::Vector2 m_viewportSize = DirectX::SimpleMath::Vector2(400.0f, 225.0f);
		bool m_isViewportVisibleInUI = true;

	public:
		DebugUIManager() = default;
		~DebugUIManager() = default;

		bool IsViewportPreviewEnabled() const { return m_showViewportPreview; }
		void SetViewportPreviewEnabled(bool enabled) { m_showViewportPreview = enabled; }

		bool IsShowCollidersEnabled() const { return m_showColliders; }
		void SetShowCollidersEnabled(bool enabled) { m_showColliders = enabled; }

		DirectX::SimpleMath::Vector2 GetViewportPos() const { return m_viewportPos; }
		DirectX::SimpleMath::Vector2 GetViewportSize() const { return m_viewportSize; }
		bool IsViewportVisibleInUI() const { return m_isViewportVisibleInUI; }

		void SetSelectedActor(HEIN::Actor* actor) { SelectActor(actor); }
		HEIN::Actor* GetSelectedActor() const { return m_selectedActor; }

		void SelectActor(HEIN::Actor* actor);
		void DrawActorTreeNode(
			HEIN::Actor* actor,
			HEIN::ActorManager& manager,
			GameContext& gameContext,
			HEIN::ActorID& actorToDelete
		);

		void DrawViewportWindow(GameContext& gameContext, bool isMagnified);

		void Update(
			const GameContext& gameContext,
			HEIN::ActorManager& actorManager,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);

		EditorAction Draw(
			GameContext& gameContext,
			HEIN::ActorManager& manager,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);
	};
}