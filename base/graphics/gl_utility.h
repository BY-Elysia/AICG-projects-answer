#pragma once

#include <format>
#include <iostream>
#include <source_location>
#include <string>

#include <glad/glad.h>

namespace gfx {
inline GLenum checkGLErrors(std::source_location location = std::source_location::current()) {
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        std::string error;
        switch (errorCode) {
        case GL_INVALID_ENUM:
            error = "INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error = "INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            error = "INVALID_OPERATION";
            break;
#ifndef __EMSCRIPTEN__
        case GL_STACK_OVERFLOW:
            error = "STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            error = "STACK_UNDERFLOW";
            break;
#endif
        case GL_OUT_OF_MEMORY:
            error = "OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
        default:
            error = "UNKNOWN_ERROR";
            break;
        }

        std::cerr << std::format("OpenGL error: {} | {}:{}\n", error, location.file_name(), location.line());
    }

    return errorCode;
}
} // namespace gfx
