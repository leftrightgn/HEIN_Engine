#pragma once
#include <Components/IComponent.h>
#include <Entities/Actor.h>


namespace HEIN
{
	class ActorManager;
	class TransformComponent;
	class TargetTrackingComponent : public IComponent
	{
	private:

		HEIN::ActorManager* m_actorManager = nullptr;
		HEIN::TransformComponent* m_transform = nullptr;
		HEIN::ActorID m_targetID = HEIN::INVALID_ACTOR_ID;
		DirectX::SimpleMath::Vector3 m_dirToTarget = DirectX::SimpleMath::Vector3::Zero;
		float m_distanceToTarget = 0.0f;
		bool m_isLockedOn = false;

		HEIN::ActorType m_targetTypeToFind;

		HEIN::ActorID FindBestTarget() const;

	public:
		std::string GetComponentName() const override { return "TargetTrackingComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI() override;


		TargetTrackingComponent(Actor* owner, ActorManager* manager, HEIN::ActorType targetType = HEIN::ActorType::Default);

		void Start() override;

		void Update(float deltaTime) override;

		ActorID GetTargetID() const { return m_targetID; }
		void SetTargetID(ActorID id) { m_targetID = id; }

		DirectX::SimpleMath::Vector3 GetDirToTarget() const { return m_dirToTarget; }
		float GetDistanceToTarget() const { return m_distanceToTarget; }
		bool IsLockedOn() const { return m_isLockedOn; }
		void SetLockedOn(bool locked) { m_isLockedOn = locked; }

	};
}
