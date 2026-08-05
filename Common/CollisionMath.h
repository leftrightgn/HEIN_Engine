#pragma once


namespace HEIN
{
	class CapsuleColliderComponent;
	class OBBColliderComponent;
	class AABBColliderComponent;
	class MeshColliderComponent;

	struct CollisionManifold
	{
		bool isColliding = false;
		DirectX::SimpleMath::Vector3 normal = DirectX::SimpleMath::Vector3::Zero; // The direction to push out
		float penetrationDepth = 0.0f;                                            // How far to push out
		DirectX::SimpleMath::Vector3 contactPoint = DirectX::SimpleMath::Vector3::Zero;
	};

	struct Triangle
	{
		DirectX::SimpleMath::Vector3 v0, v1, v2;
	};

	class CollisionMath
	{
	public:

		static HEIN::CollisionManifold CheckCapsuleVsOBB(HEIN::CapsuleColliderComponent* capsule, HEIN::OBBColliderComponent* obb);

		static HEIN::CollisionManifold CheckOBBvsOBB(HEIN::OBBColliderComponent* obbA, HEIN::OBBColliderComponent* obbB);

		static HEIN::CollisionManifold CheckCapsuleVsAABB(HEIN::CapsuleColliderComponent* capsule, HEIN::AABBColliderComponent* aabb);

		static HEIN::CollisionManifold CheckCapsuleVsCapsule(HEIN::CapsuleColliderComponent* capsuleA, HEIN::CapsuleColliderComponent* capsuleB);

		static HEIN::CollisionManifold CheckCapsuleVsMesh(HEIN::CapsuleColliderComponent* capsule, HEIN::MeshColliderComponent* mesh);

		static bool IntersectRayTriangle(
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			const Triangle& triangle,
			float& outDistance,
			DirectX::SimpleMath::Vector3& outNormal
		);

		static bool SweepSphereVSTriangle(
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			float sphereRadius,
			const Triangle& triangle,
			float& outDistance
		);

		static bool SweepSphereVsAABB(
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			float sphereRadius,
			const DirectX::BoundingBox& aabb,
			float& outHitDistance
		);

		static bool SweepSphereVSOBB(
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			float sphereRadius,
			const DirectX::BoundingOrientedBox& obb,
			float& outHitDistance
		);

		static bool RaySphereIntersect(
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			const DirectX::SimpleMath::Vector3& center,
			float radius,
			float& time
		);

		static bool RayCapsuleIntersect(
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			const DirectX::SimpleMath::Vector3& pointA,
			const DirectX::SimpleMath::Vector3& pointB,
			float radius,
			float& time
		);
	};
}
