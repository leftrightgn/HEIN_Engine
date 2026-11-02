//--------------------------------------------------------------------------------------
// Particle_Ring_PS.hlsl
// Pluggable Pixel Shader: Expanding Shockwave Ring & Pulse Halo
//--------------------------------------------------------------------------------------
#define main HitEffectCombinedMain
#include "Particle_PS.hlsl"
#undef main

float4 main(VSOutput input) : SV_Target
{
    return EvalShockRingPS(input);
}
