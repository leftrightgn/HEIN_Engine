#include "CustomSkinned.hlsl"

float4 main(VSOutput input) : SV_TARGET
{
    return PSMain(input);
}
