#pragma once
#include <BehaviourTree/BTNode.h>

namespace HEIN
{
	/**
	 * @brief BTSequence enforces a strict, ordered execution of its child nodes.
	 * 
	 * RESPONSIBILITY:
	 * Acts as an "AND" gate for behaviors. It is used to execute a series of steps in a specific order 
	 * (e.g., Check Distance -> Turn to Target -> Attack).
	 * 
	 * STATE EVALUATION:
	 * - Success: Returns Success only if ALL children succeed in sequence.
	 * - Running: Returns Running immediately if any child returns Running (remembers the current child for the next tick).
	 * - Failure: Returns Failure immediately if any child fails, aborting the rest of the sequence.
	 */
	class BTSequence : public BTNode
	{
	private:
		
		std::vector<std::unique_ptr<HEIN::BTNode>> m_children;
		size_t m_currentChildIndex = 0;
	public:

		void AddChild(std::unique_ptr<HEIN::BTNode> child);

		/**
		 * @brief Ticks the current child in the sequence. Often relies on condition checks 
		 * (like distance or cooldown via CombatBlackBoard) to proceed to action nodes.
		 */
		BTNodeState Tick(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			float deltaTime
		) override;
	};
}
