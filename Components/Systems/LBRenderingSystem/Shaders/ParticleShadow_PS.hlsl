// ParticleShadow_PS.hlsl - a shadow lying on the ground: evenly dark in the middle, fading softly
// to nothing at its rim (the particle quad, laid flat; its alpha is how dark the middle is)
#include "Particle.hlsli"

float4 main(ParticlePixel input) : SV_Target
{
    const float radius = length(input.texcoord * 2.0f - 1.0f);
    const float shade = 1.0f - smoothstep(0.35f, 1.0f, radius);
    return float4(0.0f, 0.0f, 0.0f, input.color.a * shade);
}
