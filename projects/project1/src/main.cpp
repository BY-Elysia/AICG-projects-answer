#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include "software_renderer_app.h"

int main(int, char* argv[]) {
    try {
        Application::Config config{
            .windowConfig{
                .title = "Software Renderer",
                .width = 1280,
                .height = 720,
                .vsync = true,
                .resizable = false,
                .fullscreen = false,
                .maximize = false,
                .msaa = false,
                .driverVersion = {4, 1}
            },
            .assetRootDir = std::filesystem::absolute(argv[0]).parent_path() / "assets"
        };

        SoftwareRendererApp app{config};
        app.run();
    }
    catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "Unknown error\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
