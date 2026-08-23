#pragma once
#include "IComponent.h"

namespace HEIN
{
	class TransformComponent;

	// Handles physical forces, integrating acceleration and velocity.
	// Essential for the kinematic separation of Solid Pushboxes.
	class RigidBodyComponent : public IComponent
	{
	private:
		
		TransformComponent* m_transform;

		DirectX::SimpleMath::Vector3 m_velocity;
		DirectX::SimpleMath::Vector3 m_acceleration;

		float m_mass;
		bool m_useGravity;
		bool m_isKinematic;
		bool m_isGrounded = false;

	public:
		std::string GetComponentName() const override { return "RigidBodyComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;


		// PURPOSE: Uses an exaggerated gravity scale (architected as -98.0 m/s^2 conceptually) 
		// to enforce snappy, fast-paced action combat without floaty jump physics.
		// INTENT: Ensures that when actors become airborne, they return to the ground rapidly,
		// maintaining the high-intensity tempo of the game's combat mechanics.
		static constexpr float GRAVITY_FORCE = -98.0f;

		friend class PhysicsSystem;
	public:

		RigidBodyComponent(Actor* owner);

		void Initialize(float mass, bool useGravity, bool isKinematic);

		void Start() override;
		void Update(float deltaTime) override;
		void Draw(
			GameContext& /*gameContext*/,
			const DirectX::SimpleMath::Matrix& /*world*/,
			const DirectX::SimpleMath::Matrix& /*view*/,
			const DirectX::SimpleMath::Matrix& /*proj*/
		) override { }

		void AddForce(const DirectX::SimpleMath::Vector3& force);
		void SetVelocity(const DirectX::SimpleMath::Vector3& velocity);
		void SetHorizontalVelocity(const DirectX::SimpleMath::Vector3& velocity);
		DirectX::SimpleMath::Vector3 GetVelocity() const;
		 
		bool isKinematic() const { return m_isKinematic; }
		bool UsesGravity() const { return m_useGravity; }
	};

}

