//--------------------------------------------------------------------------------------
// WeaponTrail_Common.hlsli
// Shared structures and constant buffers for Elden Ring-style Weapon Ribbon Trail VFX
//--------------------------------------------------------------------------------------

#ifndef WEAPON_TRAIL_COMMON_HLSLI
#define WEAPON_TRAIL_COMMON_HLSLI

cbuffer cbTrailParams : register(b0)
{
    matrix ViewProj;             // World-space trail vertices -> Clip Space
    float4 CoreColor;            // Leading edge & tip hot-core color (RGB + Alpha)
    float4 EdgeColor;            // Main arc body & dissolving tail color (RGB + Alpha)
    float  EmissiveIntensity;    // HDR brightness multiplier
    float  TipFalloffPower;      // Radial concentration toward the weapon tip
    float  NoiseScale;           // Tiling scale for wispy streak erosion
    float  NoiseScrollSpeed;     // Anisotropic scroll speed along swing arc
    float  Time;                 // Elapsed time for scrolling noise
    float  ErosionStrength;      // 0 = smooth ribbon, 1 = shredded Elden Ring air-cut streaks
    float  LeadingEdgeSharpness; // Falloff rate of the bright leading-edge flash
    float  TrailPadding;         // 16-byte alignment padding
};

struct VSInput
{
    float3 Pos      : POSITION;
    float2 TexCoord : TEXCOORD0; // x = normalized age [0=newest, 1=oldest], y = blade span [0=base, 1=tip]
    float4 Color    : COLOR0;    // Per-slice vertex color / alpha modulation
};

struct VSOutput
{
    float4 Pos      : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR0;
};

#endif // WEAPON_TRAIL_COMMON_HLSLI
