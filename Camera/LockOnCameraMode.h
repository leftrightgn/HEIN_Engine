#pragma once
#include "Camera/ICameraMode.h"
#include "Entities/Actor.h"

namespace HEIN
{
	class ActorManager;
	class TransformComponent;

	class LockOnCameraMode : public ICameraMode
	{
	private:

		static constexpr float DAMPING = 2.0f;
		static constexpr float DEFAULT_FRE = 8.0f;
		static constexpr float MIN_FOLLOW_DISTANCE = 30.0f;
		static constexpr float MAX_FOLLOW_DISTANCE = 60.0f;
		static constexpr float BASE_FOLLOW_DISTANCE = 32.0f;
		static constexpr float MAX_LOCK_RANGE = 600.0f;
		static constexpr float FLICK_THRESHOLD = 18.0f;

	private:

		HEIN::ActorManager* m_manager;
		HEIN::ActorID m_playerID = HEIN::INVALID_ACTOR_ID;
		HEIN::ActorID m_targetID = HEIN::INVALID_ACTOR_ID;

		DirectX::SimpleMath::Vector3 m_currentPosition;
		DirectX::SimpleMath::Vector3 m_positionVelocity;
		DirectX::SimpleMath::Vector3 m_currentLookTarget;
		DirectX::SimpleMath::Vector3 m_currentOrbitDir;
		float m_currentFollowDistance;
		float m_currentFov;
		float m_manualPitchOffset = 0.0f;
		float m_manualYawOffset = 0.0f;
		float m_switchCooldown;
		float m_stiffness;
		float m_damping;
		bool  m_isInitialized;
		bool  m_isLookTargetInitialized;
		int   m_invalidFrames;
		bool  m_autoRetargetOnDeath;

	public:

		LockOnCameraMode(
			HEIN::ActorManager* manager,
			HEIN::ActorID playerID,
			HEIN::ActorID targetID,
			float freq = DEFAULT_FRE
		);

		void OnEnter(CameraData& data) override;
		void OnExit(CameraData& data) override;

		void ProcessInput(const CameraInputState& input) override;
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
		void SetTargetID(HEIN::ActorID target);

		bool IsAutoRetargetOnDeath() const { return m_autoRetargetOnDeath; }
		void SetAutoRetargetOnDeath(bool enable) { m_autoRetargetOnDeath = enable; }

	private:

		HEIN::ActorID FindSwitchTarget(float flickDirection) const;
		HEIN::ActorID FindNextAutoTarget() const;

		void UpdateSpring(
			const DirectX::SimpleMath::Vector3& target,
			DirectX::SimpleMath::Vector3& current,
			DirectX::SimpleMath::Vector3& velocity,
			float elapsedTime) const;
	};
}
