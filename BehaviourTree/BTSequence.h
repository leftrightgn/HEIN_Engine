#pragma once
#include "BehaviourTree/BTNode.h"
#include <vector>
#include <memory>


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

		/**
		 * @brief Propagates abort to the actively running child, triggers OnExit, and resets child index.
		 */
		void Abort(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID
		) override;

		/**
		 * @brief Recursively resets all children and resets child index to zero.
		 */
		void Reset() override;

		/**
		 * @brief Resets current child index on entry.
		 */
		void OnEnter(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID
		) override;

		/**
		 * @brief Resets current child index on exit.
		 */
		void OnExit(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID
		) override;

		std::string GetNodeName() const override { return "Sequence"; }
		std::string GetNodeDetails() const override { return "Sequential (AND): All Must Succeed"; }
		size_t GetChildCount() const override { return m_children.size(); }
		const BTNode* GetChild(size_t index) const override { return (index < m_children.size()) ? m_children[index].get() : nullptr; }
		size_t GetCurrentChildIndex() const { return m_currentChildIndex; }
	};
}
