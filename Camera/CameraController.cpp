#include "pch.h"
#include "CameraController.h"
#include "DebugCameraMode.h"
#include "ManualCameraMode.h"
#include <ImGui/imgui.h>
#include <ImGui/ImGuizmo.h>
#include <DebugingTools/DebugUIManager.h>
#include <Components/TransformComponent.h>
#include "Framework/GameContext.h"
#include <cmath>
#include <optional>

HEIN::CameraController::CameraController(Actor* owner)
	: IComponent(owner)
{
	m_data.fov = DirectX::XMConvertToRadians(50.0f);
	m_data.position = DirectX::SimpleMath::Vector3(0.0f, 15.0f, -40.0f);
	m_data.rotation = DirectX::SimpleMath::Quaternion::Identity;
	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
	camWorld.Translation(m_data.position);
	m_data.viewMatrix = camWorld.Invert();
}

void HEIN::CameraController::Start()
{
	RegisterCamera(
		HEIN::CameraType::Manual,
		[]() { return std::make_unique<HEIN::ManualCameraMode>(); }
	);
	RegisterCamera(
		HEIN::CameraType::Debug,
		[]() { return std::make_unique<HEIN::DebugCameraMode>(); }
	);

	if (m_cameraStack.empty())
	{
		SetFirstCamera(HEIN::CameraType::Manual);
	}

	// Sync from TransformComponent if owner has one
	if (m_owner != nullptr)
	{
		if (auto* transform = m_owner->GetComponent<TransformComponent>())
		{
			if (m_data.position == DirectX::SimpleMath::Vector3::Zero)
			{
				m_data.position = transform->GetPosition();
				m_data.rotation = transform->GetRotation();
				DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
				camWorld.Translation(m_data.position);
				m_data.viewMatrix = camWorld.Invert();
			}
			else
			{
				transform->SetPosition(m_data.position);
				transform->SetRotation(m_data.rotation);
			}
		}
	}
}

void HEIN::CameraController::InitializeAfterDeserialize(GameContext& /*gameContext*/)
{
	RegisterCamera(
		HEIN::CameraType::Manual,
		[]() { return std::make_unique<HEIN::ManualCameraMode>(); }
	);
	RegisterCamera(
		HEIN::CameraType::Debug,
		[]() { return std::make_unique<HEIN::DebugCameraMode>(); }
	);

	if (m_cameraStack.empty())
	{
		SetFirstCamera(HEIN::CameraType::Manual);
	}

	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
	camWorld.Translation(m_data.position);
	m_data.viewMatrix = camWorld.Invert();

	if (m_owner != nullptr)
	{
		if (auto* transform = m_owner->GetComponent<TransformComponent>())
		{
			transform->SetPosition(m_data.position);
			transform->SetRotation(m_data.rotation);
		}
	}
}

void HEIN::CameraController::RegisterCamera(CameraType key, CameraFactory factory)
{
	m_factories[key] = factory;
}

void HEIN::CameraController::SetFirstCamera(CameraType key)
{
	if (m_factories.find(key) != m_factories.end())
	{
		std::unique_ptr<ICameraMode> firstCam = m_factories[key]();
		m_cameraStack.clear();
		m_cameraStack.push_back(std::move(firstCam));
		m_cameraStack.back()->OnEnter(m_data);
	}
}

void HEIN::CameraController::Update(float deltaTime)
{
	if (!m_cameraStack.empty())
	{
		m_cameraStack.back()->Update(m_data, deltaTime, *this);
	}
	else
	{
		DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
		camWorld.Translation(m_data.position);
		m_data.viewMatrix = camWorld.Invert();
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
			m_data.rotation = blendRot;

			DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(blendRot);
			camWorld.Translation(m_data.position);
			m_data.viewMatrix = camWorld.Invert();
		}
	}

	// Keep TransformComponent in sync with Camera position & rotation
	if (m_owner != nullptr)
	{
		if (auto* transform = m_owner->GetComponent<TransformComponent>())
		{
			transform->SetPosition(m_data.position);
			transform->SetRotation(m_data.rotation);
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

		if (m_nextCameraKey.has_value() && m_factories.find(*m_nextCameraKey) != m_factories.end())
		{
			std::unique_ptr<ICameraMode> newCam = m_factories[*m_nextCameraKey]();
			if (m_cameraStack.empty())
			{
				m_cameraStack.push_back(std::move(newCam));
			}
			else
			{
				m_cameraStack.back() = std::move(newCam);
			}
			m_cameraStack.back()->OnEnter(m_data);
		}
		UpdateMouseMode();
	}
	else if (m_nextCommand == Command::Push)
	{
		if (!m_cameraStack.empty()) m_cameraStack.back()->OnSuspend(m_data);

		if (m_nextCameraKey.has_value() && m_factories.find(*m_nextCameraKey) != m_factories.end())
		{
			std::unique_ptr<ICameraMode> newCam = m_factories[*m_nextCameraKey]();
			m_cameraStack.push_back(std::move(newCam));
			m_cameraStack.back()->OnEnter(m_data);
		}
		UpdateMouseMode();
	}
	else if (m_nextCommand == Command::Pop)
	{
		if (!m_cameraStack.empty())
		{
			m_cameraStack.back()->OnExit(m_data);
			m_cameraStack.pop_back();
			if (!m_cameraStack.empty()) m_cameraStack.back()->OnResume(m_data);
		}
		UpdateMouseMode();
	}

	m_nextCommand = Command::None;
	m_nextCameraKey = std::nullopt;
}

