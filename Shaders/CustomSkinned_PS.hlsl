//--------------------------------------------------------------------------------------
// CustomSkinned_PS.hlsl
// Pixel Shader for Skinned Models with Lighting, Fog, and Shadow Mapping
//--------------------------------------------------------------------------------------

#include "CustomSkinned_Common.hlsli"

Texture2D DiffuseMap : register(t0);
Texture2D ShadowMap : register(t4);
SamplerState LinearSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

// ==============================================================================
// SHADOW CALCULATION (With Slope-Scaled Bias to prevent armor shadow noise)
// ==============================================================================
float CalculateShadow(float4 lightSpacePos, float3 normal, float3 lightDir)
{
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;

    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f || projCoords.z > 1.0f)
        return 1.0f;

    float currentDepth = projCoords.z;
    
    // Slope-scaled bias prevents striped/sparkling shadow acne on curved armor and cloth
    float cosTheta = saturate(dot(normal, lightDir));
    float bias = max(0.002f * (1.0f - cosTheta), 0.0005f);
    
    float shadow = 0.0f;
    float2 texelSize = 1.0f / 2048.0f;
    
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 uv = projCoords.xy + float2(x, y) * texelSize;
            shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv, currentDepth - bias).r;
        }
    }
    return shadow / 9.0f;
}

// ==============================================================================
// PIXEL SHADER
// ==============================================================================
float4 main(VSOutput input) : SV_TARGET
{
    float4 baseColor = DiffuseMap.Sample(LinearSampler, input.TexCoord);
    
    // Ghost Form Transparency & Ethereal Rim Glow
    if (IsGhost != 0)
    {
        if (baseColor.a < 0.05f || dot(baseColor.rgb, baseColor.rgb) < 0.001f)
        {
            baseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        float3 norm = normalize(input.Normal);
        float3 viewDir = normalize(CameraPos - input.WorldPos);

        if (dot(norm, viewDir) < 0.0f)
        {
            norm = -norm;
        }

        float NdotV = saturate(dot(norm, viewDir));
        float rim = pow(1.0f - NdotV, max(GhostRimPower, 0.1f));

        float3 lightVector = LightPos - input.WorldPos;
        float3 L = (LightType == 0) ? normalize(-LightDir) : normalize(lightVector);
        float diff = max(dot(norm, L), 0.0f);

        float3 spectralColor = GhostColor.rgb;
        float baseAlpha = saturate(GhostColor.a * baseColor.a);
        float finalAlpha = saturate(baseAlpha + rim * GhostRimIntensity * 0.45f);

        float pulse = 0.92f + 0.08f * sin(GhostTime * 3.0f);
        finalAlpha = saturate(finalAlpha * pulse);

        float3 ghostRgb = lerp(baseColor.rgb, spectralColor, 0.65f);
        float3 finalColor = ghostRgb * (LightColor.rgb * diff * LightIntensity * 0.5f + 0.4f) + spectralColor * (rim * GhostRimIntensity);

        float dist = length(input.WorldPos - CameraPos);
        float fogFactor = saturate((dist - FogStart) / (FogEnd - FogStart));
        finalColor = lerp(finalColor, FogColor.rgb, fogFactor);

        return float4(finalColor, finalAlpha);
    }

    // 1. Clean Alpha Cutout for tattered cape / chainmail holes
    // Discards transparent pixels so they don't write garbage to the Depth Buffer
    clip(baseColor.a - 0.35f);

    float3 norm = normalize(input.Normal);
    float3 viewDir = normalize(CameraPos - input.WorldPos);

    // 2. Flip normal for back-faces since SkinnedModelComponent uses CullNone!
    // Without this, the inside of the cape/armor has inverted normals and flickers black/lit
    if (dot(norm, viewDir) < 0.0f)
    {
        norm = -norm;
    }
    
    float3 lightVector = LightPos - input.WorldPos;
    float distanceToLight = length(lightVector);
    float3 L = (LightType == 0) ? normalize(-LightDir) : normalize(lightVector);

    float diff = max(dot(norm, L), 0.0f);
    
    float attenuation = 1.0f;
    if (LightType == 1 || LightType == 2)
    {
        attenuation = 1.0f - saturate(distanceToLight / LightRange);
        attenuation *= attenuation;
    }

    if (LightType == 2)
    {
        float theta = dot(L, normalize(-LightDir));
        float cosHalfAngle = cos(LightSpotAngle * 0.5f);
        
        if (theta < cosHalfAngle)
            attenuation = 0.0f;
        else
            attenuation *= smoothstep(cosHalfAngle, cosHalfAngle + 0.05f, theta);
    }

    float shadowFactor = 1.0f;
    if (attenuation > 0.0f && LightType == 0)
    {
        shadowFactor = CalculateShadow(input.LightSpacePos, norm, L);
    }

    float3 finalColor = baseColor.rgb * LightColor.rgb * diff * LightIntensity * attenuation * shadowFactor;
    float3 ambient = baseColor.rgb * 0.4f;
    finalColor += ambient;

    float dist = length(input.WorldPos - CameraPos);
    float fogFactor = saturate((dist - FogStart) / (FogEnd - FogStart));
    finalColor = lerp(finalColor, FogColor.rgb, fogFactor);

    // Damage hit flash (strength in HitFlashColor.a fades out from C++)
    finalColor = lerp(finalColor, HitFlashColor.rgb, saturate(HitFlashColor.a));

    return float4(finalColor, 1.0f);
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return main(input);
}