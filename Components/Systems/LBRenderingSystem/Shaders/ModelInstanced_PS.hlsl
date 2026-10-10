// ModelInstanced_PS.hlsl - pixel shader for instanced model parts (per-pixel lighting)
#include "ModelInstanced.hlsli"

float4 main(PixelInput input) : SV_Target
{
    const float3 normal = normalize(input.normal);

    float3 light = AmbientColor.rgb;
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        light += saturate(dot(normal, -LightDirection[i].xyz)) * LightColor[i].rgb;
    }

    // Premultiplied alpha, as the transparent pass blends with ONE / INV_SRC_ALPHA
    const float3 color = input.color.rgb * light;
    return float4(color * input.color.a, input.color.a);
}
