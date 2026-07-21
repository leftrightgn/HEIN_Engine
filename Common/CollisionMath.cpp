#include "pch.h"
#include "CollisionMath.h"
#include <Components/ColliderComponent/AABBColliderComponent.h>
#include <Components/ColliderComponent/ColliderComponent.h>
#include <Components/ColliderComponent/CapsuleColliderComponent.h>
#include <Components/ColliderComponent/OBBColliderComponent.h>
#include <Components/ColliderComponent/MeshColliderComponent.h>

HEIN::CollisionManifold HEIN::CollisionMath::CheckCapsuleVsOBB(HEIN::CapsuleColliderComponent* capsule, HEIN::OBBColliderComponent* obb)
{
    HEIN::CollisionManifold manifold;
    manifold.isColliding = false;

    if (capsule == nullptr || obb == nullptr) return manifold;
   
    DirectX::SimpleMath::Vector3 SegTop = capsule->GetWorldTopCenter();
    DirectX::SimpleMath::Vector3 SegBottom = capsule->GetWorldBottomCenter();

    DirectX::BoundingOrientedBox worldBox = obb->GetWorldOBB();
    DirectX::SimpleMath::Vector3 extents = worldBox.Extents;

    DirectX::SimpleMath::Quaternion boxRotation(worldBox.Orientation);

    DirectX::SimpleMath::Matrix cleanTransform =
        DirectX::SimpleMath::Matrix::CreateFromQuaternion(boxRotation) * DirectX::SimpleMath::Matrix::CreateTranslation(worldBox.Center);

    DirectX::SimpleMath::Matrix inverseTransform = cleanTransform.Invert();

    DirectX::SimpleMath::Vector3 localSegTop = DirectX::SimpleMath::Vector3::Transform(SegTop, inverseTransform);
    DirectX::SimpleMath::Vector3 localSegBottom = DirectX::SimpleMath::Vector3::Transform(SegBottom, inverseTransform);

    DirectX::SimpleMath::Vector3 d = localSegTop - localSegBottom;
    float len = d.Length();

    std::function<DirectX::SimpleMath::Vector3(const DirectX::SimpleMath::Vector3&)> clampToAABB =
        [&extents](const DirectX::SimpleMath::Vector3& p) -> DirectX::SimpleMath::Vector3
        {
            return DirectX::SimpleMath::Vector3(
                std::fmax(-extents.x, std::min(p.x, extents.x)),
                std::fmax(-extents.y, std::min(p.y, extents.y)),
                std::fmax(-extents.z, std::min(p.z, extents.z))
            );
        };

    DirectX::SimpleMath::Vector3 localClosestOnSeg = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Vector3 localClosestOnObb = clampToAABB(localClosestOnSeg);

    for (int i = 0; i < 3; ++i)
    {
        //Use localSegBottom instead of SegBottom!
        DirectX::SimpleMath::Vector3 toObb = localClosestOnObb - localSegBottom;
        float t = (len > 0.0001f) ? toObb.Dot(d) / (len * len) : 0.5f;
        t = std::fmax(0.0f, std::fmin(1.0f, t));
        localClosestOnSeg = localSegBottom + d * t;

        localClosestOnObb = clampToAABB(localClosestOnSeg);
    }

    DirectX::SimpleMath::Vector3 localDiff = localClosestOnSeg - localClosestOnObb;
    float distance = localDiff.Length();

    if (distance < capsule->GetRadius())
    {
        manifold.isColliding = true;
        manifold.penetrationDepth = capsule->GetRadius() - distance;

        // Rotate the normal back to world space using our clean matrix!
        if (distance > 0.0001f)
        {
            localDiff.Normalize();
            manifold.normal = DirectX::SimpleMath::Vector3::TransformNormal(localDiff, cleanTransform);
        }
        else
        {
            manifold.normal = DirectX::SimpleMath::Vector3::TransformNormal(DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f), cleanTransform);
        }

        manifold.contactPoint = DirectX::SimpleMath::Vector3::Transform(localClosestOnObb, cleanTransform);
    }

    return manifold;
}

HEIN::CollisionManifold HEIN::CollisionMath::CheckOBBvsOBB(HEIN::OBBColliderComponent* obbA, HEIN::OBBColliderComponent* obbB)
{
    HEIN::CollisionManifold maniflod;
    // TO DO::logic 
    return maniflod;
}

