// ModelSkinned_VS.hlsl - vertex shader for instanced skinned model parts
//
// Each instance's joint matrices (bind pose -> world, one per joint of the model's skeleton) sit in
// the frame's skin palette from the instance's palette offset on; a vertex blends up to four.
#include "ModelInstanced.hlsli"

StructuredBuffer<AffineColumns> SkinPalette : register(t1);

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD0;
    uint4  joints   : BLENDINDICES0;
    float4 weights  : BLENDWEIGHT0;
    InstanceInput instance;
};

PixelInput main(VertexInput input)
{
    float3 position = 0.0f;
    float3 normal = 0.0f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        const AffineColumns joint = SkinPalette[input.instance.palette + input.joints[i]];
        position += input.weights[i] * TransformPoint(joint, input.position);
        normal += input.weights[i] * TransformNormal(joint, input.normal);
    }

    PixelInput output;
    output.position = mul(float4(position, 1.0f), ViewProjection);
    output.normal = normal;
    output.texcoord = input.texcoord;
    output.tint = input.instance.tint;
    return output;
}
