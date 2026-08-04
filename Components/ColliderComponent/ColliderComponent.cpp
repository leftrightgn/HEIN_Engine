#include "pch.h"
#include "ColliderComponent.h"
#include "Components/TransformComponent.h"
#include "Components/StaticModelComponent.h"
#include "Entities/Actor.h"
#include <DebugingTools/DebugUIManager.h>

HEIN::ColliderComponent::ColliderComponent(
	Actor* owner,
	ColliderShape shape
)
	: IComponent(owner)
	, m_shape(shape)
	, m_offset(DirectX::SimpleMath::Vector3::Zero)
	, m_rotationOffset(DirectX::SimpleMath::Quaternion::Identity)
	, m_isTrigger(false)
{
}

void HEIN::ColliderComponent::Start()
{
	if (GetOwner() != nullptr)
	{
		m_transform = GetOwner()->GetComponent<TransformComponent>();
		// If attached to an actor with a static model, default to Environment layer
		if (m_layer == CollisionLayer::Layer_Default)
		{
			if (GetOwner()->GetComponent<StaticModelComponent>() != nullptr)
			{
				m_layer = CollisionLayer::Layer_Environment;
			}
		}
	}
}


DirectX::SimpleMath::Matrix HEIN::ColliderComponent::CalculateWorldMatrix()
{
	if (m_transform == nullptr && GetOwner() != nullptr)
	{
		m_transform = GetOwner()->GetComponent<TransformComponent>();
	}

	DirectX::SimpleMath::Matrix localOffset =
		DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_rotationOffset) * 
		DirectX::SimpleMath::Matrix::CreateTranslation(m_offset);

	if (m_useManualMatrix)
	{
		return localOffset * m_manualMatrix;
	}

	DirectX::SimpleMath::Matrix finalMatrix = localOffset;

	if (m_transform != nullptr)
	{
		finalMatrix = finalMatrix * m_transform->GetWorldMatrix();
	}

	return finalMatrix;
}

void HEIN::ColliderComponent::OnInspectorGUI(GameContext& gameContext)
{
    ImGui::PushID((void*)this);

    bool isActive = (HEIN::g_ActiveGizmoTarget == this);
    if (ImGui::RadioButton("Edit Collider with Gizmo", isActive))
    {
        HEIN::g_ActiveGizmoTarget = this;
    }

    std::string headerName = "Collider Component";
    if (m_shape == ColliderShape::Sphere) headerName = "Sphere Collider";
    else if (m_shape == ColliderShape::AABB) headerName = "AABB Collider";
    else if (m_shape == ColliderShape::OBB) headerName = "OBB Collider";
    else if (m_shape == ColliderShape::Capsule) headerName = "Capsule Collider";
    else if (m_shape == ColliderShape::Mesh) headerName = "Mesh Collider";

    if (ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Collider Tag
        char tagBuffer[256];
        std::string tagStr(m_colliderTag.begin(), m_colliderTag.end());
        strncpy_s(tagBuffer, tagStr.c_str(), sizeof(tagBuffer));
        if (ImGui::InputText("Collider Tag", tagBuffer, sizeof(tagBuffer)))
        {
            std::string newTagStr(tagBuffer);
            m_colliderTag = std::wstring(newTagStr.begin(), newTagStr.end());
        }

        // Collision Layer Dropdown
        const char* layerNames[] = { "Default (1)", "Environment (2)", "Player (4)", "Enemy (8)", "PlayerWeapon (16)", "EnemyWeapon (32)" };
        uint32_t layerValues[] = {
            CollisionLayer::Layer_Default,
            CollisionLayer::Layer_Environment,
            CollisionLayer::Layer_Player,
            CollisionLayer::Layer_Enemy,
            CollisionLayer::Layer_PlayerWeapon,
            CollisionLayer::Layer_EnemyWeapon
        };
        int currentLayerIdx = 0;
        for (int i = 0; i < 6; ++i)
        {
            if (m_layer == layerValues[i])
            {
                currentLayerIdx = i;
                break;
            }
        }
        if (ImGui::Combo("Collision Layer", &currentLayerIdx, layerNames, 6))
        {
            m_layer = layerValues[currentLayerIdx];
        }

        // Quick Collision Mask Presets / Bit Toggles
        if (ImGui::TreeNode("Collision Mask Settings"))
        {
            auto DrawMaskBit = [&](const char* name, uint32_t bit)
            {
                bool enabled = (m_mask & bit) != 0;
                if (ImGui::Checkbox(name, &enabled))
                {
                    if (enabled) m_mask |= bit;
                    else m_mask &= ~bit;
                }
            };
            DrawMaskBit("Collide with Default", CollisionLayer::Layer_Default);
            DrawMaskBit("Collide with Environment", CollisionLayer::Layer_Environment);
            DrawMaskBit("Collide with Player", CollisionLayer::Layer_Player);
            DrawMaskBit("Collide with Enemy", CollisionLayer::Layer_Enemy);
            DrawMaskBit("Collide with PlayerWeapon", CollisionLayer::Layer_PlayerWeapon);
            DrawMaskBit("Collide with EnemyWeapon", CollisionLayer::Layer_EnemyWeapon);

            if (ImGui::Button("Collide With All")) m_mask = CollisionLayer::Layer_All;
            ImGui::TreePop();
        }

        // Offset
        DirectX::SimpleMath::Vector3 offset = GetOffset();
        if (ImGui::DragFloat3("Offset", &offset.x, 0.05f))
        {
            SetOffset(offset);
        }

        // Rotation Offset
        DirectX::SimpleMath::Vector3 euler = GetRotationOffset();
        euler.x = DirectX::XMConvertToDegrees(euler.x);
        euler.y = DirectX::XMConvertToDegrees(euler.y);
        euler.z = DirectX::XMConvertToDegrees(euler.z);

        if (ImGui::DragFloat3("Rotation Offset", &euler.x, 0.5f))
        {
            float radX = DirectX::XMConvertToRadians(euler.x);
            float radY = DirectX::XMConvertToRadians(euler.y);
            float radZ = DirectX::XMConvertToRadians(euler.z);

            DirectX::SimpleMath::Vector3 newRotEuler(radX, radY, radZ);
            SetRotationOffset(newRotEuler);
        }

        // Trigger
        bool isTrigger = IsTrigger();
        if (ImGui::Checkbox("Is Trigger", &isTrigger))
        {
            SetTrigger(isTrigger);
        }
    }

    ImGui::PopID();
}

