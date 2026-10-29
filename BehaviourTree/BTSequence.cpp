#include "pch.h"
#include "BTSequence.h"


namespace HEIN
{
void HEIN::BTSequence::AddChild(std::unique_ptr<HEIN::BTNode> child)
{
	m_children.push_back(std::move(child));
}

void HEIN::BTSequence::OnEnter(
	HEIN::Actor* /*self*/,
	HEIN::ActorManager* /*manager*/,
	HEIN::ActorID /*targetID*/
)
{
	m_currentChildIndex = 0;
}

void HEIN::BTSequence::OnExit(
	HEIN::Actor* /*self*/,
	HEIN::ActorManager* /*manager*/,
	HEIN::ActorID /*targetID*/
)
{
	m_currentChildIndex = 0;
}

void HEIN::BTSequence::Abort(
	HEIN::Actor* self,
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID
)
{
	// Propagate abort to the currently running child if valid
	if (m_currentChildIndex < m_children.size() && m_children[m_currentChildIndex])
	{
		m_children[m_currentChildIndex]->Abort(self, manager, targetID);
	}

	m_currentChildIndex = 0;
	BTNode::Abort(self, manager, targetID);
}

void HEIN::BTSequence::Reset()
{
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

HEIN::BTNodeState HEIN::BTSequence::Tick(
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

	// If previously not running, start at the first child
	if (m_currentState != BTNodeState::Running)
	{
		m_currentChildIndex = 0;
	}

	for (; m_currentChildIndex < m_children.size(); ++m_currentChildIndex)
	{
		if (!m_children[m_currentChildIndex])
		{
			continue;
		}

		BTNodeState childState = m_children[m_currentChildIndex]->Tick(self, manager, targetID, deltaTime);

		if (childState == BTNodeState::Failure)
		{
			m_currentState = BTNodeState::Failure;
			m_currentChildIndex = 0;
			OnExit(self, manager, targetID);
			m_isActive = false;
			return m_currentState;
		}

		if (childState == BTNodeState::Running)
		{
			m_currentState = BTNodeState::Running;
			return m_currentState;
		}
	}

	// All children succeeded
	m_currentState = BTNodeState::Success;
	m_currentChildIndex = 0;
	OnExit(self, manager, targetID);
	m_isActive = false;
	return m_currentState;
}


} // namespace HEIN
