#include "raster_scene.h"

#include <cstring>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>

#include <glm/ext.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#include <tiny_gltf_v3.h>

static tg3_str_int_pair const* findAttribute(tg3_primitive const& primitive, char const* name) {
    for (uint32_t i = 0; i < primitive.attributes_count; ++i) {
        if (tg3_str_equals_cstr(primitive.attributes[i].key, name)) {
            return &primitive.attributes[i];
        }
    }
    return nullptr;
}

template <typename T>
static T const*
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

static glm::mat4 nodeTransform(tg3_node const& node) {
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

static std::vector<uint8_t> decodeBase64(tg3_str const& uri) {
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

static void loadTexture(tg3_model const& model, int textureIndex, UnifiedTexture2D& target) {
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
        throw std::runtime_error("rasterizer only supports embedded material textures");
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
    // Both renderers sample level zero with linear filtering and repeat addressing.
    target.cpuSampler = CpuSampler{CpuSampler::Description{
        .minFilter = CpuSampler::Filter::Linear,
        .magFilter = CpuSampler::Filter::Linear,
        .addressModeU = CpuSampler::AddressMode::Repeat,
        .addressModeV = CpuSampler::AddressMode::Repeat,
    }};
    stbi_image_free(decoded);
}

static Mesh loadPrimitive(tg3_model const& model, tg3_primitive const& primitive) {
    if (primitive.mode != -1 && primitive.mode != 4) {
        throw std::runtime_error("rasterizer only supports triangle primitives");
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
        throw std::runtime_error("triangle primitive needs POSITION, NORMAL and TEXCOORD_0");
    }
    if (vertexCount == 0) {
        throw std::runtime_error("triangle primitive has no vertices");
    }

    Mesh mesh;
    auto& vertices = mesh.editVertices();
    vertices.reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        auto const* p =
            reinterpret_cast<float const*>(reinterpret_cast<uint8_t const*>(positions) + i * positionStride);
        auto const* n = reinterpret_cast<float const*>(reinterpret_cast<uint8_t const*>(normals) + i * normalStride);
        auto const* uv = reinterpret_cast<float const*>(reinterpret_cast<uint8_t const*>(uvs) + i * uvStride);
        vertices.push_back({glm::make_vec3(p), glm::make_vec3(n), glm::make_vec2(uv)});
    }
    if (primitive.indices < 0) {
        if (vertexCount % 3 != 0) {
            throw std::runtime_error("triangle primitive vertex count must be divisible by three");
        }
        auto& indices = mesh.editIndices();
        indices.reserve(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            indices.push_back(static_cast<uint32_t>(i));
        }
        return mesh;
    }
    tg3_accessor const& accessor = model.accessors[primitive.indices];
    if (accessor.count == 0 || accessor.count % 3 != 0) {
        throw std::runtime_error("triangle primitive needs a positive multiple of three indices");
    }
    tg3_buffer_view const& view = model.buffer_views[accessor.buffer_view];
    uint8_t const* indexData = model.buffers[view.buffer].data.data + view.byte_offset + accessor.byte_offset;
    int const stride = tg3_accessor_byte_stride(&accessor, &view);
    auto& indices = mesh.editIndices();
    indices.reserve(static_cast<size_t>(accessor.count));
    for (uint64_t i = 0; i < accessor.count; ++i) {
        uint8_t const* value = indexData + i * stride;
        uint32_t index = 0;
        switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
            uint16_t narrow;
            std::memcpy(&narrow, value, sizeof(narrow));
            index = narrow;
            break;
        }
        case TG3_COMPONENT_TYPE_UNSIGNED_INT:
            std::memcpy(&index, value, sizeof(index));
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
            index = *value;
            break;
        default:
            throw std::runtime_error("unsupported index type");
        }
        indices.push_back(index);
    }
    return mesh;
}

static PbrMaterial loadMaterial(tg3_model const& model, tg3_material const& sourceMaterial) {
    PbrMaterial material;
    tg3_pbr_metallic_roughness const& pbr = sourceMaterial.pbr_metallic_roughness;
    material.baseColorFactor = glm::vec4(static_cast<float>(pbr.base_color_factor[0]),
                                         static_cast<float>(pbr.base_color_factor[1]),
                                         static_cast<float>(pbr.base_color_factor[2]),
                                         static_cast<float>(pbr.base_color_factor[3]));
    material.emissiveFactor = glm::vec3(static_cast<float>(sourceMaterial.emissive_factor[0]),
                                        static_cast<float>(sourceMaterial.emissive_factor[1]),
                                        static_cast<float>(sourceMaterial.emissive_factor[2]));
    material.metallicFactor = static_cast<float>(pbr.metallic_factor);
    material.roughnessFactor = static_cast<float>(pbr.roughness_factor);
    material.occlusionStrength = static_cast<float>(sourceMaterial.occlusion_texture.strength);
    int const indices[] = {
        pbr.base_color_texture.index,
        pbr.metallic_roughness_texture.index,
        sourceMaterial.normal_texture.index,
        sourceMaterial.occlusion_texture.index,
        sourceMaterial.emissive_texture.index
    };
    for (int i = 0; i < 5; ++i) {
        material.hasTexture[i] = indices[i] >= 0;
        if (material.hasTexture[i]) {
            loadTexture(model, indices[i], material.textures[i]);
        }
    }
    return material;
}

