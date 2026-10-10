// ModelInstanced_VS.hlsl - vertex shader for instanced rigid model parts
#include "ModelInstanced.hlsli"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD0;
    InstanceInput instance;
};

PixelInput main(VertexInput input)
{
    AffineColumns world;
    world.c0 = input.instance.world0;
    world.c1 = input.instance.world1;
    world.c2 = input.instance.world2;

    const float3 position = TransformPoint(world, input.position);

    PixelInput output;
    output.position = mul(float4(position, 1.0f), ViewProjection);
    output.world = position;
    output.normal = TransformNormal(world, input.normal);
    output.texcoord = input.texcoord;
    output.tint = input.instance.tint;
    return output;
}
