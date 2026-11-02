//--------------------------------------------------------------------------------------
// Particle_PS.hlsl
// Pluggable Procedural Pixel Shader for the Stateless GPU Particle System
// Supports procedural Noise-Eroded Droplets/Mist, Velocity Streak Sparks,
// Ethereal Soul/Fire Wisps, and Expanding Shock/Pulse Rings with zero external textures.
//--------------------------------------------------------------------------------------
#include "Particle_Common.hlsli"

Texture2D    NoiseTex      : register(t0);
SamplerState LinearSampler : register(s0);

// Style: Organic Noise-Eroded Droplet & Mist (Blood, Mud, Stone Debris, Poison)
float4 EvalErodedDropletPS(VSOutput input)
{
    float2 uv = input.TexCoord;
    float  r  = length(uv);
    if (r > 1.0f)
    {
        discard;
    }

    float normAge    = saturate(input.Params.x);
    float randOffset = input.Params.y;

    float radialMask = saturate(1.0f - r * r);
    float2 noiseUV = uv * (0.5f * max(ShaderParams.y, 0.2f))
                   + float2(randOffset * 3.17f, randOffset * 5.71f + normAge * 0.45f);
    float noiseVal = NoiseTex.Sample(LinearSampler, noiseUV).r;

    float erosionThreshold = normAge * max(ShaderParams.z, 0.1f) * 0.85f;
    float alpha = smoothstep(erosionThreshold, erosionThreshold + 0.35f, radialMask * (0.42f + 0.58f * noiseVal));
    if (alpha <= 0.01f)
    {
        discard;
    }

    float3 rgb = input.Color.rgb * (0.65f + 0.50f * (1.0f - r)) * max(ShaderParams.x, 0.2f);
    return float4(rgb, saturate(input.Color.a * alpha));
}

// Style: Anisotropic Incandescent Streak / Spark (Parry Sparks, Sword Clashes, Lightning)
float4 EvalStreakSparkPS(VSOutput input)
{
    float2 uv = input.TexCoord;
    float  r  = length(uv);
    if (r > 1.0f)
    {
        discard;
    }

    float normAge    = saturate(input.Params.x);
    float randOffset = input.Params.y;

    float coreMask = exp(-12.0f * (uv.x * uv.x)) * saturate(1.0f - abs(uv.y));
    float haloMask = saturate(1.0f - r);

    float2 noiseUV = uv * float2(0.5f, 1.5f) * max(ShaderParams.y, 0.2f)
                   + float2(randOffset * 2.1f, -normAge * 2.5f);
    float flicker = 0.72f + 0.28f * NoiseTex.Sample(LinearSampler, noiseUV).r;

    float intensity = (coreMask * 1.45f + haloMask * 0.55f) * (1.0f - normAge * 0.75f) * flicker;
    if (intensity <= 0.01f)
    {
        discard;
    }

    float3 hotWhite = float3(1.0f, 0.98f, 0.92f);
    float3 rgb = lerp(input.Color.rgb, hotWhite, saturate(coreMask * 0.75f)) * max(ShaderParams.x, 0.5f) * intensity;
    return float4(rgb, saturate(input.Color.a * intensity));
}

// Style: Ethereal Swirling Wisp / Flame (Ghost Soul Ectoplasm, Fire Embers, Torch Aura)
float4 EvalSoftWispPS(VSOutput input)
{
    float2 uv = input.TexCoord;
    float  r  = length(uv);
    if (r > 1.0f)
    {
        discard;
    }

    float normAge    = saturate(input.Params.x);
    float randOffset = input.Params.y;

    float angle = (normAge * 2.4f + randOffset * 6.28318f);
    float s = sin(angle);
    float c = cos(angle);
    float2 rotUV = float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);

    float2 noiseUV = rotUV * (0.45f * max(ShaderParams.y, 0.2f))
                   + float2(randOffset * 4.3f, -normAge * 0.85f);
    float n1 = NoiseTex.Sample(LinearSampler, noiseUV).r;
    float n2 = NoiseTex.Sample(LinearSampler, noiseUV * 2.1f + float2(0.37f, normAge * 0.6f)).r;
    float wispNoise = lerp(n1, n2, 0.45f);

    float softFalloff = pow(saturate(1.0f - r), 1.65f);
    float erode = normAge * max(ShaderParams.z, 0.1f) * 0.65f;
    float alpha = smoothstep(erode, erode + 0.45f, softFalloff * (0.35f + 0.65f * wispNoise));
    if (alpha <= 0.01f)
    {
        discard;
    }

    float3 innerGlow = lerp(input.Color.rgb, float3(1.0f, 1.0f, 1.0f), saturate((1.0f - r) * 0.45f));
    float3 rgb = innerGlow * max(ShaderParams.x, 0.2f) * (0.75f + 0.5f * wispNoise);
    return float4(rgb, saturate(input.Color.a * alpha));
}

// Style: Expanding Shock Ring / Pulse Halo (Parry Shockwave, Heavy Slam Ring, Portal Ripple)
float4 EvalShockRingPS(VSOutput input)
{
    float2 uv = input.TexCoord;
    float  r  = length(uv);
    if (r > 1.0f)
    {
        discard;
    }

    float normAge    = saturate(input.Params.x);
    float randOffset = input.Params.y;

    float ringRadius = lerp(0.25f, 0.88f, saturate(normAge * 0.85f + 0.15f));
    float ringWidth  = max(0.06f, 0.22f * (1.0f - normAge * 0.55f));
    float ringDist   = abs(r - ringRadius);
    float ringMask   = smoothstep(ringWidth, 0.0f, ringDist);

    float2 noiseUV = uv * max(ShaderParams.y, 0.2f) + float2(randOffset, normAge * 0.5f);
    float noiseVal = NoiseTex.Sample(LinearSampler, noiseUV).r;

    float alpha = ringMask * (0.55f + 0.45f * noiseVal) * (1.0f - normAge);
    if (alpha <= 0.01f)
    {
        discard;
    }

    float3 rgb = input.Color.rgb * max(ShaderParams.x, 0.5f) * (1.0f + ringMask * 0.65f);
    return float4(rgb, saturate(input.Color.a * alpha));
}

float4 main(VSOutput input) : SV_Target
{
    int style = (int)(ShaderParams.w + 0.5f);
    if (style == 1)
    {
        return EvalStreakSparkPS(input);
    }
    else if (style == 2)
    {
        return EvalSoftWispPS(input);
    }
    else if (style == 3)
    {
        return EvalShockRingPS(input);
    }
    return EvalErodedDropletPS(input);
}
