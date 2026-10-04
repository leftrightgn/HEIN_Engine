//--------------------------------------------------------------------------------------
// CustomSkinned_PS.hlsl
// Pixel Shader for Skinned Models with Lighting, Fog, and Shadow Mapping
//--------------------------------------------------------------------------------------

#include "CustomSkinned_Common.hlsli"

// ==============================================================================
// TEXTURES & SAMPLERS
// ==============================================================================
Texture2D DiffuseMap : register(t0);               // The model's texture
Texture2D ShadowMap  : register(t4);               // The generated shadow map
SamplerState LinearSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1); // Sampler for shadow filtering

// ==============================================================================
// SHADOW CALCULATION
// ==============================================================================
float CalculateShadow(float4 lightSpacePos)
{
    // Perspective Divide (Clip Space to NDC)
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // Convert NDC [-1, 1] to Texture UV [0, 1]
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;

    // Ignore shadow if outside the map bounds
    if (projCoords.x < 0 || projCoords.x > 1 || projCoords.y < 0 || projCoords.y > 1)
        return 1.0f;

    float currentDepth = projCoords.z;
    float shadow = 0.0f;
    float bias = 0.0005f; // Reduced bias for better precision with small objects
    
    // Hardware PCF using SampleCmpLevelZero
    float2 texelSize = 1.0f / 2048.0f;
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 uv = projCoords.xy + float2(x, y) * texelSize;
            float pcfResult = ShadowMap.SampleCmpLevelZero(ShadowSampler, uv, currentDepth - bias).r;
            // SampleCmpLevelZero returns 1.0f if the depth in texture >= currentDepth - bias. (lit)
            // It returns 0.0f if the depth in texture < currentDepth - bias. (shadowed)
            // The 'shadow' variable is accumulating '1.0' for light and '0.0' for shadow.
            shadow += pcfResult;
        }
    }
    shadow /= 9.0f; // Average the samples
    
    return shadow; // Returns 0.0 for shadow, 1.0 for light
}

// ==============================================================================
// PIXEL SHADER
// ==============================================================================
float4 main(VSOutput input) : SV_TARGET
{
    float4 baseColor = DiffuseMap.Sample(LinearSampler, input.TexCoord);
    
    // Optional Alpha Clipping in main pass if needed
    clip(baseColor.a - 0.05f);

    float3 norm = normalize(input.Normal);
    
    float3 lightVector = LightPos - input.WorldPos;
    float distanceToLight = length(lightVector);
    
    // Determine Light Direction (L)
    float3 L = (LightType == 0) ? normalize(-LightDir) : normalize(lightVector);

    // Base Diffuse Calculation
    float diff = max(dot(norm, L), 0.0f);
    
    // Attenuation (Falloff over distance for Point/Spot lights)
    float attenuation = 1.0f;
    if (LightType == 1 || LightType == 2)
    {
        attenuation = 1.0f - saturate(distanceToLight / LightRange);
        attenuation *= attenuation; // Quadratic falloff
    }

    // Spot Light Cone Calculation
    if (LightType == 2)
    {
        float theta = dot(L, normalize(-LightDir));
        float cosHalfAngle = cos(LightSpotAngle * 0.5f);
        
        if (theta < cosHalfAngle)
            attenuation = 0.0f;
        else
            attenuation *= smoothstep(cosHalfAngle, cosHalfAngle + 0.05f, theta);
    }

    // Shadow Mapping (Only calculate shadows if attenuation > 0)
    float shadowFactor = 1.0f;
    if (attenuation > 0.0f && LightType == 0) // Currently shadows only for Directional
    {
        shadowFactor = CalculateShadow(input.LightSpacePos);
    }

    // Final Color Combination
    float3 finalColor = baseColor.rgb * LightColor.rgb * diff * LightIntensity * attenuation * shadowFactor;
    
    // Add ambient light so shadows aren't pitch black
    float3 ambient = baseColor.rgb * 0.4f;
    finalColor += ambient;

    // FOG CALCULATION (distance-based)
    float dist = length(input.WorldPos - CameraPos);
    float fogFactor = saturate((dist - FogStart) / (FogEnd - FogStart));
    finalColor = lerp(finalColor, FogColor.rgb, fogFactor);

    return float4(finalColor, baseColor.a);
}

// Backwards-compatible alias for entry point PSMain
float4 PSMain(VSOutput input) : SV_TARGET
{
    return main(input);
}
