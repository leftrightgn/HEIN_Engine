#include "pch.h"
#include "DamageSystem.h"
#include <Components/DamageDealerComponent.h>
#include <Components/HealthComponent.h>
#include <Components/ColliderComponent/ColliderComponent.h>
#include <Entities/ActorManager.h>
#include <Entities/Actor.h>

void HEIN::DamageSystem::HandlTriggerHit(const HEIN::TriggerEventPayLoad& payLoad, HEIN::ActorManager& actorManager)
{
	HEIN::Actor* actorA = payLoad.triggerA->GetOwner();
	HEIN::Actor* actorB = payLoad.triggerB->GetOwner();

	if (actorA == nullptr || actorB == nullptr) return;

	HEIN::DamageDealerComponent* dealerA = actorA->GetComponent<HEIN::DamageDealerComponent>();
	HEIN::HealthComponent* victimB = actorB->GetComponent<HEIN::HealthComponent>();

	if (dealerA != nullptr && victimB != nullptr)
	{
		if (actorA->GetOwnerID() != actorB->GetID())
		{
			if (dealerA->IsActive() && !victimB->IsInvincible())
			{
				victimB->ApplyDamage(dealerA->GetDamageAmount());
			}
		}
	}

	HEIN::DamageDealerComponent* dealerB = actorB->GetComponent<HEIN::DamageDealerComponent>();
	HEIN::HealthComponent* victimA = actorA->GetComponent<HEIN::HealthComponent>();

	if (dealerB != nullptr && victimA != nullptr)
	{
		if (actorB->GetOwnerID() != actorA->GetID())
		{
			if (dealerB->IsActive() && !victimA->IsInvincible())
			{
				victimA->ApplyDamage(dealerB->GetDamageAmount());
			}
		}
	}
}
