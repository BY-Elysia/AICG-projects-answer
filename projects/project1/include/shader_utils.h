#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <graphics/gl_program.h>

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

inline std::unique_ptr<gfx::GLProgram> makeDisplayProgram(std::filesystem::path const& vertexPath,
                                                          std::filesystem::path const& fragmentPath) {
    std::vector<gfx::ShaderModule> modules;
    modules.push_back(loadShader(vertexPath, gfx::ShaderModule::Stage::Vertex));
    modules.push_back(loadShader(fragmentPath, gfx::ShaderModule::Stage::Fragment));
    return std::make_unique<gfx::GLProgram>(modules);
}

inline std::unique_ptr<gfx::GLProgram>
makeTransformFeedbackProgram(std::filesystem::path const& vertexPath,
                             gfx::GLProgram::TransformFeedbackBufferMode bufferMode,
                             std::vector<std::string> const& varyings) {
    std::vector<gfx::ShaderModule> modules;
    modules.push_back(loadShader(vertexPath, gfx::ShaderModule::Stage::Vertex));
    return std::make_unique<gfx::GLProgram>(modules, bufferMode, varyings);
}
