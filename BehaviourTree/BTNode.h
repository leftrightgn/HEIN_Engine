#pragma once
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include <string>

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
		bool m_isActive = false;

	public:
		virtual ~BTNode() = default;

		/**
		 * @brief Invoked when execution enters this node for the first time or resumes from inactive state.
		 * Allows nodes to initialize timers, register listeners, or capture baseline actor state.
		 */
		virtual void OnEnter(
			HEIN::Actor* /*self*/,
			HEIN::ActorManager* /*manager*/,
			HEIN::ActorID /*targetID*/
		)
		{
			// Base implementation is empty; overridden by nodes requiring initialization
		}

		/**
		 * @brief Invoked when execution exits this node upon completion (Success or Failure) or interruption.
		 * Allows nodes to zero velocities, unregister listeners, or release action locks.
		 */
		virtual void OnExit(
			HEIN::Actor* /*self*/,
			HEIN::ActorManager* /*manager*/,
			HEIN::ActorID /*targetID*/
		)
		{
			// Base implementation is empty; overridden by nodes requiring cleanup
		}

		/**
		 * @brief Immediately aborts an active node, triggering OnExit and transitioning to Failure.
		 * Composite nodes must override this to propagate abort downwards to their active running child.
		 */
		virtual void Abort(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID
		)
		{
			if (m_isActive || m_currentState == BTNodeState::Running)
			{
				OnExit(self, manager, targetID);
				m_isActive = false;
				m_currentState = BTNodeState::Failure;
			}
		}

		/**
		 * @brief Resets internal execution state without actor context.
		 * Composite nodes override this to recursively reset all children.
		 */
		virtual void Reset()
		{
			m_isActive = false;
			m_currentState = BTNodeState::Failure;
		}

		virtual BTNodeState Tick(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			float deltaTime
		) = 0;

		virtual std::string GetNodeName() const { return "BTNode"; }
		virtual std::string GetNodeDetails() const { return ""; }
		virtual BTNodeState GetState() const { return m_currentState; }
		virtual bool IsActive() const { return m_isActive; }
		virtual bool IsRunning() const { return m_currentState == BTNodeState::Running; }
		virtual void SetActive(bool active) { m_isActive = active; }
		virtual size_t GetChildCount() const { return 0; }
		virtual const BTNode* GetChild(size_t /*index*/) const { return nullptr; }

	};
}
