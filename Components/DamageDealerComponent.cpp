#include "pch.h"
#include "DamageDealerComponent.h"

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
