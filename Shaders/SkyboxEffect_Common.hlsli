#ifndef __SKYBOXEFFECT_COMMON_HLSLI__
#define __SKYBOXEFFECT_COMMON_HLSLI__

cbuffer SkyboxConstants : register(b0)
{
    float4x4 WorldViewProj;
}

struct VSOutput
{
    float3 TexCoord : TEXCOORD0;
    float4 PositionPS : SV_Position;
    

};

#endif