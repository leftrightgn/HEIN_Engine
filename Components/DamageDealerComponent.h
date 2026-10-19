#pragma once
#include <Components/IComponent.h>
#include "../Message/IObserver.h"
#include "../Message/Message.h"
#include <unordered_set>
#include <cstdint>
#include <algorithm>

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

		// Hitstop (Hit Freeze) frames at 60 FPS applied to both Attacker and Defender on hit
		int m_hitstopFrames = 5;

		// Per-Swing Hit Registry: guarantees strictly 1 hit + 1 hitstop per target per attack swing
		std::unordered_set<uint32_t> m_hitActorsThisSwing;

		uint32_t m_registeredListenID = 0;

	public:
		
		DamageDealerComponent(Actor* owner);
		virtual ~DamageDealerComponent() override;

		void Start() override;
		void Update(float /*deltaTime*/) override;

		void RegisterToWielderMessages();

		void Initialize(float damageAmount, DamageType damageType = DamageType::Physical, bool causesStun = false, float knockbackForce = 20.0f);

		std::string GetComponentName() const override { return "DamageDealerComponent"; }
		void OnMessageAccepted(Message::MessageID messageID) override;
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		float GetDamageAmount() const { return m_damageAmount; }
		DamageType GetDamageType() const { return m_damageType; }
		
		bool IsActive() const { return m_isActive; }
		void SetActive(bool active)
		{
			if (active && !m_isActive)
			{
				m_hitActorsThisSwing.clear();
			}
			else if (!active)
			{
				m_hitActorsThisSwing.clear();
			}
			m_isActive = active;
		}

		bool CausesStun() const { return m_causesStun || m_damageType == DamageType::Stun; }
		void SetCausesStun(bool causesStun) { m_causesStun = causesStun; }

		float GetKnockbackForce() const { return m_knockbackForce; }
		void SetKnockbackForce(float knockbackForce) { m_knockbackForce = knockbackForce; }

		int GetHitstopFrames() const { return m_hitstopFrames; }
		void SetHitstopFrames(int frames) { m_hitstopFrames = std::max(0, frames); }

		// Per-Swing Hit Registry API (One Attack = One Time Damage)
		void ClearHitList() { m_hitActorsThisSwing.clear(); }
		bool HasHitActor(uint32_t actorID) const { return m_hitActorsThisSwing.find(actorID) != m_hitActorsThisSwing.end(); }
		void RegisterHitActor(uint32_t actorID) { m_hitActorsThisSwing.insert(actorID); }
	};
}
