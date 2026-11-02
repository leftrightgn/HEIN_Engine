//--------------------------------------------------------------------------------------
// Particle_VS.hlsl
// Universal Stateless GPU Particle Vertex Shader (StructuredBuffer + SV_VertexID)
// Evaluates closed-form drag + gravity kinematics and expands camera-aligned /
// velocity-stretched billboard quads entirely on the GPU with zero CPU loop.
//--------------------------------------------------------------------------------------
#include "Particle_Common.hlsli"

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

    uint particleIndex = vertexID / 6;
    uint cornerIndex   = vertexID % 6;

    GPUParticleSeed p = ParticleBuffer[particleIndex];

    float age = CurrentTime - p.SpawnTime;
    if (p.Lifetime <= 0.0001f || age < 0.0f || age >= p.Lifetime)
    {
        // Degenerate offscreen clip-space triangle for inactive/expired particles (0 raster cost)
        output.Pos = float4(0.0f, 0.0f, -2.0f, 1.0f);
        return output;
    }

    float normAge = saturate(age / p.Lifetime);

    // Closed-form analytic integration of exponential air drag + gravity:
    // dv/dt = -k * v + g
    float k = max(p.Drag, 0.0f);
    float3 worldPos;
    float3 instVel;
    float3 gravityVec = float3(0.0f, -GravityAccel * p.GravityScale, 0.0f);

    if (k > 0.001f)
    {
        float expTerm = exp(-k * age);
        float integ   = (1.0f - expTerm) / k;
        worldPos = p.SpawnPos
                 + p.InitialVel * integ
                 + gravityVec * ((age - integ) / k);
        instVel  = p.InitialVel * expTerm
                 + gravityVec * integ;
    }
    else
    {
        worldPos = p.SpawnPos
                 + p.InitialVel * age
                 + 0.5f * gravityVec * (age * age);
        instVel  = p.InitialVel + gravityVec * age;
    }

    float radius = lerp(p.StartSize, p.EndSize, normAge);
    float4 color = lerp(p.StartColor, p.EndColor, normAge);

    float3 axisLong = CameraUp * radius;
    float3 axisLat  = CameraRight * radius;

    float speedSq = dot(instVel, instVel);
    if (p.StretchFactor > 0.0001f && speedSq > 0.01f)
    {
        float speed = sqrt(speedSq);
        float3 velDir = instVel / speed;
        float3 side = cross(CameraForward, velDir);
        float sideLenSq = dot(side, side);
        if (sideLenSq < 0.0001f)
        {
            side = CameraRight;
        }
        else
        {
            side *= rsqrt(sideLenSq);
        }

        float stretchedLength = radius * (1.0f + p.StretchFactor * speed);
        axisLong = velDir * stretchedLength;
        axisLat  = side * radius;
    }

    float2 corner = kQuadCorners[cornerIndex];
    float3 cornerWorldPos = worldPos + axisLat * corner.x + axisLong * corner.y;

    output.Pos      = mul(float4(cornerWorldPos, 1.0f), ViewProj);
    output.Color    = color;
    output.TexCoord = corner;
    output.Params   = float2(normAge, p.RandomSeed);
    return output;
}
