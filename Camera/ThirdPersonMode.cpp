#include "pch.h"
#include "ThirdPersonMode.h"
#include <Entities/ActorManager.h>
#include "CameraCollisionHelper.h"

HEIN::ThirdPersonMode::ThirdPersonMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID,
	DirectX::SimpleMath::Vector3* playerTarget,
	SkinnedModelComponent* fpsModel,
	SkinnedModelComponent* tpsModel
)
	: m_manager(manager)
	, m_targetID(targetID)
	, m_playerTarget(playerTarget)
	, m_fpsModel(fpsModel)
	, m_tpsModel(tpsModel)
	, m_yaw(YAW)
	, m_pitch(PITCH)
	, m_roll(ROLL)
	, m_boomLength(BOOM_LENGTH)
	, m_mouseSensitivity(MOUSE_SENSITIVITY)
	, m_targetHeight(TARGET_HEIGHT)
{
}

void HEIN::ThirdPersonMode::OnEnter(CameraData& data)
{
	if (m_fpsModel != nullptr) m_fpsModel->SetVisible(false);
	if (m_tpsModel != nullptr) m_tpsModel->SetVisible(true);

	DirectX::SimpleMath::Vector3 backward = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3::Backward,
		data.rotation
	);

	m_yaw = std::atan2(backward.x, backward.z);
	m_pitch = std::asin(-backward.y);
	
}

void HEIN::ThirdPersonMode::ProcessInput(const CameraInputState& input)
{
	m_yaw += -input.mouseX * m_mouseSensitivity;
	m_pitch += -input.mouseY * m_mouseSensitivity;

	constexpr float maxPitchDown = (DirectX::XMConvertToRadians(MAX_PITCH_DOWN));  // look down
	constexpr float maxPitchUp = -(DirectX::XMConvertToRadians(MAX_PITCH_UP));  // look up

	// clamp the pitch 
	m_pitch = std::clamp(m_pitch, maxPitchUp, maxPitchDown);

}

void HEIN::ThirdPersonMode::Update(CameraData& outData, float /*deltaTime*/, ICameraController& /*controller*/)
{
	HEIN::Actor* targetActor = m_manager->GetActor(m_targetID);

	if (targetActor != nullptr)
	{
		if (m_fpsModel) m_fpsModel->SetVisible(false);
		if (m_tpsModel) m_tpsModel->SetVisible(true);
	}

	DirectX::SimpleMath::Vector3 focalPoint = *m_playerTarget;
	focalPoint.y += m_targetHeight;

	DirectX::SimpleMath::Quaternion rotation = 
		DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(m_yaw, m_pitch, m_roll);

	DirectX::SimpleMath::Vector3 rotBackward = 
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Backward, rotation);

	DirectX::SimpleMath::Vector3 rotRight = 
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Right, rotation);

	DirectX::SimpleMath::Vector3 offset = rotBackward * m_boomLength;
	DirectX::SimpleMath::Vector3 shoulderOffset = rotRight * SHOULDER_OFFSET;

	DirectX::SimpleMath::Vector3 desiredPos = focalPoint + offset + shoulderOffset;
	DirectX::SimpleMath::Vector3 occludedPos = CameraCollisionHelper::ResolveOcclusion(
		m_manager,
		m_targetID,
		focalPoint,
		desiredPos,
		0.5f,
		1.0f
	);

	outData.rotation = rotation;
	outData.position = occludedPos;
	outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
		outData.position,
		focalPoint,
		DirectX::SimpleMath::Vector3::Up
	);

	outData.fov = DirectX::XMConvertToRadians(TPS_CAM_FOV);
}
