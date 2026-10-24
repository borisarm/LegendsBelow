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
    /// Handle to an animation clip loaded by RenderingSystem::LoadAnimations (0: no animation)
    /// </summary>
    using AnimationId = std::uint32_t;
    inline constexpr AnimationId InvalidAnimationId = 0;

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
    /// Skinning data of one vertex (glTF JOINTS_0 / WEIGHTS_0): up to four joints of the model's
    /// skeleton and their weights, which add up to 1
    /// </summary>
    struct SkinVertex {
        std::array<std::uint16_t, 4> joints{};
        std::array<float, 4> weights{1.0f, 0.0f, 0.0f, 0.0f};
    };

    /// <summary>
    /// An image decoded to 8-bit RGBA (sRGB color), with its full mip chain: level 0 is the
    /// image, each next level halves the size (box filtered in linear space) down to 1x1. Model
    /// textures drawn on a GPU are block compressed (CompressTexture): BC1 when opaque, BC3 with alpha.
    /// </summary>
    struct ModelTexture {
        /// <summary>How the levels' bytes are laid out</summary>
        enum class Encoding : std::uint8_t {
            Rgba8,   // 4 bytes a pixel
            Bc1,     // 8 bytes a 4x4 block (opaque)
            Bc3,     // 16 bytes a 4x4 block (with alpha)
        };

        struct MipLevel {
            std::uint32_t width{0};
            std::uint32_t height{0};
            std::vector<std::uint8_t> pixels;   // Rgba8: width * height * 4 bytes, rows top to bottom; BC: rows of 4x4 blocks
        };

        std::vector<MipLevel> mips;
        Encoding encoding{Encoding::Rgba8};

        [[nodiscard]] std::uint32_t Width() const noexcept { return mips.empty() ? 0 : mips.front().width; }
        [[nodiscard]] std::uint32_t Height() const noexcept { return mips.empty() ? 0 : mips.front().height; }
    };

    /// <summary>
    /// A glTF metallic-roughness material reduced to what the renderer shades with: a base color
    /// (factor times an optional texture, linear RGBA) and an emissive color added after lighting
    /// </summary>
    struct ModelMaterial {
        std::string name;
        std::array<float, 4> baseColor{1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 3> emissive{0.0f, 0.0f, 0.0f};
        std::optional<std::uint32_t> baseColorTexture;   // into ModelData::textures
    };

    /// <summary>
    /// Rotation quaternion (x, y, z, w), the glTF convention
    /// </summary>
    struct Quaternion {
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
        float w{1.0f};

        [[nodiscard]] static Quaternion Normalize(Quaternion q) noexcept {
            const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            if (length <= 0.0f) return {};
            return { q.x / length, q.y / length, q.z / length, q.w / length };
        }

        /// <summary>
        /// Shortest-path spherical interpolation from a (t = 0) to b (t = 1)
        /// </summary>
        [[nodiscard]] static Quaternion Slerp(Quaternion a, Quaternion b, float t) noexcept {
            float cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
            if (cosine < 0.0f) {
                b = { -b.x, -b.y, -b.z, -b.w };
                cosine = -cosine;
            }

            float wa = 1.0f - t;
            float wb = t;
            if (cosine < 0.9995f) {
                const float angle = std::acos(cosine);
                const float sine = std::sin(angle);
                wa = std::sin((1.0f - t) * angle) / sine;
                wb = std::sin(t * angle) / sine;
            }
            return Normalize({ a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb });
        }

        /// <summary>
        /// Rotation of a radians around a unit axis
        /// </summary>
        [[nodiscard]] static Quaternion FromAxisAngle(Vector3 axis, float radians) noexcept {
            const float s = std::sin(radians * 0.5f);
            return { axis.x * s, axis.y * s, axis.z * s, std::cos(radians * 0.5f) };
        }
    };

    /// <summary>
    /// Row-major 4x4 matrix for row vectors (DirectXMath convention: translation in m[3][0..2]).
    /// glTF's column-major matrices have the same memory layout. A * B applies A, then B.
    /// </summary>
    struct Matrix4 {
        std::array<float, 16> m{1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

        [[nodiscard]] Vector3 TransformPoint(Vector3 p) const noexcept {
            return {
                p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12],
                p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13],
                p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14] };
        }

        [[nodiscard]] Vector3 TransformDirection(Vector3 d) const noexcept {
            return {
                d.x * m[0] + d.y * m[4] + d.z * m[8],
                d.x * m[1] + d.y * m[5] + d.z * m[9],
                d.x * m[2] + d.y * m[6] + d.z * m[10] };
        }

        [[nodiscard]] Vector3 Translation() const noexcept { return { m[12], m[13], m[14] }; }

        [[nodiscard]] friend Matrix4 operator*(const Matrix4& a, const Matrix4& b) noexcept {
            Matrix4 result;
            for (std::size_t row = 0; row < 4; ++row) {
                for (std::size_t column = 0; column < 4; ++column) {
                    result.m[row * 4 + column] =
                        a.m[row * 4 + 0] * b.m[0 * 4 + column] + a.m[row * 4 + 1] * b.m[1 * 4 + column] +
                        a.m[row * 4 + 2] * b.m[2 * 4 + column] + a.m[row * 4 + 3] * b.m[3 * 4 + column];
                }
            }
            return result;
        }

        [[nodiscard]] static Matrix4 Translation(Vector3 t) noexcept {
            Matrix4 result;
            result.m[12] = t.x;
            result.m[13] = t.y;
            result.m[14] = t.z;
            return result;
        }

        [[nodiscard]] static Matrix4 Scaling(float s) noexcept {
            Matrix4 result;
            result.m[0] = result.m[5] = result.m[10] = s;
            return result;
        }

        // Rotations follow DirectXMath (XMMatrixRotationX/Y): yaw 0 keeps +Z, yaw pi/2 turns +Z to +X
        [[nodiscard]] static Matrix4 RotationX(float radians) noexcept {
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            Matrix4 result;
            result.m[5] = c;  result.m[6] = s;
            result.m[9] = -s; result.m[10] = c;
            return result;
        }

        [[nodiscard]] static Matrix4 RotationY(float radians) noexcept {
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            Matrix4 result;
            result.m[0] = c; result.m[2] = -s;
            result.m[8] = s; result.m[10] = c;
            return result;
        }

        /// <summary>
        /// Scale, then rotate, then translate (a glTF node's TRS)
        /// </summary>
        [[nodiscard]] static Matrix4 FromTranslationRotationScale(Vector3 t, Quaternion q, Vector3 s) noexcept {
            const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
            const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
            const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

            Matrix4 result;
            result.m = {
                s.x * (1.0f - 2.0f * (yy + zz)), s.x * 2.0f * (xy + wz),          s.x * 2.0f * (xz - wy),          0.0f,
                s.y * 2.0f * (xy - wz),          s.y * (1.0f - 2.0f * (xx + zz)), s.y * 2.0f * (yz + wx),          0.0f,
                s.z * 2.0f * (xz + wy),          s.z * 2.0f * (yz - wx),          s.z * (1.0f - 2.0f * (xx + yy)), 0.0f,
                t.x,                             t.y,                             t.z,                             1.0f };
            return result;
        }

        /// <summary>
        /// Uniform scale, a turn around Y, then a move: how the renderer places a model in the world
        /// </summary>
        [[nodiscard]] static Matrix4 Placement(Vector3 position, float yawRadians, float scale) noexcept {
            return Scaling(scale) * RotationY(yawRadians) * Translation(position);
        }
    };

    /// <summary>
    /// A bone of a skinned model, in the skin's joint order (the order JOINTS_0 indexes)
    /// </summary>
    struct Joint {
        std::string name;                    // node name, e.g. "handslot.r"
        std::int32_t parent{-1};             // joint index, or -1 for a root joint
        Vector3 translation{};               // rest pose, relative to the parent
        Quaternion rotation{};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        Matrix4 inverseBind;                 // model space -> joint space at bind time
        Matrix4 rootTransform;               // root joints: non-joint ancestors' transform
    };

    /// <summary>
    /// The joint hierarchy of a skinned model. evaluationOrder lists every joint after its parent.
    /// </summary>
    struct Skeleton {
        std::vector<Joint> joints;
        std::vector<std::uint32_t> evaluationOrder;

        [[nodiscard]] bool Empty() const noexcept { return joints.empty(); }

        [[nodiscard]] std::optional<std::uint32_t> FindJoint(std::string_view jointName) const noexcept {
            for (std::size_t i = 0; i < joints.size(); ++i) {
                if (joints[i].name == jointName) return static_cast<std::uint32_t>(i);
            }
            return std::nullopt;
        }
    };

    /// <summary>
    /// One drawable piece: a glTF mesh primitive with its node's model-space transform. Parts keep
    /// their own transforms so rigid characters (body, arms, head) can be posed (PartPose).
    /// Skinned parts are positioned by the skeleton instead (their transform is the identity).
    /// </summary>
    struct ModelPart {
        std::string name;                    // node name, e.g. "character_knightHelmet"
        std::vector<ModelVertex> vertices;   // node-local space (skinned: model space at bind time)
        std::vector<SkinVertex> skin;        // one per vertex for skinned parts, empty for rigid ones
        std::vector<std::uint32_t> indices;  // triangle list
        std::uint32_t materialIndex{0};      // into ModelData::materials
        Matrix4 transform;                   // node-local -> model space

        [[nodiscard]] bool IsSkinned() const noexcept { return !skin.empty(); }
    };

    /// <summary>
    /// A loaded model: parts, materials, textures, an optional skeleton, and model-space bounds
    /// (after node transforms; bind pose for skinned models)
    /// </summary>
    struct ModelData {
        std::string name;                    // file name without extension
        std::vector<ModelPart> parts;
        std::vector<ModelMaterial> materials;
        std::vector<std::shared_ptr<const ModelTexture>> textures;   // shared between models using the same image
        Skeleton skeleton;                   // empty for rigid models
        Vector3 boundsMin{};
        Vector3 boundsMax{};

        [[nodiscard]] bool IsSkinned() const noexcept { return !skeleton.Empty(); }

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

        [[nodiscard]] std::optional<std::uint32_t> FindPartIndex(std::string_view partName) const noexcept {
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (parts[i].name == partName) return static_cast<std::uint32_t>(i);
            }
            return std::nullopt;
        }
    };

    /// <summary>
    /// Which property of a joint an animation channel drives
    /// </summary>
    enum class AnimationPath {
        Translation,
        Rotation,
        Scale
    };

    /// <summary>
    /// Keyframes of one joint property (linear interpolation; rotations are slerped)
    /// </summary>
    struct AnimationChannel {
        std::string joint;                   // joint (node) name, matched against a model's skeleton
        AnimationPath path{AnimationPath::Translation};
        std::vector<float> times;            // seconds, increasing
        std::vector<float> values;           // 3 floats per key (4 for rotations: x, y, z, w)
    };

    /// <summary>
    /// A skeletal animation. Clips drive joints by name, so one clip plays on every model built
    /// on the same rig (the KayKit Rig_Medium characters share their animation files).
    /// </summary>
    struct AnimationClip {
        std::string name;                    // e.g. "Walking_A"
        float durationSeconds{0.0f};
        std::vector<AnimationChannel> channels;
    };

    /// <summary>
    /// One clip at a point in time
    /// </summary>
    struct AnimationLayer {
        AnimationId clip{InvalidAnimationId};
        float timeSeconds{0.0f};
        bool loop{true};                     // false: holds the last frame
    };

    /// <summary>
    /// How a skinned model is posed: the current clip, optionally cross-faded from the previous one
    /// (blend 0 shows only previous, 1 only current). No clip shows the rest pose.
    /// </summary>
    struct AnimationState {
        AnimationLayer current;
        AnimationLayer previous;
        float blend{1.0f};
    };

    /// <summary>
    /// Extra transform of one part of a rigid model, applied in the part's own space (around its
    /// node origin) before the part's transform: e.g. swinging an arm at the shoulder
    /// </summary>
    struct PartPose {
        std::uint32_t part{0};               // into ModelData::parts
        Matrix4 transform;
    };

    /// <summary>
    /// One placed copy of a model, as the renderer hands it to the GPU (instanced per part)
    /// </summary>
    struct ModelInstance {
        Matrix4 world;                                        // model space -> world space
        std::array<float, 4> tint{1.0f, 1.0f, 1.0f, 1.0f};   // multiplies material colors; alpha < 1 fades
        const std::vector<PartPose>* partPoses{nullptr};     // rigid models, may be null
        std::uint32_t paletteOffset{0};                       // skinned models: first joint matrix
    };

} // namespace LB::Systems::Rendering