void HEIN::CameraController::UpdateMouseMode()
{
	if (m_cameraStack.empty()) return;
	if (m_cameraStack.back()->RequiresRelativeMouse())
		DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_RELATIVE);
	else
		DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_ABSOLUTE);
}

void HEIN::CameraController::DrawGizmo(
	const DirectX::SimpleMath::Matrix& view,
	const DirectX::SimpleMath::Matrix& proj,
	int operation,
	int mode
)
{
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	ImGuizmo::SetGizmoSizeClipSpace(0.2f);

	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
	camWorld.Translation(m_data.position);

	ImGuizmo::Manipulate(
		(float*)&view.m[0][0],
		(float*)&proj.m[0][0],
		(ImGuizmo::OPERATION)operation,
		(ImGuizmo::MODE)mode,
		(float*)&camWorld.m[0][0]
	);

	if (ImGuizmo::IsUsing())
	{
		// Switch to manual mode if in dynamic mode so gizmo has direct control
		if (!m_cameraStack.empty() && m_cameraStack.back()->GetType() != CameraType::Manual)
		{
			if (m_factories.find(CameraType::Manual) != m_factories.end())
			{
				RequestSwitch(CameraType::Manual);
				ApplyRequest();
			}
		}

		DirectX::SimpleMath::Vector3 scale, pos;
		DirectX::SimpleMath::Quaternion rot;
		if (camWorld.Decompose(scale, rot, pos))
		{
			rot.Normalize();
			m_data.position = pos;
			m_data.rotation = rot;

			DirectX::SimpleMath::Matrix newCamWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(rot);
			newCamWorld.Translation(pos);
			m_data.viewMatrix = newCamWorld.Invert();

			if (m_owner != nullptr)
			{
				if (auto* transform = m_owner->GetComponent<TransformComponent>())
				{
					transform->SetPosition(pos);
					transform->SetRotation(rot);
				}
			}
		}
	}
}

nlohmann::json HEIN::CameraController::Serialize()
{
	nlohmann::json data = IComponent::Serialize();
	data["Fov"] = m_data.fov;
	data["BlendDuration"] = m_blendDuration;
	data["PosX"] = m_data.position.x;
	data["PosY"] = m_data.position.y;
	data["PosZ"] = m_data.position.z;
	data["RotX"] = m_data.rotation.x;
	data["RotY"] = m_data.rotation.y;
	data["RotZ"] = m_data.rotation.z;
	data["RotW"] = m_data.rotation.w;
	return data;
}

void HEIN::CameraController::Deserialize(const nlohmann::json& data)
{
	IComponent::Deserialize(data);
	if (data.contains("Fov")) m_data.fov = data["Fov"];
	if (data.contains("BlendDuration")) m_blendDuration = data["BlendDuration"];
	if (data.contains("PosX")) m_data.position.x = data["PosX"];
	if (data.contains("PosY")) m_data.position.y = data["PosY"];
	if (data.contains("PosZ")) m_data.position.z = data["PosZ"];
	if (data.contains("RotX") && data.contains("RotY") && data.contains("RotZ") && data.contains("RotW"))
	{
		m_data.rotation = DirectX::SimpleMath::Quaternion(
			static_cast<float>(data["RotX"]),
			static_cast<float>(data["RotY"]),
			static_cast<float>(data["RotZ"]),
			static_cast<float>(data["RotW"])
		);
		m_data.rotation.Normalize();
	}

	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
	camWorld.Translation(m_data.position);
	m_data.viewMatrix = camWorld.Invert();
}

