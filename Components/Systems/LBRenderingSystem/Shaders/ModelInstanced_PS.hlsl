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

    // Untextured materials sample a white texel. Emissive light (glowing eyes) ignores the
    // scene lights but follows the instance tint, so it dims with the rest during the pause.
    const float4 albedo = BaseColorTexture.Sample(LinearWrap, input.texcoord) * MaterialColor * input.tint;
    const float3 color = albedo.rgb * light + MaterialEmissive.rgb * input.tint.rgb;

    // Premultiplied alpha, as the transparent pass blends with ONE / INV_SRC_ALPHA
    return float4(color * albedo.a, albedo.a);
}
