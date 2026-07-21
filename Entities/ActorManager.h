#pragma once
#include <Entities/Actor.h>

namespace HEIN
{
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

		void DestroyID(ActorID id);

		Actor* GetActor(ActorID id);
		Actor* GetActorByName(const std::wstring& name);

		void UpdateAll(float deltaTime);

		void LateUpdateAll(float deltaTime);

		void UpdateAllHierarchies();

		void DrawAll(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);

		void CleanUpDestroyedActors();

		const std::unordered_map<ActorID, std::unique_ptr<Actor>>& GetAllActors() const { return m_actors; }
	private:

		// internal helper for scene graph map
		void CascadeTransforms(ActorID parentID);
	};
}
