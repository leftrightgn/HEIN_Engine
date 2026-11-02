//--------------------------------------------------------------------------------------
// HitEffect_PS.hlsl
// Pluggable Procedural Particle Pixel Shaders:
// - Particle_Eroded_PS: Dual-octave noise-eroded organic droplets & splatter
// - Particle_Streak_PS: Anisotropic diamond-tapered HDR incandescent streaks & sparks
// - Particle_Wisp_PS:   Volumetric swirling mist, smoke, dust, & soul wisps
// - Particle_Ring_PS:   Expanding shockwave ring & impact flash pulse
//--------------------------------------------------------------------------------------

#include "HitEffect_Common.hlsli"

Texture2D NoiseMap : register(t0);
SamplerState LinearWrapSampler : register(s0);

float SampleDualOctaveNoise(float2 p, float age, float seed, float noiseScale)
{
    float2 baseUV = p * 0.5f + 0.5f;
    float2 noiseUV1 = baseUV * noiseScale + float2(seed * 3.17f, seed * 7.43f);
    float2 noiseUV2 = baseUV * (noiseScale * 2.15f) - float2(seed * 5.89f, age * 0.65f);

    float n1 = NoiseMap.Sample(LinearWrapSampler, noiseUV1).r;
    float n2 = NoiseMap.Sample(LinearWrapSampler, noiseUV2).r;
    return saturate(n1 * 0.65f + n2 * 0.35f);
}

float4 EvalErodedDropletPS(VSOutput input)
{
    float2 p = input.TexCoord;
    float rSq = dot(p, p);
    if (rSq > 1.0f)
    {
        discard;
    }

    float age  = saturate(input.Params.x);
    float seed = input.Params.y;
    float emissiveIntensity = max(0.05f, EffectShaderParams.x);
    float noiseScale        = max(0.10f, EffectShaderParams.y);
    float erosionStrength   = saturate(EffectShaderParams.z);

    float combinedNoise = SampleDualOctaveNoise(p, age, seed, noiseScale);
    float radialMask = saturate(1.0f - rSq);

    float noisyRadius = radialMask * lerp(1.0f, combinedNoise * 1.45f, erosionStrength);
    float erosionThreshold = age * 0.62f * erosionStrength;
    float splatterAlpha = smoothstep(erosionThreshold, erosionThreshold + 0.32f, noisyRadius);

    float coreDensity = smoothstep(0.25f, 0.92f, radialMask);
    float3 dropletRGB = input.Color.rgb * lerp(0.58f, 1.18f, coreDensity) * emissiveIntensity;

    float finalAlpha = saturate(splatterAlpha * input.Color.a);
    if (finalAlpha < 0.005f)
    {
        discard;
    }

    return float4(dropletRGB * finalAlpha, finalAlpha);
}

float4 EvalStreakSparkPS(VSOutput input)
{
    float2 p = input.TexCoord;
    float rSq = dot(p, p);
    if (rSq > 1.0f)
    {
        discard;
    }

    float age  = saturate(input.Params.x);
    float seed = input.Params.y;
    float emissiveIntensity = max(0.05f, EffectShaderParams.x);
    float noiseScale        = max(0.10f, EffectShaderParams.y);

    float combinedNoise = SampleDualOctaveNoise(p, age, seed, noiseScale);

    float crossPinch = exp(-abs(p.x) * 4.2f) * saturate(1.0f - abs(p.x));
    float longTaper  = saturate(1.0f - p.y * p.y);
    float streakBody = crossPinch * longTaper;

    float sparkNoise = lerp(0.82f, 1.22f, combinedNoise);
    float hotCore = exp(-rSq * 6.5f);
    float3 whiteHot = float3(1.0f, 0.98f, 0.90f);
    float3 sparkRGB = lerp(input.Color.rgb, whiteHot, saturate(hotCore * 0.78f));
    sparkRGB *= emissiveIntensity * sparkNoise;

    float finalAlpha = saturate((streakBody * 0.90f + hotCore * 0.55f) * (1.0f - age * 0.75f) * input.Color.a);
    if (finalAlpha < 0.004f)
    {
        discard;
    }

    return float4(sparkRGB * finalAlpha, finalAlpha);
}

