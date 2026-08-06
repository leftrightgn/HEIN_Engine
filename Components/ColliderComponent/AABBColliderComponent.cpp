#include "pch.h"
#include "AABBColliderComponent.h"
#include "Components/StaticModelComponent.h"
#include "Entities/Actor.h"
#include <DirectXColors.h>
#include <ImGui/imgui.h>

HEIN::AABBColliderComponent::AABBColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::AABB)
	, m_extents(DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f))
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
        m_layer = CollisionLayer::Layer_Environment;
    }
}

void HEIN::AABBColliderComponent::Start()
{
    ColliderComponent::Start();
    if (m_extents.LengthSquared() <= 0.0001f)
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
                m_extents = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
            }
        }
        else
        {
            m_extents = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);
        }
    }
    SyncColliderState();
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

    SyncColliderState();

    DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
    if (m_isTrigger)
    {
        debugColor = DirectX::Colors::Yellow;
    }

    gameContext.debugCollisionRenderer->QueueAABB(m_worldAABB, debugColor);
}

nlohmann::json HEIN::AABBColliderComponent::Serialize()
{
    nlohmann::json data = ColliderComponent::Serialize();
    data["ExtentsX"] = m_extents.x;
    data["ExtentsY"] = m_extents.y;
    data["ExtentsZ"] = m_extents.z;
    return data;
}

void HEIN::AABBColliderComponent::Deserialize(const nlohmann::json& data)
{
    ColliderComponent::Deserialize(data);
    if (data.contains("ExtentsX")) m_extents.x = data["ExtentsX"];
    if (data.contains("ExtentsY")) m_extents.y = data["ExtentsY"];
    if (data.contains("ExtentsZ")) m_extents.z = data["ExtentsZ"];
}

void HEIN::AABBColliderComponent::OnInspectorGUI(GameContext& gameContext)
{
    ColliderComponent::OnInspectorGUI(gameContext);
    if (ImGui::CollapsingHeader("AABB Properties", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Extents", &m_extents.x, 0.05f, 0.01f, 1000.0f);

        if (GetOwner() != nullptr)
        {
            auto* model = GetOwner()->GetComponent<StaticModelComponent>();
            if (model != nullptr)
            {
                if (ImGui::Button("Auto-Fit to Static Model", ImVec2(-1, 26)))
                {
                    InitializeFromModel(model);
                }
            }
        }
    }
}