/*************************************************************************************************/
/*                                         RasterScene                                           */
/*************************************************************************************************/
RasterScene::RasterScene(std::filesystem::path const& filePath) {
    load(filePath);
    uploadTextures();
}

void RasterScene::load(std::filesystem::path const& filePath) {
    tinygltf3::Model parsed;
    tinygltf3::ErrorStack errors;
    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.images_as_is = 1;
    std::string const filePathString = filePath.string();
    if (tinygltf3::parse_file(parsed, errors, filePathString.c_str(), &options) != TG3_OK) {
        std::string message = "Failed to load model: " + filePathString;
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

    m_materials.reserve(model.materials_count + 1);
    for (uint32_t i = 0; i < model.materials_count; ++i) {
        m_materials.push_back(loadMaterial(model, model.materials[i]));
    }
    // glTF permits primitives without a material; use its default metallic-roughness values.
    uint32_t const defaultMaterial = static_cast<uint32_t>(m_materials.size());
    m_materials.emplace_back();

    struct PrimitiveRef {
        size_t mesh;
        size_t material;
    };

    std::vector<std::vector<PrimitiveRef>> primitives(model.meshes_count);
    for (uint32_t meshIndex = 0; meshIndex < model.meshes_count; ++meshIndex) {
        tg3_mesh const& source = model.meshes[meshIndex];
        for (uint32_t primitiveIndex = 0; primitiveIndex < source.primitives_count; ++primitiveIndex) {
            tg3_primitive const& primitive = source.primitives[primitiveIndex];
            if (primitive.material >= 0 && static_cast<uint32_t>(primitive.material) >= model.materials_count) {
                throw std::runtime_error("primitive references a missing material");
            }

            size_t const materialIndex = primitive.material < 0 ? defaultMaterial : primitive.material;
            primitives[meshIndex].push_back({m_meshes.size(), materialIndex});
            m_meshes.push_back(loadPrimitive(model, primitive));
        }
    }

    std::vector<bool> ancestors(model.nodes_count);
    std::function<Node(int32_t)> loadNode = [&](int32_t nodeIndex) -> Node {
        if (nodeIndex < 0 || static_cast<uint32_t>(nodeIndex) >= model.nodes_count) {
            throw std::runtime_error("scene references a missing node");
        }

        if (ancestors[nodeIndex]) {
            throw std::runtime_error("scene node hierarchy contains a cycle");
        }

        ancestors[nodeIndex] = true;
        tg3_node const& source = model.nodes[nodeIndex];
        if (source.mesh >= 0 && static_cast<uint32_t>(source.mesh) >= model.meshes_count) {
            throw std::runtime_error("node references a missing mesh");
        }

        Node node;
        node.localTransform = nodeTransform(source);
        node.mesh = source.mesh;
        node.children.reserve(source.children_count);
        for (uint32_t i = 0; i < source.children_count; ++i) {
            node.children.push_back(loadNode(source.children[i]));
        }

        ancestors[nodeIndex] = false;

        return node;
    };

    tg3_scene const& scene = model.scenes[model.default_scene >= 0 ? model.default_scene : 0];
    m_roots.reserve(scene.nodes_count);
    for (uint32_t i = 0; i < scene.nodes_count; ++i) {
        m_roots.push_back(loadNode(scene.nodes[i]));
    }
    std::function<void(Node const&, glm::mat4 const&)> submitNode = [&](Node const& node, glm::mat4 const& parent) {
        glm::mat4 const world = parent * node.localTransform;
        if (node.mesh >= 0) {
            for (PrimitiveRef const& primitive : primitives[node.mesh]) {
                m_drawItems.push_back({&m_meshes[primitive.mesh], &m_materials[primitive.material], world});
            }
        }
        for (Node const& child : node.children) {
            submitNode(child, world);
        }
    };
    for (Node const& root : m_roots) {
        submitNode(root, glm::mat4{1.0f});
    }
}

void RasterScene::uploadTextures() {
    std::shared_ptr<gfx::Sampler> materialSampler;
    for (PbrMaterial& material : m_materials) {
        for (int i = 0; i < 5; ++i) {
            UnifiedTexture2D& texture = material.textures[i];
            if (!material.hasTexture[i]) {
                continue;
            }
            texture.gpuTexture = std::make_unique<gfx::Texture2D>(
                gfx::Texture2D::Specification{
                    .width = static_cast<uint32_t>(texture.cpuTexture->width()),
                    .height = static_cast<uint32_t>(texture.cpuTexture->height()),
                    .format = gfx::Texture::Format::RGBA8,
                    .pixelFormat = gfx::Texture::ExternalFormat::RGBA,
                    .pixelType = gfx::Texture::PixelType::UByte,
                    .generateMipmap = false,
                    .mipmapLevels = 1,
                    .isImmutable = false,
                },
                texture.cpuTexture->pixels());
            if (!materialSampler) {
                materialSampler = std::make_shared<gfx::Sampler>(gfx::Sampler::Description{
                    .minFilter = gfx::Sampler::Filter::Linear,
                    .magFilter = gfx::Sampler::Filter::Linear,
                    .addressModeU = gfx::Sampler::AddressMode::Repeat,
                    .addressModeV = gfx::Sampler::AddressMode::Repeat,
                });
            }
            texture.gpuSampler = materialSampler;
        }
    }
}
