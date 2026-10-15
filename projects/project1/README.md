# Project 1: Software Renderer

This project demonstrates the rendering pipeline through two selectable paths. The CPU path transforms vertices, clips triangles, tests pixel coverage and depth, and shades visible pixels in C++. The GPU path performs corresponding work with OpenGL 4.1 shaders, including a transform-feedback vertex pass and geometry and fragment stages. Both paths display the same glTF scene through an OpenGL window.

## Requirements

- CMake 3.20 or newer and Git.
- A C++ compiler and standard library with C++20 support, including `std::format`. Visual Studio 2022 is a suitable choice on Windows.
- A graphics driver that provides an OpenGL 4.1 core context. The application requests a forward-compatible context on macOS.
- [vcpkg](https://github.com/microsoft/vcpkg) for the dependencies listed in [`vcpkg.json`](vcpkg.json). No separate `vcpkg install` command is needed when the CMake toolchain is supplied at configure time.

Run the commands below from the repository root. If you already have vcpkg, use its `scripts/buildsystems/vcpkg.cmake` path instead of cloning another copy. The repository ignores a local `vcpkg/` directory.

### Windows (PowerShell and Visual Studio 2022)

```powershell
git clone https://github.com/microsoft/vcpkg.git vcpkg
.\vcpkg\bootstrap-vcpkg.bat
$vcpkgRoot = (Resolve-Path .\vcpkg).Path

cmake -S projects/project1 -B build/project1 -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake"
cmake --build build/project1 --config Release --target software_renderer
.\build\project1\Release\software_renderer.exe
```

### macOS or Linux (shell)

```sh
git clone https://github.com/microsoft/vcpkg.git vcpkg
./vcpkg/bootstrap-vcpkg.sh

cmake -S projects/project1 -B build/project1 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build/project1 --target software_renderer --parallel
./build/project1/software_renderer
```

These commands use vcpkg's [CMake toolchain integration](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration). Configuration installs the dependencies from this project's manifest. The initial configure and build may take longer while vcpkg builds packages.

## Run and edit

The application opens the included Damaged Helmet scene. Use the **CPU** and **GPU fragment** controls to switch rendering paths; the UI also exposes the directional light's color and intensity.

The executable expects an `assets/` directory beside it. The build creates that directory automatically: non-shader assets are copied, while shaders are hard-linked to their source files when possible and copied otherwise. Shader programs load at startup, so restart the application after changing a shader. If an editor replaces a shader file on save, rebuild to refresh its runtime link.

The key implementation files are [`cpu_renderer.cpp`](src/cpu_renderer.cpp), [`gpu_renderer.cpp`](src/gpu_renderer.cpp), and the [shader directory](assets/shaders/). Student exercises are identified by `TODO` comments in the relevant source files.
