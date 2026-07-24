#include "pch.h"
#include "SocketComponent.h"
#include "Components/SkinnedModelComponent.h"
#include "Components/TransformComponent.h"
#include "Entities/Actor.h"
#include <DebugingTools/DebugUIManager.h>

HEIN::SocketComponent::SocketComponent(Actor* owner)
    : IComponent(owner)
    , m_model(nullptr)
    , m_transform(nullptr)
{
}

void HEIN::SocketComponent::Start()
{
    m_model = m_owner->GetComponent<SkinnedModelComponent>();
    m_transform = m_owner->GetComponent<TransformComponent>();
}

void HEIN::SocketComponent::UpdateSocketOffset(
    const std::wstring& socketName,
    const DirectX::SimpleMath::Vector3& newPos,
    const DirectX::SimpleMath::Vector3& newRot
)
{
    if (HasSocket(socketName))
    {
        m_sockets[socketName].localPosition = newPos;
        m_sockets[socketName].localRotation = newRot;
    }
}

void HEIN::SocketComponent::OnInspectorGUI()
{
    bool isActive = (HEIN::g_ActiveGizmoTarget == this);
    if (ImGui::RadioButton("Edit socket with Gizmo", isActive))
    {
        HEIN::g_ActiveGizmoTarget = this;
    }
    if (ImGui::CollapsingHeader("Socket Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Loop through all the sockets attached to this character
        for (auto& pair : m_sockets)
        {
            Socket& socket = pair.second;

            // Convert the wstring name to a normal string for ImGui
            std::string narrowName(socket.name.begin(), socket.name.end());
            ImGui::Text("Socket: %s", narrowName.c_str());

            // Push a unique ID so ImGui doesn't get confused if you have multiple sockets
            ImGui::PushID(narrowName.c_str());

            // Edit the Local Position offset
            ImGui::DragFloat3("Position Offset", &socket.localPosition.x, 0.05f);

            // Edit the Local Rotation offset (Convert Radians to Degrees for the UI)
            DirectX::SimpleMath::Vector3 degreesRot;
            degreesRot.x = DirectX::XMConvertToDegrees(socket.localRotation.x);
            degreesRot.y = DirectX::XMConvertToDegrees(socket.localRotation.y);
            degreesRot.z = DirectX::XMConvertToDegrees(socket.localRotation.z);

            if (ImGui::DragFloat3("Rotation Offset", &degreesRot.x, 0.5f))
            {
                // Convert back to Radians when saving
                socket.localRotation.x = DirectX::XMConvertToRadians(degreesRot.x);
                socket.localRotation.y = DirectX::XMConvertToRadians(degreesRot.y);
                socket.localRotation.z = DirectX::XMConvertToRadians(degreesRot.z);
            }

            ImGui::PopID();
            ImGui::Separator();
        }
    }
}

void HEIN::SocketComponent::DrawGizmo(
    const DirectX::SimpleMath::Matrix& view,
    const DirectX::SimpleMath::Matrix& proj,
    int operation,
    int mode
)
{
    if (m_model == nullptr || m_transform == nullptr) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.2f);

    DirectX::SimpleMath::Matrix ownerWorld = m_transform->GetWorldMatrix();

    // Since a character can have multiple sockets, we loop through all of them
    int gizmoId = 0;
    for (auto& pair : m_sockets)
    {
        Socket& socket = pair.second;

        // Get the base Bone matrix without the socket offset
        DirectX::SimpleMath::Matrix boneWorld = m_model->GetBoneWorldMatrix(socket.boneName.c_str(), ownerWorld);

        DirectX::SimpleMath::Vector3 extractedScale;
        DirectX::SimpleMath::Quaternion extractedRotation;
        DirectX::SimpleMath::Vector3 extractedTranslation;

        if (boneWorld.Decompose(extractedScale, extractedRotation, extractedTranslation))
        {
            extractedRotation.Normalize();

            // Strip out the scaling to prevent the socket math from warping
            DirectX::SimpleMath::Matrix cleanBoneMatrix =
                DirectX::SimpleMath::Matrix::CreateFromQuaternion(extractedRotation) *
                DirectX::SimpleMath::Matrix::CreateTranslation(extractedTranslation);

            // Build the Socket's current Local Offset Matrix
            DirectX::SimpleMath::Matrix offsetMatrix =
                DirectX::SimpleMath::Matrix::CreateFromYawPitchRoll(socket.localRotation.y, socket.localRotation.x, socket.localRotation.z) *
                DirectX::SimpleMath::Matrix::CreateTranslation(socket.localPosition);

            // Combine them to get the Socket's true World position for the Gizmo
            DirectX::SimpleMath::Matrix socketWorld = offsetMatrix * cleanBoneMatrix;

            // Push a unique ID so ImGuizmo doesn't get confused if you have multiple sockets
            ImGuizmo::SetID(gizmoId++);

            ImGuizmo::Manipulate(
                (float*)&view.m[0][0],
                (float*)&proj.m[0][0],
                (ImGuizmo::OPERATION)operation,
                (ImGuizmo::MODE)mode,
                (float*)&socketWorld.m[0][0]
            );

            // Apply dragged changes back to the Socket's LOCAL variables
            if (ImGuizmo::IsUsing())
            {
                // Detach the socket from the bone mathematically
                DirectX::SimpleMath::Matrix newLocal = socketWorld * cleanBoneMatrix.Invert();

                DirectX::SimpleMath::Vector3 scale, pos;
                DirectX::SimpleMath::Quaternion rot;

                if (newLocal.Decompose(scale, rot, pos))
                {
                    rot.Normalize();
                    // Save the new position
                    socket.localPosition = pos;

                    // Extract and save the new Euler angles
                    float sinp = 2.0f * (rot.w * rot.x - rot.y * rot.z);
                    if (std::abs(sinp) >= 1.0f) socket.localRotation.x = std::copysign(DirectX::XM_PIDIV2, sinp);
                    else socket.localRotation.x = std::asin(sinp);

                    float siny_cosp = 2.0f * (rot.w * rot.y + rot.z * rot.x);
                    float cosy_cosp = 1.0f - 2.0f * (rot.x * rot.x + rot.y * rot.y);
                    socket.localRotation.y = std::atan2(siny_cosp, cosy_cosp);

                    float sinr_cosp = 2.0f * (rot.w * rot.z + rot.x * rot.y);
                    float cosr_cosp = 1.0f - 2.0f * (rot.y * rot.y + rot.z * rot.z);
                    socket.localRotation.z = std::atan2(sinr_cosp, cosr_cosp);
                }
            }
        }
    }
}


