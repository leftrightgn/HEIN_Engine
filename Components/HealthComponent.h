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
		bool m_isGameplayInvincible;

	public:
		std::string GetComponentName() const override { return "HealthComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		HealthComponent(Actor* owner);

		void Initialize(float maxHealth);

		void Start() override {}
		void Update(float deltaTime) override;

		void ApplyDamage(float damage);

		float GetCurrentHealth() const { return m_currentHealth; }
		float GetMaxHealth() const { return m_maxHealth; }
		void SetMaxHealth(float maxHealth) { m_maxHealth = maxHealth; }
		bool isDead() const { return m_currentHealth <= 0.0f; }


		bool IsInvincible() const { return m_isInvincible || m_isGameplayInvincible || m_invincibilityTimer > 0.0f; }
		void SetInvincible(bool invincible) { m_isInvincible = invincible; }
		void SetGameplayInvincible(bool invincible) { m_isGameplayInvincible = invincible; }

	};
}
