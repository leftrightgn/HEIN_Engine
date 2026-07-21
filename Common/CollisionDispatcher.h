#pragma once
#include "CollisionMath.h"

namespace HEIN
{
	class ColliderComponent;

	class CollisionDispatcher
	{
	public:

		static HEIN::CollisionManifold CheckCollision(HEIN::ColliderComponent* colA, HEIN::ColliderComponent* colB);
	};
}
