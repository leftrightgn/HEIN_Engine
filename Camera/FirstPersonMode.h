#pragma once
#include "ICameraMode.h"
#include <Components/SkinnedModelComponent.h>
#include <Entities/Actor.h>

namespace HEIN
{

	class ActorManager;

	class FirstPersonMode : public ICameraMode
	{
	private:

		static constexpr float YAW = 0.0f;
		static constexpr float PITCH = 0.0f;
		static constexpr float ROLL = 0.0f;
		static constexpr float PITCH_LIMIT = 80.0f;
		static constexpr float TARGET_HEIGHT = 0.0f;
		static constexpr float BOOM_LENGTH = 1.0f;
		static constexpr float MOUSE_SENSITIVITY = 0.005f;
		static constexpr float CENTER_OFFSET = 0.0f;
		static constexpr float FPS_CAM_FOV = 60.0f;

	private:

		HEIN::ActorManager* m_manager;
		HEIN::ActorID m_targetID;

		const DirectX::SimpleMath::Vector3* m_playerHeadPosition;

		SkinnedModelComponent* m_fpsModel;
		SkinnedModelComponent* m_tpsModel;

		float m_yaw;
		float m_pitch;
		float m_roll;
		float m_mouseSensitivity;
		float m_targetHeight;
		float m_boomlenght;

		DirectX::SimpleMath::Vector3 m_lockedPosition;

	public:

		FirstPersonMode(
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			const DirectX::SimpleMath::Vector3* headPos,
			SkinnedModelComponent* fpsModel,
			SkinnedModelComponent* tpsModel
		);
	
		void OnEnter(CameraData& data) override;

		void ProcessInput(const CameraInputState& input) override;
		
		void Update(CameraData& outData, float deltaTime, ICameraController& controller) override;

		bool RequiresRelativeMouse() const override { return true; }
		bool LocksPlayerRotation() const override { return true; }

		CameraType GetType() const override { return CameraType::FirstPerson; }
	};
}
