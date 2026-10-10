// Particle_PS.hlsl - soft sprite times the particle color, for additive blending
#include "Particle.hlsli"

float4 main(ParticlePixel input) : SV_Target
{
    const float mask = Sprite.Sample(LinearClamp, input.texcoord).a;
    return float4(input.color.rgb * input.color.a * mask, 0.0f);
}
