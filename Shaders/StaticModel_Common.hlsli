//--------------------------------------------------------------------------------------
// StaticModel_Common.hlsli
// Shared structures and constant buffers for Static Model Shaders
//--------------------------------------------------------------------------------------

#ifndef STATIC_MODEL_COMMON_HLSLI
#define STATIC_MODEL_COMMON_HLSLI

// ==============================================================================
// CONSTANT BUFFERS (Data from C++)
// ==============================================================================
cbuffer cbMatrices : register(b0)
{
    matrix WorldViewProj;
    matrix World;
    matrix LightViewProj;       // Matrix from Light perspective for Shadow Mapping
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
};

// ==============================================================================
// STRUCTURES
// ==============================================================================
struct VSInput
{
    float3 Pos          : POSITION;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD0;
};

struct VSOutput
{
    float4 Pos           : SV_POSITION;
    float3 WorldPos      : TEXCOORD2;
    float3 Normal        : TEXCOORD3;
    float2 TexCoord      : TEXCOORD0;
    float4 LightSpacePos : TEXCOORD1; // Position from the Light's point of view
};

#endif // STATIC_MODEL_COMMON_HLSLI
