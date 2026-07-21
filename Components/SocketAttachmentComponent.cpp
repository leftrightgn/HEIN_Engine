#include "pch.h"
#include "SocketAttachmentComponent.h"
#include <Entities/ActorManager.h>
#include <Components/SocketComponent.h>
#include <Components/TransformComponent.h>


HEIN::SocketAttachmentComponent::SocketAttachmentComponent(Actor* owner, ActorManager* manager)
	: IComponent(owner)
	, m_manager(manager)
	, m_targetActorID(HEIN::INVALID_ACTOR_ID)
	, m_socketName(L"")
{
}

void HEIN::SocketAttachmentComponent::Initialize(HEIN::ActorID targetActorID, const std::wstring& socketName)
{
	m_targetActorID = targetActorID;
	m_socketName = socketName;
}

void HEIN::SocketAttachmentComponent::LateUpdate(float deltaTime)
{
	if (m_manager == nullptr || m_targetActorID == HEIN::INVALID_ACTOR_ID) return;

	HEIN::Actor* targetActor = m_manager->GetActor(m_targetActorID);

	if (targetActor == nullptr) return;

	HEIN::SocketComponent* targetSocket = targetActor->GetComponent<HEIN::SocketComponent>();

	if (targetSocket != nullptr && targetSocket->HasSocket(m_socketName))
	{
		DirectX::SimpleMath::Matrix socketWorld = targetSocket->GetSocketWorldMatrix(m_socketName);

		HEIN::TransformComponent* myTransform = m_owner->GetComponent<HEIN::TransformComponent>();

		if (myTransform != nullptr)
		{
			myTransform->SetParentMatrix(socketWorld);
		}
	}

}
