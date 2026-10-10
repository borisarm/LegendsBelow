// Overlay_PS.hlsl - atlas texel or distance field text, tinted
#include "Overlay.hlsli"

float4 main(OverlayPixel input) : SV_Target
{
    if (input.text.x < 0.5f)
    {
        return Atlas.Sample(PointClamp, input.texcoord) * input.color;
    }

    // Distance field: antialias across about one screen pixel around the 0.5 edge
    const float distance = Atlas.Sample(LinearClamp, input.texcoord).a;
    const float width = max(length(float2(ddx(distance), ddy(distance))) * 0.75f, 1e-4f);
    const float fill = smoothstep(0.5f - width, 0.5f + width, distance);
    if (input.text.x < 1.5f)
    {
        return float4(input.color.rgb, input.color.a * fill);
    }

    // Outlined: dark from the outline's outer edge, the text color inside the glyph edge
    const float edge = 0.5f - input.text.y;
    const float outer = smoothstep(edge - width, edge + width, distance);
    const float3 color = input.color.rgb * (fill / max(outer, 1e-4f));
    return float4(color, input.color.a * outer);
}
