//--------------------------------------------------------------------------------------
// Particle_Streak_PS.hlsl
// Pluggable Pixel Shader: Anisotropic Incandescent Sparks & Clash Streaks
//--------------------------------------------------------------------------------------
#define main HitEffectCombinedMain
#include "Particle_PS.hlsl"
#undef main

float4 main(VSOutput input) : SV_Target
{
    return EvalStreakSparkPS(input);
}
