#include "pch.h"
#include "TransformComponent.h"
//#include <ImGui/imgui.h>
//#include <ImGui/ImGuizmo.h>
#include <DebugingTools/DebugUIManager.h>

HEIN::TransformComponent::TransformComponent(Actor* owner)
    : IComponent(owner)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_rotation(DirectX::SimpleMath::Quaternion::Identity)
    , m_scale(1.0f, 1.0f, 1.0f)
    , m_parentMatrix(DirectX::SimpleMath::Matrix::Identity)
{
}

void HEIN::TransformComponent::OnInspectorGUI()
{
    bool isActive = (HEIN::g_ActiveGizmoTarget == this);
    if (ImGui::RadioButton("Edit Transform with Gizmo", isActive))
    {
        HEIN::g_ActiveGizmoTarget = this; // Point the global tracker to THIS specific component!
    }
    if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Position
        DirectX::SimpleMath::Vector3 pos = GetPosition();
        if (ImGui::DragFloat3("Position", &pos.x, 0.05f))
        {
            SetPosition(pos);
        }

        // Scale
        DirectX::SimpleMath::Vector3 scale = GetScale();
        if (ImGui::DragFloat3("Scale", &scale.x, 0.05f))
        {
            SetScale(scale);
        }

        // Rotation
        DirectX::SimpleMath::Vector3 euler = GetRotationEuler();
        euler.x = DirectX::XMConvertToDegrees(euler.x);
        euler.y = DirectX::XMConvertToDegrees(euler.y);
        euler.z = DirectX::XMConvertToDegrees(euler.z);

        if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f))
        {
            float radX = DirectX::XMConvertToRadians(euler.x);
            float radY = DirectX::XMConvertToRadians(euler.y);
            float radZ = DirectX::XMConvertToRadians(euler.z);

            DirectX::SimpleMath::Quaternion newRot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(radY, radX, radZ);
            SetRotation(newRot);
        }
    }
}

void HEIN::TransformComponent::DrawGizmo(
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

    DirectX::SimpleMath::Matrix worldMat = GetWorldMatrix();

    // Use the operation/mode passed in from the UI manager
    ImGuizmo::Manipulate((float*)&view.m[0][0], (float*)&proj.m[0][0], (ImGuizmo::OPERATION)operation, (ImGuizmo::MODE)mode, (float*)&worldMat.m[0][0]);

    if (ImGuizmo::IsUsing())
    {
        DirectX::SimpleMath::Vector3 scale, pos;
        DirectX::SimpleMath::Quaternion rot;
        if (worldMat.Decompose(scale, rot, pos))
        {
            rot.Normalize();
            SetPosition(pos);
            SetRotation(rot);
            SetScale(scale);
        }
    }
}

void HEIN::TransformComponent::SetRotationEuler(const DirectX::SimpleMath::Vector3& eulerAngles)
{
    m_rotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(eulerAngles.y, eulerAngles.x, eulerAngles.z);
    m_rotation.Normalize();
}

DirectX::SimpleMath::Vector3 HEIN::TransformComponent::GetRotationEuler() const
{
   DirectX::SimpleMath::Vector3 euler;

   // Extract pitch(x-axis)
   //x=arcsin(2(𝑤⋅𝑦−𝑧⋅𝑥) 
   float sinp = 2.0f * (m_rotation.w * m_rotation.x - m_rotation.y * m_rotation.z);
   if (std::abs(sinp) >= 1.0f)
   {
       euler.x = std::copysign(DirectX::XM_PIDIV2, sinp);
   }
   else
   {
       euler.x = std::asin(sinp);
   }

   // Extract yaw(y-axis)
   //y=atan2(2(w*x+y*z),1-2(x*x+y*y)
   float siny_cosp = 2.0f * (m_rotation.w * m_rotation.y + m_rotation.z * m_rotation.x);
   float cosy_cosp = 1.0f - 2.0f * (m_rotation.x * m_rotation.x + m_rotation.y * m_rotation.y);
   euler.y = std::atan2(siny_cosp, cosy_cosp);

   // Extract Roll(z-axis)
   //z=atan2(2(w*z+x*y),1-2(y*y+z*z))
   float sinr_cosp = 2.0f * (m_rotation.w * m_rotation.z + m_rotation.x * m_rotation.y);
   float cosr_cosp = 1.0f - 2.0f * (m_rotation.y * m_rotation.y + m_rotation.z * m_rotation.z);
   euler.z = std::atan2(sinr_cosp, cosr_cosp);

   return euler;
}

nlohmann::json HEIN::TransformComponent::Serialize()
{
    nlohmann::json data;
    data["Position"] = nlohmann::json::array({ m_position.x, m_position.y, m_position.z });
    data["Scale"] = nlohmann::json::array({ m_scale.x, m_scale.y, m_scale.z });

    data["Rotation"] = nlohmann::json::array({ m_rotation.x, m_rotation.y, m_rotation.z, m_rotation.w });

    return data;
}

void HEIN::TransformComponent::Deserialize(const nlohmann::json& data)
{
    if (data.contains("Position"))
    {
        SetPosition(DirectX::SimpleMath::Vector3(data["Position"][0], data["Position"][1], data["Position"][2]));
    }
    if (data.contains("Scale"))
    {
        SetScale(DirectX::SimpleMath::Vector3(data["Scale"][0], data["Scale"][1], data["Scale"][2]));
    }
    if (data.contains("Rotation")) { m_rotation.x = data["Rotation"][0]; m_rotation.y = data["Rotation"][1]; m_rotation.z = data["Rotation"][2]; m_rotation.w = data["Rotation"][3]; m_rotation.Normalize(); } else if (data.contains("RotationEuler")) { SetRotationEuler(DirectX::SimpleMath::Vector3(data["RotationEuler"][0], data["RotationEuler"][1], data["RotationEuler"][2])); }
}

DirectX::SimpleMath::Matrix HEIN::TransformComponent::GetWorldMatrix() const
{
    // Ensure strictly normalized before passing to CreateFromQuaternion!
    DirectX::SimpleMath::Quaternion normRot = m_rotation;
    normRot.Normalize();

    // the order of matrix multiplication is Scale * Rotation * Translation
    // CreateFromYawPitchRoll takes (Y, X, Z) 
    return  DirectX::SimpleMath::Matrix::CreateScale(m_scale) *
        DirectX::SimpleMath::Matrix::CreateFromQuaternion(normRot) *
        DirectX::SimpleMath::Matrix::CreateTranslation(m_position) *
        m_parentMatrix;
}

void HEIN::TransformComponent::Update(float /*deltaTime*/)
{
}
