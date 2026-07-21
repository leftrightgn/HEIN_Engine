#pragma once
#include <SimpleMath.h>

namespace HEIN
{
	class CameraController;

	// Data Container
	struct CameraData
	{
		DirectX::SimpleMath::Vector3 position = {};
		DirectX::SimpleMath::Quaternion rotation = DirectX::SimpleMath::Quaternion::Identity;
		DirectX::SimpleMath::Matrix viewMatrix = {};
		DirectX::SimpleMath::Matrix projMatrix = {};
		float fov = DirectX::XMConvertToRadians(50.0f);
	};

	// Input Data Container
	struct CameraInputState
	{
		float mouseX{};
		float mouseY{};
		float scrollWheelDelta{};
		bool isLeftMouseDown = false;
		DirectX::SimpleMath::Vector3 movementIntent;
	};

	enum class CameraType
	{
		FirstPerson,
		ThirdPerson,
		Debug,
		Cinematic,
		Spring,
		LockOn
	};

	// Controller interface
	class ICameraController
	{
	public:

		virtual ~ICameraController() = default;
		virtual void RequestSwitch(CameraType key) = 0;
		virtual void RequestPush(CameraType key) = 0;
		virtual void RequestPop(CameraType key) = 0;

	};
	// Strategy interface
	class ICameraMode
	{
	public:

		virtual ~ICameraMode() = default;

		virtual void Update(CameraData& outData, float deltaTime, ICameraController& controller ) = 0;
		virtual void ProcessInput(const CameraInputState& input) = 0;
        virtual bool RequiresRelativeMouse() const { return false; }
		virtual bool LocksPlayerRotation() const { return false; }

		virtual void OnEnter(CameraData& /*data*/) {};
		virtual void OnExit(CameraData& /*data*/) {};
		virtual void OnSuspend(CameraData& /*data*/) {};
		virtual void OnResume(CameraData& /*data*/) {};

		virtual CameraType GetType() const = 0;
	};
}
