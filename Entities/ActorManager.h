#pragma once
#include <Entities/Actor.h>

namespace HEIN
{
	// Primary storage container and lifecycle manager for all entities in the scene.
	// Uses a handle-based memory layout (ActorID map) to ensure memory stability.
	// Systems pass ActorID (uint32_t) integers instead of raw pointers to prevent dangling references.
	class ActorManager
	{
	private:

		ActorID m_nextID = 1;

		std::unordered_map<ActorID, std::unique_ptr<Actor>> m_actors;

		std::vector<ActorID> m_pendingDestorys;

	public:

		ActorManager() = default;
		~ActorManager() = default;

		Actor* CreateActor(const std::wstring& tag);

		// Queues an actor for destruction at the end of the frame (Phase D).
		// Ensures no dangling pointers during the current frame's iteration.
		void DestroyID(ActorID id);
		void DeleteActor(ActorID id);

		Actor* GetActor(ActorID id);
		bool HasActor(ActorID id) const { return m_actors.find(id) != m_actors.end(); }
		Actor* GetActorByName(const std::wstring& name);

		void UpdateAll(float deltaTime);

		void LateUpdateAll(float deltaTime);

		// Traverses the parent-child scene graph and recalculates world matrices.
		// Formula: M_world = M_local * M_parent_world (if parent exists)
		void UpdateAllHierarchies();

		void DrawAll(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);

		void DrawAllShadows(
			GameContext& gameContext, 
			const DirectX::SimpleMath::Matrix& lightViewProj
		);

		// Called at the end of the frame (Phase D) to safely delete all queued actors.
		void CleanUpDestroyedActors();

		const std::unordered_map<ActorID, std::unique_ptr<Actor>>& GetAllActors() const { return m_actors; }

		nlohmann::json Serialize();
		void Deserialize(const nlohmann::json& sceneData);
		void InitializeAfterDeserialize(GameContext& gameContext);

		Actor* DuplicateActor(Actor* sourceActor, GameContext& gameContext, ActorID newParentID = INVALID_ACTOR_ID);

		void SetParent(ActorID childID, ActorID newParentID, bool keepWorldTransform = true);
		bool IsDescendantOf(ActorID potentialChild, ActorID potentialParent) const;

		void ClearAllActors();
	private:

		// internal helper for scene graph map: recursively applies parent transform to children
		void CascadeTransforms(ActorID parentID);
	};
}
