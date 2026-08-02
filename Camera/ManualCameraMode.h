#pragma once
#include "ICameraMode.h"
#include <SimpleMath.h>

namespace HEIN
{
	class ManualCameraMode : public ICameraMode
	{
	public:
		ManualCameraMode() = default;
		~ManualCameraMode() override = default;

		void Update(CameraData& outData, float /*deltaTime*/, ICameraController& /*controller*/) override
		{
			DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(outData.rotation);
			camWorld.Translation(outData.position);
			outData.viewMatrix = camWorld.Invert();
		}

		void ProcessInput(const CameraInputState& /*input*/) override
		{
		}

		CameraType GetType() const override { return CameraType::Manual; }
	};
}
