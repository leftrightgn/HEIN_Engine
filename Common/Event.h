#pragma once
#include <functional>

namespace HEIN
{
	class ColliderComponent;

	struct TriggerEventPayLoad
	{
		HEIN::ColliderComponent* triggerA;
		HEIN::ColliderComponent* triggerB;
	};

	typedef std::function<void(const TriggerEventPayLoad&)> TriggerEventCallback;
}
