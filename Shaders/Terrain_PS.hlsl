#include "Terrain_Common.hlsli"

Texture2D shaderTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D alphaTexture  : register(t2);
Texture2D texture2      : register(t3);
Texture2D shadowMap     : register(t4);
SamplerState SampleType : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

float4 main(PixelInputType input) : SV_Target
{
    float4 textureColor;
    float4 bumpMap;
    float3 bumpNormal;
    float3 lightDir;
    float  lightIntensity;
    float4 color;
    
    if (hasTexture > 0.5f)
    {
        // Sample Base Texture (Grass) using TILED UVs
        textureColor = shaderTexture.Sample(SampleType, input.tex * textureTiling);
        
        // TEXTURE SPLATTING
        if (hasAlphaMap > 0.5f && hasTexture2 > 0.5f)
        {
            float4 alphaMap = alphaTexture.Sample(SampleType, input.tex);
            float4 tex2Color = texture2.Sample(SampleType, input.tex * textureTiling);
            textureColor = lerp(textureColor, tex2Color, alphaMap.r);
        }
    }
    else
    {
        textureColor = float4(1.0f, 1.0f, 1.0f, 1.0f); // Default white texture
    }
    
    if (hasNormalMap > 0.5f)
    {
        bumpMap = normalTexture.Sample(SampleType, input.tex * textureTiling);
        bumpMap = (bumpMap * 2.0f) - 1.0f;
        
        float3 perfectTangent = normalize(input.tangent - dot(input.tangent, input.normal) * input.normal);
        float3 perfectBinormal = cross(input.normal, perfectTangent);
        
        bumpNormal = (bumpMap.x * perfectTangent) + (bumpMap.y * perfectBinormal) + (bumpMap.z * input.normal);
        bumpNormal = normalize(bumpNormal);
    }
    else
    {
        bumpNormal = normalize(input.normal); // Fallback
    }
    
    // --- SHADOW MAPPING ---
    float shadow = 1.0f; // 1.0 = fully lit, 0.5 = shadowed
    
    // Convert shadow clip space to NDC
    float3 shadowCoords = input.shadowPos.xyz / input.shadowPos.w;
    
    // Check if we are inside the orthographic projection box
    if (shadowCoords.x >= -1.0f && shadowCoords.x <= 1.0f &&
        shadowCoords.y >= -1.0f && shadowCoords.y <= 1.0f &&
        shadowCoords.z >= 0.0f && shadowCoords.z <= 1.0f)
    {
        // Convert XY from [-1, 1] to [0, 1] for Texture UVs
        float2 shadowUV;
        shadowUV.x = shadowCoords.x * 0.5f + 0.5f;
        shadowUV.y = -shadowCoords.y * 0.5f + 0.5f; // Invert Y because V is down
        
        // Hardware PCF using SampleCmpLevelZero. 
        // This is the only 100% hardware-compliant way to read a Depth Texture without driver bugs.
        // It returns 1.0f if the depth in the texture is >= shadowCoords.z - 0.0005f (lit), and 0.0f if not (shadowed).
        float shadowPercent = shadowMap.SampleCmpLevelZero(ShadowSampler, shadowUV, shadowCoords.z - 0.0005f).r;
        
        shadow = lerp(0.5f, 1.0f, shadowPercent);
    }
    
   // INVERT THE LIGHT DIRECTION!
    lightDir = -lightDirection;
    
    lightIntensity = saturate(dot(bumpNormal, lightDir)) * shadow;
    
    //ADD AMBIENT LIGHT (So shadows aren't 100% pitch black)
    float4 ambientColor = float4(0.3f, 0.3f, 0.3f, 1.0f);
    color = saturate((diffuseColor * lightIntensity) + ambientColor);
    
    // Force the alpha channel to 1.0f so it is fully opaque
    color = color * textureColor * input.color;
    
    // FOG CALCULATION
    float fogFactor = saturate((input.viewZ - fogStart) / (fogEnd - fogStart));
    color = lerp(color, fogColor, fogFactor);
    
    color.a = 1.0f;
    
    return color;
}
