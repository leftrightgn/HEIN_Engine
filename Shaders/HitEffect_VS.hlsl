//--------------------------------------------------------------------------------------
// HitEffect_VS.hlsl
// Universal Stateless GPU Particle Vertex Shader (SV_VertexID + StructuredBuffer)
// Evaluates closed-form drag + gravity kinematics and velocity-stretched billboards on GPU
//--------------------------------------------------------------------------------------

#include "HitEffect_Common.hlsli"

StructuredBuffer<GPUParticleSeed> ParticleBuffer : register(t1);

static const float2 kQuadCorners[6] =
{
    float2(-1.0f, -1.0f),
    float2(-1.0f,  1.0f),
    float2( 1.0f, -1.0f),
    float2( 1.0f, -1.0f),
    float2(-1.0f,  1.0f),
    float2( 1.0f,  1.0f)
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output = (VSOutput)0;

    uint particleIdx = vertexID / 6;
    uint cornerIdx   = vertexID % 6;

    GPUParticleSeed p = ParticleBuffer[particleIdx];
    float age = CurrentTime - p.SpawnTime;

    if (p.Flags == 0u || (ActiveSlotIndex != 0xFFFFFFFFu && p.SlotIndex != ActiveSlotIndex) || p.Lifetime <= 0.0001f || age < 0.0f || age >= p.Lifetime)
    {
        return output;
    }

    float t = saturate(age / max(0.0001f, p.Lifetime));
    float3 accel = float3(0.0f, -p.Gravity, 0.0f);
    float3 vel = p.InitialVelocity;
    float3 pos = p.SpawnPos;

    if (p.Drag > 0.001f)
    {
        float invDrag   = 1.0f / p.Drag;
        float expFactor = exp(-p.Drag * age);
        float oneMinus  = 1.0f - expFactor;

        vel = p.InitialVelocity * expFactor + accel * (invDrag * oneMinus);
        pos = p.SpawnPos + p.InitialVelocity * (invDrag * oneMinus)
            + accel * (invDrag * (age - invDrag * oneMinus));
    }
    else
    {
        vel = p.InitialVelocity + accel * age;
        pos = p.SpawnPos + p.InitialVelocity * age + 0.5f * accel * (age * age);
    }

    float radius = lerp(p.StartSize, p.EndSize, t);
    float3 axisLong = CameraUp * radius;
    float3 axisLat  = CameraRight * radius;

    float speedSq = dot(vel, vel);
    if (p.StretchFactor > 0.0001f && speedSq > 0.01f)
    {
        float speed = sqrt(speedSq);
        float3 velDir = vel / speed;
        float3 side = cross(CameraForward, velDir);
        float sideLenSq = dot(side, side);
        side = (sideLenSq > 0.0001f) ? (side * rsqrt(sideLenSq)) : CameraRight;

        axisLong = velDir * (radius * (1.0f + p.StretchFactor * speed));
        axisLat  = side * radius;
    }

    float2 uv = kQuadCorners[cornerIdx];
    float3 worldCorner = pos + axisLat * uv.x + axisLong * uv.y;

    output.Pos      = mul(float4(worldCorner, 1.0f), ViewProj);
    output.Color    = lerp(p.StartColor, p.EndColor, t);
    output.TexCoord = uv;
    output.Params   = float2(t, p.RandomSeed);
    return output;
}

VSOutput VSMain(uint vertexID : SV_VertexID)
{
    return main(vertexID);
}
