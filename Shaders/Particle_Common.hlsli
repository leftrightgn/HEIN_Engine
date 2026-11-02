//--------------------------------------------------------------------------------------
// Particle_Common.hlsli
// Shared structures and constant buffers for the Stateless GPU Particle System
//--------------------------------------------------------------------------------------

cbuffer cbParticleFrame : register(b0)
{
    row_major float4x4 ViewProj;
    float3 CameraRight;
    float  CurrentTime;
    float3 CameraUp;
    float  FramePadding;
    float3 CameraForward;
    float  GravityAccel;
    float4 ShaderParams; // x = EmissiveIntensity, y = NoiseScale, z = ErosionStrength, w = StyleIndex (0=Eroded, 1=Streak, 2=Wisp, 3=Ring)
};

// 96-byte (6 x float4) GPU Particle Seed stored in StructuredBuffer<GPUParticleSeed> : register(t1)
struct GPUParticleSeed
{
    float3 SpawnPos;
    float  SpawnTime;

    float3 InitialVel;
    float  Lifetime;

    float4 StartColor;
    float4 EndColor;

    float  StartSize;
    float  EndSize;
    float  Drag;
    float  GravityScale;

    float  StretchFactor;
    float  RandomSeed;
    uint   SlotIndex;
    float  SeedPadding;
};

struct VSOutput
{
    float4 Pos      : SV_POSITION;
    float4 Color    : COLOR0;
    float2 TexCoord : TEXCOORD0; // Quad local coordinates in [-1.0, 1.0]
    float2 Params   : TEXCOORD1; // x = normalized age [0.0..1.0], y = per-particle random seed [0.0..1.0]
};
