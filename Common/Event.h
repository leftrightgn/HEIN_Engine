#pragma once
#include <functional>
#include <SimpleMath.h>

namespace HEIN
{
	class ColliderComponent;

	struct TriggerEventPayLoad
	{
		HEIN::ColliderComponent* triggerA;
		HEIN::ColliderComponent* triggerB;
		DirectX::SimpleMath::Vector3 contactPoint = DirectX::SimpleMath::Vector3::Zero;
		DirectX::SimpleMath::Vector3 contactNormal = DirectX::SimpleMath::Vector3::UnitY;
	};

	typedef std::function<void(const TriggerEventPayLoad&)> TriggerEventCallback;
}
