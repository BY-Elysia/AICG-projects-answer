cmake_minimum_required(VERSION 3.20)

project(Rasterizer LANGUAGES CXX)

find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(TINYGLTF_INCLUDE_DIR tiny_gltf_v3.h REQUIRED)
find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS include/*.h)
file(GLOB_RECURSE PROJECT_ASSETS CONFIGURE_DEPENDS assets/*)
set_source_files_properties(${PROJECT_ASSETS} PROPERTIES HEADER_FILE_ONLY TRUE)

set(BASE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../../base/common/application.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/common/window.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/common/input/input.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/common/input/input_mapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/scene/transform.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/scene/camera/camera.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/graphics_context.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/rhi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/framebuffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/renderbuffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/gl_program.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/sampler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/shader_module.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/buffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/transform_feedback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/vertex_array.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/texture/texture.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/texture/texture2d.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/texture/texture_cubemap.cpp)
file(GLOB_RECURSE BASE_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/../../base/common/*.h
    ${CMAKE_CURRENT_LIST_DIR}/../../base/graphics/*.h
    ${CMAKE_CURRENT_LIST_DIR}/../../base/scene/*.h)

add_executable(Rasterizer
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_ASSETS}
    CMakeLists.txt
    vcpkg.json
    ${BASE_SOURCES}
    ${BASE_HEADERS})

source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_ASSETS}
    CMakeLists.txt
    vcpkg.json)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/../../base PREFIX base FILES ${BASE_SOURCES} ${BASE_HEADERS})

target_compile_features(Rasterizer PRIVATE cxx_std_20)
target_include_directories(Rasterizer PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../../base
    ${Stb_INCLUDE_DIR}
    ${TINYGLTF_INCLUDE_DIR})
target_link_libraries(Rasterizer PRIVATE glad::glad glfw glm::glm imgui::imgui)
add_custom_command(TARGET Rasterizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/assets
        $<TARGET_FILE_DIR:Rasterizer>/assets
    VERBATIM
)

set_property(TARGET Rasterizer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Rasterizer>")
enable_testing()
add_test(NAME rasterizer_backends COMMAND Rasterizer --validate)
