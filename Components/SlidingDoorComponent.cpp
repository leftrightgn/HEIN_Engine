#include "pch.h"
#include "SlidingDoorComponent.h"
#include "Components/TransformComponent.h"
#include "Components/ColliderComponent/ColliderComponent.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include <ImGui/imgui.h>
#include <cmath>

HEIN::SlidingDoorComponent::SlidingDoorComponent(Actor* owner, ActorManager* manager)
	: IComponent(owner)
	, m_actorManager(manager)
{
}

void HEIN::SlidingDoorComponent::Start()
{
	TransformComponent* targetTrans = ResolveTargetTransform();
	if (targetTrans != nullptr && !m_initialPositionSaved)
	{
		m_closedPosition = targetTrans->GetPosition();
		m_initialPositionSaved = true;
	}
}

HEIN::TransformComponent* HEIN::SlidingDoorComponent::ResolveTargetTransform()
{
	if (m_targetActorID != 0 && m_actorManager != nullptr)
	{
		Actor* targetActor = m_actorManager->GetActor(m_targetActorID);
		if (targetActor != nullptr)
		{
			return targetActor->GetComponent<TransformComponent>();
		}
	}

	if (!m_targetActorTag.empty() && m_actorManager != nullptr)
	{
		Actor* targetActor = m_actorManager->GetActorByName(m_targetActorTag);
		if (targetActor != nullptr)
		{
			return targetActor->GetComponent<TransformComponent>();
		}
	}

	if (m_owner != nullptr)
	{
		return m_owner->GetComponent<TransformComponent>();
	}

	return nullptr;
}

