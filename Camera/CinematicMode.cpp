#include "pch.h"
#include "CinematicMode.h"
#include <Entities/ActorManager.h>

HEIN::CinematicMode::CinematicMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID,
    DirectX::SimpleMath::Vector3 centerPoint,
    float radius, float currentAngle,
    float orbitSpeed,
    float cameraHeight
)
	: m_manager(manager)
	, m_targetID(targetID)
	, m_centerofStage(centerPoint)
	, m_radius(radius)
	, m_currentAngle(currentAngle)
	, m_orbitSpeed(orbitSpeed)
	, m_cameraHeight(cameraHeight)
{
}

void HEIN::CinematicMode::Update(CameraData& outData, float deltaTime, ICameraController& /*controller*/)
{


	// Advance the angle over time
	m_currentAngle += m_orbitSpeed * deltaTime;

	// Calculate the camera position by trigonometry for a circle

	float x = m_centerofStage.x + (cosf(m_currentAngle) * m_radius);
	float z = m_centerofStage.z + (sinf(m_currentAngle) * m_radius);

	outData.position = DirectX::SimpleMath::Vector3(x, m_centerofStage.y + m_cameraHeight, z);

	// Always look perfectly at the center of the stage 
	outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
		outData.position,
		m_centerofStage,
		DirectX::SimpleMath::Vector3::Up
	);

	DirectX::SimpleMath::Matrix inverseView = outData.viewMatrix.Invert();
	outData.rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(inverseView);
	// FOV
	outData.fov = DirectX::XMConvertToRadians(50.0f);
}
