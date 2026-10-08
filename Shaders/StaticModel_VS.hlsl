//--------------------------------------------------------------------------------------
// StaticModel_VS.hlsl
// Vertex Shader for Static Models with Lighting, Fog, and Shadow Mapping
//--------------------------------------------------------------------------------------

#include "StaticModel_Common.hlsli"

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 localPos = float4(input.Pos, 1.0f);
    float4 worldPos = mul(localPos, World);

    // Transform to World & Screen Space
    output.WorldPos = worldPos.xyz;
    output.Pos = mul(localPos, WorldViewProj);
    
    // Normal in world space
    output.Normal = normalize(mul(input.Normal, (float3x3)World));
    output.TexCoord = input.TexCoord;
    
    // Transform to Light Space for Shadow Calculation
    output.LightSpacePos = mul(worldPos, LightViewProj);

    return output;
}

// Backwards-compatible alias for entry point VSMain
VSOutput VSMain(VSInput input)
{
    return main(input);
}
