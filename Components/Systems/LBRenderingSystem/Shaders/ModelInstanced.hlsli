// ModelInstanced.hlsli - instanced, lit, textured model parts (rigid and skinned)
//
// One draw per model part covers every instance of it: the instance stream carries each copy's
// world transform, tint, and (skinned parts) where its joint matrices start in the skin palette.
// Lighting mirrors the DirectXTK BasicEffect setup used for primitives (three directional lights
// plus ambient, no specular), so both look alike.

cbuffer Frame : register(b0)
{
    row_major float4x4 ViewProjection;  // DirectXMath layout (row vectors)
    float4 LightDirection[3];           // xyz: direction the light travels
    float4 LightColor[3];
    float4 AmbientColor;
};

// Per part: the material's base color factor (multiplies the texture) and emissive color
cbuffer Material : register(b1)
{
    float4 MaterialColor;
    float4 MaterialEmissive;
};

Texture2D<float4> BaseColorTexture : register(t0);   // sRGB, sampled as linear
SamplerState LinearWrap : register(s0);

// A row-vector matrix stored as its first three columns: transformed.x = dot(float4(p, 1), c0)...
struct AffineColumns
{
    float4 c0;
    float4 c1;
    float4 c2;
};

float3 TransformPoint(AffineColumns m, float3 p)
{
    const float4 h = float4(p, 1.0f);
    return float3(dot(h, m.c0), dot(h, m.c1), dot(h, m.c2));
}

// Rotation and uniform scale only, so the same 3x3 turns normals
float3 TransformNormal(AffineColumns m, float3 n)
{
    return float3(dot(n, m.c0.xyz), dot(n, m.c1.xyz), dot(n, m.c2.xyz));
}

struct InstanceInput
{
    float4 world0   : WORLD0;    // the instance's world matrix columns (rigid parts)
    float4 world1   : WORLD1;
    float4 world2   : WORLD2;
    float4 tint     : COLOR0;    // linear RGB, alpha < 1 for faded instances
    uint   palette  : PALETTE0;  // first joint matrix of the instance (skinned parts)
};

struct PixelInput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD0;
    float4 tint     : COLOR0;
};
