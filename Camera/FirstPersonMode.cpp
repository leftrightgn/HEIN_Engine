#include "pch.h"
#include "FirstPersonMode.h"
#include <Entities/Actor.h>
#include <Entities/ActorManager.h>

HEIN::FirstPersonMode::FirstPersonMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID, 
	const DirectX::SimpleMath::Vector3* headPos, 
	SkinnedModelComponent* fpsModel, 
	SkinnedModelComponent* tpsModel
)
	: m_manager(manager)
	, m_targetID(targetID)
	, m_playerHeadPosition(headPos)
	, m_fpsModel(fpsModel)
	, m_tpsModel(tpsModel)
	, m_yaw(YAW)
	, m_pitch(PITCH)
	, m_roll(ROLL)
	, m_mouseSensitivity(MOUSE_SENSITIVITY)
	, m_targetHeight(TARGET_HEIGHT)
	, m_boomlenght(BOOM_LENGTH)
{
}

void HEIN::FirstPersonMode::OnEnter(CameraData& data)
{
	if (m_fpsModel != nullptr) m_fpsModel->SetVisible(true);
	if (m_tpsModel != nullptr) m_tpsModel->SetVisible(false);

	DirectX::SimpleMath::Vector3 backward = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3::Backward,
		data.rotation
	);

	m_pitch = std::asin(-backward.y);
	m_yaw = std::atan2(backward.x, backward.z);

}

void HEIN::FirstPersonMode::ProcessInput(const CameraInputState& input)
{
	m_yaw += -input.mouseX * m_mouseSensitivity;
	m_pitch += -input.mouseY * m_mouseSensitivity;


	constexpr float pitchLimit = (DirectX::XMConvertToRadians(PITCH_LIMIT));
	m_pitch = std::clamp(m_pitch, -pitchLimit, pitchLimit);

	/*constexpr float yawLimit = (DirectX::XMConvertToRadians(60.0f));
	m_yaw = std::clamp(m_yaw, -yawLimit, yawLimit);*/
}

void HEIN::FirstPersonMode::Update(CameraData& outData, float /*deltaTime*/, ICameraController& /*controller*/)
{
	HEIN::Actor* targetActor = m_manager->GetActor(m_targetID);

	if (targetActor != nullptr)
	{
		if (m_fpsModel) m_fpsModel->SetVisible(true);
		if (m_tpsModel) m_tpsModel->SetVisible(false);
	}

	outData.position = *m_playerHeadPosition;
	outData.position.y += m_targetHeight;

	DirectX::SimpleMath::Quaternion rotation = 
		DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(m_yaw, m_pitch, m_roll);

	DirectX::SimpleMath::Vector3 rotForward = 
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Forward, rotation);

	DirectX::SimpleMath::Quaternion yawOnly = 
		DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(DirectX::SimpleMath::Vector3::Up, m_yaw);

	DirectX::SimpleMath::Vector3 flatForward = 
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Forward, yawOnly);

	DirectX::SimpleMath::Vector3 right = 
		DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Right, yawOnly);

	flatForward.Normalize();

	outData.position += flatForward * m_boomlenght;
	outData.position += right * CENTER_OFFSET;
	outData.rotation = rotation;
	DirectX::SimpleMath::Vector3 target = outData.position + rotForward;
	DirectX::SimpleMath::Vector3 up = DirectX::SimpleMath::Vector3::Up;

	outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
		outData.position,
		target,
		up
	);

	outData.fov = DirectX::XMConvertToRadians(FPS_CAM_FOV);
}