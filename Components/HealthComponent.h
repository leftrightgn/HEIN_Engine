#pragma once
#include <Components/IComponent.h>

namespace HEIN
{
	// Manages combat life cycle: tracks hit points (HP), processes incoming damage, 
	// and queues the actor for safe end-of-frame destruction upon death.
	class HealthComponent : public IComponent
	{
	private:
		float m_maxHealth;
		float m_currentHealth;

		// Invincibility Frames (i-frames) to prevent staggering or instant death 
		// from multi-hit overlap within a single attack sweep.
		float m_invincibilityTimer;
		bool m_isInvincible;
		bool m_isGameplayInvincible;

		// Hit Flash (visual feedback when damage is applied; rendered by SkinnedModelComponent)
		bool m_enableHitFlash = true;
		DirectX::SimpleMath::Vector4 m_hitFlashColor = { 1.0f, 0.1f, 0.1f, 0.8f }; // rgb + strength
		float m_hitFlashDuration = 0.15f;

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
		void SetCurrentHealth(float currentHealth) { m_currentHealth = currentHealth; }
		bool isDead() const { return m_currentHealth <= 0.0f; }


		bool IsInvincible() const { return m_isInvincible || m_isGameplayInvincible || m_invincibilityTimer > 0.0f; }
		void SetInvincible(bool invincible) { m_isInvincible = invincible; }
		void SetGameplayInvincible(bool invincible) { m_isGameplayInvincible = invincible; }

		// Hit Flash API
		bool IsHitFlashEnabled() const { return m_enableHitFlash; }
		void SetHitFlashEnabled(bool enable) { m_enableHitFlash = enable; }
		const DirectX::SimpleMath::Vector4& GetHitFlashColor() const { return m_hitFlashColor; }
		void SetHitFlashColor(const DirectX::SimpleMath::Vector4& color) { m_hitFlashColor = color; }
		float GetHitFlashDuration() const { return m_hitFlashDuration; }
		void SetHitFlashDuration(float duration) { m_hitFlashDuration = duration; }
	};
}
