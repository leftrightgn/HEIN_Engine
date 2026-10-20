#include "pch.h"
#include "SocketAttachmentComponent.h"
#include <Entities/ActorManager.h>
#include <Components/SocketComponent.h>
#include <Components/TransformComponent.h>
#include <DebugingTools/DebugUIManager.h>

HEIN::SocketAttachmentComponent::SocketAttachmentComponent(Actor* owner, ActorManager* manager)
	: IComponent(owner)
	, m_actorManager(manager)
	, m_socketOwnerActorID(HEIN::INVALID_ACTOR_ID)
	, m_socketOwnerTag(L"")
	, m_socketName(L"")
{
}

void HEIN::SocketAttachmentComponent::Initialize(HEIN::ActorID targetActorID, const std::wstring& socketName)
{
	m_socketOwnerActorID = targetActorID;
	m_socketName = socketName;
	if (m_owner && targetActorID != HEIN::INVALID_ACTOR_ID)
	{
		m_owner->SetOwnerID(targetActorID);
	}
	if (m_actorManager)
	{
		Actor* target = m_actorManager->GetActor(targetActorID);
		if (target) m_socketOwnerTag = target->GetTag();
	}
}

void HEIN::SocketAttachmentComponent::LateUpdate(float deltaTime)
{
	if (m_actorManager == nullptr) return;

	if (m_socketOwnerActorID == HEIN::INVALID_ACTOR_ID && m_socketOwnerTag != L"")
	{
		Actor* target = m_actorManager->GetActorByName(m_socketOwnerTag);
		if (target)
		{
			m_socketOwnerActorID = target->GetID();
			if (m_owner) m_owner->SetOwnerID(m_socketOwnerActorID);
		}
	}

	// Fallback to parent actor if no socket owner is set or found by tag
	if (m_socketOwnerActorID == HEIN::INVALID_ACTOR_ID && m_owner && m_owner->GetParentID() != HEIN::INVALID_ACTOR_ID)
	{
		Actor* parent = m_actorManager->GetActor(m_owner->GetParentID());
		if (parent)
		{
			m_socketOwnerActorID = parent->GetID();
			m_socketOwnerTag = parent->GetTag();
			m_owner->SetOwnerID(m_socketOwnerActorID);
		}
	}

	// Fallback: If still invalid, try to find active Player actor
	if (m_socketOwnerActorID == HEIN::INVALID_ACTOR_ID && m_owner && m_owner->GetActorType() != ActorType::Enemy)
	{
		if (Actor* player = m_actorManager->GetFirstActorByType(ActorType::Player))
		{
			m_socketOwnerActorID = player->GetID();
			m_socketOwnerTag = player->GetTag();
			m_owner->SetOwnerID(m_socketOwnerActorID);
		}
	}

	if (m_socketOwnerActorID == HEIN::INVALID_ACTOR_ID) return;

	HEIN::Actor* targetActor = m_actorManager->GetActor(m_socketOwnerActorID);
	if (targetActor == nullptr)
	{
		// Target actor was destroyed or missing; fallback to parent actor
		m_socketOwnerActorID = HEIN::INVALID_ACTOR_ID;
		if (m_owner && m_owner->GetParentID() != HEIN::INVALID_ACTOR_ID)
		{
			targetActor = m_actorManager->GetActor(m_owner->GetParentID());
			if (targetActor)
			{
				m_socketOwnerActorID = targetActor->GetID();
				m_socketOwnerTag = targetActor->GetTag();
				m_owner->SetOwnerID(m_socketOwnerActorID);
			}
		}
		if (targetActor == nullptr && m_owner && m_owner->GetActorType() != ActorType::Enemy)
		{
			targetActor = m_actorManager->GetFirstActorByType(ActorType::Player);
			if (targetActor)
			{
				m_socketOwnerActorID = targetActor->GetID();
				m_socketOwnerTag = targetActor->GetTag();
				m_owner->SetOwnerID(m_socketOwnerActorID);
			}
		}
		if (targetActor == nullptr) return;
	}

	if (m_owner && m_owner->GetOwnerID() != m_socketOwnerActorID)
	{
		m_owner->SetOwnerID(m_socketOwnerActorID);
	}

	HEIN::SocketComponent* targetSocket = targetActor->GetComponent<HEIN::SocketComponent>();

	DirectX::SimpleMath::Matrix socketWorld = DirectX::SimpleMath::Matrix::Identity;
	bool foundSocket = false;

	if (targetSocket != nullptr)
	{
		if (!m_socketName.empty() && targetSocket->HasSocket(m_socketName))
		{
			socketWorld = targetSocket->GetSocketWorldMatrix(m_socketName);
			foundSocket = true;
		}
		else if (targetSocket->HasSocket(L"WeaponSocket"))
		{
			socketWorld = targetSocket->GetSocketWorldMatrix(L"WeaponSocket");
			foundSocket = true;
		}
	}

	if (!foundSocket)
	{
		if (auto* targetTrans = targetActor->GetComponent<HEIN::TransformComponent>())
		{
			socketWorld = targetTrans->GetWorldMatrix();
			foundSocket = true;
		}
	}

	if (foundSocket)
	{
		HEIN::TransformComponent* myTransform = m_owner->GetComponent<HEIN::TransformComponent>();
		if (myTransform != nullptr)
		{
			myTransform->SetParentMatrix(socketWorld);
		}
	}
}

nlohmann::json HEIN::SocketAttachmentComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    std::string narrowTag(m_socketOwnerTag.begin(), m_socketOwnerTag.end());
    std::string narrowName(m_socketName.begin(), m_socketName.end());
    data["OwnerTag"] = narrowTag;
    data["SocketName"] = narrowName;
    return data;
}

void HEIN::SocketAttachmentComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("OwnerTag"))
    {
        std::string narrowTag = data["OwnerTag"];
        m_socketOwnerTag = std::wstring(narrowTag.begin(), narrowTag.end());
    }
    if (data.contains("SocketName"))
    {
        std::string narrowName = data["SocketName"];
        m_socketName = std::wstring(narrowName.begin(), narrowName.end());
    }
}

void HEIN::SocketAttachmentComponent::OnInspectorGUI(GameContext& gameContext)
{
	if (ImGui::CollapsingHeader("SocketAttachmentComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		std::string narrowTag(m_socketOwnerTag.begin(), m_socketOwnerTag.end());
		char bufferTag[256];
		strcpy_s(bufferTag, sizeof(bufferTag), narrowTag.c_str());
		if (ImGui::InputText("Owner Tag", bufferTag, sizeof(bufferTag)))
		{
			std::string newTag(bufferTag);
			m_socketOwnerTag = std::wstring(newTag.begin(), newTag.end());
			m_socketOwnerActorID = HEIN::INVALID_ACTOR_ID;
		}

		std::string narrowName(m_socketName.begin(), m_socketName.end());
		char bufferName[256];
		strcpy_s(bufferName, sizeof(bufferName), narrowName.c_str());
		if (ImGui::InputText("Socket Name", bufferName, sizeof(bufferName)))
		{
			std::string newName(bufferName);
			m_socketName = std::wstring(newName.begin(), newName.end());
		}
	}
}
