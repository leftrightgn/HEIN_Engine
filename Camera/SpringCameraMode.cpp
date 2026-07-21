#include "pch.h"
#include "SpringCameraMode.h"
#include "Components/TransformComponent.h"
#include <Entities/ActorManager.h>


HEIN::SpringCameraMode::SpringCameraMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID,
	const DirectX::SimpleMath::Vector3* desiredTarget, 
	float followDistance, 
	float heightOffset, 
	float freq
)
	: m_manager(manager)
	, m_targetID(targetID)
	, m_desiredTarget(desiredTarget)
	, m_currentPosition(DirectX::SimpleMath::Vector3::Zero)
	, m_currentLookAt(DirectX::SimpleMath::Vector3::Zero)
	, m_positionVelocity(DirectX::SimpleMath::Vector3::Zero)
	, m_lookAtVelocity(DirectX::SimpleMath::Vector3::Zero)
	, m_yaw(YAW)
	, m_pitch(PITCH)
	, m_roll(ROLL)
	, m_mouseSensitivity(DEFAULT_MOUSE_SENSITIVITY)
	, m_followDistance(followDistance)
	, m_heightOffset(heightOffset)
	, m_isInitialized(false)
{
	SetFrequency(freq);
}

void HEIN::SpringCameraMode::OnEnter(CameraData& data)
{
	DirectX::SimpleMath::Vector3 backward = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3::Backward,
		data.rotation
	);
	DirectX::SimpleMath::Vector3 targetLookAt = *m_desiredTarget;

	m_yaw = std::atan2(backward.x, backward.z);
	m_pitch = std::asin(-backward.y);

}

void HEIN::SpringCameraMode::ProcessInput(const CameraInputState& input)
{
	m_yaw += -input.mouseX * m_mouseSensitivity;
	m_pitch += -input.mouseY * m_mouseSensitivity;

	constexpr float maxPitchDown = (DirectX::XMConvertToRadians(MAX_PITCH_DOWN));  // look down
	constexpr float maxPitchUp = -(DirectX::XMConvertToRadians(MAX_PITCH_UP));  // look up

	// clamp the pitch 
	m_pitch = std::clamp(m_pitch, maxPitchUp, maxPitchDown);
}

void HEIN::SpringCameraMode::Update(CameraData& outData, float deltaTime, ICameraController& /*controller*/)
{

	if (!m_desiredTarget) return;

	DirectX::SimpleMath::Vector3 targetLookAt = *m_desiredTarget;

	DirectX::SimpleMath::Quaternion rotation = 
		DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(m_yaw, m_pitch, m_roll);

	DirectX::SimpleMath::Vector3 rotRight = 
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Right, rotation);

	DirectX::SimpleMath::Vector3 shoulderOffset = rotRight * SHOULDER_OFFSET;

	DirectX::SimpleMath::Vector3 camBackWard = 
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Backward, rotation);
	

	DirectX::SimpleMath::Vector3 targetEye = 
		targetLookAt + (camBackWard * m_followDistance) + 
		DirectX::SimpleMath::Vector3::Up * m_heightOffset + 
		shoulderOffset;

	if (!m_isInitialized)
	{
		m_currentPosition = targetEye;
		m_currentLookAt = targetLookAt;
		m_isInitialized = true;
	}

	UpdateSpring(targetEye, m_currentPosition, m_positionVelocity, deltaTime);
	UpdateSpring(targetLookAt, m_currentLookAt, m_lookAtVelocity, deltaTime);

	outData.rotation = rotation;
	outData.position = m_currentPosition;

	outData.viewMatrix = 
		DirectX::SimpleMath::Matrix::CreateLookAt(
			outData.position, m_currentLookAt, 
			DirectX::SimpleMath::Vector3::Up
		);

	outData.fov = DirectX::XMConvertToRadians(SPRING_CAM_FOV);

}

void HEIN::SpringCameraMode::SetFrequency(float freq)
{
	m_stiffness = freq * freq;
	m_damping = DAMPING * freq;
}

void HEIN::SpringCameraMode::UpdateSpring(
	const DirectX::SimpleMath::Vector3& target,
	DirectX::SimpleMath::Vector3& current,
	DirectX::SimpleMath::Vector3& velocity,
	float elapsedTime
) const
	{
		DirectX::SimpleMath::Vector3 delta = target - current;
		DirectX::SimpleMath::Vector3 accel = (m_stiffness * delta) - (m_damping * velocity);
		velocity += accel * elapsedTime;
		current += velocity * elapsedTime;
	}
