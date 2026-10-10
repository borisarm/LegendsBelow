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

    PixelInput output;
    output.position = mul(float4(TransformPoint(world, input.position), 1.0f), ViewProjection);
    output.normal = TransformNormal(world, input.normal);
    output.texcoord = input.texcoord;
    output.tint = input.instance.tint;
    return output;
}
