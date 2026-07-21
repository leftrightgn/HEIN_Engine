#include "pch.h"
#include "BTSelector.h"

void HEIN::BTSelector::AddChild(std::unique_ptr<HEIN::BTNode> child)
{
	m_children.push_back(std::move(child));
}

HEIN::BTNodeState HEIN::BTSelector::Tick(
	HEIN::Actor* self,
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID,
	float deltaTime
)
{
	if (m_currentState != BTNodeState::Running)
	{
		m_currentChildIndex = 0;
	}

	for (; m_currentChildIndex < m_children.size(); ++m_currentChildIndex)
	{
		BTNodeState childState = m_children[m_currentChildIndex]->Tick(self, manager, targetID, deltaTime);

		if (childState == BTNodeState::Success)
		{
			m_currentState = BTNodeState::Success;
			return m_currentState;
		}
		if (childState == BTNodeState::Running)
		{
			m_currentState = BTNodeState::Running;
			return m_currentState;
		}
	}
	m_currentState = BTNodeState::Failure;
	return m_currentState;
}