HEIN::CollisionManifold HEIN::CollisionMath::CheckCapsuleVsAABB(HEIN::CapsuleColliderComponent* capsule, HEIN::AABBColliderComponent* aabb)
{
    HEIN::CollisionManifold manifold;
    manifold.isColliding = false;   

    if (capsule == nullptr || aabb == nullptr) return manifold;

    
    DirectX::SimpleMath::Matrix aabbWorld = aabb->GetCalculateWorldMatrix();
    DirectX::SimpleMath::Vector3 aabbPos = aabbWorld.Translation();
    DirectX::SimpleMath::Vector3 scale(
        DirectX::SimpleMath::Vector3(aabbWorld._11, aabbWorld._12, aabbWorld._13).Length(),
        DirectX::SimpleMath::Vector3(aabbWorld._21, aabbWorld._22, aabbWorld._23).Length(),
        DirectX::SimpleMath::Vector3(aabbWorld._31, aabbWorld._32, aabbWorld._33).Length()
    );

    DirectX::SimpleMath::Vector3 extents = aabb->GetExtents() * scale;
    float halfHeight = capsule->GetHeight() * 0.5f;
    DirectX::SimpleMath::Vector3 capPos = capsule->GetCalculateWorldMatrix().Translation();

    // Capsule segment
    DirectX::SimpleMath::Vector3 SegTop = capsule->GetWorldTopCenter();
    DirectX::SimpleMath::Vector3 SegBottom = capsule->GetWorldBottomCenter();
    DirectX::SimpleMath::Vector3 d = SegTop - SegBottom;
    float len = d.Length();
    
    // Clamp point to AABB
    std::function<DirectX::SimpleMath::Vector3(const DirectX::SimpleMath::Vector3&)> clampToAABB =
        [&aabbPos, &extents](const DirectX::SimpleMath::Vector3& p) -> DirectX::SimpleMath::Vector3
        {
            return DirectX::SimpleMath::Vector3(
                std::fmax(aabbPos.x - extents.x, std::min(p.x, aabbPos.x + extents.x)),
                std::fmax(aabbPos.y - extents.y, std::min(p.y, aabbPos.y + extents.y)),
                std::fmax(aabbPos.z - extents.z, std::min(p.z, aabbPos.z + extents.z))
            );
        };

    // Find the closest point on the segment to the AABB
    DirectX::SimpleMath::Vector3 closestOnSeg = capPos; // Start at center
    DirectX::SimpleMath::Vector3 closestOnAABB = clampToAABB(closestOnSeg);
    
    // Perform a few iterations to find the closest points
    for (int i = 0; i < 3; ++i) {
        // Project closestOnAABB onto the segment
        DirectX::SimpleMath::Vector3 toAABB = closestOnAABB - SegBottom;
        float t = (len > 0.0001f) ? toAABB.Dot(d) / (len * len) : 0.5f;
        t = std::fmax(0.0f, std::fmin(1.0f, t));
        closestOnSeg = SegBottom + d * t;
        
        // Update closestOnAABB
        closestOnAABB = clampToAABB(closestOnSeg);
    }

    DirectX::SimpleMath::Vector3 diff = closestOnSeg - closestOnAABB;
    float distance = diff.Length();

    if (distance < capsule->GetRadius())
    {
        manifold.isColliding = true;
        manifold.penetrationDepth = capsule->GetRadius() - distance;

        if (distance > 0.001f)
        {
            diff.Normalize();
            manifold.normal = diff;
        }
        else
        {
            // If centers are identical, use a default normal (e.g., up)
            manifold.normal = DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);
        }
    }

    return manifold;
}

