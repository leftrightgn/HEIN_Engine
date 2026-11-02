//--------------------------------------------------------------------------------------
// Particle_Eroded_PS.hlsl
// Pluggable Pixel Shader: Organic Noise-Eroded Droplets & Mist (Blood, Mud, Debris)
//--------------------------------------------------------------------------------------
#define main HitEffectCombinedMain
#include "Particle_PS.hlsl"
#undef main

float4 main(VSOutput input) : SV_Target
{
    return EvalErodedDropletPS(input);
}
