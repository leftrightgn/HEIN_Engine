#include "pch.h"
#include "DamageSystem.h"
#include <Components/DamageDealerComponent.h>
#include <Components/HealthComponent.h>
#include <Components/ColliderComponent/ColliderComponent.h>
#include <Entities/ActorManager.h>
#include <Entities/Actor.h>
#include <Message/Messenger.h>

// Validates and applies combat damage from Trigger Hitboxes.
// This is intentionally decoupled from Solid Pushbox collision resolution.
// Damage validation checks for active invincibility frames (i-frames) 
// and block states before applying HP reductions.
void HEIN::DamageSystem::HandlTriggerHit(const HEIN::TriggerEventPayLoad& payLoad, HEIN::ActorManager& actorManager)
{
	HEIN::Actor* actorA = payLoad.triggerA->GetOwner();
	HEIN::Actor* actorB = payLoad.triggerB->GetOwner();

	if (actorA == nullptr || actorB == nullptr) return;

	auto ProcessHit = [&](HEIN::Actor* weaponActor, HEIN::DamageDealerComponent* dealer, HEIN::Actor* victimActor, HEIN::HealthComponent* victimHealth)
	{
		if (dealer == nullptr || victimHealth == nullptr) return;

		ActorID attackerID = weaponActor->GetOwnerID();
		if (attackerID == INVALID_ACTOR_ID) attackerID = weaponActor->GetParentID();

		if (attackerID == victimActor->GetID()) return;

		// One Attack = One Time Damage: strictly ignore if this swing already registered a hit on this victim
		if (!dealer->IsActive() || victimHealth->IsInvincible() || dealer->HasHitActor(victimActor->GetID()))
		{
			return;
		}

		// Immediately lock in this victim for the remainder of this swing
		dealer->RegisterHitActor(victimActor->GetID());

		Actor* attacker = actorManager.GetActor(attackerID);
		if (!attacker) attacker = weaponActor;

		float damage = dealer->GetDamageAmount();
		bool applyDamage = true;
		if (s_damageInterceptor)
		{
			applyDamage = s_damageInterceptor(attacker, victimActor, dealer, damage);
		}

		// Trigger Symmetric Hitstop (Hit Freeze) on both Attacker and Defender at the exact moment of impact
		int hitstopFrames = dealer->GetHitstopFrames();
		if (victimHealth->GetCurrentHealth() <= damage)
		{
			// Fatal blow: enhance impact freeze to at least 14 frames for dramatic killing blow
			hitstopFrames = std::max(hitstopFrames, 14);
		}
		if (hitstopFrames > 0)
		{
			if (attacker) attacker->TriggerHitstopFrames(hitstopFrames);
			victimActor->TriggerHitstopFrames(hitstopFrames);
		}

		if (applyDamage)
		{
			victimHealth->ApplyDamage(damage);
			Messenger::GetInstance()->Notify(attackerID, Message::ATTACK_HIT_CONFIRMED);

			if (dealer->CausesStun())
			{
				Messenger::GetInstance()->Notify(victimActor->GetID(), Message::HIT_STUN);
			}
		}
	};

	ProcessHit(actorA, actorA->GetComponent<HEIN::DamageDealerComponent>(), actorB, actorB->GetComponent<HEIN::HealthComponent>());
	ProcessHit(actorB, actorB->GetComponent<HEIN::DamageDealerComponent>(), actorA, actorA->GetComponent<HEIN::HealthComponent>());
}
