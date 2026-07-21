#pragma once
#include "ICameraMode.h"
#include <SimpleMath.h>
#include <Entities/Actor.h>

namespace HEIN
{
	class ActorManager;

	class TransformComponent;

	class SpringCameraMode : public ICameraMode
	{

	private:

		static constexpr float DEFAULT_MOUSE_SENSITIVITY = 0.005f;
		static constexpr float DEFAULT_FOLLOW_DISTANCE = 40.0f;
		static constexpr float DEFAULT_HEIGHT_OFFSET = 10.0f;
		static constexpr float DEFAULT_FRE = 8.0f;
		static constexpr float YAW = 0.0f;
		static constexpr float PITCH = -0.35f;
		static constexpr float ROLL = 0.0f;
		static constexpr float MAX_PITCH_DOWN = 5.0f;
		static constexpr float MAX_PITCH_UP = 45.0f;
		static constexpr float SPRING_CAM_FOV = 50.0f;
		static constexpr float SHOULDER_OFFSET = 0.5f;
		static constexpr float DAMPING = 2.0f;
	private:

		HEIN::ActorManager* m_manager;
		HEIN::ActorID m_targetID;

		const DirectX::SimpleMath::Vector3* m_desiredTarget;
		DirectX::SimpleMath::Vector3 m_currentPosition;
		DirectX::SimpleMath::Vector3 m_currentLookAt;
		DirectX::SimpleMath::Vector3 m_positionVelocity;
		DirectX::SimpleMath::Vector3 m_lookAtVelocity;
		float m_stiffness;
		float m_damping;
		float m_yaw;
		float m_pitch;
		float m_roll;
		float m_mouseSensitivity;
		float m_followDistance;
		float m_heightOffset;
		bool  m_isInitialized;
	public:

		SpringCameraMode(
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			const DirectX::SimpleMath::Vector3* desiredTarget,
			float followDistance = DEFAULT_FOLLOW_DISTANCE,
			float heightOffset = DEFAULT_HEIGHT_OFFSET,
			float freq = DEFAULT_FRE
		);

		void OnEnter(CameraData& data) override;
			
		void ProcessInput(const CameraInputState& input) override;

		void Update(
			CameraData& outData,
			float deltaTime, 
			ICameraController& controller
		) override;
		
		void SetFrequency(float freq);
	
		bool RequiresRelativeMouse() const override { return true; }
		bool LocksPlayerRotation() const override { return false; }

		CameraType GetType() const override { return CameraType::Spring; }
	private:

		void UpdateSpring(
			const DirectX::SimpleMath::Vector3& target, 
			DirectX::SimpleMath::Vector3& current, 
			DirectX::SimpleMath::Vector3& velocity, 
			float elapsedTime) const;

	};
}
