#include "pch.h"
#include "PhysicsSystem.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include "Components/RigidBodyComponent.h"
#include "Components/TransformComponent.h"
#include "Components/ColliderComponent/ColliderComponent.h"
#include "Components/ColliderComponent/TerrainColliderComponent.h"
#include "../../../Dual/BlackBoard/CombatBlackBoard.h"


void HEIN::PhysicsSystem::UpdateMovement(GameContext& gameContext, HEIN::ActorManager& actorManager, float deltaTime)
{
    // Dynamic terrain collider lookup for ground alignment
    HEIN::TerrainColliderComponent* activeTerrain = nullptr;
    for (auto& pair : actorManager.GetAllActors())
    {
        activeTerrain = pair.second->GetComponent<HEIN::TerrainColliderComponent>();
        if (activeTerrain) break;
    }

    // Player position query for distance-based simulation culling
    DirectX::SimpleMath::Vector3 playerPos = DirectX::SimpleMath::Vector3::Zero;
    HEIN::Actor* playerActor = actorManager.GetActorByName(L"Player");
    if (playerActor != nullptr)
    {
        HEIN::TransformComponent* playerTrans = playerActor->GetComponent<HEIN::TransformComponent>();
        if (playerTrans != nullptr)
        {
            playerPos = playerTrans->GetPosition();
        }
    }

    const float SIMULATION_RADIUS_SQ = 200.0f * 200.0f;

    // Traverse all active actors in the registry
    for (auto& pair : actorManager.GetAllActors())
    {
        HEIN::Actor* actor = pair.second.get();
        HEIN::RigidBodyComponent* rb = actor->GetComponent<HEIN::RigidBodyComponent>();
        HEIN::TransformComponent* transform = actor->GetComponent<HEIN::TransformComponent>();

        if (rb) rb->m_isGrounded = false;

        std::vector<HEIN::ColliderComponent*> actorColliders = actor->GetComponents<HEIN::ColliderComponent>();
        for (HEIN::ColliderComponent* col : actorColliders)
        {
            col->SetCollidingThisFrame(false);
        }

        if (!rb || !transform) continue;

        // Initial Ground Snapping & Spawn Anchor Persistence
        if (rb->NeedsInitialSnap() && activeTerrain != nullptr)
        {
            DirectX::SimpleMath::Vector3 pos = transform->GetPosition();
            float terrainHeight = 0.0f;
            DirectX::SimpleMath::Vector3 normal;

            if (activeTerrain->GetHeightAtPosition(pos.x, pos.z, terrainHeight, normal))
            {
                // Align vertical elevation to terrain contact surface with offset
                pos.y = terrainHeight + 0.5f;
                transform->SetPosition(pos);
            }

            // Persist initial ground position as spawn anchor for tether return logic
            HEIN::CombatBlackBoard* bb = actor->GetComponent<HEIN::CombatBlackBoard>();
            if (bb)
            {
                bb->spawnPosition = pos;
                bb->hasSetSpawnPosition = true;
            }

            // Deactivate initial snap flag after first successful alignment
            rb->SetNeedsInitialSnap(false);
        }

        // Distance-Based Simulation Culling (Hibernation)
        // Exempt Player and primary environmental roots from simulation culling
        if (actor->GetActorType() != HEIN::ActorType::Player && actor->GetTag() != L"StageRoot")
        {
            float distSq = DirectX::SimpleMath::Vector3::DistanceSquared(playerPos, transform->GetPosition());

            if (distSq > SIMULATION_RADIUS_SQ)
            {
                // Off-Screen Distance Reset: return dormant actors to their anchor coordinate
                HEIN::CombatBlackBoard* bb = actor->GetComponent<HEIN::CombatBlackBoard>();
                if (bb && bb->hasSetSpawnPosition)
                {
                    transform->SetPosition(bb->spawnPosition);
                    bb->moveIntent = DirectX::SimpleMath::Vector3::Zero;
                }

                // Bypass remaining dynamics calculations for dormant actor
                continue;
            }
        }

        // Numerical Integration (Semi-Implicit Euler: Acceleration -> Velocity -> Position)
        if (!rb->isKinematic())
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

    // Gather active collider components across all actors
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

    // Narrowphase collision pair detection and manifold resolution
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

                // Separation of Solid Pushboxes from Trigger Hitboxes.
                // Solid pushboxes block movement and resolve penetration via MTV.
                // Trigger hitboxes pass through and dispatch damage events.
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

