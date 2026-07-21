#include "pch.h"
#include "ColliderComponent.h"
#include "Components/TransformComponent.h"
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
	}
}


DirectX::SimpleMath::Matrix HEIN::ColliderComponent::CalculateWorldMatrix()
{
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

void HEIN::ColliderComponent::OnInspectorGUI()
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
