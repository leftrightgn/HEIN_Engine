#pragma once
#include <Common/Event.h>
namespace HEIN
{
	class DamageSystem
	{
	public:

		DamageSystem() = default;
		~DamageSystem() = default;

		void HandlTriggerHit(const HEIN::TriggerEventPayLoad& payLoad, class ActorManager& actorManager);
	};
}
