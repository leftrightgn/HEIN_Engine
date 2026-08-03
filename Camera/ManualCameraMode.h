#pragma once
#include "ICameraMode.h"
#include <SimpleMath.h>

namespace HEIN
{
	class ManualCameraMode : public ICameraMode
	{
	public:
		ManualCameraMode();
		~ManualCameraMode() override = default;

		void Update(CameraData& outData, float /*deltaTime*/, ICameraController& /*controller*/) override;
	
		void ProcessInput(const CameraInputState& /*input*/) override {}

		CameraType GetType() const override { return CameraType::Manual; }
	};
}