void HEIN::ColliderComponent::DrawGizmo(
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

    DirectX::SimpleMath::Matrix worldMat = GetCalculateWorldMatrix();

    // Use the operation/mode passed in from the UI manager
    ImGuizmo::Manipulate((float*)&view.m[0][0], (float*)&proj.m[0][0], (ImGuizmo::OPERATION)operation, (ImGuizmo::MODE)mode, (float*)&worldMat.m[0][0]);

    if (ImGuizmo::IsUsing())
    {
        DirectX::SimpleMath::Matrix parentMat = DirectX::SimpleMath::Matrix::Identity;
        if (m_useManualMatrix)
        {
            parentMat = m_manualMatrix;
        }
        else if (m_transform != nullptr)
        {
            parentMat = m_transform->GetWorldMatrix();
        }

        DirectX::SimpleMath::Matrix localOffset = worldMat * parentMat.Invert();
        
        DirectX::SimpleMath::Vector3 scale, localPos;
        DirectX::SimpleMath::Quaternion localRot;
        if (localOffset.Decompose(scale, localRot, localPos))
        {
            localRot.Normalize();
            SetOffset(localPos);

            // Extract Euler angles similar to GetRotationEuler
            DirectX::SimpleMath::Vector3 euler;
            
            // Extract pitch(x-axis)
            float sinp = 2.0f * (localRot.w * localRot.x - localRot.y * localRot.z);
            if (std::abs(sinp) >= 1.0f)
                euler.x = std::copysign(DirectX::XM_PIDIV2, sinp);
            else
                euler.x = std::asin(sinp);

            // Extract yaw(y-axis)
            float siny_cosp = 2.0f * (localRot.w * localRot.y + localRot.z * localRot.x);
            float cosy_cosp = 1.0f - 2.0f * (localRot.x * localRot.x + localRot.y * localRot.y);
            euler.y = std::atan2(siny_cosp, cosy_cosp);

            // Extract Roll(z-axis)
            float sinr_cosp = 2.0f * (localRot.w * localRot.z + localRot.x * localRot.y);
            float cosr_cosp = 1.0f - 2.0f * (localRot.y * localRot.y + localRot.z * localRot.z);
            euler.z = std::atan2(sinr_cosp, cosr_cosp);

            SetRotationOffset(euler);
        }
    }
}

nlohmann::json HEIN::ColliderComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    std::string tagStr(m_colliderTag.begin(), m_colliderTag.end());
    data["ColliderTag"] = tagStr;
    
    data["OffsetX"] = m_offset.x;
    data["OffsetY"] = m_offset.y;
    data["OffsetZ"] = m_offset.z;

    data["RotX"] = m_rotationOffset.x;
    data["RotY"] = m_rotationOffset.y;
    data["RotZ"] = m_rotationOffset.z;
    data["RotW"] = m_rotationOffset.w;

    data["IsTrigger"] = m_isTrigger;
    data["Layer"] = m_layer;
    data["Mask"] = m_mask;
    return data;
}

void HEIN::ColliderComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("ColliderTag"))
    {
        std::string tagStr = data["ColliderTag"];
        m_colliderTag = std::wstring(tagStr.begin(), tagStr.end());
    }
    
    if (data.contains("OffsetX")) m_offset.x = data["OffsetX"];
    if (data.contains("OffsetY")) m_offset.y = data["OffsetY"];
    if (data.contains("OffsetZ")) m_offset.z = data["OffsetZ"];

    if (data.contains("RotX")) m_rotationOffset.x = data["RotX"];
    if (data.contains("RotY")) m_rotationOffset.y = data["RotY"];
    if (data.contains("RotZ")) m_rotationOffset.z = data["RotZ"];
    if (data.contains("RotW")) m_rotationOffset.w = data["RotW"];

    if (data.contains("IsTrigger")) m_isTrigger = data["IsTrigger"];
    if (data.contains("Layer")) m_layer = data["Layer"];
    if (data.contains("Mask")) m_mask = data["Mask"];
}
