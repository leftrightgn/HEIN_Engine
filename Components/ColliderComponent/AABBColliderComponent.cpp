#include "pch.h"
#include "AABBColliderComponent.h"
#include "Components/StaticModelComponent.h"
#include <DirectXColors.h>

HEIN::AABBColliderComponent::AABBColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::AABB)
	, m_extents(DirectX::SimpleMath::Vector3::Zero)
{
}

void HEIN::AABBColliderComponent::Initialize(const DirectX::SimpleMath::Vector3 extents)
{
	m_extents = extents;
}

void HEIN::AABBColliderComponent::InitializeFromModel(StaticModelComponent* staticModel)
{
    if (staticModel != nullptr)
    {
        DirectX::BoundingBox box = staticModel->GetBoundingBox();

        m_extents = box.Extents;

        m_offset = box.Center;
    }
}

void HEIN::AABBColliderComponent::SyncColliderState()
{
    DirectX::SimpleMath::Matrix worldMatrix = CalculateWorldMatrix();

    DirectX::BoundingBox localBox(DirectX::SimpleMath::Vector3::Zero, m_extents);

    localBox.Transform(m_worldAABB, worldMatrix);
}

void HEIN::AABBColliderComponent::Draw(
    GameContext& gameContext,
    const DirectX::SimpleMath::Matrix& world, 
    const DirectX::SimpleMath::Matrix& view, 
    const DirectX::SimpleMath::Matrix& proj
)
{
    if (gameContext.debugCollisionRenderer == nullptr) return;


    DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
    if (m_isTrigger)
    {
        debugColor = DirectX::Colors::Yellow;
    }

    gameContext.debugCollisionRenderer->QueueAABB(m_worldAABB, debugColor);
}

nlohmann::json HEIN::AABBColliderComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    return data;
}

void HEIN::AABBColliderComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
}
