#include "raster_scene.h"
#include "texture_utils.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#include <glm/ext.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#include <tiny_gltf_v3.h>

namespace {
const tg3_str_int_pair* findAttribute(const tg3_primitive& primitive, const char* name) {
    for (uint32_t i = 0; i < primitive.attributes_count; ++i) {
        if (tg3_str_equals_cstr(primitive.attributes[i].key, name)) {
            return &primitive.attributes[i];
        }
    }
    return nullptr;
}

template <typename T>
const T* attributeData(
    const tg3_model& model, const tg3_primitive& primitive, const char* name, size_t& count,
    int& stride) {
    const tg3_str_int_pair* attribute = findAttribute(primitive, name);
    if (!attribute) {
        return nullptr;
    }
    const tg3_accessor& accessor = model.accessors[attribute->value];
    const tg3_buffer_view& view = model.buffer_views[accessor.buffer_view];
    count = static_cast<size_t>(accessor.count);
    stride = tg3_accessor_byte_stride(&accessor, &view);
    if (stride == 0) {
        stride = tg3_num_components(accessor.type) * sizeof(T);
    }
    return reinterpret_cast<const T*>(
        model.buffers[view.buffer].data.data + view.byte_offset + accessor.byte_offset);
}

glm::mat4 nodeTransform(const tg3_node& node) {
    if (node.has_matrix) {
        glm::mat4 matrix(1.0f);
        for (int i = 0; i < 16; ++i) {
            matrix[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
        return matrix;
    }
    const glm::vec3 translation(
        static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]),
        static_cast<float>(node.translation[2]));
    const glm::quat rotation(
        static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
        static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
    const glm::vec3 scale(
        static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]),
        static_cast<float>(node.scale[2]));
    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation)
           * glm::scale(glm::mat4(1.0f), scale);
}

std::vector<uint8_t> decodeBase64(const tg3_str& uri) {
    const std::string value(uri.data, uri.len);
    const size_t comma = value.find(',');
    if (comma == std::string::npos || value.find(";base64") == std::string::npos) {
        throw std::runtime_error("unsupported image URI");
    }
    constexpr const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> bytes;
    uint32_t accumulator = 0;
    int bits = -8;
    for (size_t i = comma + 1; i < value.size(); ++i) {
        const char character = value[i];
        if (character == '=') {
            break;
        }
        const char* position = std::strchr(alphabet, character);
        if (!position) {
            continue;
        }
        accumulator = (accumulator << 6) + static_cast<int>(position - alphabet);
        bits += 6;
        if (bits >= 0) {
            bytes.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xff));
            bits -= 8;
        }
    }
    return bytes;
}

