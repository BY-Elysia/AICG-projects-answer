#include "raster_scene.h"
#include "texture_utils.h"

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
tg3_str_int_pair const* findAttribute(tg3_primitive const& primitive, char const* name) {
    for (uint32_t i = 0; i < primitive.attributes_count; ++i) {
        if (tg3_str_equals_cstr(primitive.attributes[i].key, name)) {
            return &primitive.attributes[i];
        }
    }
    return nullptr;
}

template <typename T>
T const*
attributeData(tg3_model const& model, tg3_primitive const& primitive, char const* name, size_t& count, int& stride) {
    tg3_str_int_pair const* attribute = findAttribute(primitive, name);
    if (!attribute) {
        return nullptr;
    }
    tg3_accessor const& accessor = model.accessors[attribute->value];
    tg3_buffer_view const& view = model.buffer_views[accessor.buffer_view];
    count = static_cast<size_t>(accessor.count);
    stride = tg3_accessor_byte_stride(&accessor, &view);
    if (stride == 0) {
        stride = tg3_num_components(accessor.type) * sizeof(T);
    }
    return reinterpret_cast<T const*>(model.buffers[view.buffer].data.data + view.byte_offset + accessor.byte_offset);
}

glm::mat4 nodeTransform(tg3_node const& node) {
    if (node.has_matrix) {
        glm::mat4 matrix(1.0f);
        for (int i = 0; i < 16; ++i) {
            matrix[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
        return matrix;
    }
    glm::vec3 const translation(static_cast<float>(node.translation[0]),
                                static_cast<float>(node.translation[1]),
                                static_cast<float>(node.translation[2]));
    glm::quat const rotation(static_cast<float>(node.rotation[3]),
                             static_cast<float>(node.rotation[0]),
                             static_cast<float>(node.rotation[1]),
                             static_cast<float>(node.rotation[2]));
    glm::vec3 const scale(
        static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2]));
    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

std::vector<uint8_t> decodeBase64(tg3_str const& uri) {
    std::string const value(uri.data, uri.len);
    size_t const comma = value.find(',');
    if (comma == std::string::npos || value.find(";base64") == std::string::npos) {
        throw std::runtime_error("unsupported image URI");
    }
    constexpr char const* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> bytes;
    uint32_t accumulator = 0;
    int bits = -8;
    for (size_t i = comma + 1; i < value.size(); ++i) {
        char const character = value[i];
        if (character == '=') {
            break;
        }
        char const* position = std::strchr(alphabet, character);
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

void loadTexture(tg3_model const& model, int textureIndex, UnifiedTexture2D& target) {
    if (textureIndex < 0) {
        return;
    }
    tg3_texture const& texture = model.textures[textureIndex];
    tg3_image const& image = model.images[texture.source];
    uint8_t const* bytes = nullptr;
    size_t byteCount = 0;
    std::vector<uint8_t> dataUriBytes;
    if (image.buffer_view >= 0) {
        tg3_buffer_view const& view = model.buffer_views[image.buffer_view];
        tg3_buffer const& buffer = model.buffers[view.buffer];
        bytes = buffer.data.data + view.byte_offset;
        byteCount = static_cast<size_t>(view.byte_length);
    }
    else if (image.uri.data && image.uri.len && std::string(image.uri.data, image.uri.len).rfind("data:", 0) == 0) {
        dataUriBytes = decodeBase64(image.uri);
        bytes = dataUriBytes.data();
        byteCount = dataUriBytes.size();
    }
    else if (image.image.data && image.image.count) {
        bytes = image.image.data;
        byteCount = static_cast<size_t>(image.image.count);
    }
    else {
        throw std::runtime_error("rasterizer only supports embedded DamagedHelmet textures");
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(bytes, static_cast<int>(byteCount), &width, &height, &channels, 4);
    if (!decoded) {
        throw std::runtime_error("failed to decode a material texture");
    }
    target.cpuTexture = std::make_unique<CpuTexture2D>(CpuTexture2D::InternalFormat::RGBA8,
                                                       width,
                                                       height,
                                                       CpuTexture2D::Format::RedGreenBlueAlpha,
                                                       CpuTexture2D::DataType::UnsignedByte,
                                                       decoded);
    // DamagedHelmet's empty glTF sampler uses the specification's default LINEAR minification
    // filter, so its source asset does not request a generated mip chain.
    target.cpuTexture->setMinFilter(CpuTexture2D::Filter::Linear);
    target.cpuTexture->setMagFilter(CpuTexture2D::Filter::Linear);
    target.cpuTexture->setAddressModeS(CpuTexture2D::AddressMode::Repeat);
    target.cpuTexture->setAddressModeT(CpuTexture2D::AddressMode::Repeat);
    stbi_image_free(decoded);
}

} // namespace

RasterScene::RasterScene(std::string const& filePath) {
    load(filePath);
    uploadTextures();
}

RasterScene::~RasterScene() = default;

void RasterScene::load(std::string const& filePath) {
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

    tg3_model const& model = *parsed.get();
    if (model.scenes_count == 0) {
        throw std::runtime_error("model has no scene");
    }

    tg3_scene const& scene = model.scenes[model.default_scene >= 0 ? model.default_scene : 0];
    if (scene.nodes_count != 1 || model.nodes[scene.nodes[0]].mesh < 0) {
        throw std::runtime_error("rasterizer expects DamagedHelmet's single mesh node");
    }

    tg3_node const& node = model.nodes[scene.nodes[0]];
    tg3_mesh const& mesh = model.meshes[node.mesh];
    if (mesh.primitives_count != 1) {
        throw std::runtime_error("rasterizer expects one primitive");
    }

    tg3_primitive const& primitive = mesh.primitives[0];
    if (primitive.mode != -1 && primitive.mode != 4) {
        throw std::runtime_error("rasterizer only supports triangles");
    }

    size_t vertexCount = 0;
    int positionStride = 0;
    float const* positions = attributeData<float>(model, primitive, "POSITION", vertexCount, positionStride);

    size_t normalCount = 0;
    int normalStride = 0;
    float const* normals = attributeData<float>(model, primitive, "NORMAL", normalCount, normalStride);

    size_t uvCount = 0;
    int uvStride = 0;
    float const* uvs = attributeData<float>(model, primitive, "TEXCOORD_0", uvCount, uvStride);

    if (!positions || !normals || !uvs || normalCount != vertexCount || uvCount != vertexCount) {
        throw std::runtime_error("DamagedHelmet must contain POSITION, NORMAL and TEXCOORD_0");
    }

    m_mesh.transform = nodeTransform(node);
    m_mesh.vertices.reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        auto const* p =
            reinterpret_cast<float const*>(reinterpret_cast<uint8_t const*>(positions) + i * positionStride);
        auto const* n = reinterpret_cast<float const*>(reinterpret_cast<uint8_t const*>(normals) + i * normalStride);
        auto const* uv = reinterpret_cast<float const*>(reinterpret_cast<uint8_t const*>(uvs) + i * uvStride);
        m_mesh.vertices.push_back({glm::make_vec3(p), glm::make_vec3(n), glm::make_vec2(uv)});
    }

    tg3_accessor const& accessor = model.accessors[primitive.indices];
    tg3_buffer_view const& view = model.buffer_views[accessor.buffer_view];
    uint8_t const* indexData = model.buffers[view.buffer].data.data + view.byte_offset + accessor.byte_offset;
    m_mesh.indices.reserve(static_cast<size_t>(accessor.count));
    for (uint64_t i = 0; i < accessor.count; ++i) {
        switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            m_mesh.indices.push_back(reinterpret_cast<uint16_t const*>(indexData)[i]);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_INT:
            m_mesh.indices.push_back(reinterpret_cast<uint32_t const*>(indexData)[i]);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
            m_mesh.indices.push_back(indexData[i]);
            break;
        default:
            throw std::runtime_error("unsupported index type");
        }
    }

    if (primitive.material < 0) {
        throw std::runtime_error("DamagedHelmet needs a PBR material");
    }
    tg3_material const& sourceMaterial = model.materials[primitive.material];
    tg3_pbr_metallic_roughness const& pbr = sourceMaterial.pbr_metallic_roughness;
    m_material.baseColorFactor = glm::vec4(static_cast<float>(pbr.base_color_factor[0]),
                                           static_cast<float>(pbr.base_color_factor[1]),
                                           static_cast<float>(pbr.base_color_factor[2]),
                                           static_cast<float>(pbr.base_color_factor[3]));
    m_material.emissiveFactor = glm::vec3(static_cast<float>(sourceMaterial.emissive_factor[0]),
                                          static_cast<float>(sourceMaterial.emissive_factor[1]),
                                          static_cast<float>(sourceMaterial.emissive_factor[2]));
    m_material.metallicFactor = static_cast<float>(pbr.metallic_factor);
    m_material.roughnessFactor = static_cast<float>(pbr.roughness_factor);
    m_material.occlusionStrength = static_cast<float>(sourceMaterial.occlusion_texture.strength);
    int const indices[] = {
        pbr.base_color_texture.index,
        pbr.metallic_roughness_texture.index,
        sourceMaterial.normal_texture.index,
        sourceMaterial.occlusion_texture.index,
        sourceMaterial.emissive_texture.index
    };
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
        texture.gpuTexture = makeRasterTexture({texture.cpuTexture->width(), texture.cpuTexture->height()},
                                               gfx::Texture::Format::RGBA8,
                                               gfx::Texture::ExternalFormat::RGBA,
                                               gfx::Texture::PixelType::UByte,
                                               texture.cpuTexture->pixels(),
                                               true);
    }
}
