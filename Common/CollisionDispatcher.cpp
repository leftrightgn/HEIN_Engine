#include "pch.h"
#include "CollisionDispatcher.h"
#include "CollisionMath.h"
#include "Components/ColliderComponent/ColliderComponent.h"
#include "Components/ColliderComponent/OBBColliderComponent.h"
#include "Components/ColliderComponent/CapsuleColliderComponent.h"
#include "Components/ColliderComponent/AABBColliderComponent.h"
#include <Components/ColliderComponent/MeshColliderComponent.h>

HEIN::CollisionManifold HEIN::CollisionDispatcher::CheckCollision(HEIN::ColliderComponent* colA, HEIN::ColliderComponent* colB)
{
    HEIN::CollisionManifold resultManifold;
    resultManifold.isColliding = false;

    if (colA == nullptr || colB == nullptr) return resultManifold;

    HEIN::ColliderShape shapeA = colA->GetShape();
    HEIN::ColliderShape shapeB = colB->GetShape();

    // Capsule vs AABB
    if (shapeA == HEIN::ColliderShape::Capsule && shapeB == HEIN::ColliderShape::AABB)
    {
        HEIN::CapsuleColliderComponent* capsule = dynamic_cast<HEIN::CapsuleColliderComponent*>(colA);
        HEIN::AABBColliderComponent* aabb = dynamic_cast<HEIN::AABBColliderComponent*>(colB);
        return HEIN::CollisionMath::CheckCapsuleVsAABB(capsule, aabb);
    }
    else if (shapeA == HEIN::ColliderShape::AABB && shapeB == HEIN::ColliderShape::Capsule)
    {
        HEIN::CapsuleColliderComponent* capsule = dynamic_cast<HEIN::CapsuleColliderComponent*>(colB);
        HEIN::AABBColliderComponent* aabb = dynamic_cast<HEIN::AABBColliderComponent*>(colA);

        resultManifold = HEIN::CollisionMath::CheckCapsuleVsAABB(capsule, aabb);
   
        resultManifold.normal = resultManifold.normal * -1.0f;
        return resultManifold;
    }
    else if (shapeA == HEIN::ColliderShape::Capsule && shapeB == HEIN::ColliderShape::OBB)
    {
        HEIN::CapsuleColliderComponent* capsule = dynamic_cast<HEIN::CapsuleColliderComponent*>(colA);
        HEIN::OBBColliderComponent* obb = dynamic_cast<HEIN::OBBColliderComponent*>(colB);
        return HEIN::CollisionMath::CheckCapsuleVsOBB(capsule, obb);
    }
    else if (shapeA == HEIN::ColliderShape::OBB && shapeB == HEIN::ColliderShape::Capsule)
    {
        HEIN::CapsuleColliderComponent* capsule = dynamic_cast<HEIN::CapsuleColliderComponent*>(colB);
        HEIN::OBBColliderComponent* obb = dynamic_cast<HEIN::OBBColliderComponent*>(colA);

        resultManifold = HEIN::CollisionMath::CheckCapsuleVsOBB(capsule, obb);

        resultManifold.normal = resultManifold.normal * -1.0f;

        return resultManifold;
    }
    else if (shapeA == HEIN::ColliderShape::Capsule && shapeB == HEIN::ColliderShape::Capsule)
    {
        HEIN::CapsuleColliderComponent* capsuleA = dynamic_cast<HEIN::CapsuleColliderComponent*>(colA);
        HEIN::CapsuleColliderComponent* capsuleB = dynamic_cast<HEIN::CapsuleColliderComponent*>(colB);
        return HEIN::CollisionMath::CheckCapsuleVsCapsule(capsuleA, capsuleB);
    }
    else if (shapeA == HEIN::ColliderShape::Capsule && shapeB == HEIN::ColliderShape::Capsule)
    {
        HEIN::CapsuleColliderComponent* capsuleA = dynamic_cast<HEIN::CapsuleColliderComponent*>(colB);
        HEIN::CapsuleColliderComponent* capsuleB = dynamic_cast<HEIN::CapsuleColliderComponent*>(colA);

        resultManifold = HEIN::CollisionMath::CheckCapsuleVsCapsule(capsuleA, capsuleB);
        resultManifold.normal = resultManifold.normal * -1.0f;

        return resultManifold;
    }
    else if (shapeA == HEIN::ColliderShape::Capsule && shapeB == HEIN::ColliderShape::Mesh)
    {
        HEIN::CapsuleColliderComponent* capsuleA = dynamic_cast<HEIN::CapsuleColliderComponent*>(colA);
        HEIN::MeshColliderComponent* meshB = dynamic_cast<HEIN::MeshColliderComponent*>(colB);

        return HEIN::CollisionMath::CheckCapsuleVsMesh(capsuleA, meshB);
    }

    return resultManifold;

}
