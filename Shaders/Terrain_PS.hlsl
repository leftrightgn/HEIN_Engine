#include "Terrain_Common.hlsli"

Texture2D shaderTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D alphaTexture  : register(t2);
Texture2D texture2      : register(t3);
SamplerState SampleType : register(s0);

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
            // Sample Alpha Map using RAW UVs (input.tex) so it stretches across the whole map!
            float4 alphaMap = alphaTexture.Sample(SampleType, input.tex);
            
            // Sample Second Texture (Dirt) using TILED UVs (input.tex * textureTiling)
            float4 tex2Color = texture2.Sample(SampleType, input.tex * textureTiling);
            
            // Blend them together using the Red channel of the Alpha Map
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
        
        // ADD GRAM-SCHMIDT ORTHOGONALIZATION
        // Force the Tangent to be exactly 90 degrees to the Normal
        float3 perfectTangent = normalize(input.tangent - dot(input.tangent, input.normal) * input.normal);
        
        // Mathematically generate a perfect Binormal using the Cross Product
        float3 perfectBinormal = cross(input.normal, perfectTangent);
        
        // Use the perfect vectors to apply the bump map!
        bumpNormal = (bumpMap.x * perfectTangent) + (bumpMap.y * perfectBinormal) + (bumpMap.z * input.normal);
        bumpNormal = normalize(bumpNormal);
    }
    else
    {
        bumpNormal = normalize(input.normal); // Fallback
    }
    
   // INVERT THE LIGHT DIRECTION!
    lightDir = -lightDirection;
    
    lightIntensity = saturate(dot(bumpNormal, lightDir));
    
    //ADD AMBIENT LIGHT (So shadows aren't 100% pitch black)
    float4 ambientColor = float4(0.3f, 0.3f, 0.3f, 1.0f);
    color = saturate((diffuseColor * lightIntensity) + ambientColor);
    
    // Force the alpha channel to 1.0f so it is fully opaque
    color = color * textureColor * input.color;
    color.a = 1.0f;
    
    return color;
}