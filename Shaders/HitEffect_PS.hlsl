//--------------------------------------------------------------------------------------
// HitEffect_PS.hlsl
// Pixel Shader for Dual-Mode Combat Impact VFX:
// - Blood Pass (PassMode < 0.5): Dual-octave noise-eroded crimson splatter & volumetric mist
// - Spark Pass (PassMode >= 0.5): Anisotropic diamond-tapered HDR incandescent steel sparks
//--------------------------------------------------------------------------------------

#include "HitEffect_Common.hlsli"

Texture2D NoiseMap : register(t0);
SamplerState LinearWrapSampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    // Local quad coordinates p in [-1.0, 1.0]
    float2 p = input.TexCoord;
    float rSq = dot(p, p);
    if (rSq > 1.0f)
    {
        discard;
    }

    float age  = saturate(input.Params.x);
    float seed = input.Params.y;

    // Convert [-1, 1] quad space to [0, 1] UV space with per-particle phase offset
    float2 baseUV = p * 0.5f + 0.5f;
    float2 noiseUV1 = baseUV * NoiseScale + float2(seed * 3.17f, seed * 7.43f);
    float2 noiseUV2 = baseUV * (NoiseScale * 2.15f) - float2(seed * 5.89f, age * 0.65f);

    float n1 = NoiseMap.Sample(LinearWrapSampler, noiseUV1).r;
    float n2 = NoiseMap.Sample(LinearWrapSampler, noiseUV2).r;
    float combinedNoise = saturate(n1 * 0.65f + n2 * 0.35f);

    // - Mode A: Organic Blood Splatter & Volumetric Mist (PassMode < 0.5)
    if (PassMode < 0.5f)
    {
        float radialMask = saturate(1.0f - rSq);

        // Ragged organic boundary distortion using dual-octave noise
        float noisyRadius = radialMask * lerp(1.0f, combinedNoise * 1.45f, saturate(ErosionStrength));

        // Progressive age-driven threshold carves realistic droplets and dissolving mist
        float erosionThreshold = age * 0.62f * saturate(ErosionStrength);
        float splatterAlpha = smoothstep(erosionThreshold, erosionThreshold + 0.32f, noisyRadius);

        // Coagulated dark-crimson core darkening for visceral liquid thickness
        float coreDensity = smoothstep(0.25f, 0.92f, radialMask);
        float3 bloodRGB = input.Color.rgb * lerp(0.58f, 1.18f, coreDensity);

        float finalAlpha = saturate(splatterAlpha * input.Color.a);
        if (finalAlpha < 0.005f)
        {
            discard;
        }

        // Output premultiplied-compatible RGBA for AlphaBlend pass
        return float4(bloodRGB * finalAlpha, finalAlpha);
    }
    // - Mode B: Incandescent Steel Deflection Sparks & Clash Flash (PassMode >= 0.5)
    else
    {
        // Anisotropic diamond-tapered streak profile (sharp across width p.x, tapered along velocity p.y)
        float crossPinch = exp(-abs(p.x) * 4.2f) * saturate(1.0f - abs(p.x));
        float longTaper  = saturate(1.0f - p.y * p.y);
        float streakBody = crossPinch * longTaper;

        // Subtle high-frequency spark flicker along the hot streak
        float sparkNoise = lerp(0.82f, 1.22f, combinedNoise);

        // Intense white-hot thermal core (blackbody radiation center)
        float hotCore = exp(-rSq * 6.5f);
        float3 whiteHot = float3(1.0f, 0.98f, 0.90f);
        float3 sparkRGB = lerp(input.Color.rgb, whiteHot, saturate(hotCore * 0.78f));
        sparkRGB *= EmissiveIntensity * sparkNoise;

        float finalAlpha = saturate((streakBody * 0.90f + hotCore * 0.55f) * (1.0f - age * 0.75f) * input.Color.a);
        if (finalAlpha < 0.004f)
        {
            discard;
        }

        return float4(sparkRGB * finalAlpha, finalAlpha);
    }
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return main(input);
}
