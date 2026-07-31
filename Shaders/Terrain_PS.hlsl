#include "Terrain_Common.hlsli"

Texture2D shaderTexture : register(t0);
SamplerState SampleType : register(s0);

float4 main(PixelInputType input) : SV_Target
{
    float4 textureColor;
    float3 lightDir;
    float  lightIntensity;
    float4 color;
    
    if (hasTexture > 0.5f) {
        textureColor = shaderTexture.Sample(SampleType, input.tex);
    } else {
        textureColor = float4(1.0f, 1.0f, 1.0f, 1.0f); // Default white texture
    }
    
   // INVERT THE LIGHT DIRECTION!
    lightDir = -lightDirection;
    
    lightIntensity = saturate(dot(input.normal, lightDir));
    
    //ADD AMBIENT LIGHT (So shadows aren't 100% pitch black)
    float4 ambientColor = float4(0.3f, 0.3f, 0.3f, 1.0f);
    color = saturate((diffuseColor * lightIntensity) + ambientColor);
    
    // Force the alpha channel to 1.0f so it is fully opaque
    color = color * textureColor * input.color;
    color.a = 1.0f;
    
    return color;
}