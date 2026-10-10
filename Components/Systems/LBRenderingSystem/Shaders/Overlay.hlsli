// Overlay.hlsli - screen-space 2D overlay: solid rectangles and text as textured quads
//
// Vertices come in pixels from the top-left corner. Rectangles and the built-in pixel font sample
// their atlas with point filtering (glyph coverage in alpha, plus a white strip for rectangles);
// TrueType text samples a signed distance field (alpha 0.5 on the outline) with linear filtering.
// Colors are linear and blended with straight alpha.

cbuffer Screen : register(b0)
{
    float2 ScreenSize;   // pixels
};

Texture2D<float4> Atlas : register(t0);
SamplerState PointClamp : register(s0);
SamplerState LinearClamp : register(s1);

struct OverlayVertex
{
    float2 position : POSITION;    // pixels
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
    float2 text     : TEXCOORD1;   // x: 0 texel, 1 distance field, 2 with outline; y: outline reach
};

struct OverlayPixel
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
    float2 text     : TEXCOORD1;
};
