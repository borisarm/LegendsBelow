// GltfLoader.cpp - glTF 2.0 model and animation loading with fastgltf
//
// Implementation unit of LB.RenderingSystem (declared in the interface as Detail::LoadGltfModel and
// Detail::LoadGltfAnimations): fastgltf and stb_image stay in this file's global module fragment
// and are never imported elsewhere.
// Reads .gltf (embedded or external buffers and images) and .glb files into ModelData: one part
// per mesh primitive (triangles, POSITION + NORMAL, optional TEXCOORD_0), node transforms resolved
// to model space, base-color materials with an optional texture (PNG or JPEG, decoded to RGBA with
// a mip chain) and an emissive color, and one skin (joints, inverse bind matrices, JOINTS_0 and
// WEIGHTS_0). Triangles are turned from glTF's counter-clockwise front faces to the clockwise ones
// the renderer uses (DirectXTK convention). Animations are read separately, by joint name, so clips
// from one file play on every model sharing the rig.
module;

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include <stb_image.h>
#pragma warning(pop)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

module LB.RenderingSystem;

namespace LB::Systems::Rendering::Detail {

    namespace {
        constexpr auto ParserOptions = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages
                                     | fastgltf::Options::DecomposeNodeMatrices;

        Matrix4 ToMatrix(const fastgltf::math::fmat4x4& matrix) {
            // glTF matrices are column-major; stored column by column they have the same memory
            // layout as a row-major matrix for row vectors (DirectXMath)
            Matrix4 result;
            for (std::size_t column = 0; column < 4; ++column) {
                for (std::size_t row = 0; row < 4; ++row) {
                    result.m[column * 4 + row] = matrix.col(column)[row];
                }
            }
            return result;
        }

        // Parse a file; the asset, or an error message without the file name
        fastgltf::Expected<fastgltf::Asset> Parse(const std::filesystem::path& path, std::string& error) {
            auto data = fastgltf::GltfDataBuffer::FromPath(path);
            if (data.error() != fastgltf::Error::None) {
                error = std::string(fastgltf::getErrorMessage(data.error()));
                return fastgltf::Expected<fastgltf::Asset>(data.error());
            }

            fastgltf::Parser parser;
            auto loaded = parser.loadGltf(data.get(), path.parent_path(), ParserOptions);
            if (loaded.error() != fastgltf::Error::None) {
                error = std::string(fastgltf::getErrorMessage(loaded.error()));
            }
            return loaded;
        }

        // ---------------------------------------------------------------------------------------
        // Textures
        // ---------------------------------------------------------------------------------------

        // The bytes a buffer holds once loaded (LoadExternalBuffers), or an empty span
        std::span<const std::byte> BufferBytes(const fastgltf::Buffer& buffer) {
            return std::visit([](const auto& source) -> std::span<const std::byte> {
                using Source = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<Source, fastgltf::sources::Array> || std::is_same_v<Source, fastgltf::sources::Vector>) {
                    return { source.bytes.data(), source.bytes.size() };
                }
                else if constexpr (std::is_same_v<Source, fastgltf::sources::ByteView>) {
                    return { source.bytes.data(), source.bytes.size() };
                }
                else {
                    return {};
                }
            }, buffer.data);
        }