HEIN::CollisionManifold HEIN::CollisionMath::CheckCapsuleVsCapsule(HEIN::CapsuleColliderComponent* capsuleA, HEIN::CapsuleColliderComponent* capsuleB)
{
    HEIN::CollisionManifold manifold;
    manifold.isColliding = false;

    if (capsuleA == nullptr || capsuleB == nullptr) return manifold;

    // Get the line Segement for the both Capusule
    DirectX::SimpleMath::Vector3 p1 = capsuleA->GetWorldBottomCenter();
    DirectX::SimpleMath::Vector3 q1 = capsuleA->GetWorldTopCenter();
    DirectX::SimpleMath::Vector3 p2 = capsuleB->GetWorldBottomCenter();
    DirectX::SimpleMath::Vector3 q2 = capsuleB->GetWorldTopCenter();

    // Calculate the segement direction Vector
    DirectX::SimpleMath::Vector3 d1 = q1 - p1; // Direction of segment A
    DirectX::SimpleMath::Vector3 d2 = q2 - p2; // Direction of segment B
    DirectX::SimpleMath::Vector3 r = p1 - p2;

    float a = d1.Dot(d1); // Squared length of segment A
    float e = d2.Dot(d2); // Squared length of segment B
    float f = d2.Dot(r);

    float s = 0.0f;
    float t = 0.0f;

    const float EPSILON = 0.0001f;
    if (a <= EPSILON && e <= EPSILON)
    {
        s = 0.0f;
        t = 0.0f;
    }
    else if (a <= EPSILON)
    {
        s = 0.0f;
        t = f / e;
        t = std::fmax(0.0f, std::fmin(1.0f, t));
    }
    else
    {
        float c = d1.Dot(r);
        if (e <= EPSILON)
        {
            t = 0.0f;
            s = std::fmax(0.0f, std::fmin(1.0f, -c / a));
        }
        else
        {
            // The general non-degenerate case
            float b = d1.Dot(d2);
            float denom = a * e - b * b; // Denominator to check for parallel lines

            // If segments are not parallel, compute closest point on L1 to L2 and clamp to segment 1.
            if (denom != 0.0f)
            {
                s = std::fmax(0.0f, std::fmin(1.0f, (b * f - c * e) / denom));
            }
            else
            {
                s = 0.0f; // Lines are parallel, pick an arbitrary point on segment 1
            }

            // Compute point on L2 closest to S1(s) using s, clamp to segment 2
            t = (b * s + f) / e;

            if (t < 0.0f)
            {
                t = 0.0f;
                s = std::fmax(0.0f, std::fmin(1.0f, -c / a));
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = std::fmax(0.0f, std::fmin(1.0f, (b - c) / a));
            }

        }

    }

    // Calculate the actual closest points in 3D space
    DirectX::SimpleMath::Vector3 closestPointA = p1 + d1 * s;
    DirectX::SimpleMath::Vector3 closestPointB = p2 + d2 * t;

    // Check the distance between the closest points
    DirectX::SimpleMath::Vector3 diff = closestPointA - closestPointB;
    float distanceSq = diff.LengthSquared();
    float radiusSum = capsuleA->GetRadius() + capsuleB->GetRadius();

    // Resolve Collision
    if (distanceSq <= (radiusSum * radiusSum))
    {
        manifold.isColliding = true;

        float distance = std::sqrt(distanceSq);

        // Generate Penetration depth
        manifold.penetrationDepth = radiusSum - distance;

        // Generate Collision Normal (pointing from B to A to push A out)
        if (distance > EPSILON)
        {
            manifold.normal = diff / distance; // Normalize the diff vector
        }
        else
        {
            // If perfectly overlapping, push up by default
            manifold.normal = DirectX::SimpleMath::Vector3::Up;
        }

        // The exact point of contact in world space
        manifold.contactPoint = closestPointB + (manifold.normal * capsuleB->GetRadius());
    }

    return manifold;

}

