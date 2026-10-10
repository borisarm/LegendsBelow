// Overlay_VS.hlsl - pixels to clip space for the 2D overlay
#include "Overlay.hlsli"

OverlayPixel main(OverlayVertex input)
{
    OverlayPixel output;
    output.position = float4(input.position.x / ScreenSize.x * 2.0f - 1.0f, 1.0f - input.position.y / ScreenSize.y * 2.0f, 0.0f, 1.0f);
    output.texcoord = input.texcoord;
    output.color = input.color;
    output.text = input.text;
    return output;
}
