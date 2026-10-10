// ModelInstanced_VS.hlsl - vertex shader for instanced model parts
#include "ModelInstanced.hlsli"

PixelInput main(VertexInput input)
{
    const float4 local = float4(input.position, 1.0f);
    const float3 world = float3(dot(local, input.world0), dot(local, input.world1), dot(local, input.world2));

    // Instances use rotation and uniform scale only, so the same 3x3 turns normals
    const float3 normal = float3(dot(input.normal, input.world0.xyz), dot(input.normal, input.world1.xyz), dot(input.normal, input.world2.xyz));

    PixelInput output;
    output.position = mul(float4(world, 1.0f), ViewProjection);
    output.normal = normal;
    output.color = input.color;
    return output;
}
