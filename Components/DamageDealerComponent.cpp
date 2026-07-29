#include "pch.h"
#include "DamageDealerComponent.h"
#include <ImGui/imgui.h>

HEIN::DamageDealerComponent::DamageDealerComponent(Actor* owner)
	: IComponent(owner)
	, m_damageAmount(0.0f)
	, m_damageType(DamageType::Physical)
	, m_isActive(true)
{
}

void HEIN::DamageDealerComponent::Initialize(float damageAmount, DamageType damageType)
{
	m_damageAmount = damageAmount;
	m_damageType = damageType;
}

nlohmann::json HEIN::DamageDealerComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    data["DamageAmount"] = m_damageAmount;
    data["DamageType"] = static_cast<int>(m_damageType);
    data["IsActive"] = m_isActive;
    return data;
}

void HEIN::DamageDealerComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("DamageAmount")) m_damageAmount = data["DamageAmount"];
    if (data.contains("DamageType")) m_damageType = static_cast<DamageType>(data["DamageType"]);
    if (data.contains("IsActive")) m_isActive = data["IsActive"];
}

void HEIN::DamageDealerComponent::OnInspectorGUI(GameContext& gameContext)
{
    if (ImGui::CollapsingHeader("DamageDealerComponent", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Damage Amount", &m_damageAmount, 0.5f);
        ImGui::Checkbox("Is Active", &m_isActive);
    }
}
