// Model.ixx - CPU-side 3D model data (loaded from glTF, uploaded to the GPU by the renderer)
export module LB.RenderingSystem:Model;

import std;
import :Camera;

export namespace LB::Systems::Rendering {

    /// <summary>
    /// Handle to a model loaded by RenderingSystem::LoadModel (0 is never a valid model)
    /// </summary>
    using ModelId = std::uint32_t;
    inline constexpr ModelId InvalidModelId = 0;

    /// <summary>
    /// One vertex, laid out like DirectXTK's VertexPositionNormalTexture
    /// </summary>
    struct ModelVertex {
        Vector3 position{};
        Vector3 normal{0.0f, 1.0f, 0.0f};
        float u{0.0f};
        float v{0.0f};
    };

    /// <summary>
    /// A flat-colored material (glTF metallic-roughness base color factor, linear RGBA)
    /// </summary>
    struct ModelMaterial {
        std::string name;
        std::array<float, 4> baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    };

    /// <summary>
    /// Row-major 4x4 matrix for row vectors (DirectXMath convention: translation in m[3][0..2]).
    /// glTF's column-major matrices have the same memory layout.
    /// </summary>
    struct Matrix4 {
        std::array<float, 16> m{1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

        [[nodiscard]] Vector3 TransformPoint(Vector3 p) const noexcept {
            return {
                p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12],
                p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13],
                p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14] };
        }

        [[nodiscard]] Vector3 Translation() const noexcept { return { m[12], m[13], m[14] }; }
    };

    /// <summary>
    /// One drawable piece: a glTF mesh primitive with its node's model-space transform. Parts keep
    /// their own transforms so rigid characters (body, arms, head) can be posed later.
    /// </summary>
    struct ModelPart {
        std::string name;                    // node name, e.g. "character_knightHelmet"
        std::vector<ModelVertex> vertices;   // node-local space
        std::vector<std::uint32_t> indices;  // triangle list
        std::uint32_t materialIndex{0};      // into ModelData::materials
        Matrix4 transform;                   // node-local -> model space
    };

    /// <summary>
    /// A loaded model: parts, materials, and model-space bounds (after node transforms)
    /// </summary>
    struct ModelData {
        std::string name;                    // file name without extension
        std::vector<ModelPart> parts;
        std::vector<ModelMaterial> materials;
        Vector3 boundsMin{};
        Vector3 boundsMax{};

        [[nodiscard]] std::size_t TriangleCount() const noexcept {
            std::size_t count = 0;
            for (const auto& part : parts) count += part.indices.size() / 3;
            return count;
        }

        [[nodiscard]] const ModelPart* FindPart(std::string_view partName) const noexcept {
            for (const auto& part : parts) {
                if (part.name == partName) return &part;
            }
            return nullptr;
        }
    };

    /// <summary>
    /// One placed copy of a model, as the renderer hands it to the GPU (instanced per part)
    /// </summary>
    struct ModelInstance {
        Vector3 position{};
        float yawRadians{0.0f};
        float scale{1.0f};
        std::array<float, 4> tint{1.0f, 1.0f, 1.0f, 1.0f};   // multiplies material colors; alpha < 1 fades
    };

} // namespace LB::Systems::Rendering