void loadTexture(const tg3_model& model, int textureIndex, UnifiedTexture2D& target) {
    if (textureIndex < 0) {
        return;
    }
    const tg3_texture& texture = model.textures[textureIndex];
    const tg3_image& image = model.images[texture.source];
    const uint8_t* bytes = nullptr;
    size_t byteCount = 0;
    std::vector<uint8_t> dataUriBytes;
    if (image.buffer_view >= 0) {
        const tg3_buffer_view& view = model.buffer_views[image.buffer_view];
        const tg3_buffer& buffer = model.buffers[view.buffer];
        bytes = buffer.data.data + view.byte_offset;
        byteCount = static_cast<size_t>(view.byte_length);
    } else if (
        image.uri.data && image.uri.len
        && std::string(image.uri.data, image.uri.len).rfind("data:", 0) == 0) {
        dataUriBytes = decodeBase64(image.uri);
        bytes = dataUriBytes.data();
        byteCount = dataUriBytes.size();
    } else if (image.image.data && image.image.count) {
        bytes = image.image.data;
        byteCount = static_cast<size_t>(image.image.count);
    } else {
        throw std::runtime_error("rasterizer only supports embedded DamagedHelmet textures");
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded =
        stbi_load_from_memory(bytes, static_cast<int>(byteCount), &width, &height, &channels, 4);
    if (!decoded) {
        throw std::runtime_error("failed to decode a material texture");
    }
    target.cpuTexture = std::make_unique<CpuTexture2D>(
        CpuTexture2D::InternalFormat::RGBA8, width, height,
        CpuTexture2D::Format::RedGreenBlueAlpha, CpuTexture2D::DataType::UnsignedByte, decoded);
    // DamagedHelmet's empty glTF sampler uses the specification's default LINEAR minification
    // filter, so its source asset does not request a generated mip chain.
    target.cpuTexture->setMinFilter(CpuTexture2D::Filter::Linear);
    target.cpuTexture->setMagFilter(CpuTexture2D::Filter::Linear);
    target.cpuTexture->setAddressModeS(CpuTexture2D::AddressMode::Repeat);
    target.cpuTexture->setAddressModeT(CpuTexture2D::AddressMode::Repeat);
    stbi_image_free(decoded);
}

struct ClipVertex {
    glm::vec4 clip;
    glm::vec3 worldPosition;
    glm::vec3 normal;
    glm::vec2 uv;
};

ClipVertex interpolate(const ClipVertex& a, const ClipVertex& b, float t) {
    return {
        glm::mix(a.clip, b.clip, t), glm::mix(a.worldPosition, b.worldPosition, t),
        glm::normalize(glm::mix(a.normal, b.normal, t)), glm::mix(a.uv, b.uv, t)};
}

template <typename Distance>
void clipAgainst(std::vector<ClipVertex>& polygon, Distance distance) {
    std::vector<ClipVertex> result;
    if (polygon.empty()) {
        return;
    }
    ClipVertex previous = polygon.back();
    float previousDistance = distance(previous);
    for (const ClipVertex& current : polygon) {
        const float currentDistance = distance(current);
        const bool currentInside = currentDistance >= 0.0f;
        const bool previousInside = previousDistance >= 0.0f;
        if (currentInside != previousInside) {
            const float t = previousDistance / (previousDistance - currentDistance);
            result.push_back(interpolate(previous, current, t));
        }
        if (currentInside) {
            result.push_back(current);
        }
        previous = current;
        previousDistance = currentDistance;
    }
    polygon = std::move(result);
}
}

RasterScene::RasterScene(const std::string& filePath) {
    load(filePath);
    uploadTextures();
}

RasterScene::~RasterScene() = default;

