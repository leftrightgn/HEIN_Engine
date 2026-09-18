#include "Terrain_Common.hlsli"

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    input.position.w = 1.0f;
    
    output.position = mul(input.position, worldMatrix);
    
    float4 viewPos = mul(output.position, viewMatrix);
    // In a right-handed system, objects in front of the camera have negative Z.
    // We use abs() to get a positive distance for the fog calculation.
    output.viewZ = abs(viewPos.z);
    
    output.position = mul(viewPos, projectionMatrix);
    
    output.tex = input.tex;
    output.color = input.color;
    output.normal = mul(input.normal, (float3x3) worldMatrix);
    output.normal = normalize(output.normal);
    
    output.tangent = mul(input.tangent, (float3x3) worldMatrix);
    output.tangent = normalize(output.tangent);
    
    output.binormal = mul(input.binormal, (float3x3) worldMatrix);
    output.binormal = normalize(output.binormal);
    
    
    return output;
}