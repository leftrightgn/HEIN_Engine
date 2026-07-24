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
		LoadPressed,
		NewScenePressed
	};

	class ActorManager;
	class IGizmoEditable;

	extern IGizmoEditable* g_ActiveGizmoTarget;
	class DebugUIManager
	{
	private:
		bool m_isVisible = true;

		double m_lastleftClickTime = -1;
		const double DOUBLE_CLICK_THRESHOLD = 0.3;

		HEIN::Actor* m_selectedActor = nullptr;
		ImGuizmo::OPERATION m_currentGinzmoOperation = ImGuizmo::TRANSLATE;
		ImGuizmo::MODE m_currentGinzmo = ImGuizmo::WORLD;

	public:
		DebugUIManager() = default;
		~DebugUIManager() = default;

		void Update(
			const GameContext& gameContext,
			HEIN::ActorManager& actorManager,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);

		EditorAction Draw(
			HEIN::ActorManager& manager,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);
	};
}