void RasterScene::load(const std::string& filePath) {
    tinygltf3::Model parsed;
    tinygltf3::ErrorStack errors;
    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.images_as_is = 1;
    if (tinygltf3::parse_file(parsed, errors, filePath.c_str(), &options) != TG3_OK) {
        std::string message = "Failed to load model: " + filePath;
        for (uint32_t i = 0; i < errors.count(); ++i) {
            if (errors.entry(i)->message) {
                message += "\n" + std::string(errors.entry(i)->message);
            }
        }
        throw std::runtime_error(message);
    }
    const tg3_model& model = *parsed.get();
    if (model.scenes_count == 0) {
        throw std::runtime_error("model has no scene");
    }
    const tg3_scene& scene = model.scenes[model.default_scene >= 0 ? model.default_scene : 0];
    if (scene.nodes_count != 1 || model.nodes[scene.nodes[0]].mesh < 0) {
        throw std::runtime_error("rasterizer expects DamagedHelmet's single mesh node");
    }

    const tg3_node& node = model.nodes[scene.nodes[0]];
    const tg3_mesh& mesh = model.meshes[node.mesh];
    if (mesh.primitives_count != 1) {
        throw std::runtime_error("rasterizer expects one primitive");
    }
    const tg3_primitive& primitive = mesh.primitives[0];
    if (primitive.mode != -1 && primitive.mode != 4) {
        throw std::runtime_error("rasterizer only supports triangles");
    }

    size_t vertexCount = 0;
    int positionStride = 0;
    const float* positions =
        attributeData<float>(model, primitive, "POSITION", vertexCount, positionStride);
    size_t normalCount = 0;
    int normalStride = 0;
    const float* normals =
        attributeData<float>(model, primitive, "NORMAL", normalCount, normalStride);
    size_t uvCount = 0;
    int uvStride = 0;
    const float* uvs = attributeData<float>(model, primitive, "TEXCOORD_0", uvCount, uvStride);
    if (!positions || !normals || !uvs || normalCount != vertexCount || uvCount != vertexCount) {
        throw std::runtime_error("DamagedHelmet must contain POSITION, NORMAL and TEXCOORD_0");
    }

    const glm::mat4 transform = nodeTransform(node);
    const glm::mat3 normalTransform = glm::transpose(glm::inverse(glm::mat3(transform)));
    m_vertices.reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        const auto* p = reinterpret_cast<const float*>(
            reinterpret_cast<const uint8_t*>(positions) + i * positionStride);
        const auto* n = reinterpret_cast<const float*>(
            reinterpret_cast<const uint8_t*>(normals) + i * normalStride);
        const auto* uv =
            reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(uvs) + i * uvStride);
        m_vertices.push_back(
            {glm::vec3(transform * glm::vec4(glm::make_vec3(p), 1.0f)),
             glm::normalize(normalTransform * glm::make_vec3(n)), glm::make_vec2(uv)});
    }

    const tg3_accessor& accessor = model.accessors[primitive.indices];
    const tg3_buffer_view& view = model.buffer_views[accessor.buffer_view];
    const uint8_t* indexData =
        model.buffers[view.buffer].data.data + view.byte_offset + accessor.byte_offset;
    m_indices.reserve(static_cast<size_t>(accessor.count));
    for (uint64_t i = 0; i < accessor.count; ++i) {
        switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            m_indices.push_back(reinterpret_cast<const uint16_t*>(indexData)[i]);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_INT:
            m_indices.push_back(reinterpret_cast<const uint32_t*>(indexData)[i]);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE: m_indices.push_back(indexData[i]); break;
        default: throw std::runtime_error("unsupported index type");
        }
    }

    if (primitive.material < 0) {
        throw std::runtime_error("DamagedHelmet needs a PBR material");
    }
    const tg3_material& sourceMaterial = model.materials[primitive.material];
    const tg3_pbr_metallic_roughness& pbr = sourceMaterial.pbr_metallic_roughness;
    m_material.baseColorFactor = glm::vec4(
        static_cast<float>(pbr.base_color_factor[0]), static_cast<float>(pbr.base_color_factor[1]),
        static_cast<float>(pbr.base_color_factor[2]), static_cast<float>(pbr.base_color_factor[3]));
    m_material.emissiveFactor = glm::vec3(
        static_cast<float>(sourceMaterial.emissive_factor[0]),
        static_cast<float>(sourceMaterial.emissive_factor[1]),
        static_cast<float>(sourceMaterial.emissive_factor[2]));
    m_material.metallicFactor = static_cast<float>(pbr.metallic_factor);
    m_material.roughnessFactor = static_cast<float>(pbr.roughness_factor);
    m_material.occlusionStrength = static_cast<float>(sourceMaterial.occlusion_texture.strength);
    const int indices[] = {
        pbr.base_color_texture.index, pbr.metallic_roughness_texture.index,
        sourceMaterial.normal_texture.index, sourceMaterial.occlusion_texture.index,
        sourceMaterial.emissive_texture.index};
    for (int i = 0; i < 5; ++i) {
        m_material.hasTexture[i] = indices[i] >= 0;
        if (m_material.hasTexture[i]) {
            loadTexture(model, indices[i], m_material.textures[i]);
        }
    }
}

void RasterScene::uploadTextures() {
    for (int i = 0; i < 5; ++i) {
        UnifiedTexture2D& texture = m_material.textures[i];
        if (!m_material.hasTexture[i]) {
            continue;
        }
        texture.gpuTexture = makeRasterTexture(
            {texture.cpuTexture->width(), texture.cpuTexture->height()},
            gfx::Texture::Format::RGBA8, gfx::Texture::ExternalFormat::RGBA,
            gfx::Texture::PixelType::UByte, texture.cpuTexture->pixels(), true);
    }
}

