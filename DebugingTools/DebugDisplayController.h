#pragma once
#include "Entities/Actor.h"
#include "Framework/GameContext.h"
#include "DebugUIManager.h"
#include <Entities/ActorManager.h>

namespace HEIN
{
	class Skybox;

	class DebugDisplayController
	{
	private:

	
		bool m_isVisible;
		bool m_isMagnified;

		DirectX::SimpleMath::Matrix m_projMatrix;

		float m_virtualMouseX = 0.0f;
		float m_virtualMouseY = 0.0f;

		HEIN::ActorManager m_actorManager;

		DebugUIManager m_debugUI;
		HEIN::ActorID m_debugPlayerID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugSwordID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugAxeID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugStageID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugEnemyID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_debugCameraID = HEIN::INVALID_ACTOR_ID;

		HEIN::EditorAction m_currentAction = HEIN::EditorAction::None;
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

		void SetDebugTargets(HEIN::ActorID playerID, HEIN::ActorID swordID, HEIN::ActorID axeID, HEIN::ActorID stageID, HEIN::ActorID enemyID);

		EditorAction GetUIAction() const { return m_currentAction; }
	};
}

