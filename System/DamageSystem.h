#pragma once
#include "Common/Event.h"
#include <functional>
#include <cstdint>
#include <SimpleMath.h>

namespace HEIN
{
	class Actor;
	class ActorManager;
	class DamageDealerComponent;

	enum class CombatImpactType : uint8_t
	{
		Damage,
		Block,
		Parry,
		GuardBreak
	};

	struct CombatImpactDesc
	{
		DirectX::SimpleMath::Vector3 contactPoint  = DirectX::SimpleMath::Vector3::Zero;
		DirectX::SimpleMath::Vector3 contactNormal = DirectX::SimpleMath::Vector3::UnitY;
		CombatImpactType             impactType    = CombatImpactType::Damage;
		float                        intensity     = 1.0f;
	};

	class DamageSystem
	{
	public:
		using DamageInterceptorFunc = std::function<bool(Actor* attacker, Actor* victim, DamageDealerComponent* dealer, float& damage)>;
		inline static DamageInterceptorFunc s_damageInterceptor = nullptr;
		static void SetDamageInterceptor(DamageInterceptorFunc interceptor) { s_damageInterceptor = std::move(interceptor); }

		DamageSystem() = default;
		~DamageSystem() = default;

		void HandlTriggerHit(const HEIN::TriggerEventPayLoad& payLoad, class ActorManager& actorManager);
	};
}