        // The encoded (PNG/JPEG) bytes of an image: embedded in a buffer view, a data URI, or an
        // external file loaded by LoadExternalImages
        std::span<const std::byte> EncodedImage(const fastgltf::Asset& asset, const fastgltf::Image& image) {
            return std::visit([&](const auto& source) -> std::span<const std::byte> {
                using Source = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<Source, fastgltf::sources::BufferView>) {
                    const auto& view = asset.bufferViews[source.bufferViewIndex];
                    const auto bytes = BufferBytes(asset.buffers[view.bufferIndex]);
                    if (view.byteOffset + view.byteLength > bytes.size()) return {};
                    return bytes.subspan(view.byteOffset, view.byteLength);
                }
                else if constexpr (std::is_same_v<Source, fastgltf::sources::Array> || std::is_same_v<Source, fastgltf::sources::Vector>) {
                    return { source.bytes.data(), source.bytes.size() };
                }
                else if constexpr (std::is_same_v<Source, fastgltf::sources::ByteView>) {
                    return { source.bytes.data(), source.bytes.size() };
                }
                else {
                    return {};
                }
            }, image.data);
        }

        // FNV-1a: identical image files share one decoded texture
        std::uint64_t Hash(std::span<const std::byte> bytes) {
            std::uint64_t hash = 14695981039346656037ull;
            for (const auto byte : bytes) {
                hash = (hash ^ static_cast<std::uint64_t>(byte)) * 1099511628211ull;
            }
            return hash ^ bytes.size();
        }

        float SrgbToLinear(std::uint8_t value) {
            const float c = static_cast<float>(value) / 255.0f;
            return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }

        std::uint8_t LinearToSrgb(float value) {
            const float c = std::clamp(value, 0.0f, 1.0f);
            const float encoded = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
            return static_cast<std::uint8_t>(std::lround(encoded * 255.0f));
        }

        // Halve each level (2x2 box filter, colors averaged in linear space) down to 1x1
        void BuildMipChain(ModelTexture& texture) {
            std::array<float, 256> toLinear{};
            for (std::size_t i = 0; i < toLinear.size(); ++i) toLinear[i] = SrgbToLinear(static_cast<std::uint8_t>(i));

            while (texture.mips.back().width > 1 || texture.mips.back().height > 1) {
                const auto& source = texture.mips.back();
                ModelTexture::MipLevel level;
                level.width = std::max(1u, source.width / 2);
                level.height = std::max(1u, source.height / 2);
                level.pixels.resize(std::size_t{ level.width } * level.height * 4);

                for (std::uint32_t y = 0; y < level.height; ++y) {
                    for (std::uint32_t x = 0; x < level.width; ++x) {
                        std::array<float, 4> sum{};
                        for (std::uint32_t dy = 0; dy < 2; ++dy) {
                            for (std::uint32_t dx = 0; dx < 2; ++dx) {
                                const std::uint32_t sx = std::min(x * 2 + dx, source.width - 1);
                                const std::uint32_t sy = std::min(y * 2 + dy, source.height - 1);
                                const auto* p = &source.pixels[(std::size_t{ sy } * source.width + sx) * 4];
                                for (std::size_t c = 0; c < 3; ++c) sum[c] += toLinear[p[c]];
                                sum[3] += static_cast<float>(p[3]) / 255.0f;
                            }
                        }
                        auto* out = &level.pixels[(std::size_t{ y } * level.width + x) * 4];
                        for (std::size_t c = 0; c < 3; ++c) out[c] = LinearToSrgb(sum[c] * 0.25f);
                        out[3] = static_cast<std::uint8_t>(std::lround(std::clamp(sum[3] * 0.25f, 0.0f, 1.0f) * 255.0f));
                    }
                }
                texture.mips.push_back(std::move(level));
            }
        }

        // Decode an image (or reuse an identical one decoded before)
        std::shared_ptr<const ModelTexture> DecodeImage(std::span<const std::byte> encoded, TextureCache& cache, std::string& error) {
            const auto key = Hash(encoded);
            if (const auto cached = cache.find(key); cached != cache.end()) return cached->second;

            int width = 0;
            int height = 0;
            int channels = 0;
            // Owned from the start: freed even if copying it out throws
            const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
                stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size()), &width, &height, &channels, 4),
                &stbi_image_free);
            if (!pixels) {
                error = std::format("image could not be decoded ({})", stbi_failure_reason());
                return nullptr;
            }

            auto texture = std::make_shared<ModelTexture>();
            ModelTexture::MipLevel top;
            top.width = static_cast<std::uint32_t>(width);
            top.height = static_cast<std::uint32_t>(height);
            top.pixels.assign(pixels.get(), pixels.get() + std::size_t{ top.width } * top.height * 4);
            texture->mips.push_back(std::move(top));
            BuildMipChain(*texture);

            cache.emplace(key, texture);
            return texture;
        }

        // Materials, with the images their base-color textures use
        std::string ReadMaterials(const fastgltf::Asset& asset, ModelData& model, TextureCache& cache) {
            std::vector<std::optional<std::uint32_t>> textureOfImage(asset.images.size());

            for (const auto& material : asset.materials) {
                const auto& color = material.pbrData.baseColorFactor;
                ModelMaterial result{ std::string(material.name), { color.x(), color.y(), color.z(), color.w() } };
                result.emissive = { material.emissiveFactor.x(), material.emissiveFactor.y(), material.emissiveFactor.z() };

                if (const auto& info = material.pbrData.baseColorTexture) {
                    if (info->texCoordIndex != 0) {
                        return std::format("material '{}': only TEXCOORD_0 textures are supported", result.name);
                    }
                    const auto& texture = asset.textures[info->textureIndex];
                    if (!texture.imageIndex) {
                        return std::format("material '{}': the base color texture has no PNG or JPEG image", result.name);
                    }

                    auto& index = textureOfImage[*texture.imageIndex];
                    if (!index) {
                        const auto encoded = EncodedImage(asset, asset.images[*texture.imageIndex]);
                        if (encoded.empty()) {
                            return std::format("material '{}': the base color image could not be read", result.name);
                        }
                        std::string error;
                        auto decoded = DecodeImage(encoded, cache, error);
                        if (!decoded) return std::format("material '{}': {}", result.name, error);

                        index = static_cast<std::uint32_t>(model.textures.size());
                        model.textures.push_back(std::move(decoded));
                    }
                    result.baseColorTexture = *index;
                }
                model.materials.push_back(std::move(result));
            }

            if (model.materials.empty()) {
                model.materials.push_back(ModelMaterial{ "Default", { 1.0f, 1.0f, 1.0f, 1.0f } });
            }
            return {};
        }

        // ---------------------------------------------------------------------------------------
        // Geometry
        // ---------------------------------------------------------------------------------------

        std::string ReadPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, ModelPart& part, std::size_t jointCount) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                return "only triangle lists are supported";
            }

            const auto position = primitive.findAttribute("POSITION");
            const auto normal = primitive.findAttribute("NORMAL");
            if (position == primitive.attributes.end()) return "primitive has no POSITION";
            if (normal == primitive.attributes.end()) return "primitive has no NORMAL";
            if (!primitive.indicesAccessor) return "primitive has no indices";

            const auto& positions = asset.accessors[position->accessorIndex];
            const auto& normals = asset.accessors[normal->accessorIndex];
            if (normals.count != positions.count) return "NORMAL and POSITION counts differ";

            part.vertices.resize(positions.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, positions,
                [&](fastgltf::math::fvec3 p, std::size_t i) { part.vertices[i].position = { p.x(), p.y(), p.z() }; });
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, normals,
                [&](fastgltf::math::fvec3 n, std::size_t i) { part.vertices[i].normal = { n.x(), n.y(), n.z() }; });

            const auto texcoord = primitive.findAttribute("TEXCOORD_0");
            if (texcoord != primitive.attributes.end() && asset.accessors[texcoord->accessorIndex].count == positions.count) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset, asset.accessors[texcoord->accessorIndex],
                    [&](fastgltf::math::fvec2 t, std::size_t i) { part.vertices[i].u = t.x(); part.vertices[i].v = t.y(); });
            }

            // Skinned meshes: four joints and weights per vertex, weights normalized to add up to 1
            if (jointCount > 0) {
                const auto joints = primitive.findAttribute("JOINTS_0");
                const auto weights = primitive.findAttribute("WEIGHTS_0");
                if (joints == primitive.attributes.end() || weights == primitive.attributes.end()) return "skinned primitive has no JOINTS_0/WEIGHTS_0";
                if (asset.accessors[joints->accessorIndex].count != positions.count || asset.accessors[weights->accessorIndex].count != positions.count) {
                    return "JOINTS_0/WEIGHTS_0 and POSITION counts differ";
                }

                part.skin.resize(positions.count);
                fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec4>(asset, asset.accessors[joints->accessorIndex],
                    [&](fastgltf::math::u16vec4 j, std::size_t i) { part.skin[i].joints = { j.x(), j.y(), j.z(), j.w() }; });
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset, asset.accessors[weights->accessorIndex],
                    [&](fastgltf::math::fvec4 w, std::size_t i) { part.skin[i].weights = { w.x(), w.y(), w.z(), w.w() }; });

                for (auto& vertex : part.skin) {
                    float total = 0.0f;
                    for (std::size_t k = 0; k < 4; ++k) {
                        if (vertex.weights[k] > 0.0f && vertex.joints[k] >= jointCount) return "a vertex uses a joint the skin does not have";
                        total += vertex.weights[k];
                    }
                    if (total <= 0.0f) return "a vertex has no joint weights";
                    for (auto& weight : vertex.weights) weight /= total;
                }
            }

            const auto& indices = asset.accessors[*primitive.indicesAccessor];
            if (indices.count % 3 != 0) return "index count is not a multiple of 3";
            part.indices.resize(indices.count);
            fastgltf::copyFromAccessor<std::uint32_t>(asset, indices, part.indices.data());
            for (auto index : part.indices) {
                if (index >= part.vertices.size()) return "index out of range";
            }

            part.materialIndex = primitive.materialIndex ? static_cast<std::uint32_t>(*primitive.materialIndex) : 0u;
            return {};
        }

        // Determinant of the upper 3x3: negative for mirroring transforms, which already flip winding
        float Determinant3x3(const Matrix4& t) {
            const auto& m = t.m;
            return m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) + m[2] * (m[4] * m[9] - m[5] * m[8]);
        }

        void MakeFrontFacesClockwise(ModelPart& part) {
            if (Determinant3x3(part.transform) < 0.0f) return;
            for (std::size_t i = 0; i + 2 < part.indices.size(); i += 3) {
                std::swap(part.indices[i + 1], part.indices[i + 2]);
            }
        }

        // A rigid mesh hanging from a joint (a helmet on the head bone) becomes a part skinned
        // entirely to that joint: vertices move to bind-pose model space, so the skeleton carries
        // it like the rest of the body
        void BindToJoint(ModelPart& part, std::uint16_t joint) {
            for (auto& vertex : part.vertices) {
                vertex.position = part.transform.TransformPoint(vertex.position);
                const auto n = part.transform.TransformDirection(vertex.normal);
                const float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                vertex.normal = length > 0.0f ? Vector3{ n.x / length, n.y / length, n.z / length } : vertex.normal;
            }
            part.skin.assign(part.vertices.size(), SkinVertex{ { joint, 0, 0, 0 }, { 1.0f, 0.0f, 0.0f, 0.0f } });
            part.transform = Matrix4{};
        }

        // ---------------------------------------------------------------------------------------
        // Skins
        // ---------------------------------------------------------------------------------------

        // Build the skeleton of a skin. nodeWorld holds each scene node's model-space transform,
        // parentOf each node's parent (or SIZE_MAX).
        std::string ReadSkeleton(const fastgltf::Asset& asset, const fastgltf::Skin& skin, const std::vector<Matrix4>& nodeWorld,
                                 const std::vector<std::size_t>& parentOf, Skeleton& skeleton) {
            constexpr auto none = std::numeric_limits<std::size_t>::max();
            if (skin.joints.empty()) return "the skin has no joints";
            if (skin.joints.size() > std::numeric_limits<std::uint16_t>::max()) return "the skin has too many joints";

            std::vector<Matrix4> inverseBind(skin.joints.size());
            if (skin.inverseBindMatrices) {
                const auto& accessor = asset.accessors[*skin.inverseBindMatrices];
                if (accessor.count != skin.joints.size()) return "the skin's inverse bind matrices do not match its joints";
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(asset, accessor,
                    [&](const fastgltf::math::fmat4x4& matrix, std::size_t i) { inverseBind[i] = ToMatrix(matrix); });
            }

            const auto jointOf = [&](std::size_t node) -> std::int32_t {
                const auto it = std::ranges::find(skin.joints, node);
                return it == skin.joints.end() ? -1 : static_cast<std::int32_t>(it - skin.joints.begin());
            };

            std::vector<std::uint32_t> depth(skin.joints.size(), 0);
            for (std::size_t i = 0; i < skin.joints.size(); ++i) {
                const auto nodeIndex = skin.joints[i];
                const auto& node = asset.nodes[nodeIndex];
                const auto* trs = std::get_if<fastgltf::TRS>(&node.transform);
                if (!trs) return std::format("joint '{}' has no decomposable transform", std::string(node.name));

                Joint joint;
                joint.name = std::string(node.name);
                joint.translation = { trs->translation.x(), trs->translation.y(), trs->translation.z() };
                joint.rotation = { trs->rotation.x(), trs->rotation.y(), trs->rotation.z(), trs->rotation.w() };
                joint.scale = { trs->scale.x(), trs->scale.y(), trs->scale.z() };
                joint.inverseBind = inverseBind[i];

                // Nearest ancestor that is a joint; root joints keep their ancestors' transform
                for (auto ancestor = parentOf[nodeIndex]; ancestor != none; ancestor = parentOf[ancestor]) {
                    ++depth[i];
                    if (const auto parent = jointOf(ancestor); parent >= 0) {
                        joint.parent = parent;
                        break;
                    }
                }
                if (joint.parent < 0 && parentOf[nodeIndex] != none) {
                    joint.rootTransform = nodeWorld[parentOf[nodeIndex]];
                }
                skeleton.joints.push_back(std::move(joint));
            }

            skeleton.evaluationOrder.resize(skeleton.joints.size());
            for (std::uint32_t i = 0; i < skeleton.evaluationOrder.size(); ++i) skeleton.evaluationOrder[i] = i;
            std::ranges::stable_sort(skeleton.evaluationOrder, {}, [&](std::uint32_t i) { return depth[i]; });
            return {};
        }
    }

    /// <summary>
    /// Read a .gltf or .glb file. Every problem is reported with the file name; a model with
    /// no drawable geometry is an error.
    /// </summary>
    GltfLoadResult LoadGltfModel(const std::filesystem::path& path, TextureCache& textureCache) {
        GltfLoadResult result;
        const auto fail = [&](std::string_view reason) {
            result.error = std::format("{}: {}", path.filename().string(), reason);
            return std::move(result);
        };

        std::string parseError;
        auto loaded = Parse(path, parseError);
        if (!parseError.empty()) {
            return fail(parseError);
        }
        auto& asset = loaded.get();  // non-const: iterateSceneNodes hands out Node&

        auto& model = result.model;
        model.name = path.stem().string();
        if (model.name.ends_with(".gltf")) model.name.resize(model.name.size() - 5);  // "wall.gltf.glb"

        if (auto error = ReadMaterials(asset, model, textureCache); !error.empty()) {
            return fail(error);
        }

        if (asset.scenes.empty()) {
            return fail("the file has no scene");
        }
        const std::size_t scene = asset.defaultScene.value_or(0);

        // Model-space transform and parent of every node in the scene
        constexpr auto none = std::numeric_limits<std::size_t>::max();
        std::vector<Matrix4> nodeWorld(asset.nodes.size());
        std::vector<std::size_t> parentOf(asset.nodes.size(), none);
        for (std::size_t i = 0; i < asset.nodes.size(); ++i) {
            for (const auto child : asset.nodes[i].children) parentOf[child] = i;
        }
        fastgltf::iterateSceneNodes(asset, scene, fastgltf::math::fmat4x4(), [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
            nodeWorld[static_cast<std::size_t>(&node - asset.nodes.data())] = ToMatrix(world);
        });

        // One skin per model: the skin of the first skinned mesh node
        std::optional<std::size_t> skinIndex;
        for (const auto& node : asset.nodes) {
            if (!node.meshIndex || !node.skinIndex) continue;
            if (skinIndex && *skinIndex != *node.skinIndex) {
                return fail("meshes use more than one skin");
            }
            skinIndex = *node.skinIndex;
        }
        if (skinIndex) {
            if (auto error = ReadSkeleton(asset, asset.skins[*skinIndex], nodeWorld, parentOf, model.skeleton); !error.empty()) {
                return fail(error);
            }
        }

        // The joint a rigid mesh node hangs from (its nearest joint ancestor), in skinned models
        const auto attachedJoint = [&](std::size_t nodeIndex) -> std::optional<std::uint16_t> {
            if (!skinIndex) return std::nullopt;
            const auto& joints = asset.skins[*skinIndex].joints;
            for (auto ancestor = parentOf[nodeIndex]; ancestor != none; ancestor = parentOf[ancestor]) {
                if (const auto it = std::ranges::find(joints, ancestor); it != joints.end()) {
                    return static_cast<std::uint16_t>(it - joints.begin());
                }
            }
            return std::nullopt;
        };

        // Nodes with meshes. Rigid parts are transformed to model space through the scene
        // hierarchy; skinned parts are placed by the skeleton (glTF ignores their node transform).
        std::string error;
        const auto visit = [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
            if (!error.empty() || !node.meshIndex) return;
            const auto& mesh = asset.meshes[*node.meshIndex];
            const std::size_t jointCount = node.skinIndex ? model.skeleton.joints.size() : 0;
            const auto joint = node.skinIndex ? std::nullopt : attachedJoint(static_cast<std::size_t>(&node - asset.nodes.data()));
            for (const auto& primitive : mesh.primitives) {
                ModelPart part;
                part.name = std::string(node.name);
                part.transform = jointCount > 0 ? Matrix4{} : ToMatrix(world);
                if (auto reason = ReadPrimitive(asset, primitive, part, jointCount); !reason.empty()) {
                    error = std::format("mesh '{}': {}", std::string(mesh.name), reason);
                    return;
                }
                MakeFrontFacesClockwise(part);
                if (joint) BindToJoint(part, *joint);
                if (part.materialIndex >= model.materials.size()) {
                    error = std::format("mesh '{}': material {} does not exist", std::string(mesh.name), part.materialIndex);
                    return;
                }
                model.parts.push_back(std::move(part));
            }
        };

        fastgltf::iterateSceneNodes(asset, scene, fastgltf::math::fmat4x4(), visit);
        if (!error.empty()) {
            return fail(error);
        }
        if (model.parts.empty()) {
            return fail("the scene has no meshes");
        }

        // Model-space bounds
        constexpr float big = std::numeric_limits<float>::max();
        model.boundsMin = { big, big, big };
        model.boundsMax = { -big, -big, -big };
        for (const auto& part : model.parts) {
            for (const auto& vertex : part.vertices) {
                const auto p = part.transform.TransformPoint(vertex.position);
                model.boundsMin = { std::min(model.boundsMin.x, p.x), std::min(model.boundsMin.y, p.y), std::min(model.boundsMin.z, p.z) };
                model.boundsMax = { std::max(model.boundsMax.x, p.x), std::max(model.boundsMax.y, p.y), std::max(model.boundsMax.z, p.z) };
            }
        }
        return result;
    }

    /// <summary>
    /// Read every animation of a .gltf or .glb file (linear keyframes on joint translation,
    /// rotation, and scale). A file without animations is an error.
    /// </summary>
    GltfAnimationsResult LoadGltfAnimations(const std::filesystem::path& path) {
        GltfAnimationsResult result;
        const auto fail = [&](std::string_view reason) {
            result.error = std::format("{}: {}", path.filename().string(), reason);
            result.clips.clear();
            return std::move(result);
        };

        std::string parseError;
        auto loaded = Parse(path, parseError);
        if (!parseError.empty()) {
            return fail(parseError);
        }
        const auto& asset = loaded.get();
        if (asset.animations.empty()) {
            return fail("the file has no animations");
        }

        for (const auto& animation : asset.animations) {
            AnimationClip clip;
            clip.name = std::string(animation.name);

            for (const auto& channel : animation.channels) {
                if (!channel.nodeIndex || channel.path == fastgltf::AnimationPath::Weights) continue;
                const auto& sampler = animation.samplers[channel.samplerIndex];
                if (sampler.interpolation != fastgltf::AnimationInterpolation::Linear) {
                    return fail(std::format("animation '{}': only linear interpolation is supported", clip.name));
                }

                AnimationChannel out;
                out.joint = std::string(asset.nodes[*channel.nodeIndex].name);
                out.path = channel.path == fastgltf::AnimationPath::Rotation ? AnimationPath::Rotation
                         : channel.path == fastgltf::AnimationPath::Scale ? AnimationPath::Scale
                         : AnimationPath::Translation;

                const auto& input = asset.accessors[sampler.inputAccessor];
                const auto& output = asset.accessors[sampler.outputAccessor];
                if (input.count == 0 || output.count != input.count) {
                    return fail(std::format("animation '{}': keyframe times and values do not match", clip.name));
                }

                out.times.resize(input.count);
                fastgltf::copyFromAccessor<float>(asset, input, out.times.data());
                if (!std::ranges::is_sorted(out.times)) {
                    return fail(std::format("animation '{}': keyframe times are not increasing", clip.name));
                }

                if (out.path == AnimationPath::Rotation) {
                    out.values.reserve(output.count * 4);
                    fastgltf::iterateAccessor<fastgltf::math::fvec4>(asset, output, [&](fastgltf::math::fvec4 q) {
                        const auto n = Quaternion::Normalize({ q.x(), q.y(), q.z(), q.w() });
                        out.values.insert(out.values.end(), { n.x, n.y, n.z, n.w });
                    });
                }
                else {
                    out.values.reserve(output.count * 3);
                    fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, output, [&](fastgltf::math::fvec3 v) {
                        out.values.insert(out.values.end(), { v.x(), v.y(), v.z() });
                    });
                }

                clip.durationSeconds = std::max(clip.durationSeconds, out.times.back());
                clip.channels.push_back(std::move(out));
            }

            if (clip.channels.empty()) {
                return fail(std::format("animation '{}' animates no joints", clip.name));
            }
            result.clips.push_back(std::move(clip));
        }
        return result;
    }

} // namespace LB::Systems::Rendering::Detail
