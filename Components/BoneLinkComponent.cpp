#include "pch.h"
#include "BoneLinkComponent.h"
#include "SkinnedModelComponent.h"
#include "StaticModelComponent.h"
#include "Entities/Actor.h"
#include "TransformComponent.h"
#include "ColliderComponent/ColliderComponent.h"
#include <DebugingTools/DebugUIManager.h>

HEIN::BoneLinkComponent::BoneLinkComponent(Actor* owner)
	: IComponent(owner)
	, m_targetModel(nullptr)
	, m_targetStaticModel(nullptr)
	, m_targetBoneName(L"")
	, m_targetBoneIndex(-1)
	, m_linkedTransform(nullptr)
	, m_linkedCollider(nullptr)
	, m_linkedPosition(nullptr)
	, m_linkedColliderTag(L"")
{
}

void HEIN::BoneLinkComponent::Initialize(SkinnedModelComponent* targetModel, const std::wstring& targetBoneName)
{
	m_targetModel = targetModel;
	m_targetStaticModel = nullptr;
	m_targetBoneName = targetBoneName;
	m_targetBoneIndex = -1;
}

void HEIN::BoneLinkComponent::Initialize(SkinnedModelComponent* targetModel, int targetBoneIndex)
{
	m_targetModel = targetModel;
	m_targetStaticModel = nullptr;
	m_targetBoneIndex = targetBoneIndex;
	m_targetBoneName = L"";
}

void HEIN::BoneLinkComponent::Initialize(StaticModelComponent* targetModel, const std::wstring& targetBoneName)
{
	m_targetStaticModel = targetModel;
	m_targetModel = nullptr;
	m_targetBoneName = targetBoneName;
	m_targetBoneIndex = -1;
}

void HEIN::BoneLinkComponent::Initialize(StaticModelComponent* targetModel, int targetBoneIndex)
{
	m_targetStaticModel = targetModel;
	m_targetModel = nullptr;
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
	if (collider) m_linkedColliderTag = collider->GetColliderTag();
}

void HEIN::BoneLinkComponent::LinkTo(DirectX::SimpleMath::Vector3* position)
{
	m_linkedPosition = position;
}

void HEIN::BoneLinkComponent::Start()
{
}

void HEIN::BoneLinkComponent::LateUpdate(float deltaTime)
{
	if (m_targetModel == nullptr && m_targetStaticModel == nullptr)
	{
		m_targetModel = m_owner->GetComponent<SkinnedModelComponent>();
		if (m_targetModel == nullptr)
		{
			m_targetStaticModel = m_owner->GetComponent<StaticModelComponent>();
		}
	}

	if (m_targetBoneName != L"" && m_targetBoneIndex == -1)
	{
		if (m_targetModel != nullptr) m_targetBoneIndex = m_targetModel->GetBoneIndex(m_targetBoneName);
		else if (m_targetStaticModel != nullptr) m_targetBoneIndex = m_targetStaticModel->GetBoneIndex(m_targetBoneName);
	}

	if (m_linkedCollider == nullptr && m_linkedColliderTag != L"")
	{
		auto colliders = m_owner->GetComponents<ColliderComponent>();
		for (auto* col : colliders)
		{
			if (col->GetColliderTag() == m_linkedColliderTag)
			{
				m_linkedCollider = col;
				break;
			}
		}
	}

	if ((m_targetModel == nullptr && m_targetStaticModel == nullptr) || m_targetBoneIndex == -1) return;

	Actor* modelOwner = m_targetModel ? m_targetModel->GetOwner() : m_targetStaticModel->GetOwner();
	TransformComponent* ownerTransform = modelOwner ? modelOwner->GetComponent<TransformComponent>() : nullptr;
	if (ownerTransform == nullptr) return;

	DirectX::SimpleMath::Matrix actorWorld = ownerTransform->GetWorldMatrix();
	DirectX::SimpleMath::Matrix boneWorld = m_targetModel ? 
		m_targetModel->GetBoneWorldMatrix(m_targetBoneIndex, actorWorld) :
		m_targetStaticModel->GetBoneWorldMatrix(m_targetBoneIndex, actorWorld);

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



nlohmann::json HEIN::BoneLinkComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    std::string narrowName(m_targetBoneName.begin(), m_targetBoneName.end());
    std::string narrowCol(m_linkedColliderTag.begin(), m_linkedColliderTag.end());
    data["TargetBoneName"] = narrowName;
    data["LinkedColliderTag"] = narrowCol;
    return data;
}

void HEIN::BoneLinkComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("TargetBoneName"))
    {
        std::string narrowName = data["TargetBoneName"];
        m_targetBoneName = std::wstring(narrowName.begin(), narrowName.end());
    }
    if (data.contains("LinkedColliderTag"))
    {
        std::string narrowCol = data["LinkedColliderTag"];
        m_linkedColliderTag = std::wstring(narrowCol.begin(), narrowCol.end());
    }
}

void HEIN::BoneLinkComponent::OnInspectorGUI(GameContext& gameContext)
{
	if (m_targetModel == nullptr && m_targetStaticModel == nullptr)
	{
		m_targetModel = m_owner->GetComponent<SkinnedModelComponent>();
		if (m_targetModel == nullptr)
		{
			m_targetStaticModel = m_owner->GetComponent<StaticModelComponent>();
		}
	}

	if (ImGui::CollapsingHeader("BoneLinkComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		std::string narrowName(m_targetBoneName.begin(), m_targetBoneName.end());
		char buffer[256];
		strcpy_s(buffer, sizeof(buffer), narrowName.c_str());
		if (ImGui::InputText("Target Bone", buffer, sizeof(buffer)))
		{
			std::string newName(buffer);
			m_targetBoneName = std::wstring(newName.begin(), newName.end());
			if (m_targetModel) m_targetBoneIndex = m_targetModel->GetBoneIndex(m_targetBoneName);
			else if (m_targetStaticModel) m_targetBoneIndex = m_targetStaticModel->GetBoneIndex(m_targetBoneName);
		}

		std::string narrowCol(m_linkedColliderTag.begin(), m_linkedColliderTag.end());
		char bufferCol[256];
		strcpy_s(bufferCol, sizeof(bufferCol), narrowCol.c_str());
		if (ImGui::InputText("Linked Collider Tag", bufferCol, sizeof(bufferCol)))
		{
			std::string newName(bufferCol);
			m_linkedColliderTag = std::wstring(newName.begin(), newName.end());
			m_linkedCollider = nullptr;
			auto colliders = m_owner->GetComponents<ColliderComponent>();
			for (auto* col : colliders)
			{
				if (col->GetColliderTag() == m_linkedColliderTag)
				{
					m_linkedCollider = col;
					break;
				}
			}
		}
	}
}

