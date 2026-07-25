#pragma once
#include "ColliderComponent.h"

namespace HEIN
{
	class StaticModelComponent;

	class CapsuleColliderComponent : public ColliderComponent
	{
	private:

		float m_radius;
		float m_height;

		DirectX::SimpleMath::Vector3 m_worldTopCenter;
		DirectX::SimpleMath::Vector3 m_worldBottomCenter;
		DirectX::SimpleMath::Vector3 m_worldupDir;
		DirectX::SimpleMath::Vector3 m_worldrightDir;
		DirectX::SimpleMath::Vector3 m_worldforwardDir;

	public:
		std::string GetComponentName() const override { return "CapsuleColliderComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI() override;


		CapsuleColliderComponent(Actor* owner);
	

		void Initialize(float radius, float height);

		void Update(float /*deltaTime*/) override {}

		void SyncColliderState() override;

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		float GetRadius() const { return m_radius; }
		float GetHeight() const { return m_height; }

		void SetRadius(float radius) { m_radius = radius; }
		void SetHeight(float height) { m_height = height; }
		
		DirectX::SimpleMath::Vector3 GetWorldTopCenter() const { return m_worldTopCenter; }
		DirectX::SimpleMath::Vector3 GetWorldBottomCenter() const { return m_worldBottomCenter; }
	};


}

