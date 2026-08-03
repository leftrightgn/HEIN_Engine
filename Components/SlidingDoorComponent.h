#pragma once
#include "IComponent.h"
#include <SimpleMath.h>
#include <string>

namespace HEIN
{
	class TransformComponent;
	class ColliderComponent;
	class ActorManager;

	enum class DoorState
	{
		Closed = 0,
		Opening,
		Open,
		Closing
	};

	class SlidingDoorComponent : public IComponent
	{
	private:
		ActorManager* m_actorManager = nullptr;
		TransformComponent* m_targetTransform = nullptr;

		// Configuration
		uint32_t m_targetActorID = 0;          // 0 = self (m_owner)
		std::wstring m_targetActorTag = L"";   // optional target actor tag

		DirectX::SimpleMath::Vector3 m_slideOffset = DirectX::SimpleMath::Vector3(0.0f, -6.0f, 0.0f); // Move down into floor
		float m_moveSpeed = 3.0f;              // Units per second
		float m_triggerDistance = 5.0f;        // Distance to player for auto-open
		bool m_useProximityTrigger = true;     // Open when player approaches
		bool m_useCollisionTrigger = true;     // Open when player touches collider
		bool m_autoClose = true;               // Automatically close when player leaves
		float m_closeDelay = 2.0f;             // Delay before closing in seconds

		// Runtime state
		DoorState m_state = DoorState::Closed;
		float m_currentProgress = 0.0f;        // 0.0 = Closed, 1.0 = Fully Lowered/Open
		DirectX::SimpleMath::Vector3 m_closedPosition = DirectX::SimpleMath::Vector3::Zero;
		bool m_initialPositionSaved = false;
		float m_closeTimer = 0.0f;
		bool m_playerDetected = false;

	public:
		SlidingDoorComponent(Actor* owner, ActorManager* manager = nullptr);
		~SlidingDoorComponent() override = default;

		std::string GetComponentName() const override { return "SlidingDoorComponent"; }

		void Start() override;
		void Update(float deltaTime) override;

		void Open();
		void Close();
		void Toggle();

		DoorState GetState() const { return m_state; }
		float GetProgress() const { return m_currentProgress; }

		void SetSlideOffset(const DirectX::SimpleMath::Vector3& offset) { m_slideOffset = offset; }
		DirectX::SimpleMath::Vector3 GetSlideOffset() const { return m_slideOffset; }

		void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
		float GetMoveSpeed() const { return m_moveSpeed; }

		void SetTriggerDistance(float dist) { m_triggerDistance = dist; }
		float GetTriggerDistance() const { return m_triggerDistance; }

		void SetTargetActorID(uint32_t id) { m_targetActorID = id; }
		uint32_t GetTargetActorID() const { return m_targetActorID; }

		void SetActorManager(ActorManager* manager) { m_actorManager = manager; }

		void OnInspectorGUI(GameContext& gameContext) override;
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;

	private:
		TransformComponent* ResolveTargetTransform();
		bool CheckPlayerInteraction(TransformComponent* doorTransform);
	};
}
