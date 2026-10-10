#pragma once
#include <Components/IComponent.h>
#include "../Message/IObserver.h"
#include "../Message/Message.h"

namespace HEIN
{
	enum DamageType
	{
		Physical,
		Magical,
		Fire,
		Posion,
		Stun
	};

	class DamageDealerComponent : public IComponent, public IObserver
	{
	private:

		float m_damageAmount;
		DamageType m_damageType;
		bool m_isActive;
		bool m_causesStun;
		float m_knockbackForce;

	public:
		
		DamageDealerComponent(Actor* owner);

		void Start() override;
		void Update(float /*deltaTime*/) override{}

		void Initialize(float damageAmount, DamageType damageType = DamageType::Physical, bool causesStun = false, float knockbackForce = 20.0f);

		std::string GetComponentName() const override { return "DamageDealerComponent"; }
		void OnMessageAccepted(Message::MessageID messageID) override;
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		float GetDamageAmount() const { return m_damageAmount; }
		DamageType GetDamageType() const { return m_damageType; }
		
		bool IsActive() const { return m_isActive; }
		void SetActive(bool active) { m_isActive = active; }

		bool CausesStun() const { return m_causesStun || m_damageType == DamageType::Stun; }
		void SetCausesStun(bool causesStun) { m_causesStun = causesStun; }

		float GetKnockbackForce() const { return m_knockbackForce; }
		void SetKnockbackForce(float knockbackForce) { m_knockbackForce = knockbackForce; }
	};
}
