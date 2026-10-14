#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "software_renderer_app.h"

int main(int argc, char* argv[]) {
    try {
        bool validate = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string_view{argv[i]} == "--validate") {
                validate = true;
            }
            else {
                throw std::invalid_argument("Usage: software_renderer [--validate]");
            }
        }
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
        if (validate) {
            app.validateBackends();
        }
        else {
            app.run();
        }
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
