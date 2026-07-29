#include "pch.h"
#include "OBBColliderComponent.h"
#include "Components/StaticModelComponent.h"
#include <DirectXColors.h>
#include <ImGui/imgui.h>

HEIN::OBBColliderComponent::OBBColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::OBB)
	, m_extents(DirectX::SimpleMath::Vector3::Zero)
{
}

void HEIN::OBBColliderComponent::Initialize(const DirectX::SimpleMath::Vector3 extents)
{
	m_extents = extents;
}

void HEIN::OBBColliderComponent::InitializeFromModel(StaticModelComponent* staticModel)
{
    if (staticModel != nullptr)
    {
        DirectX::BoundingBox box = staticModel->GetBoundingBox();

        m_extents = box.Extents;

        m_offset = box.Center;
    }
}

void HEIN::OBBColliderComponent::SyncColliderState()
{
    DirectX::SimpleMath::Matrix worldMatrix = CalculateWorldMatrix();

    DirectX::SimpleMath::Vector3 center = worldMatrix.Translation();

    DirectX::SimpleMath::Vector3 right(worldMatrix._11, worldMatrix._12, worldMatrix._13);
    DirectX::SimpleMath::Vector3 up(worldMatrix._21, worldMatrix._22, worldMatrix._23);
    DirectX::SimpleMath::Vector3 forward(worldMatrix._31, worldMatrix._32, worldMatrix._33);

    DirectX::SimpleMath::Vector3 scale(right.Length(), up.Length(), forward.Length());

    if (scale.x > 0.0001f) right /= scale.x;
    if (scale.y > 0.0001f) up /= scale.y;
    if (scale.z > 0.0001f) forward /= scale.z;

    worldMatrix._11 = right.x; worldMatrix._12 = right.y; worldMatrix._13 = right.z;
    worldMatrix._21 = up.x;    worldMatrix._22 = up.y;    worldMatrix._23 = up.z;
    worldMatrix._31 = forward.x; worldMatrix._32 = forward.y; worldMatrix._33 = forward.z;

    DirectX::SimpleMath::Quaternion rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(worldMatrix);

    DirectX::SimpleMath::Vector3 scaledExtents = m_extents * scale;

    m_worldOBB = DirectX::BoundingOrientedBox(center, scaledExtents, rotation);
}

void HEIN::OBBColliderComponent::Draw(
    GameContext& gameContext,
    const DirectX::SimpleMath::Matrix& world,
    const DirectX::SimpleMath::Matrix& view,
    const DirectX::SimpleMath::Matrix& proj
)
{
    if (gameContext.debugCollisionRenderer == nullptr) return;

    DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
    if (m_isCollidingThisFrame)
    {
        debugColor = DirectX::Colors::Yellow;
    }

    gameContext.debugCollisionRenderer->QueueOBB(m_worldOBB, debugColor);
}

nlohmann::json HEIN::OBBColliderComponent::Serialize()
{
    nlohmann::json data = ColliderComponent::Serialize();
    data["ExtentsX"] = m_extents.x;
    data["ExtentsY"] = m_extents.y;
    data["ExtentsZ"] = m_extents.z;
    return data;
}

void HEIN::OBBColliderComponent::Deserialize(const nlohmann::json& data)
{
    ColliderComponent::Deserialize(data);
    if (data.contains("ExtentsX")) m_extents.x = data["ExtentsX"];
    if (data.contains("ExtentsY")) m_extents.y = data["ExtentsY"];
    if (data.contains("ExtentsZ")) m_extents.z = data["ExtentsZ"];
}

void HEIN::OBBColliderComponent::OnInspectorGUI(GameContext& gameContext)
{
    ColliderComponent::OnInspectorGUI(gameContext);
    if (ImGui::CollapsingHeader("OBB Properties", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Extents", &m_extents.x, 0.05f);
    }
}
