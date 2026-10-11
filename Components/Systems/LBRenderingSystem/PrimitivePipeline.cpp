// PrimitivePipeline.cpp - Lit unit primitives (box, cylinder, sphere, cone) drawn with the
// DirectXTK12 BasicEffect: placeholders and simple shapes in the world pass
//
// Internal partition of LB.RenderingSystem.
module;

#include "Direct3D.h"

module LB.RenderingSystem:PrimitivePipeline;

import :ModelPipeline;

namespace LB::Systems::Rendering::Detail
{
    class PrimitivePipeline
    {
    public:
        // Unit shapes by MeshShape value
        static constexpr int ShapeCount = 4;

        // The effect (per-pixel lighting, the scene's lights, almost no specular so the flat KayKit
        // colors read as stone and cloth rather than plastic) and the unit shapes
        void Create(ID3D12Device* device, const DirectX::RenderTargetState& renderTargetState, const SceneLights& lights)
        {
            using namespace DirectX;
            const EffectPipelineStateDescription pipeline(
                &GeometricPrimitive::VertexType::InputLayout,
                CommonStates::Opaque,
                CommonStates::DepthDefault,
                CommonStates::CullCounterClockwise,
                renderTargetState);

            m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::PerPixelLighting, pipeline);
            m_effect->EnableDefaultLighting();
            m_effect->SetSpecularColor(XMVectorSet(0.04f, 0.04f, 0.04f, 1.0f));
            SetLights(lights);

            // Centered on the origin; scaled per draw
            m_shapes[0] = GeometricPrimitive::CreateBox(XMFLOAT3(1.0f, 1.0f, 1.0f));
            m_shapes[1] = GeometricPrimitive::CreateCylinder(1.0f, 1.0f);
            m_shapes[2] = GeometricPrimitive::CreateSphere(1.0f);
            m_shapes[3] = GeometricPrimitive::CreateCone(1.0f, 1.0f);
        }

        void Reset() noexcept
        {
            for (auto& shape : m_shapes)
                shape.reset();
            m_effect.reset();
        }

        [[nodiscard]] bool IsReady() const noexcept { return m_effect != nullptr; }

        void SetLights(const SceneLights& lights)
        {
            if (!m_effect) return;
            using namespace DirectX;
            m_effect->SetAmbientLightColor(XMLoadFloat4(&lights.ambient));
            for (int i = 0; i < 3; ++i)
            {
                m_effect->SetLightDirection(i, XMLoadFloat4(&lights.direction[i]));
                m_effect->SetLightDiffuseColor(i, XMLoadFloat4(&lights.color[i]));
            }
        }

        void SetCamera(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection)
        {
            if (!m_effect) return;
            m_effect->SetView(view);
            m_effect->SetProjection(projection);
        }

        // A unit shape scaled, turned around Y, and moved to a world position, lit with a diffuse
        // color and glowing with an emissive one
        void Draw(ID3D12GraphicsCommandList* commandList, int shape, DirectX::FXMMATRIX world, DirectX::XMFLOAT4 color, DirectX::XMFLOAT3 emissive)
        {
            if (!m_effect || shape < 0 || shape >= ShapeCount || !m_shapes[shape]) return;
            using namespace DirectX;
            m_effect->SetWorld(world);
            m_effect->SetDiffuseColor(XMVectorSet(color.x, color.y, color.z, 1.0f));
            m_effect->SetEmissiveColor(XMVectorSet(emissive.x, emissive.y, emissive.z, 1.0f));
            m_effect->SetAlpha(color.w);
            m_effect->Apply(commandList);
            m_shapes[shape]->Draw(commandList);
        }

    private:
        std::unique_ptr<DirectX::BasicEffect> m_effect;
        std::unique_ptr<DirectX::GeometricPrimitive> m_shapes[ShapeCount];
    };
}
