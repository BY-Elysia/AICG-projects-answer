#include "skybox.h"

#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Skybox::Skybox(std::array<std::filesystem::path, 6> const& facePaths) {
    for (size_t i = 0; i < facePaths.size(); ++i) {
        int loadedWidth{};
        int loadedHeight{};
        int channels{};
        auto const& path = facePaths[i];
        auto* pixels = stbi_load(path.string().c_str(), &loadedWidth, &loadedHeight, &channels, STBI_rgb);
        if (pixels == nullptr) {
            throw std::runtime_error("Unable to load skybox texture: " + path.string());
        }
        if (i != 0 && (loadedWidth != width || loadedHeight != height)) {
            stbi_image_free(pixels);
            throw std::runtime_error("Skybox faces must use the same dimensions");
        }
        width = static_cast<uint32_t>(loadedWidth);
        height = static_cast<uint32_t>(loadedHeight);
        faces[i].assign(pixels, pixels + static_cast<size_t>(loadedWidth) * loadedHeight * 3);
        stbi_image_free(pixels);
    }
}
