// GltfLoader.cpp - glTF 2.0 model loading with fastgltf
//
// Implementation unit of LB.RenderingSystem (declared in the interface as Detail::LoadGltfModel):
// fastgltf stays in this file's global module fragment and is never imported elsewhere.
// Reads .gltf (embedded or external buffers) and .glb files into ModelData: one part per mesh
// primitive (triangles, POSITION + NORMAL, optional TEXCOORD_0), node transforms resolved to
// model space, and flat base-color materials. Triangles are turned from glTF's counter-clockwise
// front faces to the clockwise ones the renderer uses (DirectXTK convention). Textures, skins, and
// animations are not read yet.
module;

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <vector>

module LB.RenderingSystem;

namespace LB::Systems::Rendering::Detail {

    namespace {
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

        std::string ReadPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, ModelPart& part) {
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
    }

    /// <summary>
    /// Read a .gltf or .glb file. Every problem is reported with the file name; a model with
    /// no drawable geometry is an error.
    /// </summary>
    GltfLoadResult LoadGltfModel(const std::filesystem::path& path) {
        GltfLoadResult result;
        const auto fail = [&](std::string_view reason) {
            result.error = std::format("{}: {}", path.filename().string(), reason);
            return std::move(result);
        };

        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            return fail(fastgltf::getErrorMessage(data.error()));
        }

        fastgltf::Parser parser;
        auto loaded = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);
        if (loaded.error() != fastgltf::Error::None) {
            return fail(fastgltf::getErrorMessage(loaded.error()));
        }
        auto& asset = loaded.get();  // non-const: iterateSceneNodes hands out Node&

        auto& model = result.model;
        model.name = path.stem().string();
        if (model.name.ends_with(".gltf")) model.name.resize(model.name.size() - 5);  // "wall.gltf.glb"

        for (const auto& material : asset.materials) {
            const auto& color = material.pbrData.baseColorFactor;
            model.materials.push_back(ModelMaterial{ std::string(material.name), { color.x(), color.y(), color.z(), color.w() } });
        }
        if (model.materials.empty()) {
            model.materials.push_back(ModelMaterial{ "Default", { 1.0f, 1.0f, 1.0f, 1.0f } });
        }

        // Nodes with meshes, transformed to model space through the scene hierarchy
        std::string error;
        const auto visit = [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
            if (!error.empty() || !node.meshIndex) return;
            const auto& mesh = asset.meshes[*node.meshIndex];
            for (const auto& primitive : mesh.primitives) {
                ModelPart part;
                part.name = std::string(node.name);
                part.transform = ToMatrix(world);
                if (auto reason = ReadPrimitive(asset, primitive, part); !reason.empty()) {
                    error = std::format("mesh '{}': {}", std::string(mesh.name), reason);
                    return;
                }
                MakeFrontFacesClockwise(part);
                if (part.materialIndex >= model.materials.size()) {
                    error = std::format("mesh '{}': material {} does not exist", std::string(mesh.name), part.materialIndex);
                    return;
                }
                model.parts.push_back(std::move(part));
            }
        };

        if (asset.scenes.empty()) {
            return fail("the file has no scene");
        }
        const std::size_t scene = asset.defaultScene.value_or(0);
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

} // namespace LB::Systems::Rendering::Detail
