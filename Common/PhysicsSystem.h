#pragma once
#include <vector>
#include <memory>
#include "Framework/GameContext.h"
#include "CollisionDispatcher.h"

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