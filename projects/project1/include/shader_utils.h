#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <graphics/shader_module.h>

inline gfx::ShaderModule loadShader(std::filesystem::path const& path, gfx::ShaderModule::Stage stage) {
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Unable to open shader: " + path.string());
    }
    try {
        return gfx::ShaderModule{std::string{std::istreambuf_iterator<char>{input}, {}}, stage};
    }
    catch (std::exception const& error) {
        throw std::runtime_error("Shader " + path.string() + ": " + error.what());
    }
}
