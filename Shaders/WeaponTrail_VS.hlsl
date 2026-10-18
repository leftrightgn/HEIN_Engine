//--------------------------------------------------------------------------------------
// WeaponTrail_VS.hlsl
// Vertex Shader for World-Space Catmull-Rom Subdivided Weapon Ribbon Trails
//--------------------------------------------------------------------------------------

#include "WeaponTrail_Common.hlsli"

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Pos = mul(float4(input.Pos, 1.0f), ViewProj);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    return output;
}

VSOutput VSMain(VSInput input)
{
    return main(input);
}
