#include "pch.h"
#include "HealthComponent.h"
#include <DebugingTools/DebugUIManager.h>
#include <Components/SkinnedModelComponent.h>
#include <Entities/Actor.h>

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

		if (m_enableHitFlash && m_owner != nullptr)
		{
			if (auto* model = m_owner->GetComponent<HEIN::SkinnedModelComponent>())
			{
				model->TriggerHitFlash(m_hitFlashColor, m_hitFlashDuration);
			}
		}
	}
}

nlohmann::json HEIN::HealthComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    data["MaxHealth"] = m_maxHealth;
    data["CurrentHealth"] = m_currentHealth;
    data["IsInvincible"] = m_isInvincible;
    data["IsGameplayInvincible"] = m_isGameplayInvincible;
    data["EnableHitFlash"] = m_enableHitFlash;
    data["HitFlashColor"] = { m_hitFlashColor.x, m_hitFlashColor.y, m_hitFlashColor.z, m_hitFlashColor.w };
    data["HitFlashDuration"] = m_hitFlashDuration;
    return data;
}

void HEIN::HealthComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("MaxHealth")) m_maxHealth = data["MaxHealth"];
    if (data.contains("CurrentHealth")) m_currentHealth = data["CurrentHealth"];
    if (data.contains("IsInvincible")) m_isInvincible = data["IsInvincible"];
    if (data.contains("IsGameplayInvincible")) m_isGameplayInvincible = data["IsGameplayInvincible"];
    if (data.contains("EnableHitFlash")) m_enableHitFlash = data["EnableHitFlash"];
    if (data.contains("HitFlashColor") && data["HitFlashColor"].is_array() && data["HitFlashColor"].size() == 4)
    {
        const auto& c = data["HitFlashColor"];
        m_hitFlashColor = DirectX::SimpleMath::Vector4(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>());
    }
    if (data.contains("HitFlashDuration")) m_hitFlashDuration = data["HitFlashDuration"];
}

void HEIN::HealthComponent::OnInspectorGUI(GameContext& gameContext)
{
	if (ImGui::CollapsingHeader("HealthComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		float maxHealth = GetMaxHealth();
		if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 1.0f, 99999.0f, "%.0f"))
		{
			SetMaxHealth(maxHealth);
			if (m_currentHealth > m_maxHealth)
			{
				m_currentHealth = m_maxHealth;
			}
		}
		if (ImGui::DragFloat("Current Health", &m_currentHealth, 1.0f, 0.0f, m_maxHealth, "%.0f"))
		{
			m_currentHealth = std::clamp(m_currentHealth, 0.0f, m_maxHealth);
		}

		// Visual Health Bar in Inspector
		float ratio = (m_maxHealth > 0.0f) ? std::clamp(m_currentHealth / m_maxHealth, 0.0f, 1.0f) : 0.0f;
		char hpOverlay[64];
		sprintf_s(hpOverlay, "%.0f / %.0f (%.0f%%)", m_currentHealth, m_maxHealth, ratio * 100.0f);
		ImGui::ProgressBar(ratio, ImVec2(-1, 0), hpOverlay);

		if (ImGui::Button("Reset Health"))
		{
			m_currentHealth = m_maxHealth;
		}
		ImGui::Checkbox("UI Invincible", &m_isInvincible);
		ImGui::Checkbox("Gameplay Invincible", &m_isGameplayInvincible);

		// Hit Flash settings
		ImGui::Separator();
		ImGui::Text("Hit Effect:");
		ImGui::Checkbox("Enable Hit Flash", &m_enableHitFlash);
		ImGui::ColorEdit4("Hit Color (A = Strength)", &m_hitFlashColor.x);
		ImGui::DragFloat("Hit Flash Duration", &m_hitFlashDuration, 0.01f, 0.01f, 2.0f, "%.2f s");
		if (ImGui::Button("Preview Hit Flash") && m_owner != nullptr)
		{
			if (auto* model = m_owner->GetComponent<HEIN::SkinnedModelComponent>())
			{
				model->TriggerHitFlash(m_hitFlashColor, m_hitFlashDuration);
			}
		}
	}
}
