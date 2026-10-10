#include "window.h"

#include "../graphics/graphics_context.h"

#include <common/event/keyboard_event.h>
#include <common/event/mouse_event.h>
#include <common/event/window_event.h>

#include <format>
#include <iostream>
#include <limits>
#include <stdexcept>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

/*************************************************************************************************/
/*                                       GLFW Input Interpretion                                 */
/*************************************************************************************************/
static KeyCode toKeyCode(int glfwKeyCode) {
    switch (glfwKeyCode) {
    case GLFW_KEY_UNKNOWN:
        return KeyCode::Unknown;
    case GLFW_KEY_WORLD_1:
        return KeyCode::Unknown;
    case GLFW_KEY_WORLD_2:
        return KeyCode::Unknown;
    }

    return static_cast<KeyCode>(glfwKeyCode);
}

static MouseButton toMouseButton(int glfwMouseButton) {
    switch (glfwMouseButton) {
    case GLFW_MOUSE_BUTTON_LEFT:
        return MouseButton::Left;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        return MouseButton::Middle;
    case GLFW_MOUSE_BUTTON_RIGHT:
        return MouseButton::Right;
    }

    return MouseButton::Unknown;
}

/*************************************************************************************************/
/*                                             Window                                            */
/*************************************************************************************************/
int Window::s_instanceCount = 0;

Window::Window(Config const& config)
    : m_title(config.title)
    , m_width(config.width)
    , m_height(config.height)
    , m_framebufferWidth(config.width)
    , m_framebufferHeight(config.height) {
    if (config.width > std::numeric_limits<int>::max() || config.height > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("Window dimensions exceed GLFW's supported range");
    }

    bool const isFirstWindow{s_instanceCount == 0};
    if (isFirstWindow) {
        glfwSetErrorCallback([](int error, char const* description) {
            std::cerr << std::format("GLFW error {}: {}\n", error, description);
        });

        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    glfwDefaultWindowHints();

    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, config.msaa ? 4 : 0);

    // Set up window hint based on monitor
    GLFWmonitor* monitor{nullptr};
    if (config.fullscreen || config.maximize) {
        monitor = glfwGetPrimaryMonitor();

        if (monitor == nullptr) {
            if (isFirstWindow) {
                glfwTerminate();
            }
            throw std::runtime_error("glfwGetPrimaryMonitor failure");
        }

        auto const videoMode{glfwGetVideoMode(monitor)};
        if (videoMode == nullptr) {
            if (isFirstWindow) {
                glfwTerminate();
            }
            throw std::runtime_error("glfwGetVideoMode failure");
        }

        if (config.fullscreen) {
            glfwWindowHint(GLFW_DECORATED, false);
            glfwWindowHint(GLFW_RED_BITS, videoMode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, videoMode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, videoMode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate);
        }
        else {
            monitor = nullptr;
            glfwWindowHint(GLFW_DECORATED, true);
            glfwWindowHint(GLFW_MAXIMIZED, true);
        }

        m_width = videoMode->width;
        m_height = videoMode->height;
    }

    // Since OpenGL context is managed by GLFW, we need to specify window hints for
    // correct graphic context creation.
    m_graphicsContext = std::make_unique<gfx::GraphicsContext>(config.driverVersion);

    // Create GLFW window
    m_handle = glfwCreateWindow(int(m_width), int(m_height), m_title.c_str(), monitor, nullptr);
    if (m_handle == nullptr) {
        if (isFirstWindow) {
            glfwTerminate();
        }
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Get actual window size and framebuffer size
    int width, height;
    glfwGetWindowSize((GLFWwindow*)m_handle, &width, &height);
    m_width = static_cast<uint32_t>(width);
    m_height = static_cast<uint32_t>(height);

    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize((GLFWwindow*)m_handle, &framebufferWidth, &framebufferHeight);
    m_framebufferWidth = static_cast<uint32_t>(framebufferWidth);
    m_framebufferHeight = static_cast<uint32_t>(framebufferHeight);

    // Init graphics context
    try {
        m_graphicsContext->init((GLFWwindow*)m_handle);
    }
    catch (...) {
        destroyNativeWindow();
        if (isFirstWindow) {
            glfwTerminate();
        }
        throw;
    }

    // Set window related attributes
    setVSync(config.vsync);
    setResizable(config.resizable);

    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode((GLFWwindow*)m_handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // Set this pointer to glfw window user pointer for callback access
    glfwSetWindowUserPointer((GLFWwindow*)m_handle, this);

    // Set callback functions
    registerWindowSizeCallback();
    registerWindowIconifyCallback();
    registerFramebufferSizeCallback();
    registerWindowCloseCallback();

    registerKeyCallback();
    registerCharCallback();

    registerCursorPosCallback();
    registerScrollCallback();
    registerMouseButtonCallback();

    // Increment instance count to track GLFW initialization and termination
    ++s_instanceCount;
}

Window::~Window() {
    destroyNativeWindow();
    if (s_instanceCount > 0 && --s_instanceCount == 0) {
        glfwTerminate();
    }
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose((GLFWwindow*)m_handle) != 0;
}

void Window::requestClose() const {
    glfwSetWindowShouldClose((GLFWwindow*)m_handle, GLFW_TRUE);
}

void Window::close() const {
    requestClose();
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::swapBuffers() const {
    glfwSwapBuffers((GLFWwindow*)m_handle);
}

void Window::setTitle(std::string_view title) {
    glfwSetWindowTitle((GLFWwindow*)m_handle, title.data());
    m_title = title;
}

void Window::setVSync(bool enabled) {
    if (enabled) {
        glfwSwapInterval(1);
    }
    else {
        glfwSwapInterval(0);
    }

    m_vsync = enabled;
}

void Window::setResizable(bool resizable) const {
    glfwSetWindowAttrib((GLFWwindow*)m_handle, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
}

void Window::maximize() {
    glfwMaximizeWindow((GLFWwindow*)m_handle);
}

void Window::centerAlign() {
    // Wayland deliberately does not allow clients to choose a top-level
    // window position.  Calling glfwSetWindowPos there only emits GLFW 65548.
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        return;
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor == nullptr) {
        std::cerr << "Unable to center window because the primary monitor is unavailable\n";
        return;
    }

    int workAreaX = 0;
    int workAreaY = 0;
    int workAreaWidth = 0;
    int workAreaHeight = 0;
    glfwGetMonitorWorkarea(monitor, &workAreaX, &workAreaY, &workAreaWidth, &workAreaHeight);
    if (workAreaWidth <= 0 || workAreaHeight <= 0) {
        std::cerr << "Unable to center window because the primary monitor work area is unavailable\n";
        return;
    }

    int const x = workAreaX + (workAreaWidth - static_cast<int>(m_width)) / 2;
    int const y = workAreaY + (workAreaHeight - static_cast<int>(m_height)) / 2;
    glfwSetWindowPos((GLFWwindow*)m_handle, x, y);
}

void Window::setEventCallback(EventCallbackFunc const& callback) {
    m_eventCallback = callback;
}

void Window::registerWindowSizeCallback() {
    glfwSetWindowSizeCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, int width, int height) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};
        win->m_width = static_cast<uint32_t>(width);
        win->m_height = static_cast<uint32_t>(height);

        if (win->m_eventCallback) [[likely]] {
            auto event{WindowResizeEvent(win->m_width, win->m_height)};
            win->m_eventCallback(event);
        }
    });
}

