#include "pch.h"
#include "BTSelector.h"


namespace HEIN
{
void HEIN::BTSelector::AddChild(std::unique_ptr<HEIN::BTNode> child)
{
	m_children.push_back(std::move(child));
}

void HEIN::BTSelector::OnEnter(
	HEIN::Actor* /*self*/,
	HEIN::ActorManager* /*manager*/,
	HEIN::ActorID /*targetID*/
)
{
	m_currentChildIndex = 0;
}

void HEIN::BTSelector::OnExit(
	HEIN::Actor* /*self*/,
	HEIN::ActorManager* /*manager*/,
	HEIN::ActorID /*targetID*/
)
{
	m_currentChildIndex = 0;
}

HEIN::BTNodeState HEIN::BTSelector::Tick(
	HEIN::Actor* self,
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID,
	float deltaTime
)
{
	// Trigger OnEnter when transitioning from inactive state
	if (!m_isActive)
	{
		OnEnter(self, manager, targetID);
		m_isActive = true;
		m_currentChildIndex = 0;
	}

	// Defensive guard: an empty selector fails immediately
	if (m_children.empty())
	{
		m_currentState = BTNodeState::Failure;
		m_currentChildIndex = 0;
		OnExit(self, manager, targetID);
		m_isActive = false;
		return m_currentState;
	}

	// Record index of the child that was running on the previous tick
	// If selector was not in Running state, no child was running
	size_t previousRunningIndex = (m_currentState == BTNodeState::Running && m_currentChildIndex < m_children.size())
		? m_currentChildIndex
		: m_children.size();

	// Reactive Priority Evaluation: Always evaluate candidate branches starting from index 0 (highest priority)
	for (size_t i = 0; i < m_children.size(); ++i)
	{
		if (!m_children[i])
		{
			continue;
		}

		BTNodeState childState = m_children[i]->Tick(self, manager, targetID, deltaTime);

		// Branch Success: Selector succeeds immediately
		if (childState == BTNodeState::Success)
		{
			// If a different (lower-priority) child was running, preempt and abort it
			if (previousRunningIndex < m_children.size() && previousRunningIndex != i)
			{
				if (m_children[previousRunningIndex])
				{
					m_children[previousRunningIndex]->Abort(self, manager, targetID);
				}
			}

			m_currentChildIndex = 0;
			m_currentState = BTNodeState::Success;
			OnExit(self, manager, targetID);
			m_isActive = false;
			return m_currentState;
		}

		// Branch Running: Active branch selected or maintained
		if (childState == BTNodeState::Running)
		{
			// If higher-priority child preempted a lower-priority running child, abort the lower child
			if (previousRunningIndex < m_children.size() && previousRunningIndex != i)
			{
				if (m_children[previousRunningIndex])
				{
					m_children[previousRunningIndex]->Abort(self, manager, targetID);
				}
			}

			m_currentChildIndex = i;
			m_currentState = BTNodeState::Running;
			return m_currentState;
		}

		// Branch Failure: childState == BTNodeState::Failure
		// If this was the child running on previous tick and it just completed with Failure,
		// clear previousRunningIndex so lower-priority branches are not erroneously aborted
		if (previousRunningIndex == i)
		{
			previousRunningIndex = m_children.size();
		}
	}

	// All children evaluated and failed: selector fails
	m_currentChildIndex = 0;
	m_currentState = BTNodeState::Failure;
	OnExit(self, manager, targetID);
	m_isActive = false;
	return m_currentState;
}

void HEIN::BTSelector::Abort(
	HEIN::Actor* self,
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID
)
{
	// Cascade abort to currently active running child if one exists
	if (m_currentState == BTNodeState::Running && m_currentChildIndex < m_children.size())
	{
		if (m_children[m_currentChildIndex])
		{
			m_children[m_currentChildIndex]->Abort(self, manager, targetID);
		}
	}

	m_currentChildIndex = 0;
	BTNode::Abort(self, manager, targetID);
}

void HEIN::BTSelector::Reset()
{
	// Cascade reset to all child nodes to ensure clean internal subtree state
	for (auto& child : m_children)
	{
		if (child)
		{
			child->Reset();
		}
	}

	m_currentChildIndex = 0;
	BTNode::Reset();
}


} // namespace HEIN