std::vector<PreparedTriangle> RasterScene::prepare(const FrameState& frame) const {
    std::vector<PreparedTriangle> result;
    result.reserve(m_indices.size() / 3);
    const glm::mat3 normalTransform = glm::transpose(glm::inverse(glm::mat3(frame.model)));
    const glm::mat4 modelViewProjection = frame.projection * frame.view * frame.model;
    for (size_t index = 0; index + 2 < m_indices.size(); index += 3) {
        std::vector<ClipVertex> polygon;
        polygon.reserve(8);
        for (int corner = 0; corner < 3; ++corner) {
            const RasterVertex& source = m_vertices[m_indices[index + corner]];
            polygon.push_back(
                {modelViewProjection * glm::vec4(source.position, 1.0f),
                 glm::vec3(frame.model * glm::vec4(source.position, 1.0f)),
                 glm::normalize(normalTransform * source.normal), source.uv});
        }
        clipAgainst(polygon, [](const ClipVertex& v) {
            return v.clip.x + v.clip.w;
        });
        clipAgainst(polygon, [](const ClipVertex& v) {
            return v.clip.w - v.clip.x;
        });
        clipAgainst(polygon, [](const ClipVertex& v) {
            return v.clip.y + v.clip.w;
        });
        clipAgainst(polygon, [](const ClipVertex& v) {
            return v.clip.w - v.clip.y;
        });
        clipAgainst(polygon, [](const ClipVertex& v) {
            return v.clip.z + v.clip.w;
        });
        clipAgainst(polygon, [](const ClipVertex& v) {
            return v.clip.w - v.clip.z;
        });
        for (size_t triangle = 1; triangle + 1 < polygon.size(); ++triangle) {
            const ClipVertex clipped[] = {polygon[0], polygon[triangle], polygon[triangle + 1]};
            PreparedTriangle output{};
            for (int corner = 0; corner < 3; ++corner) {
                const float reciprocalW = 1.0f / clipped[corner].clip.w;
                const glm::vec3 ndc = glm::vec3(clipped[corner].clip) * reciprocalW;
                output.vertices[corner].screenAndInvW = glm::vec4(
                    (ndc.x * 0.5f + 0.5f) * frame.extent.x, (ndc.y * 0.5f + 0.5f) * frame.extent.y,
                    ndc.z * 0.5f + 0.5f, reciprocalW);
                output.vertices[corner].worldPosition =
                    glm::vec4(clipped[corner].worldPosition, 1.0f);
                output.vertices[corner].normal = glm::vec4(clipped[corner].normal, 0.0f);
                output.vertices[corner].uv = glm::vec4(clipped[corner].uv, 0.0f, 0.0f);
            }
            const glm::vec3 edge1 = clipped[1].worldPosition - clipped[0].worldPosition;
            const glm::vec3 edge2 = clipped[2].worldPosition - clipped[0].worldPosition;
            const glm::vec2 deltaUv1 = clipped[1].uv - clipped[0].uv;
            const glm::vec2 deltaUv2 = clipped[2].uv - clipped[0].uv;
            const float determinant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
            if (std::abs(determinant) > 1e-8f) {
                output.tangent = glm::vec4(
                    glm::normalize((edge1 * deltaUv2.y - edge2 * deltaUv1.y) / determinant), 0.0f);
                output.bitangent = glm::vec4(
                    glm::normalize((-edge1 * deltaUv2.x + edge2 * deltaUv1.x) / determinant), 0.0f);
            }
            const glm::vec2 a(output.vertices[0].screenAndInvW);
            const glm::vec2 b(output.vertices[1].screenAndInvW);
            const glm::vec2 c(output.vertices[2].screenAndInvW);
            if (glm::cross(glm::vec3(b - a, 0.0f), glm::vec3(c - a, 0.0f)).z > 0.0f) {
                result.push_back(output);
            }
        }
    }
    return result;
}
