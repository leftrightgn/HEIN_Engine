#pragma once
#include "Framework/GameContext.h"
#include "Entities/Actor.h"
#include "Scene/SceneManager.h"
#if HEIN_WITH_EDITOR
#include <ImGui/imgui.h>
#include <ImGui/ImGuizmo.h>
#endif

namespace HEIN
{
	enum class RightPanelMode
	{
		Inspector,
		SceneTransition
	};

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
	#if HEIN_WITH_EDITOR
	class DebugUIManager
	{
	private:
		bool m_isVisible = true;
		bool m_showViewportPreview = false;
		bool m_isSceneViewActive = true;
		EditorViewportLayout m_viewportLayout = EditorViewportLayout::SceneViewOnly;
		bool m_showColliders = true;
		bool m_showUI = true;

		double m_lastleftClickTime = -1;
		const double DOUBLE_CLICK_THRESHOLD = 0.3;

		HEIN::Actor* m_selectedActor = nullptr;
		ImGuizmo::OPERATION m_currentGinzmoOperation = ImGuizmo::TRANSLATE;
		ImGuizmo::MODE m_currentGinzmo = ImGuizmo::WORLD;

		DirectX::SimpleMath::Vector2 m_viewportPos = DirectX::SimpleMath::Vector2(0.0f, 0.0f);
		DirectX::SimpleMath::Vector2 m_viewportSize = DirectX::SimpleMath::Vector2(400.0f, 225.0f);
		bool m_isViewportVisibleInUI = false;
		float m_hierarchyWidth = 0.0f;
		float m_inspectorWidth = 0.0f;
		float m_toolbarHeight = 88.0f;
		bool m_isHierarchyFloating = false;
		bool m_isInspectorFloating = false;
		bool m_isToolBarFloating = false;
		bool m_resetLayoutRequested = false;
		ID3D11ShaderResourceView* m_sceneSRV = nullptr;
		ID3D11ShaderResourceView* m_gameSRV = nullptr;
		ID3D11ShaderResourceView* m_particleSRV = nullptr;
		HEIN::CameraController* m_debugCameraController = nullptr;

		ImVec2 m_sceneContentSize = ImVec2(1280.0f, 720.0f);
		ImVec2 m_sceneContentPos = ImVec2(0.0f, 0.0f);
		ImVec2 m_gameContentSize = ImVec2(1280.0f, 720.0f);
		ImVec2 m_gameContentPos = ImVec2(0.0f, 0.0f);
		ImVec2 m_particleContentSize = ImVec2(1280.0f, 720.0f);
		ImVec2 m_particleContentPos = ImVec2(0.0f, 0.0f);
		bool m_isSceneViewHovered = false;
		bool m_isGameViewHovered = false;
		bool m_isSceneViewFocused = false;
		bool m_isGameViewFocused = false;

		// Right Panel Mode (Inspector vs Scene Transition)
		RightPanelMode m_rightPanelMode = RightPanelMode::Inspector;
		SceneTransitionDesc m_editorTransitionDesc;
		bool m_hasLoadedEditorDesc = false;
		std::string m_transitionStatusMessage;
		float m_transitionStatusTimer = 0.0f;
		int m_selectedTargetSceneIndex = 0;
		int m_transitionStyleIndex = 0;
		int m_draggingTransitionElement = 0; // 0 = None, 1 = Tip Text, 2 = Loading Bar

	public:
		DebugUIManager() = default;
		~DebugUIManager();

		RightPanelMode GetRightPanelMode() const { return m_rightPanelMode; }
		void SetRightPanelMode(RightPanelMode mode) { m_rightPanelMode = mode; }
		void RenderSceneTransitionPanel(GameContext& gameContext);

		static void GetActiveViewportRect(float& outX, float& outY, float& outW, float& outH);
		static void SetActiveViewportRect(float x, float y, float w, float h);

		D3D11_VIEWPORT GetCenterViewport(const GameContext& gameContext) const;
		D3D11_VIEWPORT GetSceneViewport(const GameContext& gameContext) const;
		D3D11_VIEWPORT GetGameViewport(const GameContext& gameContext) const;
		D3D11_VIEWPORT GetParticleViewport(const GameContext& gameContext) const;

		EditorViewportLayout GetViewportLayout() const { return m_viewportLayout; }
		void SetViewportLayout(EditorViewportLayout layout)
		{
			m_viewportLayout = layout;
			m_isSceneViewActive = (layout != EditorViewportLayout::GameViewOnly);
		}

		bool IsUIVisible() const { return m_isVisible; }

		bool IsViewportPreviewEnabled() const { return m_showViewportPreview; }
		void SetViewportPreviewEnabled(bool enabled) { m_showViewportPreview = enabled; }

