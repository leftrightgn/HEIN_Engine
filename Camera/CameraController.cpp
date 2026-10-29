#include "pch.h"
#include "CameraController.h"
#include "DebugCameraMode.h"
#include "ManualCameraMode.h"
#include "CinematicMode.h"
#include "Entities/ActorManager.h"
#include <string>
#include "Components/TransformComponent.h"
#include "DebugingTools/DebugUIManager.h"
#include "Framework/GameContext.h"
#if HEIN_WITH_EDITOR
#include "ImGui/imgui.h"
#include "ImGui/ImGuizmo.h"
#endif
#include <cmath>
#include <optional>

namespace HEIN
{
CameraController::CameraController(Actor* owner)
	: IComponent(owner)
{
	m_data.fov = DirectX::XMConvertToRadians(50.0f);
	m_data.position = DirectX::SimpleMath::Vector3(0.0f, 15.0f, -40.0f);
	m_data.rotation = DirectX::SimpleMath::Quaternion::Identity;
	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
	camWorld.Translation(m_data.position);
	m_data.viewMatrix = camWorld.Invert();
}

void CameraController::Start()
{
	RegisterCamera(
		CameraType::Manual,
		[]() { return std::make_unique<ManualCameraMode>(); }
	);
	RegisterCamera(
		CameraType::Debug,
		[]() { return std::make_unique<DebugCameraMode>(); }
	);

	if (m_cameraStack.empty())
	{
		SetFirstCamera(CameraType::Manual);
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

void CameraController::InitializeAfterDeserialize(GameContext& /*gameContext*/)
{
	RegisterCamera(
		CameraType::Manual,
		[]() { return std::make_unique<ManualCameraMode>(); }
	);
	RegisterCamera(
		CameraType::Debug,
		[]() { return std::make_unique<DebugCameraMode>(); }
	);

	if (m_cameraStack.empty())
	{
		SetFirstCamera(CameraType::Manual);
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

void CameraController::RegisterCamera(CameraType key, CameraFactory factory)
{
	m_factories[key] = factory;
}

void CameraController::SetFirstCamera(CameraType key)
{
	if (m_factories.find(key) != m_factories.end())
	{
		std::unique_ptr<ICameraMode> firstCam = m_factories[key]();

		if (auto* cin = dynamic_cast<CinematicMode*>(firstCam.get()))
		{
			if (HasSavedCinematicTracks())
			{
				cin->DeserializeTracks(m_cinematicTracksJson);
			}
		}

		m_cameraStack.clear();
		m_cameraStack.push_back(std::move(firstCam));
		m_cameraStack.back()->OnEnter(m_data);
		UpdateMouseMode();
	}
}

void CameraController::Update(float deltaTime)
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
			// Smoothstep easing (3t^2 - 2t^3) for seamless acceleration and soft-landing deceleration
			float smoothT = t * t * (3.0f - 2.0f * t);
			m_data.position = DirectX::SimpleMath::Vector3::Lerp(m_previousCameraData.position, m_data.position, smoothT);
			m_data.fov = std::lerp(m_previousCameraData.fov, m_data.fov, smoothT);
			DirectX::SimpleMath::Quaternion blendRot = DirectX::SimpleMath::Quaternion::Slerp(m_previousCameraData.rotation, m_data.rotation, smoothT);
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

void CameraController::ProcessInput(const CameraInputState& input)
{
	if (!m_cameraStack.empty())
	{
		m_cameraStack.back()->ProcessInput(input);
	}
}

bool CameraController::LocksPlayerRotation() const
{
	if (m_cameraStack.empty()) return false;

	return m_cameraStack.back()->LocksPlayerRotation();
}

void CameraController::RequestSwitch(CameraType type)
{
	if (!m_cameraStack.empty() && m_cameraStack.back()->GetType() == type)
	{
		return;
	}

	m_nextCommand = Command::Switch;
	m_nextCameraKey = type;
}

void CameraController::RequestPush(CameraType type)
{
	m_nextCommand = Command::Push;
	m_nextCameraKey = type;
}

void CameraController::RequestPop(CameraType /*type*/)
{
	m_nextCommand = Command::Pop;
}

void CameraController::ApplyRequest()
{
	if (m_nextCommand == Command::None) return;

	if (m_nextCommand == Command::Switch && m_nextCameraKey.has_value() && !m_cameraStack.empty() && m_cameraStack.back()->GetType() == *m_nextCameraKey)
	{
		m_nextCommand = Command::None;
		m_nextCameraKey = std::nullopt;
		return;
	}

	// Cache cinematic tracks prior to leaving or replacing camera modes
	if (auto* currentCin = dynamic_cast<CinematicMode*>(GetCurrentCameraMode()))
	{
		m_cinematicTracksJson = currentCin->SerializeTracks();
	}

	m_previousCameraData = m_data;
	m_isBlending = true;
	m_blendTimer = 0.0f;

	if (m_nextCommand == Command::Switch)
	{
		if (m_nextCameraKey.has_value() && m_factories.find(*m_nextCameraKey) != m_factories.end())
		{
			if (!m_cameraStack.empty()) m_cameraStack.back()->OnExit(m_data);
			std::unique_ptr<ICameraMode> newCam = m_factories[*m_nextCameraKey]();

			if (auto* cin = dynamic_cast<CinematicMode*>(newCam.get()))
			{
				if (HasSavedCinematicTracks())
				{
					cin->DeserializeTracks(m_cinematicTracksJson);
				}
			}

			if (m_cameraStack.empty())
			{
				m_cameraStack.push_back(std::move(newCam));
			}
			else
			{
				m_cameraStack.back() = std::move(newCam);
			}

			m_cameraStack.back()->OnEnter(m_data);
			UpdateMouseMode();
		}
	}
	else if (m_nextCommand == Command::Push)
	{
		if (m_nextCameraKey.has_value() && m_factories.find(*m_nextCameraKey) != m_factories.end())
		{
			if (!m_cameraStack.empty()) m_cameraStack.back()->OnSuspend(m_data);
			std::unique_ptr<ICameraMode> newCam = m_factories[*m_nextCameraKey]();

			if (auto* cin = dynamic_cast<CinematicMode*>(newCam.get()))
			{
				if (HasSavedCinematicTracks())
				{
					cin->DeserializeTracks(m_cinematicTracksJson);
				}
			}

			m_cameraStack.push_back(std::move(newCam));
			m_cameraStack.back()->OnEnter(m_data);
			UpdateMouseMode();
		}
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

void CameraController::UpdateMouseMode()
{
	if (m_cameraStack.empty()) return;

	if (m_cameraStack.back()->RequiresRelativeMouse())
		DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_RELATIVE);
	else
		DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_ABSOLUTE);
}

#if HEIN_WITH_EDITOR
void CameraController::DrawGizmo(
	const DirectX::SimpleMath::Matrix& view,
	const DirectX::SimpleMath::Matrix& proj,
	int operation,
	int mode
)
{
	float vpX = 0.0f, vpY = 0.0f, vpW = 0.0f, vpH = 0.0f;
	HEIN::DebugUIManager::GetActiveViewportRect(vpX, vpY, vpW, vpH);

	if (vpW > 0.0f && vpH > 0.0f)
	{
		ImGuizmo::SetRect(vpX, vpY, vpW, vpH);
	}

	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
	camWorld.Translation(m_data.position);

	bool isPerspective = (std::abs(proj.m[3][3]) < 1e-4f);
	if (isPerspective)
	{
		DirectX::SimpleMath::Vector4 clipPos = DirectX::SimpleMath::Vector4::Transform(
			DirectX::SimpleMath::Vector4(m_data.position.x, m_data.position.y, m_data.position.z, 1.0f),
			view * proj
		);
		if (clipPos.w <= 0.1f && !ImGuizmo::IsUsing())
		{
			return;
		}
	}

	ImGuizmo::Manipulate(
		(float*)&view.m[0][0],
		(float*)&proj.m[0][0],
		(ImGuizmo::OPERATION)operation,
		(ImGuizmo::MODE)mode,
		(float*)&camWorld.m[0][0]
	);

	if (ImGuizmo::IsUsing())
	{
		if (GetCurrentCameraType() != CameraType::Cinematic && !m_cameraStack.empty() && m_cameraStack.back()->GetType() != CameraType::Manual)
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

			if (auto* cinematic = dynamic_cast<CinematicMode*>(GetCurrentCameraMode()))
			{
				if (!cinematic->IsPlaying() && cinematic->GetSelectedSlotIndex() < cinematic->GetWaypoints().size())
				{
					auto& wp = cinematic->GetWaypoints()[cinematic->GetSelectedSlotIndex()];
					wp.eyePosition = pos;
					DirectX::SimpleMath::Matrix rotMat = DirectX::SimpleMath::Matrix::CreateFromQuaternion(rot);
					wp.lookAtPosition = pos + rotMat.Forward() * 10.0f;
				}

				m_cinematicTracksJson = cinematic->SerializeTracks();
			}

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
#else
void CameraController::DrawGizmo(const DirectX::SimpleMath::Matrix&, const DirectX::SimpleMath::Matrix&, int, int) {}
#endif

nlohmann::json CameraController::Serialize()
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

	if (auto currentType = GetCurrentCameraType())
	{
		data["ActiveCameraType"] = static_cast<int>(*currentType);
	}

	if (auto* cinematic = dynamic_cast<CinematicMode*>(GetCurrentCameraMode()))
	{
		m_cinematicTracksJson = cinematic->SerializeTracks();
	}

	if (HasSavedCinematicTracks())
	{
		data["CinematicTracks"] = m_cinematicTracksJson;
	}

	return data;
}

void CameraController::Deserialize(const nlohmann::json& data)
{
	IComponent::Deserialize(data);

	if (data.contains("Fov")) m_data.fov = data["Fov"].is_number() ? data["Fov"].get<float>() : DirectX::XM_PI / 4.0f;

	if (data.contains("BlendDuration")) m_blendDuration = data["BlendDuration"].is_number() ? data["BlendDuration"].get<float>() : 1.0f;

	if (data.contains("PosX")) m_data.position.x = data["PosX"].is_number() ? data["PosX"].get<float>() : 0.0f;

	if (data.contains("PosY")) m_data.position.y = data["PosY"].is_number() ? data["PosY"].get<float>() : 0.0f;

	if (data.contains("PosZ")) m_data.position.z = data["PosZ"].is_number() ? data["PosZ"].get<float>() : 0.0f;

	if (data.contains("RotX") && data.contains("RotY") && data.contains("RotZ") && data.contains("RotW"))
	{
		m_data.rotation = DirectX::SimpleMath::Quaternion(
			data["RotX"].is_number() ? data["RotX"].get<float>() : 0.0f,
			data["RotY"].is_number() ? data["RotY"].get<float>() : 0.0f,
			data["RotZ"].is_number() ? data["RotZ"].get<float>() : 0.0f,
			data["RotW"].is_number() ? data["RotW"].get<float>() : 1.0f
		);
		m_data.rotation.Normalize();
	}

	if (data.contains("ActiveCameraType") && data["ActiveCameraType"].is_number())
	{
		m_savedCameraType = static_cast<CameraType>(data["ActiveCameraType"].get<int>());
	}

	if (data.contains("CinematicTracks"))
	{
		m_cinematicTracksJson = data["CinematicTracks"];

		if (auto* cinematic = dynamic_cast<CinematicMode*>(GetCurrentCameraMode()))
		{
			cinematic->DeserializeTracks(m_cinematicTracksJson);
		}
	}

	DirectX::SimpleMath::Matrix camWorld = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
	camWorld.Translation(m_data.position);
	m_data.viewMatrix = camWorld.Invert();
}

#if HEIN_WITH_EDITOR
void CameraController::OnInspectorGUI(GameContext& gameContext)
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
		ImGui::Checkbox("Elden Ring Auto Lock-On", &m_autoLockOn);

		if (m_factories.find(CameraType::Manual) != m_factories.end())
		{
			if (ImGui::Button("Manual (Unity Style)")) RequestSwitch(CameraType::Manual);
			ImGui::SameLine();
		}

		if (m_factories.find(CameraType::Debug) != m_factories.end())
		{
			if (ImGui::Button("Debug (Orbit)")) RequestSwitch(CameraType::Debug);
			ImGui::SameLine();
		}

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

		if (auto* cinematic = dynamic_cast<CinematicMode*>(GetCurrentCameraMode()))
		{
			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Cinematic Multi-Track Studio");
			auto& tracks = cinematic->GetTracks();
			size_t activeTrackIdx = cinematic->GetActiveTrackIndex();
			const char* currentTrackName = (activeTrackIdx < tracks.size()) ? tracks[activeTrackIdx].name.c_str() : "Default";

			// Multi-Track Selector & Management Row
			ImGui::Text("Active Sequence Track:");
			ImGui::SetNextItemWidth(220.0f);

			if (ImGui::BeginCombo("##TrackCombo", currentTrackName))
			{
				for (size_t i = 0; i < tracks.size(); ++i)
				{
					bool isSel = (i == activeTrackIdx);

					if (ImGui::Selectable(tracks[i].name.c_str(), isSel))
					{
						cinematic->SetActiveTrackIndex(i);
					}

					if (isSel) ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			ImGui::SameLine();

			if (ImGui::Button("+ New Track"))
			{
				std::string newName = "Track_" + std::to_string(tracks.size() + 1);
				cinematic->AddTrack(newName);
			}

			ImGui::SameLine();

			if (ImGui::Button("Duplicate"))
			{
				cinematic->DuplicateTrack(activeTrackIdx);
			}

			ImGui::SameLine();

			if (ImGui::Button("Delete Track") && tracks.size() > 1)
			{
				cinematic->RemoveTrack(activeTrackIdx);
			}

			if (activeTrackIdx < tracks.size())
			{
				char nameBuf[64];
				strcpy_s(nameBuf, tracks[activeTrackIdx].name.c_str());
				ImGui::SetNextItemWidth(220.0f);

				if (ImGui::InputText("Rename Track", nameBuf, sizeof(nameBuf)))
				{
					cinematic->RenameTrack(activeTrackIdx, nameBuf);
				}
			}

			// Target-Agnostic Track Generator (Any Boss, Gate, Item, or Landmark)
			if (ImGui::TreeNode("Generate Track for Scene Entity (Any Boss / Object)"))
			{
				static int selectedEntityIdx = 0;
				std::vector<Actor*> candidateActors;
				std::vector<std::string> candidateNames;

				if (gameContext.actorManager)
				{
					for (const auto& pair : gameContext.actorManager->GetAllActors())
					{
						Actor* act = pair.second.get();

						if (act && act->GetComponent<TransformComponent>())
						{
							candidateActors.push_back(act);
							std::wstring tagW = act->GetTag();
							std::string tagA(tagW.begin(), tagW.end());
							candidateNames.push_back(tagA + " (ID:" + std::to_string(act->GetID()) + ")");
						}
					}
				}

				if (!candidateActors.empty())
				{
					if (selectedEntityIdx >= static_cast<int>(candidateActors.size())) selectedEntityIdx = 0;

					if (ImGui::BeginCombo("Target Entity", candidateNames[selectedEntityIdx].c_str()))
					{
						for (int i = 0; i < static_cast<int>(candidateActors.size()); ++i)
						{
							bool isSel = (i == selectedEntityIdx);

							if (ImGui::Selectable(candidateNames[i].c_str(), isSel))
							{
								selectedEntityIdx = i;
							}

							if (isSel) ImGui::SetItemDefaultFocus();
						}

						ImGui::EndCombo();
					}

					if (ImGui::Button("Generate Dramatic Reveal Preset"))
					{
						auto* target = candidateActors[selectedEntityIdx];
						auto* tTrans = target->GetComponent<TransformComponent>();
						DirectX::SimpleMath::Vector3 viewerPos = m_data.position;

						if (auto* player = gameContext.actorManager->GetFirstActorByType(ActorType::Player))
						{
							if (auto* pTrans = player->GetComponent<TransformComponent>())
							{
								viewerPos = pTrans->GetPosition();
							}
						}

						std::string genName = candidateNames[selectedEntityIdx] + "_Reveal";
						cinematic->GenerateActorRevealTrack(genName, tTrans->GetPosition(), viewerPos);
					}

					ImGui::SameLine();

					if (ImGui::Button("Generate 360 Orbit Preset"))
					{
						auto* target = candidateActors[selectedEntityIdx];
						auto* tTrans = target->GetComponent<TransformComponent>();
						std::string genName = candidateNames[selectedEntityIdx] + "_Orbit";
						cinematic->GenerateOrbitTrack(genName, tTrans->GetPosition(), 10.0f, 3.5f, 6.0f);
					}
				}

				ImGui::TreePop();
			}

			auto& waypoints = cinematic->GetWaypoints();
			size_t count = waypoints.size();
			size_t selectedIndex = cinematic->GetSelectedSlotIndex();
			bool isPlaying = cinematic->IsPlaying();

			if (isPlaying)
			{
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "[STATE: PLAYING TRACK: %s] Shot %zu / %zu", currentTrackName, cinematic->GetCurrentWaypointIndex() + 1, count);
				float currentDur = (waypoints.empty() || cinematic->GetCurrentWaypointIndex() >= count) ? 1.0f : waypoints[cinematic->GetCurrentWaypointIndex()].duration;
				ImGui::ProgressBar(cinematic->GetWaypointTimer() / (currentDur > 0.001f ? currentDur : 1.0f), ImVec2(-1, 0), "Playing Sequence...");

				if (ImGui::Button("Pause / Edit Shots"))
				{
					cinematic->Pause();
				}

				ImGui::SameLine();

				if (ImGui::Button("Skip Track"))
				{
					cinematic->Skip();
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "[STATE: EDITING TRACK: %s | SHOT %zu / %zu]", currentTrackName, count == 0 ? 0 : (selectedIndex + 1), count);

				if (ImGui::Button("Play Track"))
				{
					cinematic->Play();
				}

				ImGui::SameLine();

				if (ImGui::Button("Clear All Shots"))
				{
					cinematic->ClearWaypoints();
				}
			}

			ImGui::Spacing();
			ImGui::Text("Shot Slots (Click any slot to switch camera view to it):");

			for (size_t i = 0; i < count; ++i)
			{
				if (i > 0) ImGui::SameLine();
				std::string slotLabel = "Shot " + std::to_string(i + 1);

				if (!isPlaying && i == selectedIndex)
				{
					slotLabel = "[ Shot " + std::to_string(i + 1) + " (Active) ]";
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
				}

				if (ImGui::Button(slotLabel.c_str()))
				{
					cinematic->SetSelectedSlotIndex(i);
				}

				if (!isPlaying && i == selectedIndex)
				{
					ImGui::PopStyleColor();
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("+ Add Slot"))
			{
				CinematicWaypoint newWp;
				newWp.eyePosition = m_data.position;
				DirectX::SimpleMath::Matrix rotMat = DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_data.rotation);
				newWp.lookAtPosition = m_data.position + rotMat.Forward() * 10.0f;
				newWp.fovDegrees = DirectX::XMConvertToDegrees(m_data.fov);
				newWp.duration = 2.5f;
				cinematic->AddWaypoint(newWp);
			}

			if (!waypoints.empty() && selectedIndex < count && !isPlaying)
			{
				auto& activeWp = waypoints[selectedIndex];
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.7f, 1.0f), "Shot %zu Controls (Live Viewport Sync):", selectedIndex + 1);
				ImGui::DragFloat3("Camera Post (Eye Pos)", &activeWp.eyePosition.x, 0.1f);
				ImGui::DragFloat3("Look-At Target", &activeWp.lookAtPosition.x, 0.1f);
				ImGui::SliderFloat("FOV (deg)", &activeWp.fovDegrees, 15.0f, 120.0f, "%.1f deg");
				ImGui::DragFloat("Duration (s)", &activeWp.duration, 0.1f, 0.1f, 30.0f, "%.2f s");

				if (ImGui::Button("Look at Boss"))
				{
					if (gameContext.actorManager)
					{
						auto* enemy = gameContext.actorManager->GetFirstActorByType(ActorType::Enemy);

						if (enemy)
						{
							auto* eTrans = enemy->GetComponent<TransformComponent>();

							if (eTrans)
							{
								activeWp.lookAtPosition = eTrans->GetPosition() + DirectX::SimpleMath::Vector3(0.0f, 2.5f, 0.0f);
							}
						}
					}
				}

				ImGui::SameLine();

				if (ImGui::Button("Look at Player"))
				{
					if (gameContext.actorManager)
					{
						auto* player = gameContext.actorManager->GetFirstActorByType(ActorType::Player);

						if (player)
						{
							auto* pTrans = player->GetComponent<TransformComponent>();

							if (pTrans)
							{
								activeWp.lookAtPosition = pTrans->GetPosition() + DirectX::SimpleMath::Vector3(0.0f, 1.5f, 0.0f);
							}
						}
					}
				}

				ImGui::SameLine();

				if (ImGui::Button("Delete Shot") && count > 1)
				{
					cinematic->RemoveWaypoint(selectedIndex);
				}

				ImGui::TextDisabled("Controls: Left Mouse drag rotates look direction; WASD flies camera.");
			}

			// Keep cached tracks synchronized with all edits made in Inspector
			m_cinematicTracksJson = cinematic->SerializeTracks();
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
#else
void CameraController::OnInspectorGUI(GameContext&) {}
#endif

} // namespace HEIN
