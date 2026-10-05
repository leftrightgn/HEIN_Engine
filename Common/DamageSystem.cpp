#include "pch.h"
#include "DamageSystem.h"
#include <Components/DamageDealerComponent.h>
#include <Components/HealthComponent.h>
#include <Components/ColliderComponent/ColliderComponent.h>
#include <Entities/ActorManager.h>
#include <Entities/Actor.h>

// Validates and applies combat damage from Trigger Hitboxes.
// This is intentionally decoupled from Solid Pushbox collision resolution.
// Damage validation checks for active invincibility frames (i-frames) 
// and block states before applying HP reductions.
void HEIN::DamageSystem::HandlTriggerHit(const HEIN::TriggerEventPayLoad& payLoad, HEIN::ActorManager& actorManager)
{
	HEIN::Actor* actorA = payLoad.triggerA->GetOwner();
	HEIN::Actor* actorB = payLoad.triggerB->GetOwner();

	if (actorA == nullptr || actorB == nullptr) return;

	HEIN::DamageDealerComponent* dealerA = actorA->GetComponent<HEIN::DamageDealerComponent>();
	HEIN::HealthComponent* victimB = actorB->GetComponent<HEIN::HealthComponent>();

	if (dealerA != nullptr && victimB != nullptr)
	{
		ActorID ownerA = actorA->GetOwnerID();
		if (ownerA == INVALID_ACTOR_ID) ownerA = actorA->GetParentID();

		if (ownerA != actorB->GetID())
		{
			if (dealerA->IsActive() && !victimB->IsInvincible())
			{
				float damage = dealerA->GetDamageAmount();
				bool applyDamage = true;
				if (s_damageInterceptor)
				{
					Actor* attacker = actorManager.GetActor(ownerA);
					if (!attacker) attacker = actorA;
					applyDamage = s_damageInterceptor(attacker, actorB, damage);
				}
				if (applyDamage)
				{
					victimB->ApplyDamage(damage);
				}
			}
		}
	}

	HEIN::DamageDealerComponent* dealerB = actorB->GetComponent<HEIN::DamageDealerComponent>();
	HEIN::HealthComponent* victimA = actorA->GetComponent<HEIN::HealthComponent>();

	if (dealerB != nullptr && victimA != nullptr)
	{
		ActorID ownerB = actorB->GetOwnerID();
		if (ownerB == INVALID_ACTOR_ID) ownerB = actorB->GetParentID();

		if (ownerB != actorA->GetID())
		{
			if (dealerB->IsActive() && !victimA->IsInvincible())
			{
				float damage = dealerB->GetDamageAmount();
				bool applyDamage = true;
				if (s_damageInterceptor)
				{
					Actor* attacker = actorManager.GetActor(ownerB);
					if (!attacker) attacker = actorB;
					applyDamage = s_damageInterceptor(attacker, actorA, damage);
				}
				if (applyDamage)
				{
					victimA->ApplyDamage(damage);
				}
			}
		}
	}
}
