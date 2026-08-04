#pragma once
#include "ColliderComponent.h"
#include <Framework/GameContext.h>

namespace HEIN
{
	class StaticModelComponent;

	class OBBColliderComponent : public ColliderComponent
	{
	private:

		DirectX::SimpleMath::Vector3 m_extents;

		DirectX::BoundingOrientedBox m_worldOBB;

	public:
		std::string GetComponentName() const override { return "OBBColliderComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;


		OBBColliderComponent(Actor* owner);

		void Initialize(const DirectX::SimpleMath::Vector3 extents);
		void InitializeFromModel(StaticModelComponent* staticModel);

		void Start() override;
		void Update(float /*deltaTime*/) override {}

		void SyncColliderState() override;

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;
		DirectX::SimpleMath::Vector3 GetExtents() const  { return m_extents; }

		void SetExtents(DirectX::SimpleMath::Vector3 extents)
		{
			m_extents = extents;
		}

		DirectX::BoundingOrientedBox GetWorldOBB() const { return m_worldOBB; }
	};
}