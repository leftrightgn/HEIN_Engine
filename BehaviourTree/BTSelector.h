#pragma once
#include "BehaviourTree/BTNode.h"
#include <vector>
#include <memory>
#include <string>


namespace HEIN
{
	/**
	 * @brief BTSelector (Priority Selector) evaluates its children in order from left to right (highest to lowest priority).
	 * 
	 * RESPONSIBILITY:
	 * Acts as the decision-maker branching node in the Behaviour Tree. It stops at the first child that succeeds
	 * or is currently running.
	 * 
	 * STATE EVALUATION:
	 * - Success: Returns Success immediately if any child succeeds.
	 * - Running: Returns Running immediately if a child returns Running (halts evaluation of lower priority children).
	 * - Failure: Returns Failure only if ALL children fail.
	 */
	class BTSelector : public BTNode
	{
	private:

		std::vector<std::unique_ptr<HEIN::BTNode>> m_children;
		size_t m_currentChildIndex = 0;

	public:

		void AddChild(std::unique_ptr<HEIN::BTNode> child);

		/**
		 * @brief Executes the active child or starts from the highest priority child.
		 * Reads the decoupled state from the CombatBlackBoard (if used by children) to determine branch viability.
		 */
		BTNodeState Tick(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			float deltaTime
		) override;

		/**
		 * @brief Aborts the currently running child and resets selector state.
		 */
		void Abort(
			HEIN::Actor* self,
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID
		) override;

		/**
		 * @brief Resets the selector state and cascades reset to all child nodes.
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

		std::string GetNodeName() const override { return "Selector"; }
		std::string GetNodeDetails() const override { return "Reactive Priority (OR): First Success / Running with Preemption"; }
		size_t GetChildCount() const override { return m_children.size(); }
		const BTNode* GetChild(size_t index) const override { return (index < m_children.size()) ? m_children[index].get() : nullptr; }
		size_t GetCurrentChildIndex() const { return m_currentChildIndex; }
	};
}
