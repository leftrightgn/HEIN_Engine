//--------------------------------------------------------------------------------------
// WeaponTrail_PS.hlsl
// Pixel Shader for Dual-Layer Eroding Weapon Ribbon Trail VFX
//--------------------------------------------------------------------------------------

#include "WeaponTrail_Common.hlsli"

Texture2D NoiseMap : register(t0);
SamplerState LinearWrapSampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    // u: 0.0 at leading blade edge (newest), 1.0 at tail end (oldest)
    // v: 0.0 at blade hilt/base, 1.0 at blade tip
    float u = saturate(input.TexCoord.x);
    float v = saturate(input.TexCoord.y);

    // 1. Radial Blade Distribution
    float radialBody = pow(v, max(0.2f, TipFalloffPower));
    float tipRimBoost = smoothstep(0.68f, 0.96f, v);
    // Crisp anti-aliased outer tip edge and soft inner hilt fade
    float outerEdgeMask = smoothstep(1.0f, 0.965f, v) * smoothstep(0.0f, 0.08f, v);

    // 2. Dual-Octave Anisotropic Scrolled Noise for Air-Cut Streak Erosion
    float2 noiseUV1 = float2(u * NoiseScale - Time * NoiseScrollSpeed, v * (NoiseScale * 2.2f));
    float2 noiseUV2 = float2(u * (NoiseScale * 2.1f) - Time * (NoiseScrollSpeed * 1.45f), v * (NoiseScale * 4.0f) + 0.37f);

    float n1 = NoiseMap.Sample(LinearWrapSampler, noiseUV1).r;
    float n2 = NoiseMap.Sample(LinearWrapSampler, noiseUV2).r;
    float combinedNoise = saturate(n1 * 0.65f + n2 * 0.35f);

    // Carve streaky wisps along the tail as u approaches 1.0
    float erosionMask = smoothstep(u * 0.75f, 1.0f, lerp(0.85f, combinedNoise, saturate(ErosionStrength)));

    // 3. Longitudinal Age Envelope (brightest right behind the blade, tapering to 0 at tail)
    float leadingFlash = exp(-u * max(1.0f, LeadingEdgeSharpness));
    float tailEnvelope = pow(saturate(1.0f - u), 1.35f);

    // 4. Dual-Color HDR Blending (EdgeColor body -> CoreColor at leading edge & outer tip arc)
    float coreWeight = saturate(leadingFlash * 0.85f + tipRimBoost * 0.55f * (1.0f - u * 0.6f));
    float3 finalRGB = lerp(EdgeColor.rgb, CoreColor.rgb, coreWeight);

    // Subtle hot white-gold specular highlight at the very tip of the leading edge
    float apexHighlight = leadingFlash * tipRimBoost * 0.45f;
    finalRGB += CoreColor.rgb * apexHighlight;
    finalRGB *= EmissiveIntensity * input.Color.rgb;

    // 5. Final Alpha Composition
    float baseAlpha = lerp(EdgeColor.a, CoreColor.a, coreWeight);
    float finalAlpha = baseAlpha
                     * radialBody
                     * outerEdgeMask
                     * tailEnvelope
                     * erosionMask
                     * input.Color.a;

    // Prevent sub-pixel ghosting
    if (finalAlpha < 0.004f)
    {
        discard;
    }

    // Output premultiplied-friendly RGB & Alpha (works seamlessly with both Additive and AlphaBlend)
    return float4(finalRGB * finalAlpha, finalAlpha);
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return main(input);
}
