#include "pch.h"
#include "FootIKComponent.h"
#include "Components/SkinnedModelComponent.h"
#include "Components/ColliderComponent/TerrainColliderComponent.h"
#include "Components/TransformComponent.h"
#include "Entities/ActorManager.h"
#include <ImGui/imgui.h>
#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace HEIN
{
    FootIKComponent::FootIKComponent(Actor* owner, ActorManager* manager) : IComponent(owner), m_actorManager(manager)
    {
        // Set fallback defaults if Initialize is not explicitly called
        Initialize();
    }

    void FootIKComponent::Initialize(
        const std::wstring& lThigh, const std::wstring& lCalf, const std::wstring& lFoot,
        const std::wstring& rThigh, const std::wstring& rCalf, const std::wstring& rFoot)
    {
        m_leftThighName = lThigh;
        m_leftCalfName = lCalf;
        m_leftFootName = lFoot;
        m_rightThighName = rThigh;
        m_rightCalfName = rCalf;
        m_rightFootName = rFoot;
    }

    nlohmann::json FootIKComponent::Serialize()
    {
        nlohmann::json data = IComponent::Serialize();
        data["FootOffset"] = m_footOffset;
        data["IKBlendWeight"] = m_ikBlendWeight;

        data["LThigh"] = std::string(m_leftThighName.begin(), m_leftThighName.end());
        data["LCalf"] = std::string(m_leftCalfName.begin(), m_leftCalfName.end());
        data["LFoot"] = std::string(m_leftFootName.begin(), m_leftFootName.end());
        data["RThigh"] = std::string(m_rightThighName.begin(), m_rightThighName.end());
        data["RCalf"] = std::string(m_rightCalfName.begin(), m_rightCalfName.end());
        data["RFoot"] = std::string(m_rightFootName.begin(), m_rightFootName.end());

        return data;
    }

    void FootIKComponent::Deserialize(const nlohmann::json& data)
    {
        IComponent::Deserialize(data);
        if (data.contains("FootOffset")) m_footOffset = data["FootOffset"];
        if (data.contains("IKBlendWeight")) m_ikBlendWeight = data["IKBlendWeight"];

        auto parseString = [](const nlohmann::json& d, const char* key, std::wstring& out) {
            if (d.contains(key)) {
                std::string str = d[key];
                out = std::wstring(str.begin(), str.end());
            }
            };

        parseString(data, "LThigh", m_leftThighName);
        parseString(data, "LCalf", m_leftCalfName);
        parseString(data, "LFoot", m_leftFootName);
        parseString(data, "RThigh", m_rightThighName);
        parseString(data, "RCalf", m_rightCalfName);
        parseString(data, "RFoot", m_rightFootName);
    }

    void FootIKComponent::OnInspectorGUI(GameContext& gameContext)
    {
        if (ImGui::CollapsingHeader("Foot IK Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("IK Blend Weight", &m_ikBlendWeight, 0.0f, 1.0f);
            ImGui::DragFloat("Foot Offset", &m_footOffset, 0.01f, -1.0f, 1.0f);

            ImGui::Separator();

            auto drawBoneInput = [](const char* label, std::wstring& boneName) {
                std::string narrowName(boneName.begin(), boneName.end());
                char buffer[256];
                strcpy_s(buffer, sizeof(buffer), narrowName.c_str());
                if (ImGui::InputText(label, buffer, sizeof(buffer))) {
                    std::string newName(buffer);
                    boneName = std::wstring(newName.begin(), newName.end());
                }
                };

            ImGui::Text("Left Leg Bones");
            drawBoneInput("L Thigh", m_leftThighName);
            drawBoneInput("L Calf", m_leftCalfName);
            drawBoneInput("L Foot", m_leftFootName);

            ImGui::Separator();

            ImGui::Text("Right Leg Bones");
            drawBoneInput("R Thigh", m_rightThighName);
            drawBoneInput("R Calf", m_rightCalfName);
            drawBoneInput("R Foot", m_rightFootName);

            ImGui::Separator();

            if (m_terrain)
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Terrain Linked Automatically");
            else
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Terrain NOT Linked!");
        }
    }

    void FootIKComponent::Start()
    {
        m_skinnedModel = m_owner->GetComponent<SkinnedModelComponent>();
    }

    void FootIKComponent::LateUpdate(float deltaTime)
    {
        if (!m_terrain && m_actorManager)
        {
            for (auto& pair : m_actorManager->GetAllActors())
            {
                if (auto terrain = pair.second->GetComponent<TerrainColliderComponent>())
                {
                    m_terrain = terrain;
                    break;
                }
            }
        }

        if (!m_skinnedModel || !m_terrain || m_ikBlendWeight <= 0.01f) return;

        const Matrix* currentLocalBones = m_skinnedModel->GetCurrentLocalBones();
        if (!currentLocalBones) return;

        size_t boneCount = m_skinnedModel->GetBoneCount();
        std::vector<Matrix> modifiedBones(currentLocalBones, currentLocalBones + boneCount);

        TransformComponent* transform = m_owner->GetComponent<TransformComponent>();
        if (!transform) return;

        Matrix worldMatrix = transform->GetWorldMatrix();

        int lThigh = m_skinnedModel->GetBoneIndex(m_leftThighName);
        int lCalf = m_skinnedModel->GetBoneIndex(m_leftCalfName);
        int lFoot = m_skinnedModel->GetBoneIndex(m_leftFootName);

        int rThigh = m_skinnedModel->GetBoneIndex(m_rightThighName);
        int rCalf = m_skinnedModel->GetBoneIndex(m_rightCalfName);
        int rFoot = m_skinnedModel->GetBoneIndex(m_rightFootName);

        if (lThigh != -1 && lCalf != -1 && lFoot != -1)
            SolveTwoBoneIK(modifiedBones.data(), lThigh, lCalf, lFoot, worldMatrix);

        if (rThigh != -1 && rCalf != -1 && rFoot != -1)
            SolveTwoBoneIK(modifiedBones.data(), rThigh, rCalf, rFoot, worldMatrix);

        m_skinnedModel->OverrideBones(modifiedBones.data());
    }

    void FootIKComponent::SolveTwoBoneIK(
        Matrix* localBones,
        int thighIdx, int calfIdx, int footIdx,
        const Matrix& worldMatrix)
    {
        int p1 = m_skinnedModel->GetParentBoneIndex(thighIdx);
        int p2 = m_skinnedModel->GetParentBoneIndex(p1);
        int p3 = m_skinnedModel->GetParentBoneIndex(p2);

        Matrix hipToWorld = Matrix::Identity;
        int curr = m_skinnedModel->GetParentBoneIndex(thighIdx);
        while (curr != -1)
        {
            hipToWorld = hipToWorld * localBones[curr];
            curr = m_skinnedModel->GetParentBoneIndex(curr);
        }
        hipToWorld = hipToWorld * worldMatrix;

        Matrix thighWorld = localBones[thighIdx] * hipToWorld;
        Matrix calfWorld = localBones[calfIdx] * thighWorld;
        Matrix footWorld = localBones[footIdx] * calfWorld;

        Vector3 hipPos = thighWorld.Translation();
        Vector3 kneePos = calfWorld.Translation();
        Vector3 footPos = footWorld.Translation();

        float l1 = Vector3::Distance(hipPos, kneePos);
        float l2 = Vector3::Distance(kneePos, footPos);

        if (l1 < 0.001f || l2 < 0.001f) return;

        float h = 0.0f;
        Vector3 normal;
        bool hit = m_terrain->GetHeightAtPosition(footPos.x, footPos.z, h, normal);

        if (!hit) return;

        float rootY = worldMatrix.Translation().y;
        float targetY = h + (footPos.y - rootY);
        
        // If the character is in the air (jumping/falling) or over a cliff,
        // and the foot is too high above the ground, skip IK to keep the animated pose.
        if ((footPos.y - targetY) > 25.0f) return;

        Vector3 targetPos = footPos;
        targetPos.y = targetY;

        Vector3 hipToTarget = targetPos - hipPos;
        float d = hipToTarget.Length();

        if (d < 0.001f) return;

        d = std::clamp(d, 0.01f, l1 + l2 - 0.001f);
        hipToTarget.Normalize();

        Vector3 hipToFoot = footPos - hipPos;
        if (hipToFoot.LengthSquared() < 0.001f) return;
        hipToFoot.Normalize();

        Quaternion alignRot = Quaternion::Slerp(Quaternion::Identity, Quaternion::FromToRotation(hipToFoot, hipToTarget), m_ikBlendWeight);
        Matrix alignMatrix = Matrix::CreateFromQuaternion(alignRot);

        Vector3 hipToKnee = kneePos - hipPos;
        if (hipToKnee.LengthSquared() < 0.001f) return;
        
        Vector3 alignedHipToKnee = Vector3::TransformNormal(hipToKnee, alignMatrix);
        alignedHipToKnee.Normalize();
        hipToKnee.Normalize();

        Vector3 bendAxis = hipToTarget.Cross(alignedHipToKnee);
        if (bendAxis.LengthSquared() < 0.001f)
        {
            bendAxis = worldMatrix.Right();
            if (bendAxis.LengthSquared() < 0.0001f) bendAxis = Vector3::Right;
        }

        bendAxis.Normalize();

        float cosHip = std::clamp((l1 * l1 + d * d - l2 * l2) / (2.0f * l1 * d), -1.0f, 1.0f);
        float cosKnee = std::clamp((l1 * l1 + l2 * l2 - d * d) / (2.0f * l1 * l2), -1.0f, 1.0f);

        float hipAngle = acosf(cosHip);
        float kneeAngle = acosf(cosKnee);

        float currentHipAngle = acosf(std::clamp(alignedHipToKnee.Dot(hipToTarget), -1.0f, 1.0f));

        Vector3 kneeToFoot = footPos - kneePos;
        if (kneeToFoot.LengthSquared() < 0.001f) return;
        kneeToFoot.Normalize();
        float currentKneeAngle = acosf(std::clamp(hipToKnee.Dot(kneeToFoot), -1.0f, 1.0f));

  
        float thighBendDelta = (hipAngle - currentHipAngle) * m_ikBlendWeight;
        Matrix thighWorldRotation = Matrix::CreateFromAxisAngle(bendAxis, thighBendDelta);

        float calfBendDelta = (currentKneeAngle - (DirectX::XM_PI - kneeAngle)) * m_ikBlendWeight;
        Matrix calfWorldRotation = Matrix::CreateFromAxisAngle(bendAxis, calfBendDelta);

        // Update Thigh
        Matrix newThighWorld = thighWorld;
        newThighWorld.Translation(Vector3::Zero); // Strip world translation
        newThighWorld = newThighWorld * alignMatrix * thighWorldRotation; // Rotate in place
        newThighWorld.Translation(hipPos); // Restore world translation
        localBones[thighIdx] = newThighWorld * hipToWorld.Invert();

        // Update Calf (Calculated relative to the newly rotated Thigh)
        Matrix updatedCalfWorld = localBones[calfIdx] * newThighWorld;
        Vector3 newKneePos = updatedCalfWorld.Translation(); // Get the new world position of the knee

        Matrix newCalfWorld = updatedCalfWorld;
        newCalfWorld.Translation(Vector3::Zero); // Strip world translation
        newCalfWorld = newCalfWorld * calfWorldRotation; // Rotate in place
        newCalfWorld.Translation(newKneePos); // Restore world translation
        localBones[calfIdx] = newCalfWorld * newThighWorld.Invert();

        // Update Foot (Preserve original world rotation, but adapt to terrain slope)
        Matrix updatedFootWorld = localBones[footIdx] * newCalfWorld;
        Vector3 newFootPos = updatedFootWorld.Translation();

        // Calculate rotation to match terrain normal
        Quaternion terrainRot = Quaternion::FromToRotation(Vector3::Up, normal);
        Quaternion blendedRot = Quaternion::Slerp(Quaternion::Identity, terrainRot, m_ikBlendWeight);
        Matrix footRotMatrix = Matrix::CreateFromQuaternion(blendedRot);

        Matrix preservedFootWorld = footWorld;
        preservedFootWorld.Translation(Vector3::Zero); // Strip translation
        preservedFootWorld = preservedFootWorld * footRotMatrix; // Apply slope rotation
        preservedFootWorld.Translation(newFootPos); // Set to new IK position

        localBones[footIdx] = preservedFootWorld * newCalfWorld.Invert();
    }
}