#include "pch.h"
#include "SphereColliderComponent.h"
#include "Components/StaticModelComponent.h"
#include "Entities/Actor.h"
#include <DirectXColors.h>
#include <ImGui/imgui.h>

HEIN::SphereColliderComponent::SphereColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::Sphere)
	, m_radius(1.0f)
{
}

void HEIN::SphereColliderComponent::Initialize(const float radius)
{
    m_radius = radius;
}

void HEIN::SphereColliderComponent::InitializeFromModel(StaticModelComponent* staticModel)
{
    if (staticModel != nullptr)
    {
        DirectX::BoundingSphere sphere = staticModel->GetBoundingSphere();
        m_radius = sphere.Radius;
        m_offset = sphere.Center;
    }
}

void HEIN::SphereColliderComponent::Start()
{
    ColliderComponent::Start();
    if (m_radius <= 0.0001f)
    {
        if (GetOwner() != nullptr)
        {
            auto* model = GetOwner()->GetComponent<StaticModelComponent>();
            if (model != nullptr)
            {
                InitializeFromModel(model);
            }
            else
            {
                m_radius = 1.0f;
            }
        }
        else
        {
            m_radius = 1.0f;
        }
    }
    SyncColliderState();
}

void HEIN::SphereColliderComponent::SyncColliderState()
{
    DirectX::SimpleMath::Matrix worldMatrix = CalculateWorldMatrix();

    DirectX::BoundingSphere localSphere(DirectX::SimpleMath::Vector3::Zero, m_radius);

    localSphere.Transform(m_worldSphere, worldMatrix);
}

void HEIN::SphereColliderComponent::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
{
    if (gameContext.debugCollisionRenderer == nullptr) return;

    SyncColliderState();

    DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
    if (m_isTrigger)
    {
        debugColor = DirectX::Colors::Yellow;
    }

    gameContext.debugCollisionRenderer->QueueSphere(m_worldSphere, debugColor);
}

void HEIN::SphereColliderComponent::OnInspectorGUI(GameContext& gameContext)
{
    ColliderComponent::OnInspectorGUI(gameContext);
    if (ImGui::CollapsingHeader("Sphere Properties", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Radius", &m_radius, 0.05f, 0.01f, 100.0f);
    }
}

nlohmann::json HEIN::SphereColliderComponent::Serialize()
{
    nlohmann::json data = ColliderComponent::Serialize();
    data["Radius"] = m_radius;
    return data;
}

void HEIN::SphereColliderComponent::Deserialize(const nlohmann::json& data)
{
    ColliderComponent::Deserialize(data);
    if (data.contains("Radius")) m_radius = data["Radius"];
}
