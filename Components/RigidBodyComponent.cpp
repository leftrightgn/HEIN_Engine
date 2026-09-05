#include "pch.h"
#include "RigidBodyComponent.h"
#include "Entities/Actor.h"
#include <Components/TransformComponent.h>
#include <DebugingTools/DebugUIManager.h>

HEIN::RigidBodyComponent::RigidBodyComponent(Actor* owner)
	: IComponent(owner)
	, m_transform(nullptr)
	, m_velocity(DirectX::SimpleMath::Vector3::Zero)
	, m_acceleration(DirectX::SimpleMath::Vector3::Zero)
	, m_mass(1.0f)
	, m_useGravity(true)
	, m_isKinematic(false)
{
}

void HEIN::RigidBodyComponent::Initialize(float mass, bool useGravity, bool isKinematic)
{
	m_mass = mass;
	m_useGravity = useGravity;
	m_isKinematic = isKinematic;
}

void HEIN::RigidBodyComponent::Start()
{
	m_transform = GetOwner()->GetComponent<HEIN::TransformComponent>();
}

void HEIN::RigidBodyComponent::Update(float deltaTime)
{
	
}

void HEIN::RigidBodyComponent::AddForce(const DirectX::SimpleMath::Vector3& force)
{
	if (m_mass > 0.0f)
	{
		m_acceleration += (force / m_mass);
	}
}

void HEIN::RigidBodyComponent::SetVelocity(const DirectX::SimpleMath::Vector3& velocity)
{
	m_velocity = velocity;
}

void HEIN::RigidBodyComponent::SetHorizontalVelocity(const DirectX::SimpleMath::Vector3& velocity)
{
	m_velocity.x = velocity.x;
	m_velocity.z = velocity.z;
}

DirectX::SimpleMath::Vector3 HEIN::RigidBodyComponent::GetVelocity() const
{
	return m_velocity;
}

nlohmann::json HEIN::RigidBodyComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    data["Mass"] = m_mass;
    data["UseGravity"] = m_useGravity;
    data["IsKinematic"] = m_isKinematic;
	data["NeedsInitialSnap"] = m_needsInitialSnap;
    return data;
}

void HEIN::RigidBodyComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("Mass")) m_mass = data["Mass"];
    if (data.contains("UseGravity")) m_useGravity = data["UseGravity"];
    if (data.contains("IsKinematic")) m_isKinematic = data["IsKinematic"];
	if (data.contains("NeedsInitialSnap")) m_needsInitialSnap = data["NeedsInitialSnap"];
}

void HEIN::RigidBodyComponent::OnInspectorGUI(GameContext& gameContext)
{
	if (ImGui::CollapsingHeader("RigidBodyComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Mass", &m_mass, 0.1f, 0.0f, 1000.0f);
		ImGui::Checkbox("Use Gravity", &m_useGravity);
		ImGui::Checkbox("Is Kinematic", &m_isKinematic);
		ImGui::Text("Terrain Tools");
		ImGui::Checkbox("Snap To Ground on Play/Load", &m_needsInitialSnap);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("If checked, this actor will instantly snap to the Terrain Height on the first frame of gameplay.");
		}
	}
}
