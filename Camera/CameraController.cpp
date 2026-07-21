#include "pch.h"
#include "CameraController.h"


HEIN::CameraController::CameraController(Actor* owner)
	: IComponent(owner)
{
}

void HEIN::CameraController::RegisterCamera(CameraType key, CameraFactory factory)
{
	m_factories[key] = factory;
}

void HEIN::CameraController::SetFirstCamera(CameraType key)
{
	std::unique_ptr<ICameraMode> firstCam = m_factories[key]();
	m_cameraStack.push_back(std::move(firstCam));
	m_cameraStack.back()->OnEnter(m_data);
}

void HEIN::CameraController::Update(float deltaTime)
{
	if (!m_cameraStack.empty())
	{
		m_cameraStack.back()->Update(m_data, deltaTime, *this);
	}

	if (m_isBlending)
	{
		m_blendTimer += deltaTime;
		float t = m_blendTimer / m_blendDuration;

		if (t >= 1.0f)
		{
			m_isBlending = false;
		}
		else
		{
			m_data.position = DirectX::SimpleMath::Vector3::Lerp(m_previousCameraData.position, m_data.position, t);

			m_data.fov = std::lerp(m_previousCameraData.fov, m_data.fov, t);

			DirectX::SimpleMath::Quaternion blendRot = DirectX::SimpleMath::Quaternion::Slerp(m_previousCameraData.rotation, m_data.rotation, t);

			DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(blendRot);
			camWorld.Translation(m_data.position);
			m_data.viewMatrix = camWorld.Invert();
		}
	}
	ApplyRequest();
}

void HEIN::CameraController::ProcessInput(const CameraInputState& input)
{
	if (!m_cameraStack.empty())
	{
		m_cameraStack.back()->ProcessInput(input);
	}
}

bool HEIN::CameraController::LocksPlayerRotation() const
{
	if (m_cameraStack.empty()) return false;
	return m_cameraStack.back()->LocksPlayerRotation();
}

void HEIN::CameraController::RequestSwitch(CameraType type)
{
	m_nextCommand = Command::Switch;
	m_nextCameraKey = type;
}

void HEIN::CameraController::RequestPush(CameraType type)
{
	m_nextCommand = Command::Push;
	m_nextCameraKey = type;
}

void HEIN::CameraController::RequestPop(CameraType /*type*/)
{
	m_nextCommand = Command::Pop;
}

void HEIN::CameraController::ApplyRequest()
{
	if (m_nextCommand == Command::None) return;

	m_previousCameraData = m_data;
	m_isBlending = true;
	m_blendTimer = 0.0f;

	if (m_nextCommand == Command::Switch)
	{
		if (!m_cameraStack.empty()) m_cameraStack.back()->OnExit(m_data);

		std::unique_ptr<ICameraMode> newCam = m_factories[*m_nextCameraKey]();
		m_cameraStack.back() = std::move(newCam);

		m_cameraStack.back()->OnEnter(m_data);
		UpdateMouseMode();
	}
	else if (m_nextCommand == Command::Push)
	{
		if (!m_cameraStack.empty()) m_cameraStack.back()->OnSuspend(m_data);


		std::unique_ptr<ICameraMode> newCam = m_factories[*m_nextCameraKey]();
		m_cameraStack.push_back(std::move(newCam));

		m_cameraStack.back()->OnEnter(m_data);
		UpdateMouseMode();
	}
	else if (m_nextCommand == Command::Pop)
	{
		m_cameraStack.back()->OnExit(m_data);
		m_cameraStack.pop_back();
		if (!m_cameraStack.empty()) m_cameraStack.back()->OnResume(m_data);
		UpdateMouseMode();
	}

	m_nextCommand = Command::None;
	m_nextCameraKey = std::nullopt;
}

void HEIN::CameraController::UpdateMouseMode()
{
	if (m_cameraStack.back()->RequiresRelativeMouse())
		DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_RELATIVE);
	else
		DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_ABSOLUTE);
}