		bool IsSceneViewActive() const { return m_isSceneViewActive; }
		void SetSceneViewActive(bool active)
		{
			m_isSceneViewActive = active;
			m_viewportLayout = active ? EditorViewportLayout::SceneViewOnly : EditorViewportLayout::GameViewOnly;
		}

		bool IsShowCollidersEnabled() const { return m_showColliders; }
		void SetShowCollidersEnabled(bool enabled) { m_showColliders = enabled; }

		void RequestResetLayout() { m_resetLayoutRequested = true; }

		ImVec2 GetSceneContentSize() const { return m_sceneContentSize; }
		ImVec2 GetSceneContentPos() const { return m_sceneContentPos; }
		ImVec2 GetGameContentSize() const { return m_gameContentSize; }
		ImVec2 GetGameContentPos() const { return m_gameContentPos; }
		bool IsSceneViewHovered() const { return m_isSceneViewHovered; }
		bool IsGameViewHovered() const { return m_isGameViewHovered; }
		bool IsSceneViewFocused() const { return m_isSceneViewFocused; }
		bool IsGameViewFocused() const { return m_isGameViewFocused; }

		bool IsShowUIEnabled() const { return m_showUI; }
		void SetShowUIEnabled(bool enabled) { m_showUI = enabled; }

		DirectX::SimpleMath::Vector2 GetViewportPos() const { return m_viewportPos; }
		DirectX::SimpleMath::Vector2 GetViewportSize() const { return m_viewportSize; }
		bool IsViewportVisibleInUI() const { return m_isViewportVisibleInUI; }

		void SetSelectedActor(HEIN::Actor* actor) { SelectActor(actor); }
		HEIN::Actor* GetSelectedActor() const { return m_selectedActor; }

		void FocusOnActor(HEIN::Actor* actor);
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
			const DirectX::SimpleMath::Matrix& proj,
			HEIN::CameraController* debugCameraController = nullptr
		);

		void SetViewportSRVs(ID3D11ShaderResourceView* sceneSRV, ID3D11ShaderResourceView* gameSRV, ID3D11ShaderResourceView* particleSRV = nullptr) noexcept
		{
			m_sceneSRV = sceneSRV;
			m_gameSRV = gameSRV;
			m_particleSRV = particleSRV;
		}
		ID3D11ShaderResourceView* GetSceneSRV() const noexcept { return m_sceneSRV; }
		ID3D11ShaderResourceView* GetGameSRV() const noexcept { return m_gameSRV; }
		ID3D11ShaderResourceView* GetParticleSRV() const noexcept { return m_particleSRV; }
		ImVec2 GetParticleContentSize() const { return m_particleContentSize; }
		ImVec2 GetParticleContentPos() const { return m_particleContentPos; }

		EditorAction Draw(
			GameContext& gameContext,
			HEIN::ActorManager& manager,
			const DirectX::SimpleMath::Matrix& sceneView,
			const DirectX::SimpleMath::Matrix& sceneProj,
			const DirectX::SimpleMath::Matrix& gameView,
			const DirectX::SimpleMath::Matrix& gameProj
		);
	};
#else
	inline IGizmoEditable* g_ActiveGizmoTarget = nullptr;

	class DebugUIManager
	{
	public:
		DebugUIManager() = default;
		~DebugUIManager() = default;
		static void GetActiveViewportRect(float& outX, float& outY, float& outW, float& outH) noexcept
		{
			outX = 0.0f; outY = 0.0f; outW = 0.0f; outH = 0.0f;
		}
		static void SetActiveViewportRect(float, float, float, float) noexcept {}

		D3D11_VIEWPORT GetGameViewport(GameContext& gc) const { return gc.deviceResources.GetScreenViewport(); }
		D3D11_VIEWPORT GetSceneViewport(GameContext& gc) const { return gc.deviceResources.GetScreenViewport(); }
		D3D11_VIEWPORT GetParticleViewport(GameContext& gc) const { return gc.deviceResources.GetScreenViewport(); }
		bool IsShowUIEnabled() const { return true; }
		void SetShowUIEnabled(bool) {}
		EditorViewportLayout GetViewportLayout() const { return EditorViewportLayout::GameViewOnly; }
		void SetViewportLayout(EditorViewportLayout) {}
		void SetSelectedActor(HEIN::Actor*) {}
		HEIN::Actor* GetSelectedActor() const { return nullptr; }
		bool IsGameViewHovered() const { return true; }
		bool IsSceneViewHovered() const { return false; }
		DirectX::SimpleMath::Vector2 GetViewportPos() const { return DirectX::SimpleMath::Vector2::Zero; }
		DirectX::SimpleMath::Vector2 GetViewportSize() const { return DirectX::SimpleMath::Vector2::Zero; }
		bool IsViewportVisibleInUI() const { return false; }
		RightPanelMode GetRightPanelMode() const { return RightPanelMode::Inspector; }
		void SetRightPanelMode(RightPanelMode) {}
	};
#endif
}