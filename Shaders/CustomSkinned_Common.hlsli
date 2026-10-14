//--------------------------------------------------------------------------------------
// CustomSkinned_Common.hlsli
// Shared structures and constant buffers for Skinned Model Shaders
//--------------------------------------------------------------------------------------

#ifndef CUSTOM_SKINNED_COMMON_HLSLI
#define CUSTOM_SKINNED_COMMON_HLSLI

// ==============================================================================
// CONSTANT BUFFERS (Data from C++)
// ==============================================================================
cbuffer cbMatrices : register(b0)
{
    matrix WorldViewProj;
    matrix World;
    matrix LightViewProj;       // Matrix from the Light's perspective for Shadows
    matrix BoneTransforms[256]; // Animation bone matrices
};

cbuffer cbLighting : register(b1)
{
    float3 LightPos;
    int LightType;              // 0 = Directional, 1 = Point, 2 = Spot
    
    float3 LightDir;
    float LightIntensity;
    
    float4 LightColor;
    
    float LightRange;
    float LightSpotAngle;       // Passed in radians
    float FogStart;
    float FogEnd;
    
    float4 FogColor;
    
    float3 CameraPos;
    float FogPadding;
    
    float4 GhostColor;          // (r, g, b, alphaMultiplier)
    int IsGhost;                // 1 = Ghost effect enabled, 0 = normal
    float GhostRimPower;        // Fresnel power (e.g. 3.0)
    float GhostRimIntensity;    // Glow intensity (e.g. 2.0)
    float GhostTime;            // Elapsed time for subtle aura breathing

    float4 HitFlashColor;       // (r, g, b, strength 0..1) - damage flash tint
};

// ==============================================================================
// STRUCTURES
// ==============================================================================
struct VSInput
{
    float3 Pos          : POSITION;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD0;
    uint4  BoneIndices  : BLENDINDICES0;
    float4 BoneWeights  : BLENDWEIGHT0;
};

struct VSOutput
{
    float4 Pos           : SV_POSITION;
    float3 WorldPos      : TEXCOORD2;
    float3 Normal        : TEXCOORD3;
    float2 TexCoord      : TEXCOORD0;
    float4 LightSpacePos : TEXCOORD1; // Position from the Light's point of view
};

#endif // CUSTOM_SKINNED_COMMON_HLSLI
