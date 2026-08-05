#include "pch.h"
#include "SpringCameraMode.h"
#include "Components/TransformComponent.h"
#include <Entities/ActorManager.h>
#include "CameraCollisionHelper.h"
#include <algorithm>
#include <cmath>

HEIN::SpringCameraMode::SpringCameraMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID,
	const DirectX::SimpleMath::Vector3* desiredTarget, 
	float followDistance, 
	float heightOffset, 
	float /*freq*/
)
	: m_manager(manager)
	, m_targetID(targetID)
	, m_desiredTarget(desiredTarget)
	, m_currentPivot(DirectX::SimpleMath::Vector3::Zero)
	, m_currentDistance(followDistance)
	, m_targetYaw(0.0f)
	, m_currentYaw(0.0f)
	, m_targetPitch(-0.35f)
	, m_currentPitch(-0.35f)
	, m_roll(0.0f)
	, m_mouseSensitivity(DEFAULT_MOUSE_SENSITIVITY)
	, m_followDistance(followDistance)
	, m_heightOffset(heightOffset)
	, m_shoulderOffset(DEFAULT_SHOULDER_OFFSET)
	, m_isInitialized(false)
{
}

void HEIN::SpringCameraMode::OnEnter(CameraData& data)
{
	DirectX::SimpleMath::Vector3 backward = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3::Backward,
		data.rotation
	);

	m_targetYaw = m_currentYaw = std::atan2(backward.x, backward.z);
	m_targetPitch = m_currentPitch = std::asin(std::clamp(-backward.y, -0.99f, 0.99f));
	m_targetPitch = std::clamp(m_targetPitch, MIN_PITCH, MAX_PITCH);
	m_currentPitch = m_targetPitch;

	if (m_desiredTarget != nullptr)
	{
		m_currentPivot = *m_desiredTarget;
	}
	m_currentDistance = m_followDistance;
	m_isInitialized = true;
}

void HEIN::SpringCameraMode::ProcessInput(const CameraInputState& input)
{
	m_targetYaw += -input.mouseX * m_mouseSensitivity;
	m_targetPitch += -input.mouseY * m_mouseSensitivity;

	// Clamp pitch to avoid gimbal lock and extreme flip angles
	m_targetPitch = std::clamp(m_targetPitch, MIN_PITCH, MAX_PITCH);
}

void HEIN::SpringCameraMode::Update(CameraData& outData, float deltaTime, ICameraController& /*controller*/)
{
	if (!m_desiredTarget) return;

	// Prevent physics/lag glitches if frame rate spikes or drops
	deltaTime = std::clamp(deltaTime, 0.0001f, 0.1f);

	DirectX::SimpleMath::Vector3 rawTarget = *m_desiredTarget;

	if (!m_isInitialized)
	{
		m_currentPivot = rawTarget;
		m_currentYaw = m_targetYaw;
		m_currentPitch = m_targetPitch;
		m_currentDistance = m_followDistance;
		m_isInitialized = true;
	}

	// Smooth Pivot Point (Target Lag / Anti-Jitter for Head Bobbing)
	float pivotAlpha = 1.0f - std::exp(-PIVOT_SMOOTH_SPEED * deltaTime);
	m_currentPivot = DirectX::SimpleMath::Vector3::Lerp(m_currentPivot, rawTarget, pivotAlpha);

	// Smooth Camera Orbit Rotation (Yaw & Pitch)
	float rotAlpha = 1.0f - std::exp(-ROTATION_SMOOTH_SPEED * deltaTime);

	// Shortest-angle interpolation for Yaw
	float yawDiff = m_targetYaw - m_currentYaw;
	while (yawDiff < -DirectX::XM_PI) yawDiff += DirectX::XM_2PI;
	while (yawDiff > DirectX::XM_PI) yawDiff -= DirectX::XM_2PI;
	m_currentYaw += yawDiff * rotAlpha;

	m_currentPitch = std::lerp(m_currentPitch, m_targetPitch, rotAlpha);

	DirectX::SimpleMath::Quaternion rotation =
		DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(m_currentYaw, m_currentPitch, m_roll);

	// Direction Vectors & Pivot Focus
	DirectX::SimpleMath::Vector3 camBackward =
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Backward, rotation);

	DirectX::SimpleMath::Vector3 rotRight =
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Right, rotation);

	DirectX::SimpleMath::Vector3 shoulder = rotRight * m_shoulderOffset;
	DirectX::SimpleMath::Vector3 upOffset = DirectX::SimpleMath::Vector3::Up * m_heightOffset;
	DirectX::SimpleMath::Vector3 pivotWithOffset = m_currentPivot + upOffset + shoulder;

	// Distance Occlusion Query 
	float occludedDist = CameraCollisionHelper::ResolveOcclusionDistance(
		m_manager,
		m_targetID,
		pivotWithOffset,
		camBackward,
		m_followDistance,
		CAMERA_RADIUS,
		MIN_DISTANCE
	);

	// Asymmetric Distance Smoothing ("Fast In, Smooth Out")
	if (occludedDist < m_currentDistance)
	{
		// Zooming in (wall hit): Fast response so near-plane never clips into geometry
		float inAlpha = 1.0f - std::exp(-ZOOM_IN_SPEED * deltaTime);
		m_currentDistance = std::lerp(m_currentDistance, occludedDist, inAlpha);
		if (m_currentDistance > occludedDist)
		{
			m_currentDistance = occludedDist;
		}
	}
	else
	{
		// Zooming out (open space): Smooth, graceful return to ideal distance
		float outAlpha = 1.0f - std::exp(-ZOOM_OUT_SPEED * deltaTime);
		m_currentDistance = std::lerp(m_currentDistance, occludedDist, outAlpha);
	}

	// Final Camera Placement (Perfect spherical orbit around focus pivot)
	DirectX::SimpleMath::Vector3 finalCamPos = pivotWithOffset + (camBackward * m_currentDistance);

	outData.position = finalCamPos;
	outData.rotation = rotation;

	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(rotation);
	camWorld.Translation(finalCamPos);
	outData.viewMatrix = camWorld.Invert();

	outData.fov = DirectX::XMConvertToRadians(SPRING_CAM_FOV);
}

void HEIN::SpringCameraMode::SetFrequency(float /*freq*/)
{
}