void HEIN::SocketComponent::AddSocket(const Socket& socket)
{
    m_sockets[socket.name] = socket;
}

bool HEIN::SocketComponent::HasSocket(const std::wstring& socketName) const
{
    return m_sockets.find(socketName) != m_sockets.end();
}

HEIN::Socket* HEIN::SocketComponent::GetSocket(const std::wstring& socketName)
{
    if (HasSocket(socketName))
    {
        return &m_sockets[socketName];
    }
    return nullptr;
}

DirectX::SimpleMath::Matrix HEIN::SocketComponent::GetSocketWorldMatrix(const std::wstring& socketName)
{

    if (!HasSocket(socketName) || m_model == nullptr || m_transform == nullptr)
    {
        if (m_transform != nullptr)
        {
            return m_transform->GetWorldMatrix();
        }
        return DirectX::SimpleMath::Matrix::Identity;
    }

    const Socket& socket = m_sockets[socketName];

    DirectX::SimpleMath::Matrix ownerWorld = m_transform->GetWorldMatrix();
    DirectX::SimpleMath::Matrix boneWorld = m_model->GetBoneWorldMatrix(socket.boneName.c_str(), ownerWorld);

    DirectX::SimpleMath::Vector3 extractedScale;
    DirectX::SimpleMath::Quaternion extractedRotation;
    DirectX::SimpleMath::Vector3 extractedTranslation;

    if (boneWorld.Decompose(extractedScale, extractedRotation, extractedTranslation))
    {
        DirectX::SimpleMath::Matrix offsetMatrix =
            DirectX::SimpleMath::Matrix::CreateFromYawPitchRoll(socket.localRotation.y, socket.localRotation.x, socket.localRotation.z) *
            DirectX::SimpleMath::Matrix::CreateTranslation(socket.localPosition);

        extractedRotation.Normalize();

        DirectX::SimpleMath::Matrix cleanBoneMatrix =
            DirectX::SimpleMath::Matrix::CreateFromQuaternion(extractedRotation) *
            DirectX::SimpleMath::Matrix::CreateTranslation(extractedTranslation);

        return offsetMatrix * cleanBoneMatrix;
    }

    return ownerWorld;
}
nlohmann::json HEIN::SocketComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    nlohmann::json socketsArr = nlohmann::json::array();
    for (const auto& pair : m_sockets)
    {
        nlohmann::json sData;
        std::string nStr(pair.second.name.begin(), pair.second.name.end());
        std::string bStr(pair.second.boneName.begin(), pair.second.boneName.end());
        sData["Name"] = nStr;
        sData["BoneName"] = bStr;
        sData["PosX"] = pair.second.localPosition.x;
        sData["PosY"] = pair.second.localPosition.y;
        sData["PosZ"] = pair.second.localPosition.z;
        sData["RotX"] = pair.second.localRotation.x;
        sData["RotY"] = pair.second.localRotation.y;
        sData["RotZ"] = pair.second.localRotation.z;
        socketsArr.push_back(sData);
    }
    data["Sockets"] = socketsArr;
    return data;
}

void HEIN::SocketComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("Sockets"))
    {
        for (const auto& sData : data["Sockets"])
        {
            Socket s;
            if (sData.contains("Name"))
            {
                std::string nStr = sData["Name"];
                s.name = std::wstring(nStr.begin(), nStr.end());
            }
            if (sData.contains("BoneName"))
            {
                std::string bStr = sData["BoneName"];
                s.boneName = std::wstring(bStr.begin(), bStr.end());
            }
            if (sData.contains("PosX")) s.localPosition.x = sData["PosX"];
            if (sData.contains("PosY")) s.localPosition.y = sData["PosY"];
            if (sData.contains("PosZ")) s.localPosition.z = sData["PosZ"];
            if (sData.contains("RotX")) s.localRotation.x = sData["RotX"];
            if (sData.contains("RotY")) s.localRotation.y = sData["RotY"];
            if (sData.contains("RotZ")) s.localRotation.z = sData["RotZ"];
            m_sockets[s.name] = s;
        }
    }
}