bool HEIN::SlidingDoorComponent::CheckPlayerInteraction(TransformComponent* doorTransform)
{
	if (doorTransform == nullptr) return false;

	DirectX::SimpleMath::Vector3 doorPos = doorTransform->GetPosition();

	// Check collider touch / trigger
	if (m_useCollisionTrigger && m_owner != nullptr)
	{
		auto colliders = m_owner->GetComponents<ColliderComponent>();
		for (auto* col : colliders)
		{
			if (col != nullptr && col->IsCollidingThisFrame())
			{
				return true;
			}
		}
	}

	// Check distance to Player actor in ActorManager
	if (m_useProximityTrigger && m_actorManager != nullptr)
	{
		const auto& actors = m_actorManager->GetAllActors();
		for (const auto& pair : actors)
		{
			Actor* actor = pair.second.get();
			if (actor == nullptr || actor == m_owner) continue;

			bool isPlayer = (actor->GetActorType() == ActorType::Player) ||
				(actor->GetTag() == L"Player") ||
				(actor->GetTag() == L"Knight");

			if (isPlayer)
			{
				TransformComponent* playerTrans = actor->GetComponent<TransformComponent>();
				if (playerTrans != nullptr)
				{
					DirectX::SimpleMath::Vector3 playerPos = playerTrans->GetPosition();
					float distSq = (playerPos - doorPos).LengthSquared();
					if (distSq <= (m_triggerDistance * m_triggerDistance))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void HEIN::SlidingDoorComponent::Update(float deltaTime)
{
	TransformComponent* targetTrans = ResolveTargetTransform();
	if (targetTrans == nullptr) return;

	if (!m_initialPositionSaved)
	{
		m_closedPosition = targetTrans->GetPosition();
		m_initialPositionSaved = true;
	}

	// Detect player
	m_playerDetected = CheckPlayerInteraction(targetTrans);

	if (m_playerDetected)
	{
		m_closeTimer = 0.0f;
		if (m_state != DoorState::Open && m_state != DoorState::Opening)
		{
			m_state = DoorState::Opening;
		}
	}
	else if (m_autoClose && (m_state == DoorState::Open || m_state == DoorState::Opening))
	{
		m_closeTimer += deltaTime;
		if (m_closeTimer >= m_closeDelay)
		{
			m_state = DoorState::Closing;
		}
	}

	// Animate progress
	if (m_state == DoorState::Opening)
	{
		float speed = (m_moveSpeed > 0.01f) ? (1.0f / (m_slideOffset.Length() / m_moveSpeed)) : 1.0f;
		m_currentProgress += speed * deltaTime;
		if (m_currentProgress >= 1.0f)
		{
			m_currentProgress = 1.0f;
			m_state = DoorState::Open;
		}
	}
	else if (m_state == DoorState::Closing)
	{
		float speed = (m_moveSpeed > 0.01f) ? (1.0f / (m_slideOffset.Length() / m_moveSpeed)) : 1.0f;
		m_currentProgress -= speed * deltaTime;
		if (m_currentProgress <= 0.0f)
		{
			m_currentProgress = 0.0f;
			m_state = DoorState::Closed;
		}
	}

	// Smooth easing (smoothstep: 3x^2 - 2x^3)
	float t = m_currentProgress;
	float smoothT = t * t * (3.0f - 2.0f * t);

	// Update world position
	DirectX::SimpleMath::Vector3 newPos = m_closedPosition + (m_slideOffset * smoothT);
	targetTrans->SetPosition(newPos);
}

void HEIN::SlidingDoorComponent::Open()
{
	m_state = DoorState::Opening;
	m_closeTimer = 0.0f;
}

void HEIN::SlidingDoorComponent::Close()
{
	m_state = DoorState::Closing;
}

void HEIN::SlidingDoorComponent::Toggle()
{
	if (m_state == DoorState::Closed || m_state == DoorState::Closing)
	{
		Open();
	}
	else
	{
		Close();
	}
}

void HEIN::SlidingDoorComponent::OnInspectorGUI(GameContext& /*gameContext*/)
{
	ImGui::PushID((void*)this);

	if (ImGui::CollapsingHeader("Sliding Door / Wall Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// State indicator
		const char* stateStr = "Closed";
		ImVec4 stateColor = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
		if (m_state == DoorState::Opening) { stateStr = "Opening..."; stateColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f); }
		else if (m_state == DoorState::Open) { stateStr = "OPEN"; stateColor = ImVec4(0.2f, 0.9f, 0.3f, 1.0f); }
		else if (m_state == DoorState::Closing) { stateStr = "Closing..."; stateColor = ImVec4(0.9f, 0.6f, 0.2f, 1.0f); }

		ImGui::Text("Door State: ");
		ImGui::SameLine();
		ImGui::TextColored(stateColor, "%s", stateStr);

		ImGui::ProgressBar(m_currentProgress, ImVec2(-1, 20), "");

		// Manual Controls
		if (ImGui::Button("Open Door", ImVec2(100, 24))) Open();
		ImGui::SameLine();
		if (ImGui::Button("Close Door", ImVec2(100, 24))) Close();
		ImGui::SameLine();
		if (ImGui::Button("Reset Position", ImVec2(120, 24)))
		{
			m_currentProgress = 0.0f;
			m_state = DoorState::Closed;
			TransformComponent* trans = ResolveTargetTransform();
			if (trans != nullptr && m_initialPositionSaved)
			{
				trans->SetPosition(m_closedPosition);
			}
		}

		ImGui::Separator();

		// Movement Settings
		ImGui::DragFloat3("Slide Offset (Down/Move)", &m_slideOffset.x, 0.1f);
		ImGui::DragFloat("Move Speed (units/s)", &m_moveSpeed, 0.1f, 0.1f, 100.0f);

		// Trigger Settings
		ImGui::Checkbox("Open On Proximity", &m_useProximityTrigger);
		if (m_useProximityTrigger)
		{
			ImGui::DragFloat("Trigger Distance", &m_triggerDistance, 0.1f, 0.5f, 100.0f);
		}
		ImGui::Checkbox("Open On Collider Contact", &m_useCollisionTrigger);

		ImGui::Checkbox("Auto-Close", &m_autoClose);
		if (m_autoClose)
		{
			ImGui::DragFloat("Close Delay (s)", &m_closeDelay, 0.1f, 0.0f, 60.0f);
		}

		// Closed position reference
		if (ImGui::TreeNode("Closed Base Position"))
		{
			if (ImGui::DragFloat3("Base Pos", &m_closedPosition.x, 0.1f))
			{
				m_initialPositionSaved = true;
			}
			if (ImGui::Button("Capture Current Pos as Closed Base"))
			{
				TransformComponent* trans = ResolveTargetTransform();
				if (trans != nullptr)
				{
					m_closedPosition = trans->GetPosition();
					m_initialPositionSaved = true;
				}
			}
			ImGui::TreePop();
		}
	}

	ImGui::PopID();
}

nlohmann::json HEIN::SlidingDoorComponent::Serialize()
{
	nlohmann::json data = IComponent::Serialize();
	data["SlideOffsetX"] = m_slideOffset.x;
	data["SlideOffsetY"] = m_slideOffset.y;
	data["SlideOffsetZ"] = m_slideOffset.z;

	data["ClosedPosX"] = m_closedPosition.x;
	data["ClosedPosY"] = m_closedPosition.y;
	data["ClosedPosZ"] = m_closedPosition.z;
	data["InitialPosSaved"] = m_initialPositionSaved;

	data["MoveSpeed"] = m_moveSpeed;
	data["TriggerDistance"] = m_triggerDistance;
	data["UseProximity"] = m_useProximityTrigger;
	data["UseCollision"] = m_useCollisionTrigger;
	data["AutoClose"] = m_autoClose;
	data["CloseDelay"] = m_closeDelay;

	data["TargetActorID"] = m_targetActorID;
	std::string tagStr(m_targetActorTag.begin(), m_targetActorTag.end());
	data["TargetActorTag"] = tagStr;

	return data;
}

void HEIN::SlidingDoorComponent::Deserialize(const nlohmann::json& data)
{
	IComponent::Deserialize(data);
	if (data.contains("SlideOffsetX")) m_slideOffset.x = data["SlideOffsetX"];
	if (data.contains("SlideOffsetY")) m_slideOffset.y = data["SlideOffsetY"];
	if (data.contains("SlideOffsetZ")) m_slideOffset.z = data["SlideOffsetZ"];

	if (data.contains("ClosedPosX")) m_closedPosition.x = data["ClosedPosX"];
	if (data.contains("ClosedPosY")) m_closedPosition.y = data["ClosedPosY"];
	if (data.contains("ClosedPosZ")) m_closedPosition.z = data["ClosedPosZ"];
	if (data.contains("InitialPosSaved")) m_initialPositionSaved = data["InitialPosSaved"];

	if (data.contains("MoveSpeed")) m_moveSpeed = data["MoveSpeed"];
	if (data.contains("TriggerDistance")) m_triggerDistance = data["TriggerDistance"];
	if (data.contains("UseProximity")) m_useProximityTrigger = data["UseProximity"];
	if (data.contains("UseCollision")) m_useCollisionTrigger = data["UseCollision"];
	if (data.contains("AutoClose")) m_autoClose = data["AutoClose"];
	if (data.contains("CloseDelay")) m_closeDelay = data["CloseDelay"];

	if (data.contains("TargetActorID")) m_targetActorID = data["TargetActorID"];
	if (data.contains("TargetActorTag"))
	{
		std::string tagStr = data["TargetActorTag"];
		m_targetActorTag = std::wstring(tagStr.begin(), tagStr.end());
	}
}
