//--------------------------------------------------------------------------------------
// CustomSkinned_VS.hlsl
// Vertex Shader for Skinned Models with Skeletal Animation
//--------------------------------------------------------------------------------------

#include "CustomSkinned_Common.hlsli"

// ==============================================================================
// VERTEX SHADER
// ==============================================================================
VSOutput main(VSInput input)
{
    VSOutput output;

    // Normalize weights just in case the model exporter left them unnormalized
    float totalWeight = input.BoneWeights.x + input.BoneWeights.y + input.BoneWeights.z + input.BoneWeights.w;
    if (totalWeight > 0.0f)
    {
        input.BoneWeights /= totalWeight;
    }

    // Apply Skinning directly to Position and Normal
    float4 inPos = float4(input.Pos, 1.0f);
    float4 localPos =
        mul(inPos, BoneTransforms[input.BoneIndices.x]) * input.BoneWeights.x +
        mul(inPos, BoneTransforms[input.BoneIndices.y]) * input.BoneWeights.y +
        mul(inPos, BoneTransforms[input.BoneIndices.z]) * input.BoneWeights.z +
        mul(inPos, BoneTransforms[input.BoneIndices.w]) * input.BoneWeights.w;

    float3 localNormal =
        mul(input.Normal, (float3x3)BoneTransforms[input.BoneIndices.x]) * input.BoneWeights.x +
        mul(input.Normal, (float3x3)BoneTransforms[input.BoneIndices.y]) * input.BoneWeights.y +
        mul(input.Normal, (float3x3)BoneTransforms[input.BoneIndices.z]) * input.BoneWeights.z +
        mul(input.Normal, (float3x3)BoneTransforms[input.BoneIndices.w]) * input.BoneWeights.w;

    // Transform to Screen Space for Camera
    output.WorldPos = mul(localPos, World).xyz;
    output.Pos = mul(localPos, WorldViewProj);
    
    // Pass data to Pixel Shader
    output.Normal = normalize(mul(localNormal, (float3x3) World));
    output.TexCoord = input.TexCoord;
    
    // Transform to Light Space for Shadow Calculation
    output.LightSpacePos = mul(localPos, LightViewProj);

    return output;
}

// Backwards-compatible alias for entry point VSMain
VSOutput VSMain(VSInput input)
{
    return main(input);
}
