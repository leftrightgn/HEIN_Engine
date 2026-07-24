#include "pch.h"
#include "HealthComponent.h"
#include <DebugingTools/DebugUIManager.h>

HEIN::HealthComponent::HealthComponent(Actor* owner)
	: IComponent(owner)
	, m_maxHealth()
	, m_currentHealth()
	, m_invincibilityTimer()
	, m_isInvincible(false)
	, m_isGameplayInvincible(false)
{
}

void HEIN::HealthComponent::Initialize(float maxHealth)
{
	m_maxHealth = maxHealth;
	m_currentHealth = maxHealth;
}

void HEIN::HealthComponent::Update(float deltaTime)
{
	if (m_invincibilityTimer > 0.0f)
	{
		m_invincibilityTimer -= deltaTime;
	}
}

void HEIN::HealthComponent::ApplyDamage(float damage)
{
	if (m_currentHealth > 0.0f && !IsInvincible())
	{
		m_currentHealth -= damage;
		m_invincibilityTimer = 0.5f;
	}
}

nlohmann::json HEIN::HealthComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    data["MaxHealth"] = m_maxHealth;
    data["CurrentHealth"] = m_currentHealth;
    data["IsInvincible"] = m_isInvincible;
    data["IsGameplayInvincible"] = m_isGameplayInvincible;
    return data;
}

void HEIN::HealthComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("MaxHealth")) m_maxHealth = data["MaxHealth"];
    if (data.contains("CurrentHealth")) m_currentHealth = data["CurrentHealth"];
    if (data.contains("IsInvincible")) m_isInvincible = data["IsInvincible"];
    if (data.contains("IsGameplayInvincible")) m_isGameplayInvincible = data["IsGameplayInvincible"];
}

void HEIN::HealthComponent::OnInspectorGUI()
{
	if (ImGui::CollapsingHeader("HealthComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		float maxHealth = GetMaxHealth();
		if (ImGui::DragFloat("MaxHealth", &maxHealth, 0.05f))
		{
			SetMaxHealth(maxHealth);
		}
        ImGui::DragFloat("Current Health", &m_currentHealth, 0.05f);
		if (ImGui::Button("ResetHealth"))
		{
			m_currentHealth = m_maxHealth;
		}
		ImGui::Checkbox("UI Invincible", &m_isInvincible);
        ImGui::Checkbox("Gameplay Invincible", &m_isGameplayInvincible);
	}
}
