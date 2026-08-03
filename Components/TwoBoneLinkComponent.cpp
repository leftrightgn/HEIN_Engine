#include "pch.h"
#include "SkinnedModelComponent.h"
#include "StaticModelComponent.h"
#include "Entities/Actor.h"
#include "TransformComponent.h"
#include "ColliderComponent/CapsuleColliderComponent.h"
#include "TwoBoneLinkComponent.h"
#include <DebugingTools/DebugUIManager.h>

HEIN::TwoBoneLinkComponent::TwoBoneLinkComponent(Actor* owner)
	: IComponent(owner)
	, m_targetModel(nullptr)
	, m_targetStaticModel(nullptr)
	, m_boneAName(L"")
	, m_boneBName(L"")
	, m_boneAIndex(-1)
	, m_boneBIndex(-1)
	, m_linkedCapsule(nullptr)
	, m_linkedColliderTag(L"")
{
}

void HEIN::TwoBoneLinkComponent::Initialize(
	SkinnedModelComponent* targetModel,
	const std::wstring& boneA, 
	const std::wstring& boneB
)
{
	m_targetModel = targetModel;
	m_targetStaticModel = nullptr;
	m_boneAName = boneA;
	m_boneBName = boneB;
}

void HEIN::TwoBoneLinkComponent::Initialize(
	StaticModelComponent* targetModel,
	const std::wstring& boneA, 
	const std::wstring& boneB
)
{
	m_targetStaticModel = targetModel;
	m_targetModel = nullptr;
	m_boneAName = boneA;
	m_boneBName = boneB;
}

void HEIN::TwoBoneLinkComponent::LinkTo(CapsuleColliderComponent* capsule)
{
	m_linkedCapsule = capsule;
	if (capsule) m_linkedColliderTag = capsule->GetColliderTag();
}

void HEIN::TwoBoneLinkComponent::Start()
{
}

void HEIN::TwoBoneLinkComponent::LateUpdate(float deltaTime)
{
	if (m_targetModel == nullptr && m_targetStaticModel == nullptr)
	{
		m_targetModel = m_owner->GetComponent<SkinnedModelComponent>();
		if (m_targetModel == nullptr)
		{
			m_targetStaticModel = m_owner->GetComponent<StaticModelComponent>();
		}
	}

	if (m_boneAName != L"" && m_boneAIndex == -1)
	{
		if (m_targetModel != nullptr) m_boneAIndex = m_targetModel->GetBoneIndex(m_boneAName);
		else if (m_targetStaticModel != nullptr) m_boneAIndex = m_targetStaticModel->GetBoneIndex(m_boneAName);
	}
	if (m_boneBName != L"" && m_boneBIndex == -1)
	{
		if (m_targetModel != nullptr) m_boneBIndex = m_targetModel->GetBoneIndex(m_boneBName);
		else if (m_targetStaticModel != nullptr) m_boneBIndex = m_targetStaticModel->GetBoneIndex(m_boneBName);
	}

	if (m_linkedCapsule == nullptr && m_linkedColliderTag != L"")
	{
		auto colliders = m_owner->GetComponents<CapsuleColliderComponent>();
		for (auto* col : colliders)
		{
			if (col->GetColliderTag() == m_linkedColliderTag)
			{
				m_linkedCapsule = col;
				break;
			}
		}
	}

	if ((m_targetModel == nullptr && m_targetStaticModel == nullptr) || m_boneAIndex == -1 || m_boneBIndex == -1 || m_linkedCapsule == nullptr) return;

	Actor* modelOwner = m_targetModel ? m_targetModel->GetOwner() : m_targetStaticModel->GetOwner();
	TransformComponent* ownerTransform = modelOwner ? modelOwner->GetComponent<TransformComponent>() : nullptr;
	if (ownerTransform == nullptr) return;

	DirectX::SimpleMath::Matrix actorWorld = ownerTransform->GetWorldMatrix();

	// 1. Get exact world positions
	DirectX::SimpleMath::Vector3 posA = m_targetModel ? 
		m_targetModel->GetBoneWorldPosition(m_boneAIndex, actorWorld) : 
		m_targetStaticModel->GetBoneWorldPosition(m_boneAIndex, actorWorld);
	DirectX::SimpleMath::Vector3 posB = m_targetModel ? 
		m_targetModel->GetBoneWorldPosition(m_boneBIndex, actorWorld) : 
		m_targetStaticModel->GetBoneWorldPosition(m_boneBIndex, actorWorld);

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

nlohmann::json HEIN::TwoBoneLinkComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    std::string narrowA(m_boneAName.begin(), m_boneAName.end());
    std::string narrowB(m_boneBName.begin(), m_boneBName.end());
    std::string narrowCol(m_linkedColliderTag.begin(), m_linkedColliderTag.end());
    data["BoneAName"] = narrowA;
    data["BoneBName"] = narrowB;
    data["LinkedColliderTag"] = narrowCol;
    return data;
}