void Window::registerWindowIconifyCallback() {
    glfwSetWindowIconifyCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, int iconified) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};
        if (win->m_eventCallback) [[likely]] {
            auto event{WindowIconifyEvent(iconified)};
            win->m_eventCallback(event);
        }
    });
}

void Window::registerFramebufferSizeCallback() {
    glfwSetFramebufferSizeCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, int width, int height) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};
        win->m_framebufferWidth = static_cast<uint32_t>(width);
        win->m_framebufferHeight = static_cast<uint32_t>(height);

        if (win->m_eventCallback) [[likely]] {
            auto event{WindowFramebufferResizeEvent(width, height)};
            win->m_eventCallback(event);
        }
    });
}

void Window::registerWindowCloseCallback() {
    glfwSetWindowCloseCallback((GLFWwindow*)m_handle, [](GLFWwindow* window) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};

        if (win->m_eventCallback) [[likely]] {
            auto event{WindowCloseEvent()};
            win->m_eventCallback(event);
        }
    });
}

void Window::registerKeyCallback() {
    glfwSetKeyCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};

        if (win->m_eventCallback) [[likely]] {
            auto const keyCode{toKeyCode(key)};

            switch (action) {
            case GLFW_PRESS: {
                auto event{KeyPressEvent(keyCode, false)};
                win->m_eventCallback(event);
            } break;
            case GLFW_RELEASE: {
                auto event{KeyReleaseEvent(keyCode)};
                win->m_eventCallback(event);
            } break;
            case GLFW_REPEAT: {
                auto event{KeyPressEvent(keyCode, true)};
                win->m_eventCallback(event);
            } break;
            }
        }
    });
}

void Window::registerCharCallback() {
    glfwSetCharCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, uint32_t codepoint) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};

        if (win->m_eventCallback) [[likely]] {
            auto event{KeyTypeEvent(codepoint)};
            win->m_eventCallback(event);
        }
    });
}

void Window::registerCursorPosCallback() {
    glfwSetCursorPosCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, double x, double y) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};

        if (win->m_eventCallback) [[likely]] {
            auto event{MouseMoveEvent(static_cast<float>(x), static_cast<float>(y))};
            win->m_eventCallback(event);
        }
    });
}

void Window::registerScrollCallback() {
    glfwSetScrollCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, double xOffset, double yOffset) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};

        if (win->m_eventCallback) [[likely]] {
            auto event{MouseScrollEvent(static_cast<float>(xOffset), static_cast<float>(yOffset))};
            win->m_eventCallback(event);
        }
    });
}

void Window::registerMouseButtonCallback() {
    glfwSetMouseButtonCallback((GLFWwindow*)m_handle, [](GLFWwindow* window, int button, int action, int mods) {
        Window* win{reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))};

        if (win->m_eventCallback) [[likely]] {
            auto const mouseButton{toMouseButton(button)};

            switch (action) {
            case GLFW_PRESS: {
                auto event{MouseButtonPressEvent(mouseButton)};
                win->m_eventCallback(event);
            } break;
            case GLFW_RELEASE: {
                auto event{MouseButtonReleaseEvent(mouseButton)};
                win->m_eventCallback(event);
            } break;
            case GLFW_REPEAT: {
                auto event{MouseButtonHoldEvent(mouseButton)};
                win->m_eventCallback(event);
            } break;
            }
        }
    });
}

void Window::destroyNativeWindow() {
    if (m_handle != nullptr) {
        glfwDestroyWindow((GLFWwindow*)m_handle);
        m_handle = nullptr;
    }
}
