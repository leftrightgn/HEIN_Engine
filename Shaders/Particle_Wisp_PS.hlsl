//--------------------------------------------------------------------------------------
// Particle_Wisp_PS.hlsl
// Pluggable Pixel Shader: Ethereal Swirling Wisps & Flames (Soul Ectoplasm, Fire, Smoke)
//--------------------------------------------------------------------------------------
#define main HitEffectCombinedMain
#include "Particle_PS.hlsl"
#undef main

float4 main(VSOutput input) : SV_Target
{
    return EvalSoftWispPS(input);
}
