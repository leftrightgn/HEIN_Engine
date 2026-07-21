#include "pch.h"
#include "HealthComponent.h"

HEIN::HealthComponent::HealthComponent(Actor* owner)
	: IComponent(owner)
	, m_maxHealth()
	, m_currentHealth()
	, m_invincibilityTimer()
	, m_isInvincible(false)
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
	if (m_invincibilityTimer <= 0.0f && m_currentHealth > 0.0f && !m_isInvincible)
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
    return data;
}

void HEIN::HealthComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("MaxHealth")) m_maxHealth = data["MaxHealth"];
    else m_maxHealth = 100.0f; // Prevent 0.0f

    if (data.contains("CurrentHealth")) m_currentHealth = data["CurrentHealth"];
    else m_currentHealth = m_maxHealth;
}
