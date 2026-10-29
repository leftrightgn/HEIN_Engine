#pragma once
#include "Entities/Actor.h"
#include "Framework/GameContext.h"
#include "DebugUIManager.h"
#include "Common/OffscreenRenderTarget.h"
#include <Entities/ActorManager.h>

namespace HEIN
{
	class Skybox;

	#if HEIN_WITH_EDITOR
	class DebugDisplayController
	{
	private:

	
		bool m_isVisible;
		bool m_isMagnified;
		bool m_justInitialized = false;

		DirectX::SimpleMath::Matrix m_projMatrix;

		float m_virtualMouseX = 0.0f;
		float m_virtualMouseY = 0.0f;
		bool m_isSceneCameraDragging = false;
		float m_lastVirtualScroll = 0.0f;

		HEIN::ActorManager m_actorManager;

		DebugUIManager m_debugUI;
		HEIN::ActorID m_debugPlayerID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugSwordID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugAxeID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugStageID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugEnemyID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugCameraID = HEIN::INVALID_ACTOR_ID;

		HEIN::EditorAction m_currentAction = HEIN::EditorAction::None;

		OffscreenRenderTarget m_sceneRenderTarget;
		OffscreenRenderTarget m_gameRenderTarget;
	public:

		DebugDisplayController();
		~DebugDisplayController() = default;

		void Initialize();

		void Update(const GameContext& gameContext, HEIN::ActorManager& mainActorManager);

		void Render(
			GameContext& gameContext,
			HEIN::ActorManager& actorManager,
			Skybox* skybox,
			DirectX::SimpleMath::Matrix mainView,
			DirectX::SimpleMath::Matrix mainProj);

		bool isMagnified() { return m_isMagnified; }

		bool isVisible() { return m_isVisible; }

		const DirectX::SimpleMath::Matrix GetViewMatrix() const;
		const DirectX::SimpleMath::Matrix GetProjMatrix() const;

		void SetDebugTargets(
			HEIN::ActorID playerID = HEIN::INVALID_ACTOR_ID,
			HEIN::ActorID swordID = HEIN::INVALID_ACTOR_ID,
			HEIN::ActorID axeID = HEIN::INVALID_ACTOR_ID,
			HEIN::ActorID stageID = HEIN::INVALID_ACTOR_ID,
			HEIN::ActorID enemyID = HEIN::INVALID_ACTOR_ID
		);

		EditorAction GetUIAction() const { return m_currentAction; }
		DebugUIManager& GetDebugUI() { return m_debugUI; }
		const DebugUIManager& GetDebugUI() const { return m_debugUI; }

		bool IsShowUIEnabled() const { return m_debugUI.IsShowUIEnabled(); }
		void SetShowUIEnabled(bool enabled) { m_debugUI.SetShowUIEnabled(enabled); }

		RightPanelMode GetRightPanelMode() const { return m_debugUI.GetRightPanelMode(); }
		void SetRightPanelMode(RightPanelMode mode) { m_debugUI.SetRightPanelMode(mode); }

		EditorViewportLayout GetViewportLayout() const { return m_debugUI.GetViewportLayout(); }
		void SetViewportLayout(EditorViewportLayout layout) { m_debugUI.SetViewportLayout(layout); }

		OffscreenRenderTarget& GetSceneRenderTarget() noexcept { return m_sceneRenderTarget; }
		OffscreenRenderTarget& GetGameRenderTarget() noexcept { return m_gameRenderTarget; }
		ID3D11ShaderResourceView* GetSceneSRV() const noexcept { return m_sceneRenderTarget.GetShaderResourceView(); }
		ID3D11ShaderResourceView* GetGameSRV() const noexcept { return m_gameRenderTarget.GetShaderResourceView(); }
	};
#else
	class DebugDisplayController
	{
	public:
		DebugDisplayController() = default;
		~DebugDisplayController() = default;
		void Initialize() {}
		void Update(const GameContext&, HEIN::ActorManager&) {}
		void Render(GameContext&, HEIN::ActorManager&, Skybox*, DirectX::SimpleMath::Matrix, DirectX::SimpleMath::Matrix) {}
		bool isMagnified() { return false; }
		bool isVisible() { return false; }
		const DirectX::SimpleMath::Matrix GetViewMatrix() const { return DirectX::SimpleMath::Matrix::Identity; }
		const DirectX::SimpleMath::Matrix GetProjMatrix() const { return DirectX::SimpleMath::Matrix::Identity; }
		void SetDebugTargets(HEIN::ActorID = HEIN::INVALID_ACTOR_ID, HEIN::ActorID = HEIN::INVALID_ACTOR_ID, HEIN::ActorID = HEIN::INVALID_ACTOR_ID, HEIN::ActorID = HEIN::INVALID_ACTOR_ID, HEIN::ActorID = HEIN::INVALID_ACTOR_ID) {}
		EditorAction GetUIAction() const { return EditorAction::None; }
		DebugUIManager& GetDebugUI() { static DebugUIManager dummy; return dummy; }
		const DebugUIManager& GetDebugUI() const { static DebugUIManager dummy; return dummy; }
		bool IsShowUIEnabled() const { return true; }
		void SetShowUIEnabled(bool) {}
		RightPanelMode GetRightPanelMode() const { return RightPanelMode::Inspector; }
		void SetRightPanelMode(RightPanelMode) {}
		EditorViewportLayout GetViewportLayout() const { return EditorViewportLayout::GameViewOnly; }
		void SetViewportLayout(EditorViewportLayout) {}
	};
#endif
}

