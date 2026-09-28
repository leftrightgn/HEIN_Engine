#include "Foliage_Common.hlsli"

Texture2D g_foliageTexture   : register(t0);
Texture2D g_noiseTexture     : register(t1);
Texture2D g_colorMapTexture  : register(t2);
Texture2D g_shadowMap        : register(t3);

SamplerState g_samplerWrap   : register(s0);
SamplerState g_samplerClamp  : register(s1);
SamplerComparisonState g_shadowSampler : register(s2);

float4 main(FoliagePixelInput input) : SV_Target
{
    // 1. Sample diffuse foliage texture
    float4 texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    if (flags.y > 0.5f)
    {
        texColor = g_foliageTexture.Sample(g_samplerWrap, input.texCoord);
    }

    // 2. Alpha Discard / Depth Handling ("think about the depth so it don't look weird")
    // Discard low-alpha pixels so opaque leaves/blades write solid depth to the Z-buffer.
    // This completely eliminates transparent sorting artifacts, flicker, and back-to-front sorting bugs!
    clip(texColor.a - alphaCutoff);

    // 3. Color Map Sampling (Color blending across terrain surface)
    float4 terrainColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    if (flags.x > 0.5f && terrainWidth > 0.0f && terrainHeight > 0.0f)
    {
        float2 terrainUV = float2(
            input.worldPos.x / terrainWidth + 0.5f,
            input.worldPos.z / terrainHeight + 0.5f
        );
        terrainColor = g_colorMapTexture.Sample(g_samplerClamp, terrainUV);
    }

    // 4. Stylized Color Palette Tinting based on Foliage Type
    float3 baseColor = texColor.rgb;

    if (input.foliageType == 0) // Grass
    {
        // Root-to-tip gradient (darker ground root -> sunlit luminous tip)
        float3 grassGradient = lerp(grassRootColor.rgb, grassTipColor.rgb, input.windWeight);
        // Modulate with terrain color for seamless terrain integration
        baseColor = texColor.rgb * grassGradient * lerp(float3(1, 1, 1), terrainColor.rgb, 0.45f);
    }
    else if (input.foliageType == 1) // Flower
    {
        // Lower stem is plant green, blossom tip is vibrant petal color
        float blossomMask = smoothstep(0.45f, 0.85f, input.windWeight);
        float3 flowerGradient = lerp(grassRootColor.rgb, flowerColor.rgb, blossomMask);
        baseColor = texColor.rgb * flowerGradient;
    }
    else // Tree (type 2)
    {
        // Trunk vs Canopy based on windWeight / vertex
        if (input.windWeight < 0.1f)
        {
            baseColor = texColor.rgb * barkColor.rgb;
        }
        else
        {
            baseColor = texColor.rgb * leafColor.rgb * lerp(float3(1, 1, 1), terrainColor.rgb, 0.25f);
        }
    }

    // Multiply by vertex color tint if present
    baseColor *= input.vertexColor.rgb;

    // 5. Lighting: Stylized Directional Light + Translucent Leaf Backlight
    float3 N = normalize(input.normal);
    float3 L = normalize(-lightDirection.xyz);
    float NdotL = max(dot(N, L), 0.0f);
    // Two-sided transmission (sunlight shining through thin leaves and grass blades)
    float backlight = max(dot(-N, L), 0.0f) * 0.35f;
    float diffuse = saturate(NdotL + backlight);

    // 6. Shadow Map
    float shadow = 1.0f;
    if (flags.z > 0.5f)
    {
        float3 shadowCoords = input.shadowPos.xyz / input.shadowPos.w;
        if (shadowCoords.x >= -1.0f && shadowCoords.x <= 1.0f &&
            shadowCoords.y >= -1.0f && shadowCoords.y <= 1.0f &&
            shadowCoords.z >= 0.0f  && shadowCoords.z <= 1.0f)
        {
            float2 shadowUV;
            shadowUV.x =  shadowCoords.x * 0.5f + 0.5f;
            shadowUV.y = -shadowCoords.y * 0.5f + 0.5f;
            float currentDepth = shadowCoords.z - 0.0015f; // bias
            shadow = g_shadowMap.SampleCmpLevelZero(g_shadowSampler, shadowUV, currentDepth);
            shadow = lerp(0.45f, 1.0f, shadow);
        }
    }

    float3 finalLighting = ambientColor.rgb + lightColor.rgb * diffuse * shadow;
    float3 finalColor = baseColor * finalLighting;

    return float4(finalColor, 1.0f);
}
