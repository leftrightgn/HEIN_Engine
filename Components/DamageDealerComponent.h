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
		Posion
	};

	class DamageDealerComponent : public IComponent, public IObserver
	{
	private:

		float m_damageAmount;
		DamageType m_damageType;
		bool m_isActive;

	public:
		
		DamageDealerComponent(Actor* owner);

		void Start() override;
		void Update(float /*deltaTime*/) override{}

		void Initialize(float damageAmount, DamageType damageType = DamageType::Physical);

		std::string GetComponentName() const override { return "DamageDealerComponent"; }
		void OnMessageAccepted(Message::MessageID messageID) override;
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		float GetDamageAmount() const { return m_damageAmount; }
		DamageType GetDamageType() const { return m_damageType; }
		
		bool IsActive() const { return m_isActive; }
		void SetActive(bool active) { m_isActive = active; }
	};
}
