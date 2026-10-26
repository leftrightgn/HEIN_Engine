//--------------------------------------------------------------------------------------
// HitEffect_Common.hlsli
// Shared structures and constant buffers for Combat Impact VFX (Blood Splatter & Steel Sparks)
//--------------------------------------------------------------------------------------

cbuffer cbHitEffectParams : register(b0)
{
    row_major float4x4 ViewProj;
    float  EmissiveIntensity; // HDR brightness multiplier for incandescent steel sparks
    float  NoiseScale;        // Tiling frequency for organic blood splatter erosion
    float  ErosionStrength;   // Edge shredding strength as particles age
    float  PassMode;          // 0.0 = Blood Pass (Organic Noise-Eroded Alpha), 1.0 = Spark Pass (HDR Tapered Additive)
};

struct VSInput
{
    float3 Pos      : POSITION0;
    float4 Color    : COLOR0;
    float2 TexCoord : TEXCOORD0; // Quad local coordinates in [-1.0, 1.0]
    float2 Params   : TEXCOORD1; // x = normalized age [0.0..1.0], y = per-particle random phase [0.0..1.0]
};

struct VSOutput
{
    float4 Pos      : SV_POSITION;
    float4 Color    : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float2 Params   : TEXCOORD1;
};
