#pragma once
#include "ColliderComponent.h"

namespace HEIN
{
	class StaticModelComponent;

	class AABBColliderComponent : public ColliderComponent
	{
	private:

		DirectX::SimpleMath::Vector3 m_extents;

		DirectX::BoundingBox m_worldAABB;

	public:
		std::string GetComponentName() const override { return "AABBColliderComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI() override;


		AABBColliderComponent(Actor* owner);
	
		void Initialize(const DirectX::SimpleMath::Vector3 extents);
		void InitializeFromModel(StaticModelComponent* staticModel);

		void Update(float /*deltaTime*/) override {}

		void SyncColliderState() override;

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		DirectX::SimpleMath::Vector3 GetExtents() const { return m_extents; }
		void SetExtents(DirectX::SimpleMath::Vector3 extents) { m_extents = extents; }

		DirectX::BoundingBox GetWorldAABB() const { return m_worldAABB; }
	};


}

