// ModelInstanced_PS.hlsl - pixel shader for instanced model parts (per-pixel lighting)
#include "ModelInstanced.hlsli"

float4 main(PixelInput input) : SV_Target
{
    const float3 normal = normalize(input.normal);

    float3 light = AmbientColor.rgb;
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        light += saturate(dot(normal, -LightDirection[i].xyz)) * LightColor[i].rgb * (i == 0 ? KeyLightVisible(input.world, normal) : 1.0f);
    }

    // Point lights: wrapped diffuse (surfaces turned a little away still catch some light) with a
    // smooth falloff to nothing at the radius
    const int pointLights = min((int)PointLightCount.x, MaxPointLights);
    for (int k = 0; k < pointLights; ++k)
    {
        const float3 toLight = PointLightPosition[k].xyz - input.world;
        const float distance = length(toLight);
        const float radius = PointLightPosition[k].w;
        if (distance >= radius) continue;
        const float fade = saturate(1.0f - (distance * distance) / (radius * radius));
        const float facing = saturate(dot(normal, toLight / max(distance, 1e-3f)) * 0.75f + 0.25f);
        light += PointLightColor[k].rgb * facing * fade * fade;
    }

    // Untextured materials sample a white texel. Emissive light (glowing eyes) ignores the
    // scene lights but follows the instance tint, so it dims with the rest during the pause.
    const float4 albedo = BaseColorTexture.Sample(LinearWrap, input.texcoord) * MaterialColor * input.tint;
    const float3 color = albedo.rgb * light + MaterialEmissive.rgb * input.tint.rgb;

    // Premultiplied alpha, as the transparent pass blends with ONE / INV_SRC_ALPHA
    return float4(color * albedo.a, albedo.a);
}
