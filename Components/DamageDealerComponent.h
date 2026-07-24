#pragma once
#include <Components/IComponent.h>

namespace HEIN
{
	enum DamageType
	{
		Physical,
		Magical,
		Fire,
		Posion
	};

	class DamageDealerComponent : public IComponent
	{
	private:

		float m_damageAmount;
		DamageType m_damageType;
		bool m_isActive;

	public:
		std::string GetComponentName() const override { return "DamageDealerComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;


		DamageDealerComponent(Actor* owner);

		void Initialize(float damageAmount, DamageType damageType = DamageType::Physical);

		void Start() override{}
		void Update(float /*deltaTime*/) override{}
		
		float GetDamageAmount() const { return m_damageAmount; }
		DamageType GetDamageType() const { return m_damageType; }
		
		bool IsActive() const { return m_isActive; }
		void SetActive(bool active) { m_isActive = active; }
	};
}
