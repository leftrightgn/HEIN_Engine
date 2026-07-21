#include "pch.h"
#include "EventManager.h"

void HEIN::EventManager::AddTriggerListener(HEIN::TriggerEventCallback callback)
{
	m_triggerListeners.push_back(callback);
}

void HEIN::EventManager::DispatchTriggerEvent(const HEIN::TriggerEventPayLoad& payLoad)
{
	for (const HEIN::TriggerEventCallback& listener : m_triggerListeners)
	{
		listener(payLoad);
	}
}
