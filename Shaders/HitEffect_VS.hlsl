//--------------------------------------------------------------------------------------
// HitEffect_VS.hlsl
// Vertex Shader for Velocity-Aligned Combat Impact Billboards (Blood & Steel Sparks)
//--------------------------------------------------------------------------------------

#include "HitEffect_Common.hlsli"

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Pos      = mul(float4(input.Pos, 1.0f), ViewProj);
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;
    output.Params   = input.Params;
    return output;
}

VSOutput VSMain(VSInput input)
{
    return main(input);
}
