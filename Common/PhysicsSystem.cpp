#include "pch.h"
#include "PhysicsSystem.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include "Components/RigidBodyComponent.h"
#include "Components/TransformComponent.h"
#include "Components/ColliderComponent/ColliderComponent.h"


void HEIN::PhysicsSystem::UpdateMovement(GameContext& gameContext, HEIN::ActorManager& actorManager, float deltaTime)
{
    // Loop through the manager's map
    for (auto& pair : actorManager.GetAllActors())
    {
        HEIN::Actor* actor = pair.second.get();
        HEIN::RigidBodyComponent* rb = actor->GetComponent<HEIN::RigidBodyComponent>();
        if (rb) rb->m_isGrounded = false;

        std::vector<HEIN::ColliderComponent*> actorColliders = actor->GetComponents<HEIN::ColliderComponent>();
        for (HEIN::ColliderComponent* col : actorColliders)
        {
            col->SetCollidingThisFrame(false);
        }

        // Apply Gravity and Velocity
        HEIN::TransformComponent* transform = actor->GetComponent<HEIN::TransformComponent>();
        if (rb != nullptr && !rb->isKinematic() && transform != nullptr)
        {
            if (rb->UsesGravity() && !rb->m_isGrounded)
            {
                DirectX::SimpleMath::Vector3 gravityForce(0.0f, rb->GRAVITY_FORCE, 0.0f);
                rb->m_acceleration += gravityForce;
            }

            rb->m_velocity += (rb->m_acceleration * deltaTime);

            DirectX::SimpleMath::Vector3 currentPosition = transform->GetPosition();
            currentPosition += (rb->m_velocity * deltaTime);
            transform->SetPosition(currentPosition);

            rb->m_acceleration = DirectX::SimpleMath::Vector3::Zero;
        }
    }
}

void HEIN::PhysicsSystem::UpdateCollisions(GameContext& gameContext, HEIN::ActorManager& actorManager, float deltaTime)
{
    std::vector<HEIN::ColliderComponent*> allColliders;

    // Gather all colliders from the manager
    for (auto& pair : actorManager.GetAllActors())
    {
        HEIN::Actor* actor = pair.second.get();
        std::vector<HEIN::ColliderComponent*> actorColliders = actor->GetComponents<HEIN::ColliderComponent>();
        for (HEIN::ColliderComponent* col : actorColliders)
        {
            col->SyncColliderState();
            allColliders.push_back(col);
        }
    }

    // (The rest of your exact collision resolution code stays exactly the same here!)
    for (size_t i = 0; i < allColliders.size(); ++i)
    {
        for (size_t j = i + 1; j < allColliders.size(); ++j)
        {
            HEIN::ColliderComponent* colA = allColliders[i];
            HEIN::ColliderComponent* colB = allColliders[j];

            if (colA->GetOwner() == colB->GetOwner()) continue;

            bool aCanHitB = (colA->GetCollisionMask() & colB->GetCollisionLayer()) != 0;
            bool bCanHitA = (colB->GetCollisionMask() & colA->GetCollisionLayer()) != 0;

            if (!aCanHitB || !bCanHitA) continue;

            HEIN::CollisionManifold mainfold = HEIN::CollisionDispatcher::CheckCollision(colA, colB);
            if (mainfold.isColliding)
            {
                bool isATrigger = colA->IsTrigger();
                bool isBTrigger = colB->IsTrigger();

                colA->SetCollidingThisFrame(true);
                colB->SetCollidingThisFrame(true);

                if (isATrigger == false && isBTrigger == false)
                {
                    ResolvePhysicalOverlap(colA, colB, mainfold);
                }
                else
                {
                    HEIN::TriggerEventPayLoad payload;
                    payload.triggerA = colA;
                    payload.triggerB = colB;
                    gameContext.eventManager->DispatchTriggerEvent(payload);
                }
            }
        }
    }
}

void HEIN::PhysicsSystem::ResolvePhysicalOverlap(HEIN::ColliderComponent* colA, HEIN::ColliderComponent* colB, const HEIN::CollisionManifold& manifold)
{
    HEIN::Actor* actorA = colA->GetOwner();
    HEIN::RigidBodyComponent* rbA = actorA->GetComponent<HEIN::RigidBodyComponent>();
    HEIN::TransformComponent* transformA = actorA->GetComponent<HEIN::TransformComponent>();

    HEIN::Actor* actorB = colB->GetOwner();
    HEIN::RigidBodyComponent* rbB = actorB->GetComponent<HEIN::RigidBodyComponent>();
    HEIN::TransformComponent* transformB = actorB->GetComponent<HEIN::TransformComponent>();

    bool aIsDynamic = (rbA != nullptr && !rbA->isKinematic() && transformA != nullptr);
    bool bIsDynamic = (rbB != nullptr && !rbB->isKinematic() && transformB != nullptr);

    // ---------------------------------------------------------
    // SCENARIO 1: BOTH ARE DYNAMIC (Player hitting Enemy)
    // ---------------------------------------------------------
    if (aIsDynamic && bIsDynamic)
    {
        // Split the displacement so they push EACH OTHER equally!
        float halfDepth = manifold.penetrationDepth * 0.5f;

        // Push A away
        DirectX::SimpleMath::Vector3 posA = transformA->GetPosition();
        posA += (manifold.normal * halfDepth);
        transformA->SetPosition(posA);

        // Push B away (in the exact opposite direction)
        DirectX::SimpleMath::Vector3 posB = transformB->GetPosition();
        posB -= (manifold.normal * halfDepth);
        transformB->SetPosition(posB);
    }
    // ---------------------------------------------------------
    // SCENARIO 2: ONLY 'A' IS DYNAMIC (Player hitting a Wall)
    // ---------------------------------------------------------
    else if (aIsDynamic)
    {
        DirectX::SimpleMath::Vector3 currentPos = transformA->GetPosition();
        currentPos += (manifold.normal * manifold.penetrationDepth);
        transformA->SetPosition(currentPos);

        DirectX::SimpleMath::Vector3 currentVelocity = rbA->GetVelocity();
        float velocityIntoWall = currentVelocity.Dot(manifold.normal);

        if (velocityIntoWall < 0.0f)
        {
            DirectX::SimpleMath::Vector3 fixedVelocity = currentVelocity - (manifold.normal * velocityIntoWall);
            rbA->SetVelocity(fixedVelocity);
            if (manifold.normal.y > 0.5f) rbA->m_isGrounded = true;
        }
    }
    // ---------------------------------------------------------
    // SCENARIO 3: ONLY 'B' IS DYNAMIC (Enemy hitting a Wall)
    // ---------------------------------------------------------
    else if (bIsDynamic)
    {
        DirectX::SimpleMath::Vector3 flippedNormal = manifold.normal * -1.0f;

        DirectX::SimpleMath::Vector3 currentPos = transformB->GetPosition();
        currentPos += (flippedNormal * manifold.penetrationDepth);
        transformB->SetPosition(currentPos);

        DirectX::SimpleMath::Vector3 currentVelocity = rbB->GetVelocity();
        float velocityIntoWall = currentVelocity.Dot(flippedNormal);

        if (velocityIntoWall < 0.0f)
        {
            DirectX::SimpleMath::Vector3 fixedVelocity = currentVelocity - (flippedNormal * velocityIntoWall);
            rbB->SetVelocity(fixedVelocity);
            if (flippedNormal.y > 0.5f) rbB->m_isGrounded = true;
        }
    }
}

