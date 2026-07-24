#include "pch.h"
#include "SphereColliderComponent.h"
#include "Components/StaticModelComponent.h"
#include <DirectXColors.h>

HEIN::SphereColliderComponent::SphereColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::Sphere)
	, m_radius(0.0f)
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

void HEIN::SphereColliderComponent::SyncColliderState()
{
    DirectX::SimpleMath::Matrix worldMatrix = CalculateWorldMatrix();

    DirectX::BoundingSphere localSphere(DirectX::SimpleMath::Vector3::Zero, m_radius);

    localSphere.Transform(m_worldSphere, worldMatrix);
}

void HEIN::SphereColliderComponent::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
{
    if (gameContext.debugCollisionRenderer == nullptr) return;


    DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
    if (m_isTrigger)
    {
        debugColor = DirectX::Colors::Yellow;
    }

    gameContext.debugCollisionRenderer->QueueSphere(m_worldSphere, debugColor);
}

nlohmann::json HEIN::SphereColliderComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    return data;
}

void HEIN::SphereColliderComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
}
