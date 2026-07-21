#pragma once
#include <vector>
#include "Event.h"

namespace HEIN
{
	class EventManager
	{
	private:

		std::vector<HEIN::TriggerEventCallback> m_triggerListeners;

	public:

		EventManager() = default;
		~EventManager() = default;

		void AddTriggerListener(HEIN::TriggerEventCallback callback);
		void DispatchTriggerEvent(const HEIN::TriggerEventPayLoad& payLoad);
	};

}

