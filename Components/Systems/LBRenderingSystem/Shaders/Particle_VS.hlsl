// Particle_VS.hlsl - expands a particle into a camera-facing quad
#include "Particle.hlsli"

ParticlePixel main(ParticleInstance particle, uint vertex : SV_VertexID)
{
    // Two triangles: corners (-1,-1) (1,-1) (-1,1) / (1,-1) (1,1) (-1,1)
    static const float2 corners[6] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, -1), float2(1, 1), float2(-1, 1) };
    const float2 corner = corners[vertex % 6];
    const float half = particle.positionSize.w * 0.5f;
    const float3 world = particle.positionSize.xyz + (CameraRight.xyz * corner.x + CameraUp.xyz * corner.y) * half;

    ParticlePixel output;
    output.position = mul(float4(world, 1.0f), ViewProjection);
    output.texcoord = corner * float2(0.5f, -0.5f) + 0.5f;
    output.color = particle.color;
    return output;
}
