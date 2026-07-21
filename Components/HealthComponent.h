#pragma once
#include <Components/IComponent.h>

namespace HEIN
{
	class HealthComponent : public IComponent
	{
	private:

		float m_maxHealth;
		float m_currentHealth;
		float m_invincibilityTimer;
		bool m_isInvincible;

	public:

		HealthComponent(Actor* owner);

		void Initialize(float maxHealth);

		void Start() override {}
		void Update(float deltaTime) override;

		void ApplyDamage(float damage);

		float GetCurrentHealth() const { return m_currentHealth; }
		float GetMaxHealth() const { return m_maxHealth; }
		bool isDead() const { return m_currentHealth <= 0.0f; }

		bool IsInvincible() const { return m_isInvincible; }
		void SetInvincible(bool invincible) { m_isInvincible = invincible; }

	};
}
