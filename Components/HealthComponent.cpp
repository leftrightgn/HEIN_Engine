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
