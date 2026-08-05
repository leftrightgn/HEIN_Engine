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
		static constexpr float DEFAULT_MOUSE_SENSITIVITY = 0.003f;
		static constexpr float DEFAULT_FOLLOW_DISTANCE = 40.0f;
		static constexpr float DEFAULT_HEIGHT_OFFSET = 0.0f;
		static constexpr float DEFAULT_SHOULDER_OFFSET = 0.0f;
		static constexpr float MIN_DISTANCE = 2.0f;
		static constexpr float CAMERA_RADIUS = 1.0f;
		static constexpr float PIVOT_SMOOTH_SPEED = 18.0f;
		static constexpr float ROTATION_SMOOTH_SPEED = 25.0f;
		static constexpr float ZOOM_IN_SPEED = 30.0f;
		static constexpr float ZOOM_OUT_SPEED = 6.0f;
		static constexpr float MIN_PITCH = -DirectX::XMConvertToRadians(50.0f); 
		static constexpr float MAX_PITCH = DirectX::XMConvertToRadians(50.0f);  
		static constexpr float SPRING_CAM_FOV = 50.0f;

	private:
		HEIN::ActorManager* m_manager;
		HEIN::ActorID m_targetID;

		const DirectX::SimpleMath::Vector3* m_desiredTarget;
		DirectX::SimpleMath::Vector3 m_currentPivot;
		float m_currentDistance;
		float m_targetYaw;
		float m_currentYaw;
		float m_targetPitch;
		float m_currentPitch;
		float m_roll;
		float m_mouseSensitivity;
		float m_followDistance;
		float m_heightOffset;
		float m_shoulderOffset;
		bool  m_isInitialized;

	public:
		SpringCameraMode(
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			const DirectX::SimpleMath::Vector3* desiredTarget,
			float followDistance = DEFAULT_FOLLOW_DISTANCE,
			float heightOffset = DEFAULT_HEIGHT_OFFSET,
			float freq = 8.0f
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
	};
}
