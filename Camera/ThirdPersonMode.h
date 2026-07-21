#pragma once
#include "ICameraMode.h"
#include <SimpleMath.h>
#include <Components/SkinnedModelComponent.h>
#include <Entities/Actor.h>

namespace HEIN
{
	class ActorManager;

	class ThirdPersonMode : public ICameraMode
	{
	private:

		static constexpr float YAW = 0.0f;
		static constexpr float PITCH = -0.35f;
		static constexpr float ROLL = 0.0f;
		static constexpr float BOOM_LENGTH = 30.0f;
		static constexpr float MOUSE_SENSITIVITY = 0.005f;
		static constexpr float TARGET_HEIGHT = 0.0f;
		static constexpr float MAX_PITCH_DOWN = 5.0f;
		static constexpr float MAX_PITCH_UP = 45.0f; 
		static constexpr float TPS_CAM_FOV = 50.0f;
		static constexpr float SHOULDER_OFFSET = 0.5f;

	private:

		HEIN::ActorManager* m_manager;
		HEIN::ActorID m_targetID;

		const DirectX::SimpleMath::Vector3* m_playerTarget;

		SkinnedModelComponent* m_fpsModel;
		SkinnedModelComponent* m_tpsModel;

		float m_yaw;
		float m_pitch;
		float m_roll;
		float m_boomLength;
		float m_mouseSensitivity;
		float m_targetHeight;

	public:

		ThirdPersonMode(
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			DirectX::SimpleMath::Vector3* playerTarget,
			SkinnedModelComponent* fpsModel,
			SkinnedModelComponent* tpsModel
		);

		void OnEnter(CameraData& data) override;

		void ProcessInput(const CameraInputState& input) override;
	
		void Update(CameraData& outData, float deltaTime, ICameraController& controller) override;

		bool RequiresRelativeMouse() const override { return true; }
		bool LocksPlayerRotation() const override { return true; }

		CameraType GetType() const override { return CameraType::ThirdPerson; }

	};
}