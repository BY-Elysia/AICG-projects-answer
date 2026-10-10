#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <common/event/event.h>

namespace gfx {
class GraphicsContext;
}

class Window {
public:
    using EventCallbackFunc = std::function<void(Event&)>;

    struct Config {
        std::string title;
        uint32_t width;
        uint32_t height;
        bool vsync;
        bool resizable;
        bool fullscreen;
        bool maximize;
        bool msaa;
        std::pair<int, int> driverVersion;
    };

public:
    explicit Window(Config const& config);

    ~Window();

    Window(Window const&) = delete;

    Window& operator=(Window const&) = delete;

    void* handle() const noexcept {
        return m_handle;
    }

    std::string const& title() const noexcept {
        return m_title;
    }

    uint32_t width() const noexcept {
        return m_width;
    }

    uint32_t height() const noexcept {
        return m_height;
    }

    uint32_t framebufferWidth() const noexcept {
        return m_framebufferWidth;
    }

    uint32_t framebufferHeight() const noexcept {
        return m_framebufferHeight;
    }

    bool isVSync() const noexcept {
        return m_vsync;
    }

    std::string getTitle() const noexcept {
        return m_title;
    }

    void setTitle(std::string_view title);

    void setVSync(bool enabled);

    void setResizable(bool resizable) const;

    void maximize();

    void centerAlign();

    bool shouldClose() const;

    void requestClose() const;

    void close() const;

    void pollEvents() const;

    void swapBuffers() const;

    void setEventCallback(EventCallbackFunc const& callback);

private:
    void* m_handle = nullptr;
    std::unique_ptr<gfx::GraphicsContext> m_graphicsContext;

    std::string m_title;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_framebufferWidth = 0;
    uint32_t m_framebufferHeight = 0;
    bool m_vsync = false;

    EventCallbackFunc m_eventCallback;

    static int s_instanceCount;

private:
    void destroyNativeWindow();

    void registerWindowSizeCallback();

    void registerWindowIconifyCallback();

    void registerFramebufferSizeCallback();

    void registerWindowCloseCallback();

    void registerKeyCallback();

    void registerCharCallback();

    void registerCursorPosCallback();

    void registerScrollCallback();

    void registerMouseButtonCallback();
};
