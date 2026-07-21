#pragma once
#include "ColliderComponent.h"

namespace HEIN
{
	class StaticModelComponent;

	class SphereColliderComponent : public ColliderComponent
	{
	private:

		float m_radius;

		DirectX::BoundingSphere m_worldSphere;

	public:

		SphereColliderComponent(Actor* owner);
	
		void Initialize(const float radius);
		void InitializeFromModel(StaticModelComponent* staticModel);

		void Update(float deltaTime) override {}

		void SyncColliderState() override;
		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		float GetRadius() const { return m_radius; }

		DirectX::BoundingSphere GetWorldSphere() const { return m_worldSphere; }
	};


}