HEIN::CollisionManifold HEIN::CollisionMath::CheckCapsuleVsMesh(HEIN::CapsuleColliderComponent* capsule, HEIN::MeshColliderComponent* mesh)
{
    HEIN::CollisionManifold manifold;
    manifold.isColliding = false;

    if (!capsule || !mesh) return manifold;

    DirectX::SimpleMath::Vector3 top = capsule->GetWorldTopCenter();
    DirectX::SimpleMath::Vector3 bottom = capsule->GetWorldBottomCenter();
    float radius = capsule->GetRadius();

    //  Calculate the exact total height of the player
    float capsuleHeight = DirectX::SimpleMath::Vector3::Distance(top, bottom) + (radius * 2.0f);

    // Define the Ray-Ring (1 Center, 4 Compass Points)
    // shrink the ring slightly (radius * 0.7f) so the rays don't accidentally catch on vertical walls.
    float ringOffset = radius * 0.7f;
    DirectX::SimpleMath::Vector3 rayOrigins[5] = {
        top,                                                              // Center
        top + DirectX::SimpleMath::Vector3(ringOffset, 0.0f, 0.0f),       // Right (+X)
        top + DirectX::SimpleMath::Vector3(-ringOffset, 0.0f, 0.0f),      // Left  (-X)
        top + DirectX::SimpleMath::Vector3(0.0f, 0.0f, ringOffset),       // Front (+Z)
        top + DirectX::SimpleMath::Vector3(0.0f, 0.0f, -ringOffset)       // Back  (-Z)
    };

    DirectX::SimpleMath::Vector3 rayDir(0.0f, -1.0f, 0.0f); // Pointing straight down
    float maxCheckDistance = capsuleHeight + 0.5f;

    // Variables to accumulate our smooth ring data
    int validHits = 0;
    DirectX::SimpleMath::Vector3 accumulatedNormal = DirectX::SimpleMath::Vector3::Zero;
    float highestHitY = -FLT_MAX;

    const std::vector<Triangle>& triangles = mesh->GetWorldTriangles();

    // Fire all 5 Rays
    for (int i = 0; i < 5; ++i)
    {
        float closestHitDistance = FLT_MAX;
        DirectX::SimpleMath::Vector3 bestNormal;
        bool hitSomethingForThisRay = false;

        // Loop through triangles for this specific ray
        for (const Triangle& tri : triangles)
        {
            float hitDistance = 0.0f;
            DirectX::SimpleMath::Vector3 hitNormal;

            if (IntersectRayTriangle(rayOrigins[i], rayDir, tri, hitDistance, hitNormal))
            {
                if (hitDistance <= maxCheckDistance && hitDistance < closestHitDistance)
                {
                    closestHitDistance = hitDistance;
                    bestNormal = hitNormal;
                    hitSomethingForThisRay = true;
                }
            }
        }

        // If this specific ray hit the floor, add its data to our averages!
        if (hitSomethingForThisRay)
        {
            validHits++;
            accumulatedNormal += bestNormal;

            // Calculate the exact Y position of this hit
            float hitY = rayOrigins[i].y - closestHitDistance;

            // We want the HIGHEST hit point to prevent the player's toes from clipping into slopes
            if (hitY > highestHitY)
            {
                highestHitY = hitY;
            }
        }
    }

    // Final Resolution (If at least one ray hit the floor)
    if (validHits > 0)
    {
        // Average the accumulated normals and normalize the result to get a perfectly smooth slope vector
        accumulatedNormal /= static_cast<float>(validHits);
        accumulatedNormal.Normalize();

        // Find the absolute lowest pixel of the player's feet
        float playerFeetY = bottom.y - radius;

        // If the HIGHEST floor point we hit is higher than the player's feet, push the player UP!
        if (playerFeetY < highestHitY + 0.01f)
        {
            manifold.isColliding = true;
            manifold.normal = accumulatedNormal; // Smooth blended normal!
            manifold.penetrationDepth = highestHitY - playerFeetY;
        }
    }

    return manifold;
}

bool HEIN::CollisionMath::IntersectRayTriangle(
    const DirectX::SimpleMath::Vector3& rayOrigin, 
    const DirectX::SimpleMath::Vector3& rayDir, 
    const Triangle& triangle, 
    float& outDistance, 
    DirectX::SimpleMath::Vector3& outNormal
)
{
    DirectX::SimpleMath::Vector3 edge1 = triangle.v1 - triangle.v0;
    DirectX::SimpleMath::Vector3 edge2 = triangle.v2 - triangle.v0;
    DirectX::SimpleMath::Vector3 h = rayDir.Cross(edge2);

    float a = edge1.Dot(h);
    if (a > -0.0001f && a < 0.0001f) return false; // Ray is Parallel To Triangle

    float f = 1.0f / a;
    DirectX::SimpleMath::Vector3 s = rayOrigin - triangle.v0;
    float u = f * s.Dot(h);
    if (u < 0.0f || u > 1.0f) return false;

    DirectX::SimpleMath::Vector3 q = s.Cross(edge1);
    float v = f * rayDir.Dot(q); 
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * edge2.Dot(q);
    if (t > 0.0001f)
    {
        outDistance = t;
        outNormal = edge1.Cross(edge2);
        outNormal.Normalize();

        return true;
    }
    return false;

}
