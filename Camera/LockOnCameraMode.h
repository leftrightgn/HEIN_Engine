#pragma once
#include <Camera/ICameraMode.h>
#include <Entities/Actor.h>

namespace HEIN
{
	class ActorManager;

	class TransformComponent;

	class LockOnCameraMode : public ICameraMode
	{
	private:

		static constexpr float DAMPING = 2.0f;
		static constexpr float DEFAULT_FRE = 8.0f;
		static constexpr float DESIRED_DISTANCE = 40.0f;
		static constexpr float DEFAULT_HEIGHT_OFFSET = 20.0f;
		static constexpr float MAX_ORBITABLE_SPEED = 20.0f;

	private:

		HEIN::ActorManager* m_manager;
		HEIN::ActorID m_playerID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_targetID = HEIN::INVALID_ACTOR_ID;

		DirectX::SimpleMath::Vector3 m_currentPosition;
		DirectX::SimpleMath::Vector3 m_positionVelocity;
		float m_currentYaw;
		float m_stiffness;
		float m_damping;
		float m_heightOffset;
		bool  m_isInitialized;
		int   m_invalidFrames;

	public:

		LockOnCameraMode(
			HEIN::ActorManager* manager,
			HEIN::ActorID playerID,
			HEIN::ActorID targetID,
			float freq = DEFAULT_FRE
		);

		void OnEnter(CameraData& data);

		void ProcessInput(const CameraInputState& input) override {}
		void Update(
			CameraData& outData,
			float deltaTime, 
			ICameraController& controller
		) override;
	
		void SetFrequency(float freq);

		bool RequiresRelativeMouse() const override { return true; }
		bool LocksPlayerRotation() const override { return false; }

		CameraType GetType() const override { return CameraType::LockOn; }
		
		HEIN::ActorID GetTargetID() const { return m_targetID; }
		void SetTargetID(HEIN::ActorID target) { m_targetID = target; }
	private:

		void UpdateSpring(
			const DirectX::SimpleMath::Vector3& target,
			DirectX::SimpleMath::Vector3& current,
			DirectX::SimpleMath::Vector3& velocity,
			float elapsedTime) const;
	};
}
