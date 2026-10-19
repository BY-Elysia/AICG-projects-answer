#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

class Skybox {
public:
    explicit Skybox(std::array<std::filesystem::path, 6> const& facePaths);

    uint32_t width{0};
    uint32_t height{0};
    std::array<std::vector<uint8_t>, 6> faces;
};
