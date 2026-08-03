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
	class DebugUIManager
	{
	private:
		bool m_isVisible = true;
		bool m_showViewportPreview = true;

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

		DirectX::SimpleMath::Vector2 GetViewportPos() const { return m_viewportPos; }
		DirectX::SimpleMath::Vector2 GetViewportSize() const { return m_viewportSize; }
		bool IsViewportVisibleInUI() const { return m_isViewportVisibleInUI; }

		void SetSelectedActor(HEIN::Actor* actor) { m_selectedActor = actor; }
		HEIN::Actor* GetSelectedActor() const { return m_selectedActor; }

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