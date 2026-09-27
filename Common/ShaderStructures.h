#pragma once
#include <SimpleMath.h>

namespace HEIN
{
    // __declspec(align(16)) forces the CPU to align this data perfectly for the GPU
    __declspec(align(16)) struct CB_Matrices
    {
        DirectX::SimpleMath::Matrix WorldViewProj;
        DirectX::SimpleMath::Matrix World;
        DirectX::SimpleMath::Matrix LightViewProj; // Used to project vertices into the shadow map
        DirectX::SimpleMath::Matrix BoneTransforms[256]; // Max 256 bones for modern models
    };

    __declspec(align(16)) struct CB_Lighting
    {
        // 16 bytes: Vector3 (12 bytes) + int (4 bytes)
        DirectX::SimpleMath::Vector3 LightPos;
        int LightType; // 0 = Directional, 1 = Point, 2 = Spot

        // 16 bytes: Vector3 (12 bytes) + float (4 bytes)
        DirectX::SimpleMath::Vector3 LightDir;
        float LightIntensity;

        // 16 bytes: Vector4 (16 bytes)
        DirectX::SimpleMath::Vector4 LightColor;

        // 16 bytes: float (4 bytes) + float (4 bytes) + Vector2 (8 bytes)
        float LightRange;
        float LightSpotAngle;
        DirectX::SimpleMath::Vector2 Padding; // Dead space to reach the 16-byte boundary
    };
}