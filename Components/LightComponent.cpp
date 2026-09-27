#include "pch.h"
#include "LightComponent.h"
#include <ImGui/imgui.h>
#include "Framework/GameContext.h"
#include "Entities/Actor.h"
#include "Components/TransformComponent.h"

namespace HEIN
{
    LightComponent::LightComponent(Actor* owner)
        : IComponent(owner)
    {
    }

    nlohmann::json LightComponent::Serialize()
    {
        nlohmann::json data = IComponent::Serialize();
        data["LightType"] = static_cast<int>(m_lightType);
        data["Color"] = { m_color.x, m_color.y, m_color.z, m_color.w };
        data["Intensity"] = m_intensity;
        data["Range"] = m_range;
        data["SpotAngle"] = m_spotAngle;
        data["CastShadows"] = m_castShadows;
        return data;
    }

    void LightComponent::Deserialize(const nlohmann::json& data)
    {
        IComponent::Deserialize(data);
        if (data.contains("LightType")) m_lightType = static_cast<LightType>(data["LightType"]);

        if (data.contains("Color") && data["Color"].is_array() && data["Color"].size() == 4)
        {
            m_color = DirectX::SimpleMath::Vector4(data["Color"][0], data["Color"][1], data["Color"][2], data["Color"][3]);
        }

        if (data.contains("Intensity")) m_intensity = data["Intensity"];
        if (data.contains("Range")) m_range = data["Range"];
        if (data.contains("SpotAngle")) m_spotAngle = data["SpotAngle"];
        if (data.contains("CastShadows")) m_castShadows = data["CastShadows"];
    }

    void LightComponent::OnInspectorGUI(GameContext& gameContext)
    {
        if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char* types[] = { "Directional", "Point", "Spot" };
            int typeIdx = static_cast<int>(m_lightType);
            if (ImGui::Combo("Light Type", &typeIdx, types, 3))
            {
                m_lightType = static_cast<LightType>(typeIdx);
            }

            ImGui::ColorEdit4("Color", &m_color.x);
            ImGui::DragFloat("Intensity", &m_intensity, 0.1f, 0.0f, 100.0f);

            // Only show Range for Point and Spot lights
            if (m_lightType == LightType::Point || m_lightType == LightType::Spot)
            {
                ImGui::DragFloat("Range", &m_range, 0.5f, 0.1f, 500.0f);
            }

            // Only show Spot Angle for Spot lights
            if (m_lightType == LightType::Spot)
            {
                ImGui::DragFloat("Spot Angle", &m_spotAngle, 0.5f, 1.0f, 179.0f);
            }

            ImGui::Checkbox("Cast Shadows", &m_castShadows);
        }
    }

    void LightComponent::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
    {
        if (!gameContext.isEditorMode) return;

        if (gameContext.debugRenderer)
        {
            gameContext.debugRenderer->Begin(view, proj);

            DirectX::SimpleMath::Vector3 pos = m_owner->GetComponent<TransformComponent>()->GetPosition();
            DirectX::SimpleMath::Vector4 debugColor = m_color * m_intensity;
            DirectX::XMVECTOR vColor = DirectX::XMVectorSet(debugColor.x, debugColor.y, debugColor.z, 1.0f);

            if (m_lightType == LightType::Point)
            {
                DirectX::BoundingSphere sphere(pos, m_range);
                gameContext.debugRenderer->DrawSphere(sphere, vColor);
            }
            else if (m_lightType == LightType::Spot)
            {
                DirectX::SimpleMath::Vector3 dir = m_owner->GetComponent<TransformComponent>()->GetForward();
                // Just draw a line to indicate direction and range
                gameContext.debugRenderer->DrawLine(pos, pos + dir * m_range, vColor);

                // Draw a simple ring at the end to indicate the cone
                float radius = m_range * tanf(DirectX::XMConvertToRadians(m_spotAngle / 2.0f));
                DirectX::SimpleMath::Vector3 right = m_owner->GetComponent<TransformComponent>()->GetRight();
                DirectX::SimpleMath::Vector3 up = m_owner->GetComponent<TransformComponent>()->GetUp();
                gameContext.debugRenderer->DrawRing(pos + dir * m_range, right * radius, up * radius, vColor);
            }
            else if (m_lightType == LightType::Directional)
            {
                DirectX::SimpleMath::Vector3 dir = m_owner->GetComponent<TransformComponent>()->GetForward();
                DirectX::SimpleMath::Vector3 right = m_owner->GetComponent<TransformComponent>()->GetRight();
                DirectX::SimpleMath::Vector3 up = m_owner->GetComponent<TransformComponent>()->GetUp();
                
                // Draw a circle at the source to represent the directional light
                float radius = 2.0f;
                gameContext.debugRenderer->DrawRing(pos, right * radius, up * radius, vColor);
                
                // Draw central line
                gameContext.debugRenderer->DrawLine(pos, pos + dir * 10.0f, vColor);

                // Draw 4 parallel lines coming out of the circle edge
                gameContext.debugRenderer->DrawLine(pos + up * radius, pos + up * radius + dir * 10.0f, vColor);
                gameContext.debugRenderer->DrawLine(pos - up * radius, pos - up * radius + dir * 10.0f, vColor);
                gameContext.debugRenderer->DrawLine(pos + right * radius, pos + right * radius + dir * 10.0f, vColor);
                gameContext.debugRenderer->DrawLine(pos - right * radius, pos - right * radius + dir * 10.0f, vColor);
            }

            gameContext.debugRenderer->End();
        }
    }
}