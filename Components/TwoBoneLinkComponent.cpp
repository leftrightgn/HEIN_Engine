#include "pch.h"
#include "SkinnedModelComponent.h"
#include "Entities/Actor.h"
#include "TransformComponent.h"
#include "ColliderComponent/CapsuleColliderComponent.h"
#include "TwoBoneLinkComponent.h"

HEIN::TwoBoneLinkComponent::TwoBoneLinkComponent(Actor* owner)
	: IComponent(owner)
	, m_targetModel(nullptr)
	, m_boneAName(L"")
	, m_boneBName(L"")
	, m_boneAIndex(-1)
	, m_boneBIndex(-1)
	, m_linkedCapsule(nullptr)
{
}

void HEIN::TwoBoneLinkComponent::Initialize(
	SkinnedModelComponent* targetModel,
	const std::wstring& boneA, 
	const std::wstring& boneB
)
{
	m_targetModel = targetModel;
	m_boneAName = boneA;
	m_boneBName = boneB;
}

void HEIN::TwoBoneLinkComponent::LinkTo(CapsuleColliderComponent* capsule)
{
	m_linkedCapsule = capsule;
}

void HEIN::TwoBoneLinkComponent::Start()
{
	if (m_targetModel != nullptr)
	{
		m_boneAIndex = m_targetModel->GetBoneIndex(m_boneAName);
		m_boneBIndex = m_targetModel->GetBoneIndex(m_boneBName);
	}
}

void HEIN::TwoBoneLinkComponent::LateUpdate(float deltaTime)
{
	if (m_targetModel == nullptr || m_boneAIndex == -1 || m_boneBIndex == -1 || m_linkedCapsule == nullptr) return;

	TransformComponent* ownerTransform = m_targetModel->GetOwner()->GetComponent<TransformComponent>();
	if (ownerTransform == nullptr) return;

	DirectX::SimpleMath::Matrix actorWorld = ownerTransform->GetWorldMatrix();

	// 1. Get exact world positions
	DirectX::SimpleMath::Vector3 posA = m_targetModel->GetBoneWorldPosition(m_boneAIndex, actorWorld);
	DirectX::SimpleMath::Vector3 posB = m_targetModel->GetBoneWorldPosition(m_boneBIndex, actorWorld);

	DirectX::SimpleMath::Vector3 center = (posA + posB) * 0.5f;

	DirectX::SimpleMath::Vector3 upDir = posA - posB;
	float worldDistance = upDir.Length();
	if (worldDistance < 0.0001f) return;
	upDir.Normalize();

	DirectX::SimpleMath::Vector3 worldUp = DirectX::SimpleMath::Vector3::Up;
	if (abs(upDir.Dot(worldUp)) > 0.99f) worldUp = DirectX::SimpleMath::Vector3::Forward;

	DirectX::SimpleMath::Vector3 rightDir = worldUp.Cross(upDir);
	rightDir.Normalize();

	DirectX::SimpleMath::Vector3 forwardDir = upDir.Cross(rightDir);
	forwardDir.Normalize();

	
	//  Build a pure rotation matrix (Scale is strictly 1.0)
	DirectX::SimpleMath::Matrix boneMatrix;
	boneMatrix._11 = rightDir.x;   boneMatrix._12 = rightDir.y;   boneMatrix._13 = rightDir.z;   boneMatrix._14 = 0.0f;
	boneMatrix._21 = upDir.x;      boneMatrix._22 = upDir.y;      boneMatrix._23 = upDir.z;      boneMatrix._24 = 0.0f;
	boneMatrix._31 = forwardDir.x; boneMatrix._32 = forwardDir.y; boneMatrix._33 = forwardDir.z; boneMatrix._34 = 0.0f;
	boneMatrix._41 = center.x;     boneMatrix._42 = center.y;     boneMatrix._43 = center.z;     boneMatrix._44 = 1.0f;

	// Push the Matrix
	m_linkedCapsule->SetManualMatrix(boneMatrix);

	// pass the exact world distance!
	m_linkedCapsule->SetHeight(worldDistance);
}
