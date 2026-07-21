#include "SkyboxEffect_Common.hlsli"

VSOutput main(float4 position : POSITION)
{
    VSOutput vout;

    vout.PositionPS = mul(position, WorldViewProj);
    vout.PositionPS.z = vout.PositionPS.w; // Draw on far plane
    vout.TexCoord = position.xyz;
    //vout.TexCoord.y -= 0.002f;
    return vout;
}