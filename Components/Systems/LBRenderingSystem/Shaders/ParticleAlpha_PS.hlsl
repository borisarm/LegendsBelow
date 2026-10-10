// ParticleAlpha_PS.hlsl - soft sprite with the particle's opacity, for alpha-blended particles
// (dust, smoke: they darken or tint what is behind them instead of adding light)
#include "Particle.hlsli"

float4 main(ParticlePixel input) : SV_Target
{
    const float mask = Sprite.Sample(LinearClamp, input.texcoord).a;
    return float4(input.color.rgb, input.color.a * mask);
}
