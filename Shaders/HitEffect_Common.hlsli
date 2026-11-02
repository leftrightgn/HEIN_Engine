//--------------------------------------------------------------------------------------
// HitEffect_Common.hlsli
// Shared GPU Particle Contract (Universal Vertex Shader + Pluggable Pixel Shaders)
//--------------------------------------------------------------------------------------

cbuffer cbParticleFrame : register(b0)
{
    row_major float4x4 ViewProj;
    float3 CameraRight;
    float  CurrentTime;
    float3 CameraUp;
    uint   ActiveSlotIndex;
    float3 CameraForward;
    float  ShaderStyleID;
    float4 EffectShaderParams; // x = EmissiveIntensity, y = NoiseScale, z = ErosionStrength, w = CustomParam
};

struct GPUParticleSeed
{
    float3 SpawnPos;
    float  SpawnTime;
    float3 InitialVelocity;
    float  Lifetime;
    float4 StartColor;
    float4 EndColor;
    float  StartSize;
    float  EndSize;
    float  Drag;
    float  Gravity;
    float  StretchFactor;
    float  RandomSeed;
    uint   SlotIndex;
    uint   Flags;
};

struct VSOutput
{
    float4 Pos      : SV_POSITION;
    float4 Color    : COLOR0;
    float2 TexCoord : TEXCOORD0; // Quad local coordinates in [-1.0, 1.0]
    float2 Params   : TEXCOORD1; // x = normalized age [0.0..1.0], y = per-particle random phase [0.0..1.0]
};
