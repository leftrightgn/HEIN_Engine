#pragma once
#include <Common/Event.h>
#include <functional>

namespace HEIN
{
	class Actor;
	class ActorManager;
	class DamageDealerComponent;

	class DamageSystem
	{
	public:
		using DamageInterceptorFunc = std::function<bool(Actor* attacker, Actor* victim, DamageDealerComponent* dealer, float damage)>;
		inline static DamageInterceptorFunc s_damageInterceptor = nullptr;
		static void SetDamageInterceptor(DamageInterceptorFunc interceptor) { s_damageInterceptor = std::move(interceptor); }

		DamageSystem() = default;
		~DamageSystem() = default;

		void HandlTriggerHit(const HEIN::TriggerEventPayLoad& payLoad, class ActorManager& actorManager);
	};
}
