#pragma once
#include <Entities/Actor.h>
#include <Entities/ActorManager.h>

namespace HEIN
{
	enum class BTNodeState
	{
		Success,
		Failure,
		Running
	};

	class BTNode
	{
	protected:

		BTNodeState m_currentState = BTNodeState::Failure;

	public:
		virtual ~BTNode() = default;

		virtual BTNodeState Tick(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			float deltaTime
		) = 0;

	};
}
