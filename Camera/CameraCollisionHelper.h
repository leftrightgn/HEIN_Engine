#pragma once
#include <SimpleMath.h>
#include <Entities/ActorManager.h>
#include <Components/ColliderComponent/MeshColliderComponent.h>
#include <Components/ColliderComponent/AABBColliderComponent.h>
#include <Components/ColliderComponent/OBBColliderComponent.h>
#include <Common/CollisionMath.h>
#include <vector>
#include <algorithm>

namespace HEIN
{
	class CameraCollisionHelper
	{
	public:
		static DirectX::SimpleMath::Vector3 ResolveOcclusion(
			ActorManager* manager,
			ActorID ignoredActorID,
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& desiredCamPos,
			float cameraRadius = 0.5f,
			float minDistance = 0.8f
		)
		{
			if (manager == nullptr) return desiredCamPos;

			DirectX::SimpleMath::Vector3 toCamera = desiredCamPos - rayOrigin;
			float maxDist = toCamera.Length();
			if (maxDist < 0.001f) return desiredCamPos;

			DirectX::SimpleMath::Vector3 rayDir = toCamera / maxDist;
			float closestHit = maxDist;

			for (const auto& pair : manager->GetAllActors())
			{
				Actor* actor = pair.second.get();
				if (actor == nullptr || actor->GetID() == ignoredActorID) continue;

				// Mesh Colliders
				std::vector<MeshColliderComponent*> meshes = actor->GetComponents<MeshColliderComponent>();
				for (auto* mesh : meshes)
				{
					if (mesh == nullptr || mesh->IsTrigger()) continue;
					mesh->SyncColliderState();
					const auto& triangles = mesh->GetWorldTriangles();
					for (const auto& tri : triangles)
					{
						float hitDist = 0.0f;
						DirectX::SimpleMath::Vector3 hitNormal;
						if (CollisionMath::IntersectRayTriangle(rayOrigin, rayDir, tri, hitDist, hitNormal))
						{
							if (hitDist > 0.1f && hitDist < closestHit)
							{
								closestHit = hitDist;
							}
						}
					}
				}

				// AABB Colliders
				std::vector<AABBColliderComponent*> aabbs = actor->GetComponents<AABBColliderComponent>();
				for (auto* aabb : aabbs)
				{
					if (aabb == nullptr || aabb->IsTrigger()) continue;
					aabb->SyncColliderState();
					float hitDist = 0.0f;
					if (aabb->GetWorldAABB().Intersects(rayOrigin, rayDir, hitDist))
					{
						if (hitDist > 0.1f && hitDist < closestHit)
						{
							closestHit = hitDist;
						}
					}
				}

				// OBB Colliders
				std::vector<OBBColliderComponent*> obbs = actor->GetComponents<OBBColliderComponent>();
				for (auto* obb : obbs)
				{
					if (obb == nullptr || obb->IsTrigger()) continue;
					obb->SyncColliderState();
					float hitDist = 0.0f;
					if (obb->GetWorldOBB().Intersects(rayOrigin, rayDir, hitDist))
					{
						if (hitDist > 0.1f && hitDist < closestHit)
						{
							closestHit = hitDist;
						}
					}
				}
			}

			if (closestHit < maxDist)
			{
				float adjustedDist = (std::max)(closestHit - cameraRadius, minDistance);
				return rayOrigin + rayDir * adjustedDist;
			}

			return desiredCamPos;
		}
	};
}