void HEIN::CameraController::OnInspectorGUI(GameContext& gameContext)
{
	bool isGizmoActive = (HEIN::g_ActiveGizmoTarget == this);
	if (ImGui::RadioButton("Move Camera with Gizmo", isGizmoActive))
	{
		HEIN::g_ActiveGizmoTarget = this;
	}

	if (ImGui::CollapsingHeader("Camera Controller", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool isMain = (gameContext.mainCamera == this);
		if (isMain)
		{
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[ACTIVE MAIN CAMERA]");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[INACTIVE CAMERA]");
			ImGui::SameLine();
			if (ImGui::Button("Set As Main Camera"))
			{
				gameContext.mainCamera = this;
			}
		}

		ImGui::Separator();
		ImGui::Text("Camera Modes:");

		auto currentType = GetCurrentCameraType();
		std::string currentTypeName = "None";
		if (currentType.has_value())
		{
			switch (*currentType)
			{
			case CameraType::Manual: currentTypeName = "Manual (Unity Style)"; break;
			case CameraType::Debug: currentTypeName = "Debug (Orbit / Pan)"; break;
			case CameraType::FirstPerson: currentTypeName = "FirstPerson"; break;
			case CameraType::ThirdPerson: currentTypeName = "ThirdPerson"; break;
			case CameraType::Spring: currentTypeName = "Spring"; break;
			case CameraType::LockOn: currentTypeName = "LockOn"; break;
			case CameraType::Cinematic: currentTypeName = "Cinematic"; break;
			}
		}
		ImGui::Text("Active Mode: %s", currentTypeName.c_str());

		if (ImGui::Button("Manual (Unity Style)")) RequestSwitch(CameraType::Manual);
		ImGui::SameLine();
		if (ImGui::Button("Debug (Orbit)")) RequestSwitch(CameraType::Debug);

		if (m_factories.find(CameraType::Spring) != m_factories.end())
		{
			ImGui::SameLine();
			if (ImGui::Button("Spring Mode")) RequestSwitch(CameraType::Spring);
		}
		if (m_factories.find(CameraType::ThirdPerson) != m_factories.end())
		{
			ImGui::SameLine();
			if (ImGui::Button("Third Person")) RequestSwitch(CameraType::ThirdPerson);
		}
		if (m_factories.find(CameraType::FirstPerson) != m_factories.end())
		{
			ImGui::SameLine();
			if (ImGui::Button("First Person")) RequestSwitch(CameraType::FirstPerson);
		}
		if (m_factories.find(CameraType::LockOn) != m_factories.end())
		{
			ImGui::SameLine();
			if (ImGui::Button("Lock-On")) RequestSwitch(CameraType::LockOn);
		}
		if (m_factories.find(CameraType::Cinematic) != m_factories.end())
		{
			ImGui::SameLine();
			if (ImGui::Button("Cinematic")) RequestSwitch(CameraType::Cinematic);
		}

		ImGui::Separator();
		ImGui::Text("Transform (Unity Camera Position & Rotation):");

		// FOV in degrees
		float fovDeg = DirectX::XMConvertToDegrees(m_data.fov);
		if (ImGui::SliderFloat("FOV (Degrees)", &fovDeg, 10.0f, 140.0f, "%.1f deg"))
		{
			m_data.fov = DirectX::XMConvertToRadians(fovDeg);
		}

		// Position
		if (ImGui::DragFloat3("Position", &m_data.position.x, 0.1f))
		{
			DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
			camWorld.Translation(m_data.position);
			m_data.viewMatrix = camWorld.Invert();
			if (m_owner != nullptr)
			{
				if (auto* transform = m_owner->GetComponent<TransformComponent>())
				{
					transform->SetPosition(m_data.position);
				}
			}
		}

		// Rotation Euler
		DirectX::SimpleMath::Vector3 euler = m_data.rotation.ToEuler();
		euler.x = DirectX::XMConvertToDegrees(euler.x);
		euler.y = DirectX::XMConvertToDegrees(euler.y);
		euler.z = DirectX::XMConvertToDegrees(euler.z);
		if (ImGui::DragFloat3("Rotation (Euler)", &euler.x, 0.5f))
		{
			m_data.rotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(
				DirectX::XMConvertToRadians(euler.y),
				DirectX::XMConvertToRadians(euler.x),
				DirectX::XMConvertToRadians(euler.z)
			);
			m_data.rotation.Normalize();
			DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
			camWorld.Translation(m_data.position);
			m_data.viewMatrix = camWorld.Invert();
			if (m_owner != nullptr)
			{
				if (auto* transform = m_owner->GetComponent<TransformComponent>())
				{
					transform->SetRotation(m_data.rotation);
				}
			}
		}

		if (ImGui::Button("Reset Camera Transform"))
		{
			m_data.position = DirectX::SimpleMath::Vector3(0.0f, 15.0f, -40.0f);
			m_data.rotation = DirectX::SimpleMath::Quaternion::Identity;
			DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
			camWorld.Translation(m_data.position);
			m_data.viewMatrix = camWorld.Invert();
			if (m_owner != nullptr)
			{
				if (auto* transform = m_owner->GetComponent<TransformComponent>())
				{
					transform->SetPosition(m_data.position);
					transform->SetRotation(m_data.rotation);
				}
			}
		}

		// Blend settings
		ImGui::DragFloat("Blend Duration (s)", &m_blendDuration, 0.05f, 0.0f, 5.0f);
		if (m_isBlending)
		{
			ImGui::ProgressBar(m_blendTimer / m_blendDuration, ImVec2(-1, 0), "Blending...");
		}

		ImGui::Separator();
		ImGui::Text("Viewport & Frustum Information:");
		D3D11_VIEWPORT vp = gameContext.deviceResources.GetScreenViewport();
		ImGui::Text("Screen Viewport: %.0f x %.0f", vp.Width, vp.Height);
		float aspect = (vp.Height > 0.0f) ? (vp.Width / vp.Height) : 1.777f;
		ImGui::Text("Aspect Ratio: %.3f", aspect);
		ImGui::Text("Camera Stack Depth: %zu", m_cameraStack.size());
	}
}