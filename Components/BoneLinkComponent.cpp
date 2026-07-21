#include "pch.h"
#include "BoneLinkComponent.h"
#include "SkinnedModelComponent.h"
#include "Entities/Actor.h"
#include "TransformComponent.h"
#include "ColliderComponent/ColliderComponent.h"

HEIN::BoneLinkComponent::BoneLinkComponent(Actor* owner)
	: IComponent(owner)
	, m_targetModel(nullptr)
	, m_targetBoneName(L"")
	, m_targetBoneIndex(-1)
	, m_linkedTransform(nullptr)
	, m_linkedCollider(nullptr)
	, m_linkedPosition(nullptr)
{
}

void HEIN::BoneLinkComponent::Initialize(SkinnedModelComponent* targetModel, const std::wstring& targetBoneName)
{
	m_targetModel = targetModel;
	m_targetBoneName = targetBoneName;
	m_targetBoneIndex = -1;
}

void HEIN::BoneLinkComponent::Initialize(SkinnedModelComponent* targetModel, int targetBoneIndex)
{
	m_targetModel = targetModel;
	m_targetBoneIndex = targetBoneIndex;
	m_targetBoneName = L"";
}

void HEIN::BoneLinkComponent::LinkTo(TransformComponent* transform)
{
	m_linkedTransform = transform;
}

void HEIN::BoneLinkComponent::LinkTo(ColliderComponent* collider)
{
	m_linkedCollider = collider;
}

void HEIN::BoneLinkComponent::LinkTo(DirectX::SimpleMath::Vector3* position)
{
	m_linkedPosition = position;
}

void HEIN::BoneLinkComponent::Start()
{
	if (m_targetModel != nullptr && m_targetBoneName != L"" && m_targetBoneIndex == -1)
	{
		m_targetBoneIndex = m_targetModel->GetBoneIndex(m_targetBoneName);
	}
}

void HEIN::BoneLinkComponent::LateUpdate(float deltaTime)
{
	if (m_targetModel == nullptr || m_targetBoneIndex == -1) return;

	TransformComponent* ownerTransform = m_targetModel->GetOwner()->GetComponent<TransformComponent>();
	if (ownerTransform == nullptr) return;

	DirectX::SimpleMath::Matrix actorWorld = ownerTransform->GetWorldMatrix();
	DirectX::SimpleMath::Matrix boneWorld = m_targetModel->GetBoneWorldMatrix(m_targetBoneIndex, actorWorld);

	if (m_linkedTransform != nullptr)
	{
		m_linkedTransform->SetParentMatrix(boneWorld);
	}

	if (m_linkedCollider != nullptr)
	{
		m_linkedCollider->SetManualMatrix(boneWorld);
	}

	if (m_linkedPosition != nullptr)
	{
		*m_linkedPosition = boneWorld.Translation();
	}
}


