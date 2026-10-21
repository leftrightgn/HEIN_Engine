#pragma once
#include "CollisionDispatcher.h"
#include "Framework/GameContext.h"
#include <memory>
#include <vector>

namespace HEIN
{
    class ActorManager; 
    class ColliderComponent;

    class PhysicsSystem
    {
    public:
        // Pass ActorManager by reference
        void UpdateMovement(GameContext& gameContext, HEIN::ActorManager& actorManager, float deltaTime);
        void UpdateCollisions(GameContext& gameContext, HEIN::ActorManager& actorManager, float deltaTime);
    private:
        void ResolvePhysicalOverlap(HEIN::ColliderComponent* colA, HEIN::ColliderComponent* colB, const HEIN::CollisionManifold& mainfold);
    };
}