void HEIN::TwoBoneLinkComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("BoneAName"))
    {
        std::string narrow = data["BoneAName"];
        m_boneAName = std::wstring(narrow.begin(), narrow.end());
    }
    if (data.contains("BoneBName"))
    {
        std::string narrow = data["BoneBName"];
        m_boneBName = std::wstring(narrow.begin(), narrow.end());
    }
    if (data.contains("LinkedColliderTag"))
    {
        std::string narrow = data["LinkedColliderTag"];
        m_linkedColliderTag = std::wstring(narrow.begin(), narrow.end());
    }
}

void HEIN::TwoBoneLinkComponent::OnInspectorGUI(GameContext& gameContext)
{
	if (m_targetModel == nullptr && m_targetStaticModel == nullptr)
	{
		m_targetModel = m_owner->GetComponent<SkinnedModelComponent>();
		if (m_targetModel == nullptr)
		{
			m_targetStaticModel = m_owner->GetComponent<StaticModelComponent>();
		}
	}

	if (ImGui::CollapsingHeader("TwoBoneLinkComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		std::string narrowA(m_boneAName.begin(), m_boneAName.end());
		char bufferA[256];
		strcpy_s(bufferA, sizeof(bufferA), narrowA.c_str());
		if (ImGui::InputText("Bone A", bufferA, sizeof(bufferA)))
		{
			std::string newName(bufferA);
			m_boneAName = std::wstring(newName.begin(), newName.end());
			if (m_targetModel) m_boneAIndex = m_targetModel->GetBoneIndex(m_boneAName);
			else if (m_targetStaticModel) m_boneAIndex = m_targetStaticModel->GetBoneIndex(m_boneAName);
		}

		std::string narrowB(m_boneBName.begin(), m_boneBName.end());
		char bufferB[256];
		strcpy_s(bufferB, sizeof(bufferB), narrowB.c_str());
		if (ImGui::InputText("Bone B", bufferB, sizeof(bufferB)))
		{
			std::string newName(bufferB);
			m_boneBName = std::wstring(newName.begin(), newName.end());
			if (m_targetModel) m_boneBIndex = m_targetModel->GetBoneIndex(m_boneBName);
			else if (m_targetStaticModel) m_boneBIndex = m_targetStaticModel->GetBoneIndex(m_boneBName);
		}

		std::string narrowCol(m_linkedColliderTag.begin(), m_linkedColliderTag.end());
		char bufferCol[256];
		strcpy_s(bufferCol, sizeof(bufferCol), narrowCol.c_str());
		if (ImGui::InputText("Linked Collider Tag", bufferCol, sizeof(bufferCol)))
		{
			std::string newName(bufferCol);
			m_linkedColliderTag = std::wstring(newName.begin(), newName.end());
			m_linkedCapsule = nullptr;
			auto colliders = m_owner->GetComponents<CapsuleColliderComponent>();
			for (auto* col : colliders)
			{
				if (col->GetColliderTag() == m_linkedColliderTag)
				{
					m_linkedCapsule = col;
					break;
				}
			}
		}
	}
}

