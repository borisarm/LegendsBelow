// ModelInstanced.hlsli - instanced, lit, flat-colored model parts
//
// One draw per model part covers every instance of it: the instance stream carries each copy's
// world transform and color. Lighting mirrors the DirectXTK BasicEffect setup used for
// primitives (three directional lights plus ambient, no specular), so both look alike.

cbuffer Frame : register(b0)
{
    row_major float4x4 ViewProjection;  // DirectXMath layout (row vectors)
    float4 LightDirection[3];           // xyz: direction the light travels
    float4 LightColor[3];
    float4 AmbientColor;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD0;

    // Per instance: the first three columns of the row-vector world matrix, stored as rows,
    // so worldPosition.x = dot(float4(position, 1), world0) and so on
    float4 world0   : WORLD0;
    float4 world1   : WORLD1;
    float4 world2   : WORLD2;
    float4 color    : COLOR0;   // linear RGB, alpha < 1 for faded instances
};

struct PixelInput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float4 color    : COLOR0;
};
