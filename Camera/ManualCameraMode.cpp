#include "pch.h"
#include "ManualCameraMode.h"

HEIN::ManualCameraMode::ManualCameraMode()
{
}

void HEIN::ManualCameraMode::Update(CameraData& outData, float, ICameraController&)
{
	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(outData.rotation);
	camWorld.Translation(outData.position);
	outData.viewMatrix = camWorld.Invert();
}