float4 EvalSoftWispPS(VSOutput input)
{
    float2 p = input.TexCoord;
    float rSq = dot(p, p);
    if (rSq > 1.0f)
    {
        discard;
    }

    float age  = saturate(input.Params.x);
    float seed = input.Params.y;
    float emissiveIntensity = max(0.05f, EffectShaderParams.x);
    float noiseScale        = max(0.10f, EffectShaderParams.y);
    float erosionStrength   = saturate(EffectShaderParams.z);

    float swirlAngle = (seed - 0.5f) * 3.14159f + age * 1.6f;
    float s = sin(swirlAngle);
    float c = cos(swirlAngle);
    float2 swirledP = float2(p.x * c - p.y * s, p.x * s + p.y * c);

    float combinedNoise = SampleDualOctaveNoise(swirledP, age, seed, noiseScale);
    float softRadial = exp(-rSq * 2.35f) * saturate(1.0f - rSq);
    float cloudMask = softRadial * lerp(1.0f, combinedNoise * 1.35f, erosionStrength);

    float fadeEnvelope = smoothstep(0.0f, 0.18f, age) * (1.0f - age);
    float finalAlpha = saturate(cloudMask * fadeEnvelope * input.Color.a);
    if (finalAlpha < 0.004f)
    {
        discard;
    }

    float3 wispRGB = input.Color.rgb * lerp(0.75f, 1.25f, combinedNoise) * emissiveIntensity;
    return float4(wispRGB * finalAlpha, finalAlpha);
}

float4 EvalShockRingPS(VSOutput input)
{
    float2 p = input.TexCoord;
    float rSq = dot(p, p);
    if (rSq > 1.0f)
    {
        discard;
    }

    float age  = saturate(input.Params.x);
    float seed = input.Params.y;
    float emissiveIntensity = max(0.05f, EffectShaderParams.x);
    float noiseScale        = max(0.10f, EffectShaderParams.y);
    float ringSharpness     = lerp(4.5f, 12.0f, saturate(EffectShaderParams.z));

    float r = sqrt(rSq);
    float combinedNoise = SampleDualOctaveNoise(p, age, seed, noiseScale);

    float ringRadius = lerp(0.18f, 0.88f, sqrt(age));
    float ringDiff = (r - ringRadius) * ringSharpness;
    float ringBand = exp(-ringDiff * ringDiff) * lerp(0.75f, 1.30f, combinedNoise);
    float centerFlash = exp(-rSq * 7.5f) * saturate(1.0f - age * 1.6f);

    float intensity = (ringBand + centerFlash * 0.85f) * (1.0f - age);
    float finalAlpha = saturate(intensity * input.Color.a);
    if (finalAlpha < 0.004f)
    {
        discard;
    }

    float3 whiteHot = float3(1.0f, 0.97f, 0.92f);
    float3 ringRGB = lerp(input.Color.rgb, whiteHot, saturate(centerFlash * 0.65f)) * emissiveIntensity;
    return float4(ringRGB * finalAlpha, finalAlpha);
}

float4 main(VSOutput input) : SV_TARGET
{
    if (ShaderStyleID < 0.5f)
    {
        return EvalErodedDropletPS(input);
    }
    else if (ShaderStyleID < 1.5f)
    {
        return EvalStreakSparkPS(input);
    }
    else if (ShaderStyleID < 2.5f)
    {
        return EvalSoftWispPS(input);
    }
    else
    {
        return EvalShockRingPS(input);
    }
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return main(input);
}

float4 PSMain_Eroded(VSOutput input) : SV_TARGET
{
    return EvalErodedDropletPS(input);
}

float4 PSMain_Streak(VSOutput input) : SV_TARGET
{
    return EvalStreakSparkPS(input);
}

float4 PSMain_Wisp(VSOutput input) : SV_TARGET
{
    return EvalSoftWispPS(input);
}

float4 PSMain_Ring(VSOutput input) : SV_TARGET
{
    return EvalShockRingPS(input);
}
