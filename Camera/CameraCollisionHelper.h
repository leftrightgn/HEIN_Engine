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
		static float ResolveOcclusionDistance(
			ActorManager* manager,
			ActorID ignoredActorID,
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			float maxDistance,
			float cameraRadius = 1.0f,
			float minDistance = 2.0f
		)
		{
			if (manager == nullptr || maxDistance < 0.001f) return maxDistance;

			float closestHit = maxDistance;

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
							if (hitDist >= 0.0f)
							{
								float safeDist = std::max(minDistance, hitDist - cameraRadius);
								if (safeDist < closestHit)
								{
									closestHit = safeDist;
								}
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
					if (CollisionMath::SweepSphereVsAABB(rayOrigin, rayDir, cameraRadius, aabb->GetWorldAABB(), hitDist))
					{
						if (hitDist >= 0.0f)
						{
							float safeDist = std::max(minDistance, hitDist);
							if (safeDist < closestHit)
							{
								closestHit = safeDist;
							}
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
					if (CollisionMath::SweepSphereVSOBB(rayOrigin, rayDir, cameraRadius, obb->GetWorldOBB(), hitDist))
					{
						if (hitDist >= 0.0f)
						{
							float safeDist = std::max(minDistance, hitDist);
							if (safeDist < closestHit)
							{
								closestHit = safeDist;
							}
						}
					}
				}
			}

			return closestHit;
		}

		static DirectX::SimpleMath::Vector3 ResolveOcclusion(
			ActorManager* manager,
			ActorID ignoredActorID,
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& desiredCamPos,
			float cameraRadius = 1.0f,
			float minDistance = 2.0f
		)
		{
			if (manager == nullptr) return desiredCamPos;

			DirectX::SimpleMath::Vector3 toCamera = desiredCamPos - rayOrigin;
			float maxDist = toCamera.Length();
			if (maxDist < 0.001f) return desiredCamPos;

			DirectX::SimpleMath::Vector3 rayDir = toCamera / maxDist;
			float safeDist = ResolveOcclusionDistance(manager, ignoredActorID, rayOrigin, rayDir, maxDist, cameraRadius, minDistance);

			return rayOrigin + rayDir * safeDist;
		}
	};
}
