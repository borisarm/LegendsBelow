// Particle.hlsli - camera-facing, additive particles (fire, embers)
//
// Each instance is one particle: the vertex shader turns six vertices into a quad facing the
// camera (built from the camera's right and up vectors), the pixel shader multiplies a soft round
// sprite by the particle's color. Glowing particles add to the frame (Particle_PS); dust and smoke
// blend over it with their opacity (ParticleAlpha_PS).

cbuffer ParticleFrame : register(b0)
{
    row_major float4x4 ViewProjection;
    float4 CameraRight;   // xyz: world direction of screen right
    float4 CameraUp;      // xyz: world direction of screen up
};

Texture2D<float4> Sprite : register(t0);
SamplerState LinearClamp : register(s0);

struct ParticleInstance
{
    float4 positionSize : POSITION;   // xyz: world center, w: size (tiles)
    float4 color        : COLOR0;     // linear; alpha: brightness (glowing) or opacity (dust)
};

struct ParticlePixel
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};
